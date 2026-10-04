# Let friends watch movies from anywhere (without installing a VPN on their devices)

## Why forwarding ports directly from the router to the kernel is not recommended

It is possible, but I do not recommend it. The reasons are straightforward:

1. **This kernel's network stack is new and has never been deeply tested (fuzzed).** lwIP itself is a relatively stable library, but the parts we wrote ourselves (HTTP server, virtio driver) haven't been seriously attacked yet. Opening it to the internet means every machine on the globe can scan for vulnerabilities at any time (this is normal, port-scanning bots are always active across the internet).
2. **No TLS.** Data (including typed URLs) travels unencrypted. Anyone in the middle (ISP, public Wi-Fi your friend uses to watch) can intercept and see it.
3. **Port forwarding opens an access channel to your home router.** Most home routers are not updated with firmware regularly. Opening ports increases the attack surface, even if forwarded only to the Arch machine.
4. Your home IP will be exposed to anyone who sees the URL (not a huge problem, but you should be aware).

## Recommended method: Cloudflare Tunnel (Free, no port forwarding needed, provides HTTPS)

With this method, your friend just opens a URL in a normal browser. **No need to install a VPN or any app.** Security is handled on our end, not your friend's:
- Your Arch machine only connects **outbound** to Cloudflare (no ports are opened inbound to your router at all).
- Cloudflare provides HTTPS/TLS for free.
- You can add **Cloudflare Access** (forcing a login with an email you authorize before viewing). This truly restricts access to just the team.

```bash
# Install (once)
sudo pacman -S cloudflared     # or yay -S cloudflared-bin if not in the main repo

# Can be used without owning a domain (quick tunnel, random URL like *.trycloudflare.com)
cloudflared tunnel --url http://localhost:8080
You will get a URL like https://random-words-1234.trycloudflare.com to give to your friends to open. Once the tunnel is closed, the URL can no longer be used (opening it again generates a new URL every time).

If you want a static URL + restrict access to the team only, you need your own domain (or a free domain from Freenom/others) and set up a named tunnel — this involves more detailed steps. Let me know if you want to do this, and I will guide you step-by-step.

A simpler but less stable alternative: ngrok
Bash
# Sign up for free at ngrok.com first
ngrok http 8080
Free tier limitations: The URL changes every time it is restarted, and there are monthly limits on bandwidth/usage time. It's better suited for quick testing rather than keeping it open for a long time.

If you just want to watch it during the presentation (no advance preparation needed)
The easiest way is to refer back to docs/CROSS-PLATFORM.md using the "Same Wi-Fi" method — open a mobile hotspot for everyone to connect to during the presentation. No need to rely on home internet or external services. This is the lowest risk since it is not actually exposed to the internet.

If you still insist on forwarding ports directly
Not recommended, but if absolutely necessary (e.g., the professor requests a demo that is accessible at all times), mitigate the risks by:

Use a port other than 8080/80 (makes it slightly harder to guess. It's not real security, but it reduces hits from generic scanners).

Open it only during the demo and close it immediately after use. Do not leave it open permanently.

Update your router's firmware to the latest version.

Keep an eye on the kernel logs (ps, mem in the shell) for any anomalies while it's open.

If you choose this path, run "make url" to check your IP, then go set up port forwarding in your router's management web page (the menu is usually called "Port Forwarding" or "Virtual Server") to forward the external port to the IP and port of your Arch machine.