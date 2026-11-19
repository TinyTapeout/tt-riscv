# Zephyr and MicroPython on the KianV SoC

Hirosh Dabui's [Zephyr port](https://github.com/splinedrive/kianRiscV/tree/master/asic/os/zephyr_kianv_asic)
puts a boot menu, Zephyr and a bare-metal MicroPython into one flash image. This repo builds it with one fix
(patch 0006): the system timer runs at the real 10 kHz `mtime` rate, so `uptime` and sleeps are correct.

Tested on ttsky25a and TT06 (30 MHz; TT06 with its RP2040 demo board). It uses 8 MiB of PSRAM, so it runs on both
the original 8 MiB KianV and the 16 MiB variants.

## Flash

In the [Flasher](https://tinytapeout.github.io/tinytapeout-flasher/), select the preset **KianV Zephyr + MicroPython dual-boot** and click **Flash**
(it writes `kianv-zephyr-dualboot.bin` from the [latest release](../../../releases/latest) at 0x100000). For other
builds, use **Custom** at offset **1048576**.
It covers 0x100000 - 0x3B1000 and replaces Linux. See [flash-layout.md](flash-layout.md).

## Boot

```python
import uart_console
uart_console.kianv()
```

```
KianV RISC-V uLinux ASIC Tiny Tapeout SOC
bootloader
z: Zephyr  m: MicroPython
```

Press **z** or **m**.

### Zephyr (z)

Zephyr boots in about 3 s to a shell:

```
*** Booting Zephyr OS build kianv-local ***
Zephyr RTOS on KianV RV32IMA ASIC SoC
Shell ready
kianv:~$ kernel version
Zephyr version 4.5.99
kianv:~$ device list
devices:
- uart@10000000 (READY)
- ramdisk0 (READY)
kianv:~$ fs write /RAM/test.txt 68 65 6c 6c 6f 0a
kianv:~$ fs read /RAM/test.txt
00000000  68 65 6C 6C 6F 0A                                hello.
kianv:~$ devmem 0x10600008
Read value 0x8
```

`devmem 0x10600008` reads the GPIO `ui_in` register. Other useful commands: `kernel uptime`,
`kernel sleep 1000`, `kernel thread list`, `help`.

### MicroPython (m)

```
MicroPython v1.29.0 on 2026-11-18; KianV RV32IMA SoC (Tiny Tapeout) with KianV
>>> print(2**100)
1267650600228229401496703205376
>>> import time; t = time.ticks_ms(); time.sleep(2); print(time.ticks_diff(time.ticks_ms(), t))
2009
```

MicroPython 1.29 with big integers, `time` (ticks, sleep; `time.time()` counts seconds since power-up),
`machine` (`mem8`/`mem16`/`mem32`, `reset()`), `random`, `json`, `re`, `struct` and `help()`. About
6.9 MB of heap is free at the prompt. Limits:

- `gc.collect()` and `gc.mem_free()` take tens of seconds, because they scan the whole heap.
- Wait for the `>>>` prompt before typing: characters sent while it is busy are lost.
- No filesystem: there is nothing to `import` from or `open()`.

## Credits

Zephyr and MicroPython ports: Hirosh Dabui ([splinedrive/kianRiscV](https://github.com/splinedrive/kianRiscV),
[splinedrive/micropython](https://github.com/splinedrive/micropython)); the port runs on upstream
MicroPython ([micropython/micropython](https://github.com/micropython/micropython)).
