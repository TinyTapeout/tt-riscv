# Releasing images

1. Change `pins.env` or `patches/` in a pull request. CI builds every image and boot-tests Linux in QEMU.
2. After merging, run the **Build images** workflow manually with **release** checked. It publishes a
   release `images-YYYYMMDD` (`images-YYYYMMDD.2`, ... for more releases on the same day) with all
   files, their sha256 sums and the pins, and mirrors the flashable files on GitHub Pages for the web
   Flasher. A manual run without **release** is a dry run: it collects the same files into a
   `release-files` artifact without publishing.
3. Test the release on silicon and note the results in the release description. Silicon testing is
   manual for now.

## Silicon checklist (ttsky25a demo board + QSPI Pmod)

| Image              | Steps                                                                                     | Pass when                                                                                                                           |
| ------------------ | ----------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| KianV uLinux       | Flash bootloader, dtb and Image ([guide](guides/kianv-linux.md)); `uart_console.kianv()`  | `/ #` within about 160 s; `uname -a`; `/root/hello_7seg` shows HELLO 0-9; `/root/rust_nostd_hello` and `/root/rust_std_demo` exit 0 |
| KianV memtest      | Flash at 0x100000; `uart_console.kianv_memtest()`, then `kianv_memtest(thorough=True)`    | `PASS` in both modes                                                                                                                |
| KianV Zephyr       | Flash at 0x100000; `uart_console.kianv()`, press `z`                                      | `kernel uptime` follows the wall clock; `kernel sleep 1000` returns; `fs write`/`fs read` on `/RAM`                                 |
| KianV MicroPython  | Same image, press `m`                                                                     | `>>>` prompt; `print(355/113)`                                                                                                      |
| TinyQV MicroPython | [program.tinyqv.com](https://program.tinyqv.com), Custom, 495 then 687, latency 2, 64 MHz | REPL; `2**100`; `Pin(15).value() == 1`                                                                                              |
| TinyQV Rust        | Same, with `tinyqv-rust-hello.bin`                                                        | Banner and `sieve: 1229 primes`; `tqv>` answers `primes 1000`                                                                       |
| TinyQV Breakout    | Same, with `tinyqv-breakout.bin`, design 495 only; TinyVGA, Gamepad and Audio Pmods       | Without a controller: `DEMO` and the game plays itself, with sound. With one: the paddle moves, A serves                            |

Finish with the KianV uLinux image on the Pmod, so the board is ready for the next person.
