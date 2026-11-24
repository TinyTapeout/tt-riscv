# Breakout on TinyQV (C)

How to run, play and build Breakout on TinyQV "Asteroids". What the game is, and credits:
[`examples/tinyqv-breakout`](../examples/tinyqv-breakout).

Tested on ttsky25a `tt_um_tt_tinyQV` (#495) with the QSPI, TinyVGA and Gamepad Pmods and a controller, with and
without an Audio Pmod.

## What you need

| Header                          | Pmod                                                                                                                         |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Output (`uo_out`)               | [TinyVGA Pmod](https://store.tinytapeout.com/products/TinyVGA-Pmod-p678647356), to a monitor                                 |
| Input (`ui_in`)                 | [Gamepad Pmod](https://store.tinytapeout.com/products/Gamepad-Pmod-board-p741891425), with one or two SNES-style controllers |
| Bidirectional (`uio`)           | QSPI Pmod (the program runs from its flash)                                                                                  |
| Bidirectional (`uio`), optional | [Audio Pmod](https://store.tinytapeout.com/products/Audio-Pmod-p716541601), with the QSPI Pmod stacked on top, for sound     |

The monitor must accept 1024x768 at 60 Hz. The game draws a 256x192 picture, scaled up 4x.

## Just run it

1. Download `tinyqv-breakout.bin` from the [latest release](../../../releases/latest).
2. In the [TinyQV programmer](https://program.tinyqv.com) choose **Custom**, pick the file, design **495**,
   latency **2**, **64** MHz, then **FLASH AND RUN**.

The game takes only the first 7 KB of the Pmod's flash, so other images on it, such as KianV Linux at
1 MiB and above, are kept. See [flash layout](flash-layout.md).

## Playing

| Control      | Action                                             |
| ------------ | -------------------------------------------------- |
| Any button   | Start the game (at `PRESS START`)                  |
| Left / right | Move the paddle                                    |
| Hold B       | Move the paddle faster                             |
| A or Start   | Serve without waiting; skip the `GAME OVER` screen |

Either controller port works. Without a controller, the game plays itself (demo mode,
`DEMO` in the top bar). Plug a controller in to take over; the game leaves demo mode by itself.

- 6 rows of 14 bricks. The bottom rows score 1 point, the middle rows 4, the top rows 7.
- The ball speeds up after 4 and 12 hits, and when it reaches the top two rows. Each new level
  starts a little faster.
- 3 lives. Where the ball hits the paddle sets its angle.

## Sound

The sound comes from the PiecewiseOrionSynth peripheral (33) as a PWM signal on two pins:

- **`uio[7]`**: plug an [Audio Pmod](https://store.tinytapeout.com/products/Audio-Pmod-p716541601) into
  the bidirectional header and stack the QSPI Pmod on top of it. The Audio Pmod holds the QSPI Pmod's
  `IO7` (PSRAM B's chip select) high, so PSRAM B stays off; the game does not use it. Without an Audio
  Pmod the game finds PSRAM B answering at startup and leaves `uio[7]` alone: sound on it would make
  PSRAM B disturb the reads from PSRAM A.
- **`uo_out[6]`**: blue bit 0 of the TinyVGA Pmod. The game's colours never set that bit, so the sound
  shows on the monitor only as faint blue vertical streaks that move with it, an old-CRT look.

## Build it

The toolchain is a Linux x86-64 build that needs glibc 2.38 or newer (Ubuntu 24.04 or later, Debian 13).

```sh
scripts/build-tinyqv-breakout.sh      # -> out/tinyqv-breakout/tinyqv-breakout.bin
```

The script downloads the toolchain (about 1 GB) and the SDK at the commits pinned in
[`pins.env`](../pins.env), builds the SDK's libraries, then the game. To use a toolchain you already
have, set `RISCV_TOOLCHAIN`, e.g. `RISCV_TOOLCHAIN=/opt/tinyQV`.

Use toolchain 16.1.0-tqv-2.1 or newer. Older builds assemble some `mul16` instructions (TinyQV's
16-bit multiply) with their operands swapped, which breaks the SDK's graphics routines.

Build options, passed with `make EXTRA_CFLAGS=...` in `examples/tinyqv-breakout`:

| Option        | Effect                                                               |
| ------------- | -------------------------------------------------------------------- |
| `-DSHOW_LOAD` | Show the CPU load in the top bar: VGA lines spent per frame (of 806) |

## How it works

- **Video.** The vga_gfx peripheral (9) shows a 256x192, 2-bit-per-pixel frame buffer in PSRAM. The CPU is
  slow (one instruction per 8 clocks, code read from flash), so the game never redraws the whole screen.
  At the start of vertical blank it draws only what changed (ball, paddle, broken bricks, score), then
  runs the game logic for the next frame.
- **Input.** `gamepad.h` from the SDK; the game ORs the buttons of both controllers.
- **Sound** (`sound.c`). Four synth voices for the paddle, walls, bricks (pitch by row) and a lost ball, plus short
  tunes for a new game, a cleared level and game over.
- **Maths.** Integers only, 8.8 fixed point for the ball. Multiplies use TinyQV's `mul16` instruction
  (`mul32x16()` from the SDK's `mul.h`); there is no division.
