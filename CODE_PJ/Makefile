.RECIPEPREFIX = >
CROSS  = aarch64-linux-gnu-
CFLAGS = -ffreestanding -nostdlib -O2 -Wall -mgeneral-regs-only

kernel.elf: boot.o main.o linker.ld
> $(CROSS)ld -nostdlib -T linker.ld -o $@ boot.o main.o

%.o: %.S
> $(CROSS)gcc $(CFLAGS) -c $< -o $@

%.o: %.c
> $(CROSS)gcc $(CFLAGS) -c $< -o $@

run: kernel.elf
> qemu-system-aarch64 -M virt -cpu cortex-a72 -nographic -kernel kernel.elf

clean:
> rm -f *.o kernel.elf
