ASM = nasm
CC = gcc
LD = ld

CFLAGS = -m32 -march=i386 -mno-sse -mno-sse2 -mno-mmx -ffreestanding -nostdlib -fno-exceptions -fno-pic -fno-stack-protector -Wall -Wextra -I.

OBJS = boot/boot.o kernel/kernel.o kernel/screen.o kernel/ports.o kernel/keyboard.o \
       kernel/string.o kernel/filesystem.o kernel/process.o kernel/timer.o \
       kernel/interrupts.o kernel/isr.o kernel/splash.o kernel/system.o kernel/memory.o \
       kernel/switch.o kernel/editor.o kernel/network.o kernel/netcmds.o kernel/login.o \
       kernel/paging.o kernel/vm.o kernel/gdt.o kernel/tss.o \
       kernel/syscall.o kernel/ring3.o kernel/user.o kernel/syscall_isr.o kernel/ring3_enter.o

all: kernel.bin

kernel/ring3_enter.o: kernel/ring3_enter.asm
	$(ASM) -f elf32 $< -o $@

kernel/syscall.o: kernel/syscall.c kernel/syscall.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/syscall_isr.o: kernel/syscall.asm
	$(ASM) -f elf32 $< -o $@

kernel/ring3.o: kernel/ring3.c kernel/ring3.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/user.o: kernel/user.asm
	$(ASM) -f elf32 $< -o $@

kernel/vm.o: kernel/vm.c kernel/vm.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/tss.o: kernel/tss.c kernel/tss.h kernel/gdt.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/switch.o: kernel/switch.asm
	$(ASM) -f elf32 $< -o $@

boot/boot.o: boot/boot.s
	$(ASM) -f elf32 $< -o $@

kernel/kernel.o: kernel/kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

kernel/paging.o: kernel/paging.c kernel/paging.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/screen.o: kernel/screen.c kernel/screen.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/ports.o: kernel/ports.c kernel/ports.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/keyboard.o: kernel/keyboard.c kernel/keyboard.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/string.o: kernel/string.c kernel/string.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/filesystem.o: kernel/filesystem.c kernel/filesystem.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/process.o: kernel/process.c kernel/process.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/timer.o: kernel/timer.c kernel/timer.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/interrupts.o: kernel/interrupts.c kernel/interrupts.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/isr.o: kernel/isr.asm
	$(ASM) -f elf32 $< -o $@

kernel/splash.o: kernel/splash.c kernel/splash.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/system.o: kernel/system.c kernel/system.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/memory.o: kernel/memory.c kernel/memory.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/netcmds.o: kernel/netcmds.c kernel/netcmds.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/network.o: kernel/network.c kernel/network.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/gdt.o: kernel/gdt.c kernel/gdt.h kernel/tss.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/login.o: kernel/login.c kernel/login.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel/editor.o: kernel/editor.c kernel/editor.h
	$(CC) $(CFLAGS) -c $< -o $@

kernel.bin: $(OBJS) linker.ld
	$(LD) -m elf_i386 -T linker.ld -o $@ $(OBJS) -nostdlib

iso: kernel.bin
	mkdir -p isodir/boot/grub
	cp kernel.bin isodir/boot/kernel.bin
	cp boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o JupiterOS-v0.1.iso isodir

run: iso
	qemu-system-i386 -cdrom JupiterOS-v0.1.iso -net nic,model=ne2k_isa,macaddr=52:54:00:12:34:56 -net user

clean:
	rm -rf boot/*.o kernel/*.o kernel.bin isodir JupiterOS-v0.1.iso

.PHONY: all clean run iso