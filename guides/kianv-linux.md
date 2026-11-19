# Boot Linux on the KianV RISC-V SoC

[KianV](https://github.com/splinedrive/kianRiscV) by Hirosh Dabui is an RV32IMA SoC (no MMU) that
boots Linux from the QSPI Pmod. This guide boots the 16 MiB image and gives you a root shell.

Time: about 15 minutes, most of it flashing and booting.

| Shuttle                      | Project                                                                       | Image                                             | Status                                                    |
| ---------------------------- | ----------------------------------------------------------------------------- | ------------------------------------------------- | --------------------------------------------------------- |
| ttsky25a                     | `tt_um_kianV_rv32ima_uLinux_SoC` (#846), 30 MHz                               | 16 MiB image from this repo                       | tested on silicon                                         |
| ttsky25b, ttihp25b, ttcad25a | `tt_um_kianV_rv32ima_uLinux_SoC`                                              | 16 MiB image from this repo                       | should work, not verified                                 |
| ttgf0p2                      | `tt_um_kianV_rv32ima_uLinux_SoC` (#295), run at 30 MHz (design target 16 MHz) | 16 MiB image from this repo                       | tested on silicon                                         |
| ttgf0p3                      | `tt_um_kianV_rv32ima_uLinux_SoC`                                              | 16 MiB image from this repo                       | should work, not verified                                 |
| ttsky25b                     | `tt_um_kianv_rv32_regfile` (#970), regfile edition                            | 16 MiB image from this repo                       | should work, not verified                                 |
| tt06                         | `tt_um_kianV_rv32ima_uLinux_SoC` (#910), 30 MHz, 8 MiB                        | "KianV uLinux Image (TT06)" preset in the Flasher | tested on silicon (RP2040 board, shell after about 185 s) |
| tt05, ttihp0p1               | `tt_um_kianV_rv32ima_uLinux_SoC`                                              | "KianV uLinux Image (TT06)" preset in the Flasher | not verified                                              |

## 1. Flash

Set up the board first: [board-setup.md](board-setup.md). Then open the [Flasher](https://tinytapeout.github.io/tinytapeout-flasher/), connect to the board,
select the preset **KianV uLinux, 16 MiB**, and click **Flash**. It writes the three files of the
[latest release](../../../releases/latest); the Image takes several minutes.

To flash other files (an older release, or your own build from [building.md](building.md)), choose **Custom**
and flash each file at its offset:

| File                          | Offset (decimal) |
| ----------------------------- | ---------------- |
| `kianv-ulinux-bootloader.bin` | 1048576          |
| `kianv-ulinux-kianv.dtb`      | 1572864          |
| `kianv-ulinux-Image`          | 2097152          |

The bootloader and the Image always go together: flash both from the same release or build.

## 2. Boot

At the board's REPL:

```python
import uart_console
uart_console.kianv()
```

Expected output:

```
KianV RISC-V uLinux ASIC Tiny Tapeout SOC
-----------------------------------------
loading kernel Image from flash .......       (about 8 s)
starting kernel
[    0.000000] Linux version 6.8.0-rc1 ...
...
[    0.000000] Memory: 13432K/16380K available ...
...
starting pid 32, tty '/dev/console': '/sbin/getty -L -n -l /bin/sh console 0 vt100'
/ #
```

The shell prompt appears about **140 s** after reset. Press Enter if you don't see it.

**A few garbled lines after `clk: Disabling unused clocks` are normal.** They appear on every boot,
when the kernel's serial driver takes over the console.

## 3. Try it

```sh
uname -a
cat /proc/cpuinfo        # rv32ima, mmu: none
ls /root
/root/hello_7seg         # HELLO then 0-9 on the demo board's 7-segment display
/root/rust_nostd_hello   # Rust, see rust-kianv-linux.md
```

While `hello_7seg` runs, the console is silent: the 7-segment display shares `uo_out[4]` with the
UART TX pin. The program gives it back when it finishes.

The KianV UART has no receive FIFO, so the console sends what you type one character at a time, 80 ms apart.
Pasting long text works, but slowly. Don't type while a program is busy writing output.

## Troubleshooting

| Symptom                           | Fix                                                                                                                                                  |
| --------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------- |
| Hangs after `starting kernel`     | Bootloader and Image are from different builds. Flash both from the same release.                                                                    |
| Garbage on the console            | Wrong clock or baud. Images here are built for 30 MHz / 115200.                                                                                      |
| Typed characters go missing       | Slow the input down: `uart_console.kianv(char_ms=150)`. The KianV UART has no receive buffer; ttgf0p2 needs about 80 ms per character (the default). |
| Hangs early, after running TinyQV | PSRAMs left in QPI mode. Use `uart_console.kianv()`, which resets them, or power-cycle.                                                              |
| Kernel hangs at a random point    | Not seen on ttsky25a. A lower clock may help: `uart_console.kianv(freq=25_000_000, baud=96000)` (the baud scales with the clock; not verified).      |

## Credits

KianV SoC, Linux port and bootloader: Hirosh Dabui ([splinedrive/kianRiscV](https://github.com/splinedrive/kianRiscV)).
