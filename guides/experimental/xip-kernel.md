# Experimental: Linux kernel executing in place from flash

**Status: works on ttsky25a silicon. Opt-in; the release images do not use it.**

By default the bootloader copies the 3.4 MB kernel Image from flash to PSRAM before starting it.
Patch 0005 adds an alternative: the kernel runs directly from flash (XIP), and only its data is copied
to PSRAM.

|                          | Default copy (8 words per iteration) | XIP     |
| ------------------------ | ------------------------------------ | ------- |
| Kernel copy              | 8 s                                  | none    |
| `/ #` prompt after reset | about 138 s                          | 128 s   |
| Free RAM after boot      | 10.4 MB                              | 12.2 MB |
| Code speed               | same                                 | same    |

**Why the code runs no slower.** Flash and both PSRAMs sit behind the same quad-SPI controller in the SoC
(15 MHz, 6 wait cycles, no cache). Fetching code from flash costs exactly the same as fetching it from PSRAM.

## Build and flash

```sh
scripts/fetch-kianriscv.sh
cd build/kianRiscV/asic/os/ulinux_asic_kianv_soc
make all && make xip        # -> xip/Image, xip/bootloader.bin
make xip-sim                # optional: boot it in QEMU
```

Flash `xip/bootloader.bin` at 1048576, `kianv.dtb` at 1572864 and `xip/Image` at **exactly** 2097152.
**The XIP bootloader only works with the XIP Image, and the normal bootloader only with a normal Image.**

## What the patch changes

- **New option:** a no-MMU XIP option, `CONFIG_NOMMU_XIP_KERNEL`. Linux's own `XIP_KERNEL` needs an MMU.
  The new option turns off jump labels, `PREEMPT_DYNAMIC_KEY` and `RISCV_ALTERNATIVE`, because those
  rewrite kernel code, and a write to flash would hang the CPU.
- **Linker script:** code, rodata and the initramfs stay in flash; data and bss go in PSRAM. `head.S`
  copies the data before anything else runs.
- **Bootloader:** jumps straight to 0x20200000, with the device tree left in flash.
