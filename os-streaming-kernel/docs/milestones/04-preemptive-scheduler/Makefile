.RECIPEPREFIX = >
CROSS  = aarch64-linux-gnu-
CFLAGS = -ffreestanding -nostdlib -O2 -Wall -mgeneral-regs-only
OBJS   = boot.o vectors.o switch.o thread.o main.o

kernel.elf: $(OBJS) linker.ld
> $(CROSS)ld -nostdlib -T linker.ld -o $@ $(OBJS)

%.o: %.S
> $(CROSS)gcc $(CFLAGS) -c $< -o $@

%.o: %.c
> $(CROSS)gcc $(CFLAGS) -c $< -o $@

run: kernel.elf
> qemu-system-aarch64 -M virt,gic-version=2 -cpu cortex-a72 -nographic -kernel kernel.elf

clean:
> rm -f *.o kernel.elf
