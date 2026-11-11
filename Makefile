# =============================================================================
# Makefile - Build System for Dalfon OS (x86 32-bit Multiboot)
# =============================================================================

# Toolchain configuration
CC      := gcc
LD      := ld
AS      := nasm
QEMU    := qemu-system-i386

# Compiler, Assembler, and Linker flags
CFLAGS  := -m32 -ffreestanding -O2 -Wall -Wextra -fno-exceptions \
           -fno-stack-protector -fno-pic -fno-pie -nostdlib -Iinclude
ASFLAGS := -f elf32
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib

# Directories
BUILD_DIR := build
BIN_DIR   := bin
ISO_DIR   := $(BUILD_DIR)/isodir

# Target outputs
KERNEL_BIN := $(BIN_DIR)/kernel.bin
KERNEL_ISO := $(BIN_DIR)/mini-os.iso

# Object files
OBJS    := $(BUILD_DIR)/boot.o \
           $(BUILD_DIR)/kernel.o \
           $(BUILD_DIR)/shell.o \
           $(BUILD_DIR)/thread.o \
           $(BUILD_DIR)/vga.o \
           $(BUILD_DIR)/keyboard.o

.PHONY: all build run iso run-iso clean

# Default target
all: build

# Build the Multiboot kernel binary
build: $(KERNEL_BIN)

$(BUILD_DIR)/boot.o: boot/boot.asm
	@mkdir -p $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: kernel/shell.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/thread.o: kernel/thread.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: drivers/vga.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/keyboard.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) linker.ld
	@mkdir -p $(BIN_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[SUCCESS] Kernel binary built at: $(KERNEL_BIN)"

# Run directly in QEMU using -kernel (Multiboot direct boot)
run: $(KERNEL_BIN)
	$(QEMU) -kernel $(KERNEL_BIN)

# Optional: Create a bootable GRUB ISO image (requires grub-mkrescue & xorriso)
iso: $(KERNEL_BIN)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	printf 'set timeout=0\nset default=0\nmenuentry "Dalfon OS" {\n    multiboot /boot/kernel.bin\n    boot\n}\n' > $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)
	@echo "[SUCCESS] Bootable ISO created at: $(KERNEL_ISO)"

# Run bootable ISO in QEMU
run-iso: iso
	$(QEMU) -cdrom $(KERNEL_ISO)

# Clean all build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "[CLEAN] Removed build/ and bin/ directories."
