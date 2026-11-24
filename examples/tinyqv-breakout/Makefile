# Breakout for TinyQV "Asteroids" (ttsky25a #495), built with Mike Bell's tinyQV toolchain and SDK.
# scripts/build-tinyqv-breakout.sh fetches both (pinned in pins.env) and runs this Makefile.
# Build options (make EXTRA_CFLAGS=...): see guides/tinyqv-breakout.md.
RISCV_TOOLCHAIN ?= /opt/tinyQV
TINYQV_SDK ?= ../../build/tinyQV-sdk

CC = $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-gcc
LD = $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-ld
OBJCOPY = $(RISCV_TOOLCHAIN)/bin/riscv32-unknown-elf-objcopy
CFLAGS = -O2 -I$(TINYQV_SDK) -march=rv32ec_zicsr_zcb_zicond_zilsd -mabi=ilp32e -mno-strict-align -nostdlib \
         -nostartfiles -ffreestanding -ffunction-sections -fdata-sections -Wall -Werror $(EXTRA_CFLAGS)

all: tinyqv-breakout.bin

breakout.o: breakout.c sound.h vga_timing.h
sound.o: sound.c sound.h
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

tinyqv-breakout.elf: breakout.o sound.o
	$(LD) $^ $(TINYQV_SDK)/start.o $(TINYQV_SDK)/tinyQV.a $(TINYQV_SDK)/tinyQV-asteroids.a \
	  $(RISCV_TOOLCHAIN)/riscv32-unknown-elf/lib/libc.a $(RISCV_TOOLCHAIN)/lib/gcc/riscv32-unknown-elf/*/libgcc.a \
	  -T $(TINYQV_SDK)/memmap --gc-sections -o $@

tinyqv-breakout.bin: tinyqv-breakout.elf
	$(OBJCOPY) $< -O binary $@

clean:
	rm -f *.o *.elf *.bin

.PHONY: all clean
