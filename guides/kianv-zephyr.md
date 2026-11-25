# Zephyr and MicroPython on the KianV SoC

Hirosh Dabui's [Zephyr port](https://github.com/splinedrive/kianRiscV/tree/master/asic/os/zephyr_kianv_asic)
puts a boot menu, Zephyr and a bare-metal MicroPython into one flash image. This repo builds it unchanged.

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

`devmem` can also drive the [7-segment display](#drive-the-7-segment-display). This shows a **2**:

```
kianv:~$ devmem 0x10600004 32 0x5b
kianv:~$ devmem 0x10600000 32 0xef
kianv:~$ devmem 0x10600000 32 0
```

The first command sets the segments, the second gives every `uo_out` pin except `uo_out[4]` to the GPIO block,
and the third gives them back. `uo_out[4]` stays the UART TX, so the shell keeps working. It is also segment e,
which is lit while the line idles high, and flickers while the shell prints.

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

## Drive the 7-segment display

The demo board's 7-segment display is wired to `uo_out`: segments a-g on bits 0-6, the decimal point on bit 7.
The KianV GPIO block has three registers:

| Address    | Register | Use                                         |
| ---------- | -------- | ------------------------------------------- |
| 0x10600000 | `UO_EN`  | 1 = the GPIO block drives that `uo_out` pin |
| 0x10600004 | `UO_OUT` | value for the enabled `uo_out` pins         |
| 0x10600008 | `UI_IN`  | reads `ui_in`                               |

This counts 0 to 9 from MicroPython (press **m**). Paste it with **Ctrl-E**, then press **Ctrl-D**:

```python
import machine, time
EN, OUT = 0x10600000, 0x10600004
DIGITS = (0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F)
machine.mem32[EN] = 0xFF            # GPIO drives uo_out[7:0]; UART TX (uo4) goes quiet
for d in DIGITS:
    machine.mem32[OUT] = d
    time.sleep(0.5)
machine.mem32[OUT] = 0
machine.mem32[EN] = 0               # uo4 back to the UART
```

The display counts 0 to 9, half a second per digit, then goes blank and the `>>>` prompt returns. Segment e is
also the UART TX pin (`uo_out[4]`), so the REPL is silent while the digits show. Tested on ttsky25a.
For the Zephyr shell, see the `devmem` example above. On Linux, run [`/root/hello_7seg`](kianv-linux.md#3-try-it).

## Credits

Zephyr and MicroPython ports: Hirosh Dabui ([splinedrive/kianRiscV](https://github.com/splinedrive/kianRiscV),
[splinedrive/micropython](https://github.com/splinedrive/micropython)); the port runs on upstream
MicroPython ([micropython/micropython](https://github.com/micropython/micropython)).
