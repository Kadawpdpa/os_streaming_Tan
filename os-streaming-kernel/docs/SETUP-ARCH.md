# Arch Linux Setup

## 1. Install Tools

```bash
scripts/setup-arch.sh

```

Or run manually:

```bash
sudo pacman -S --needed qemu-system-aarch64 \
     aarch64-linux-gnu-gcc aarch64-linux-gnu-binutils aarch64-linux-gnu-gdb \
     make git tar curl ffmpeg

```

| Package | Purpose | Required? |
| --- | --- | --- |
| `qemu-system-aarch64` | ARM64 emulator | Yes |
| `aarch64-linux-gnu-gcc`, `-binutils` | Compile/link the kernel (cross-compiler) | Yes |
| `make`, `tar` | Build the OS and create `disk.img` | Yes |
| `aarch64-linux-gnu-gdb` | Debug the kernel (`make debug`) | Recommended |
| `curl` | Test HTTP from the terminal | Recommended |
| `ffmpeg` | Convert videos to MP4 (`scripts/make-video.sh`) | If adding your own videos |
| `git` | Version control | Recommended |

No KVM, root access, or additional libraries are required, and there's no need to download anything else (lwIP is already included in `third_party/`).

## 2. Compile and Run

```bash
make              # Build build/kernel.elf
make run          # Create disk.img + boot QEMU

```

Open your browser: http://localhost:8080 — To exit QEMU: Press `Ctrl+A`, then `X`.

If port 8080 is already in use, change `hostfwd=tcp::8080-:80` in the `Makefile` to another port.

## 3. Move into the Existing Repo (`~/H3/PROJECT_OS`)

Your old code is located in `CODE_PJ/` and has been committed. The history won't be lost. Do the following:

```bash
cd ~/H3/PROJECT_OS
git status                              # Working directory must be clean; commit any pending changes first
git rm -r --cached CODE_PJ >/dev/null   # Remove old files from the index (they remain in the history)
rm -rf CODE_PJ
unzip ~/Downloads/os-streaming-kernel.zip -d /tmp/osk
mv /tmp/osk/os-streaming-kernel CODE_PJ
git add -A
git commit -m "Full OS tree: mm, user mode, syscalls, virtio, tarfs, lwIP, HTTP server"
git push

```

The root Makefile (`make run`) you previously set up will still forward commands to `CODE_PJ` as before.
Whenever you want to look at the old code, use `git log` and then `git show <commit>:CODE_PJ/main.c`.

## 4. Troubleshooting

| Symptom | Solution |
| --- | --- |
| `aarch64-linux-gnu-gcc: command not found` | Packages from Step 1 are not installed. |
| `ld: unrecognized option '--no-warn-rwx-segments'` | `binutils` is too old. Remove it from `LDFLAGS` in the `Makefile`. |
| `Could not set up host forwarding rule` | Port 8080 is in use (a background QEMU might be hanging: run `pkill -x qemu-system-aar`). |
| Web page doesn't load | Check the QEMU terminal; it should show a line like `[net] lwIP ready`. Try running `curl -v http://localhost:8080/status`. |
| Disk not visible (`ls` is empty) | Run `make disk` again, then `make run`. |
| Video doesn't play in browser | Must be H.264+AAC in an MP4 container. Use `scripts/make-video.sh` to convert it. |
| Thai characters show as boxes in terminal | Normal for some terminals, does not affect functionality. |

Debugging tools: Run `make debug`, then open another terminal and run:
`aarch64-linux-gnu-gdb build/kernel.elf -ex 'target remote :1234' -ex 'break kmain' -ex continue`