#!/bin/sh
# Create disk.img = tar (ustar) of the www/ directory -> kernel reads files directly via virtio-blk
set -e
cd "$(dirname "$0")/.."
[ -d www ] || { echo "Directory www/ not found"; exit 1; }
tar --format=ustar --owner=0 --group=0 --numeric-owner -cf disk.img -C www .
echo "Created disk.img ($(du -h disk.img | cut -f1)): $(ls www | tr '\n' ' ')"