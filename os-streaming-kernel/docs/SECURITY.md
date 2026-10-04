# Security: What's checked, fixed, and left to do

This doc covers the security audit of our own code (`net/httpd.c`, `net/net.c`, virtio drivers), not an attack on anyone else's system.
**Scope: We only tested the code in this project, running on our own QEMU.** (Pentesting systems you don't own is illegal.)

## Found and Fixed

### 1. No connection limit — could lead to memory exhaustion (DoS)

**The issue:** `cb_accept` was calling `kmalloc` every time a client connected, with no upper limit. If someone left a bunch of connections open (like a script opening 1000 connections and sending nothing), our 4 MB heap would dry up. This would crash both the HTTP server and any other kernel parts sharing that heap.
**The fix:** Added an `nconn` counter and capped it at `MAX_CONN=24`. Anything beyond that gets immediately rejected with a `tcp_abort` (so we don't waste time on a full handshake).
**Tested:** Fired 60 simultaneous lingering connections at it (well over the 24 cap). During the flood, `/status` and normal video streaming still worked perfectly. No memory leaks after the connections closed.

*(Side note: While fixing this, I made a typo and decremented `nconn` twice per closed connection, which messed up the count and would leak the cap over time. I caught and fixed it before pushing, but just mentioning it in case you wonder why there are 2 commits for this.)*

## Checked and looking good (so far)

* **Path traversal** (`../../etc/passwd` in the URL): `tarfs_find` uses `strcmp` to match the exact file name. It doesn't parse `..` as a path, so it just returns a 404 Not Found. You can't break out of `www/` because our "filesystem" doesn't actually understand directory paths.
* **Buffer overflow from huge requests:** Verified that if `hs->req[1024]` hits 1023 bytes without seeing a `\r\n\r\n`, it immediately drops the connection (`cb_recv`).
* **Weird Header/Range requests:** Tested `Range: bytes=-500`, `Range: bytes=99999999999-`, missing Ranges, and mixed-up headers. The server handled them correctly (returning 206/416/200) without panicking.
* **Downloading a 30 MB file with 3 concurrent connections:** The sha256 hashes matched perfectly. No data bled across connections (sharing `iobuf` is safe because all callbacks run sequentially in a single thread, so two requests are never actually processed at the exact same millisecond).
* **Reflected user input:** Misspelled paths aren't reflected back on the error page (the 404 message is static text). So there's no way to inject HTML/JS back to other browsers (reflected XSS). *Disclaimer: We only tested the basics here, not every edge case.*

## Left to do / Shouldn't do without a good reason

* **No TLS (HTTPS):** All data is sent in plaintext. Anyone on the same network (like public Wi-Fi) can snoop on the traffic. Baking TLS directly into the kernel is a massive job (we'd need a TLS library, a truly secure random number generator, and certificate management). **I highly recommend NOT doing this ourselves for this project.** If you need HTTPS, just use Cloudflare Tunnel (check `docs/PUBLIC-ACCESS.md`) since Cloudflare handles the TLS for us.
* **No authentication:** Anyone with the URL can view the stream. If we want to restrict access, we should do it at the front-end layer (like Cloudflare Access), not in the kernel.
* **No rate limit per IP** (we only have a global cap): Our fix prevents connection flood DoS, but a single IP can still spam requests (up to their own MAX_CONN limit).
* **lwIP and our code haven't been fuzzed:** There's no guarantee we don't have undiscovered memory vulnerabilities (like buffer overflows). Networking code does a lot of parsing, which makes it a prime target for these kinds of bugs.
* **UART shell has no password:** Anyone with access to the serial console (e.g., wiring UART to a real Pi 5 later on) can run any command. Not a big deal right now since UART isn't exposed to the network.

**TL;DR for professors/teammates:** This is a hobby kernel. We only tested for major, predictable vulnerabilities (connection floods, obvious buffer overflows, path traversal). It is NOT 100% secure. Don't use it to store sensitive data or expose it directly to the public internet without extra protection (see `docs/PUBLIC-ACCESS.md`).

## Newly Fixed: Missing HTTP keep-alive (Discovered while using Cloudflare Tunnel)

**The issue:** The server was immediately closing the TCP connection after responding to every request (always sending `Connection: close`). `cloudflared` (and standard reverse proxies) pools connections to reuse them and save overhead. When it tried to reuse a connection we had already closed, the proxy threw an `EOF` error. This made the Cloudflare Tunnel super unstable (it would start breaking after a short while).

**The fix:** Implemented standard HTTP/1.1 keep-alive. We now read the `Connection` header from the incoming request. If it's HTTP/1.1 and doesn't explicitly ask to `close`, we keep the TCP connection alive after replying and just clear the state to wait for the next request on that same connection. (HTTP/1.0 still closes by default so we don't break older clients).

*(Side bugs caught during this fix):*

1. I realized the idle timer (`polls`) was never being reset when data was actually sent or received. This meant that connections actively doing work for over ~60 seconds (like streaming a large file over a slow link, or long-lived keep-alive connections) were getting randomly dropped. Fixed this by resetting the counter whenever real data moves in or out.
2. While writing the keep-alive logic, I accidentally placed the `HTTP/1.1` check *after* the code that truncates the path string (which overwrote the space before `HTTP/1.1` with a `\0`). The check was always failing because the string was cut off. Moved the check up before the string gets truncated.

**Tested:** Successfully sent 3 consecutive requests (status, status, and a Range request on a video) over a single TCP connection. Verified that HTTP/1.0 still closes correctly, explicit `Connection: close` requests (HTTP/1.1) close correctly, and ran through all the old tests (Range, 404, connection floods, and flood recovery)—everything still passes with flying colors.