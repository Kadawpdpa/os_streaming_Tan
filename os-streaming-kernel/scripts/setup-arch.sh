#!/bin/sh
# Install required tools on Arch Linux
set -e
sudo pacman -S --needed \
    qemu-system-aarch64 \
    aarch64-linux-gnu-gcc aarch64-linux-gnu-binutils aarch64-linux-gnu-gdb \
    make git tar curl ffmpeg
echo
echo "== Verification =="
qemu-system-aarch64 --version | head -1
aarch64-linux-gnu-gcc --version | head -1
aarch64-linux-gnu-ld --version | head -1