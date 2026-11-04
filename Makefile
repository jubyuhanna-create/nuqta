CC      := gcc
AS      := nasm
LD      := ld

KSECT   := 64	# sectors the bootloader reads, 64 * 512 = 32K max kernel

CFLAGS  := -m32 -std=gnu11 -O2 -g -Wall -Wextra \
           -ffreestanding -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -mgeneral-regs-only -nostdlib
LDFLAGS := -m elf_i386 -T kernel/linker.ld -nostdlib -z noexecstack --no-warn-rwx-segments

QEMU    := qemu-system-i386
QFLAGS  := -drive format=raw,file=build/nuqta.img -m 128M -serial stdio

COBJS   := $(patsubst kernel/%.c,build/%.o,$(wildcard kernel/*.c))
AOBJS   := build/entry.o build/isr.o

all: build/nuqta.img

build/%.o: kernel/%.c kernel/*.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: kernel/%.asm | build
	$(AS) -f elf32 $< -o $@

build/boot.bin: boot/boot.asm | build
	$(AS) -f bin -DKERNEL_SECTORS=$(strip $(KSECT)) $< -o $@

build/kernel.elf: $(AOBJS) $(COBJS) kernel/linker.ld
	$(LD) $(LDFLAGS) $(AOBJS) $(COBJS) -o $@

build/kernel.bin: build/kernel.elf
	objcopy -O binary $< $@
	@size=$$(stat -c%s $@); max=$$(( $(strip $(KSECT)) * 512 )); \
	 if [ $$size -gt $$max ]; then \
	   echo "kernel is $$size bytes, bootloader only reads $$max. bump KSECT"; exit 1; fi

build/nuqta.img: build/boot.bin build/kernel.bin
	dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	dd if=build/boot.bin of=$@ conv=notrunc status=none
	dd if=build/kernel.bin of=$@ seek=1 conv=notrunc status=none

build:
	mkdir -p build

run: build/nuqta.img
	$(QEMU) $(QFLAGS)

# then: gdb build/kernel.elf -ex 'target remote :1234'
debug: build/nuqta.img
	$(QEMU) $(QFLAGS) -s -S

clean:
	rm -rf build

.PHONY: all run debug clean
