# MicroPython on TinyQV

[TinyQV](https://github.com/MichaelBell/tinyQV) by Mike Bell is a small RV32EC SoC that runs code
directly from the QSPI flash. The crowdsourced ttsky25a versions add peripherals contributed by the
community, and run MicroPython.

| Shuttle  | Project                                            | Status                                               |
| -------- | -------------------------------------------------- | ---------------------------------------------------- |
| ttsky25a | `tt_um_tt_tinyQV` "Asteroids" (#495)               | tested on silicon                                    |
| ttsky25a | `tt_um_tt_tinyQVb` "Berzerk" (#687)                | tested on silicon                                    |
| ttihp25b | `tt_um_tt_tinyQV` (#490) and other TinyQV shuttles | see [program.tinyqv.com](https://program.tinyqv.com) |

## Flash and run

1. Open the [TinyQV programmer](https://program.tinyqv.com) and connect to the board.
2. Select **MicroPython**, design **495** (or **687**), QSPI latency **2**, frequency **64** MHz.
3. Click **FLASH AND RUN**. The REPL appears in the page.

The same image is in the [latest release](../../../releases/latest) as `tinyqv-micropython.bin`, a mirror
of Mike's build with its sha256 checked.

## Try it

```python
>>> import sys; sys.implementation
(name='micropython', version=(1, 25, 0, ''), _machine='ttsky25a with tinyQV')
>>> 2**100
1267650600228229401496703205376
>>> from machine import Pin
>>> Pin(9).value(), Pin(15).value()      # in1, in7 (UART RX, idles high)
(0, 1)
>>> import tinyqv; tinyqv.write_byte_reg(21, 1, 100)   # peripheral registers
```

Both chips run the same image. On ttsky25a, `sum(i*i for i in range(10000))` takes about 5.6 s.

More examples, peripherals and `mpremote` over a USB bridge:
[ttsky25a-tinyQV/micropython](https://github.com/TinyTapeout/ttsky25a-tinyQV/tree/main/micropython).

## Drive the 7-segment display

The demo board's 7-segment display is wired to `uo_out`: segments a-g on bits 0-6, the decimal point on bit 7.
`Pin(n, Pin.OUT)` hands `uo_out[n]` to the GPIO peripheral, whose output register is at 0x8000040.
`uo_out[0]` is both segment a and the UART TX, so this demo borrows it and gives it back at the end.
Paste it with **Ctrl-E**, then press **Ctrl-D**:

```python
import machine, time
from machine import Pin
OUT = 0x8000040                     # TinyQV GPIO output register
DIGITS = (0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F)
seg = [Pin(n, Pin.OUT) for n in range(8)]   # takes uo0 (UART TX) too: the REPL goes quiet
for d in DIGITS:
    machine.mem32[OUT] = d
    time.sleep(0.5)
machine.mem32[OUT] = 0
Pin(0, Pin.OUT, func_sel=2)         # uo0 back to the UART
```

The display counts 0 to 9, half a second per digit, then goes blank and the `>>>` prompt returns. The REPL is
silent while the digits show. Tested on ttsky25a (#495 and #687).

## Notes

- The MicroPython image is 1.27 MB and overwrites the KianV bootloader at 0x100000. See [flash-layout.md](flash-layout.md).
- TinyQV leaves the PSRAMs in QPI mode. Before running KianV, use `uart_console.kianv()` (it resets
  them) or power-cycle.

## Credits

TinyQV, its SDK, MicroPython port and programmer: Mike Bell ([MichaelBell](https://github.com/MichaelBell)).
Peripherals: the TinyQV peripheral challenge contributors.
