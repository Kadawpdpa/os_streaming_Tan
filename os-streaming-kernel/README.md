# OS Streaming Kernel (AArch64)

This Operating System course project is a custom OS developed to handle booting, memory management, process scheduling, user mode, and disk file reading. It integrates a TCP/IP stack (lwIP) and an HTTP Server directly into the kernel, enabling it to stream video directly from the disk to a web browser.

The entire system runs on QEMU (`virt` board, Cortex-A72). It does not use Linux or a standard libc, except for lwIP, a ported TCP/IP library.

```text
Browser ──HTTP──▶ [QEMU user-net] ──▶ virtio-net ──▶ lwIP ──▶ httpd ──▶ tarfs ──▶ virtio-blk ──▶ disk.img (Video file)
                                                  └──────────────── Kernel ────────────────┘

```

## How to Run

For Arch Linux or Ubuntu:

```bash
scripts/setup-arch.sh        # Install required tools (run only once)
make                         # Compile all code (generates build/kernel.elf)
make run                     # Create disk.img and boot QEMU

```

Next, open your browser and go to `http://localhost:8080` to see the webpage and a sample video served directly by the OS.
(To exit QEMU, press `Ctrl+A` followed by `X`).

### Adding Your Own Video:

Use the provided script to convert your video file to H.264+AAC format and move it to the `www/` directory.

```bash
scripts/make-video.sh movie.mkv movie.mp4
make disk && make run

```

You can then access the file via `http://localhost:8080/movie.mp4`.

## Shell Commands (Typed in the QEMU window)

| Command | Description |
| --- | --- |
| `help` | Show all commands |
| `ps` | Show list of threads and processes with their statuses |
| `mem` | Show memory usage status (page allocator and heap) |
| `ls`, `cat <filename>` | List files and read text files on the disk |
| `ifconfig` | Show system IP address |
| `run [1-8]` | Run user programs (EL0) based on the specified number to test address space isolation |
| `crash` | Test running a program that overwrites kernel memory to observe the system killing only that specific process without crashing the OS |
| `uptime` | Show system uptime |

You can test the connection from the host side via the terminal:
`curl http://localhost:8080/status` or test Range requests using `curl -H "Range: bytes=0-99" http://localhost:8080/sample.mp4`

## Code Structure

| Folder/File | Main Function | OS Topic |
| --- | --- | --- |
| `arch/boot.S` | CPU entry point, drops privilege level to EL1, sets up stack | Booting |
| `arch/vectors.S` | Exception vector table | Interrupts & Exceptions |
| `arch/switch.S` | Manages context switches | Process/Thread |
| `arch/mmu.c` | Manages page tables, creates identity map, and isolates address space for each process | Virtual Memory & Paging |
| `mm/pmm.c`, `mm/kmalloc.c` | Manages page-level memory (bitmap) and heap | Memory Management |
| `kernel/thread.c` | Preemptive round-robin scheduler and process management | CPU Scheduling |
| `kernel/syscall.c`, `kernel/traps.c` | Handles system calls from user mode and manages faults | Protection & User Mode |
| `kernel/shell.c` | Shell for receiving commands via UART | UI / Syscall |
| `drivers/` | Simulated hardware drivers (UART, GIC, Timer, Virtio) | Device Drivers & I/O |
| `fs/tarfs.c` | Basic file system (reads tar files) | File System |
| `net/` | Connects lwIP to network drivers and contains an HTTP Server supporting Range requests | Networking |
| `user/` | Sample user program code | User Space |
| `third_party/lwip` | TCP/IP Stack library | – |

## Current Status and Limitations

* **Test Results:** Tested running on Ubuntu (GCC 13 / QEMU 8.2). The system can boot, manage MMU/Cache/Timer, and run 8 user processes concurrently. The HTTP Server can transmit large files and supports video seeking (Range/206/404) normally at speeds of approximately 9 MB/s without any memory leaks.
* **Hardware Limitations:** Currently, the system runs exclusively on QEMU because the drivers are written specifically for Virtio. Porting to a Raspberry Pi 5 would require writing new hardware drivers (e.g., RP1/Ethernet/SD).
* **Processing:** The system still operates on a single-core.
* **User Space:** User programs are still loaded using a flat binary format (ELF loader is not yet supported).
* **Network:** The IP Address is statically fixed and does not use a DHCP system.