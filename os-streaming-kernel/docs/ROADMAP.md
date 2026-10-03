# Roadmap: Future Work (Ordered by Recommendation)

## Further Development on QEMU

1. **Benchmarking for the report**: Context-switch time, round-robin fairness, and HTTP throughput per number of clients compared to Linux (Nginx) on the same machine.
2. **Advanced Scheduler**: Implement priority-based or MLFQ and compare it with CFS.
3. **ELF Loader** + Multiple user programs (currently using flat binaries), along with `fork`/`exec` support.
4. **Networking**: DHCP (`LWIP_DHCP 1`), fully bind the `net` thread to interrupts instead of polling, HTTP keep-alive, and HLS (serving `.m3u8` + segments is already feasible since the server can serve any file).
5. **Writable Filesystem**: FAT32 or ext2 to replace the read-only tarfs.
6. **Multi-core (SMP)**: Spinlocks and per-CPU run queues.

## Porting to Real Raspberry Pi 5 (High Risk, Time-Consuming)

1. `boot.S` already supports EL2→EL1; need to create `kernel8.img` + `config.txt` (`arm_64bit=1`, `kernel=`) and remap UART/GIC addresses (Pi 5 uses GIC-400 and routes UART through the RP1).
2. **PCIe → RP1** → Ethernet (Cadence GEM) and USB (xHCI) — referencing the Linux `macb` and `xhci` driver code.
3. **SD Card** (SDHCI/Arasan) instead of `virtio-blk`.
4. **Framebuffer via firmware mailbox** → MJPEG playback (OS handles rendering itself).
5. **Audio**, ordered by difficulty: PWM → I2S DAC (PCM5102) → HDMI audio → USB audio.
6. *Note*: Memory mapping used with DMA must be changed to non-cacheable (this isn't an issue on QEMU because it is cache-coherent).

## Linux + Pi 5 (Core Project System)

Outside the scope of this repository: Jellyfin/Nginx + HLS, as per the previously outlined pipeline summary document.