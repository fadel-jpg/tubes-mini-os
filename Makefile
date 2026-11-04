# =============================================================================
# Makefile - Build System for Mini-OS (x86 32-bit Multiboot)
# =============================================================================

# Toolchain configuration (uses standard gcc with -m32 or i686-elf-gcc if set)
CC      ?= gcc
LD      ?= ld
AS      ?= nasm
QEMU    ?= qemu-system-i386

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

# Source and Object files
ASM_SRC := boot/boot.asm
C_SRCS  := kernel/kernel.c kernel/shell.c drivers/vga.c drivers/keyboard.c

OBJS    := $(BUILD_DIR)/boot.o \
           $(BUILD_DIR)/kernel.o \
           $(BUILD_DIR)/shell.o \
           $(BUILD_DIR)/vga.o \
           $(BUILD_DIR)/keyboard.o

.PHONY: all build run iso run-iso clean check-env

# Default target
all: build

# Build the Multiboot kernel binary
build: $(KERNEL_BIN)

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

$(BUILD_DIR)/boot.o: boot/boot.asm | $(BUILD_DIR)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: kernel/shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: drivers/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(OBJS) linker.ld | $(BIN_DIR)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
	@echo "[SUCCESS] Kernel binary built at: $(KERNEL_BIN)"

# Run directly in QEMU using -kernel (Multiboot direct boot)
run: $(KERNEL_BIN)
	$(QEMU) -kernel $(KERNEL_BIN)

# Optional: Create a bootable GRUB ISO image (requires grub-mkrescue & xorriso)
iso: $(KERNEL_BIN)
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL_BIN) $(ISO_DIR)/boot/kernel.bin
	printf 'set timeout=0\nset default=0\nmenuentry "Mini-OS" {\n    multiboot /boot/kernel.bin\n    boot\n}\n' > $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(KERNEL_ISO) $(ISO_DIR)
	@echo "[SUCCESS] Bootable ISO created at: $(KERNEL_ISO)"

# Run bootable ISO in QEMU
run-iso: iso
	$(QEMU) -cdrom $(KERNEL_ISO)

# Clean all build artifacts
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "[CLEAN] Removed build/ and bin/ directories."

