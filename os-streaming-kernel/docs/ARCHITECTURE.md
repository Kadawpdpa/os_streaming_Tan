# Architecture

## Memory Map (QEMU virt, `-m 256M`)

```
0x00000000-0x3FFFFFFF  Device memory             GIC 0x08000000, UART 0x09000000, virtio 0x0a000000+n*0x200
0x40080000             kernel image (.text .rodata .data .bss) -> __kernel_end
__kernel_end-0x4EFFFFFF Free pages for page allocator (thread stacks, virtio rings, 4 MB heap, page tables)
0x4F000000-0x4FFFFFFF  User process memory: 8 slots x 2 MB (visible to the kernel)
0x80000000-0x801FFFFF  User window (VA): Every process sees the same VA, but maps to a different slot = separate address spaces

```

* The kernel uses identity mapping (VA = PA), so drivers can pass buffer addresses directly to devices.
* Each process has its own L1/L2 page tables. The kernel section in the tables is identical across all processes, but the `0x80000000` section is different. The scheduler switches `TTBR0_EL1` during a context switch to another process.
* User programs (AP=EL0 RW) cannot modify kernel memory -> results in a data abort -> the kernel kills that process (can be tested with the `crash` command).

## Boot Sequence

`boot.S` (select core 0, EL2→EL1, set up stack, clear .bss) → `kmain`:

1. `mmu_init` enables MMU + cache.
2. `pmm_init`, `kheap_init`.
3. Set up the vector table, `gic_init`, `thread_init`, `timer_init(100 Hz)`.
4. `blk_init` → `tarfs_init` (reads the file list from disk) → `net_init` (virtio-net + lwIP + HTTP).
5. Create `net` and `shell` threads, enable IRQs, thread 0 becomes idle (`wfi`).

## Scheduler

* Round-robin, 10 ms time slice: timer interrupt → `handle_irq` → EOI → `yield()` → `cpu_switch`.
* Thread 0 (idle) is selected only when no other threads are ready to run.
* `thread_sleep_ms` puts a thread to sleep without consuming CPU.
* Critical sections = disable IRQs (`irq_save/irq_restore`), used in `yield`, `kprintf`, and the allocator.
* The trap frame saves `ELR_EL1`, `SPSR_EL1`, and `SP_EL0` per thread, allowing a preempted thread to resume execution at the correct location.

## Network

`virtio-net` (32 RX buffers, single synchronous TX buffer) → `net/net.c` passes frames to lwIP (`NO_SYS=1`, raw API) → `net/httpd.c` (TCP port 80). The `net` thread loops: fetch packet → `sys_check_timeouts()` → if idle, execute `wfi` (IRQs are disabled during the check to avoid missing wakeup signals; virtio's IRQ wakes up the CPU).

HTTP server: `GET`/`HEAD`, `Range: bytes=a-b | a- | -n` responds with `206` + `Content-Range`, `416`, `404`, `/status`.
Reads files from disk 8 KB at a time, then calls `tcp_write` according to the TCP send window (flow control via the `sent` callback).

## Disk

`disk.img` is a tar (ustar) file of the `www/` directory — `tarfs` scans the file headers once during boot and knows the offset of every file. Reading any range is simply a matter of sector calculation (ideal for video Range requests).