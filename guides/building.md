# Building the images

Every image is built from pinned upstream sources ([`../pins.env`](../pins.env)) plus the patches in
[`../patches`](../patches). The [CI workflow](../.github/workflows/build.yml) runs the same scripts, and a
manual run with **release** checked publishes a GitHub release.

| Image                      | Script                                | Needs                                                                                                       | Time (cold)               |
| -------------------------- | ------------------------------------- | ----------------------------------------------------------------------------------------------------------- | ------------------------- |
| KianV uLinux               | `scripts/build-ulinux.sh`             | Linux, build-essential, device-tree-compiler, cpio, rsync, bc, wget, unzip, file; rustup for the Rust demos | 30-60 min (Buildroot)     |
| KianV Zephyr + MicroPython | `scripts/build-zephyr.sh`             | Docker, python3, git                                                                                        | about 15 min              |
| KianV memtest              | `scripts/build-memtest.sh`            | `gcc-riscv64-unknown-elf`                                                                                   | seconds                   |
| TinyQV Rust example        | `scripts/build-tinyqv-rust.sh`        | rustup (nightly from `pins.env`, rust-src, llvm-tools)                                                      | about 1 min               |
| TinyQV Breakout (C)        | `scripts/build-tinyqv-breakout.sh`    | wget, git, make; glibc 2.38+ (for the tinyQV toolchain)                                                     | seconds + 1 GB download   |
| TinyQV MicroPython         | `scripts/fetch-tinyqv-micropython.sh` | curl                                                                                                        | (download + sha256 check) |

Each script writes its files to `out/<image>/` with a `SHA256SUMS`. Sources go to `build/`.

```sh
scripts/build-ulinux.sh          # -> out/kianv-ulinux/{bootloader.bin,kianv.dtb,Image}
```

To change KianV code, run `scripts/fetch-kianriscv.sh` and work in `build/kianRiscV` (it is a git
checkout with the patches applied as commits). Then turn your commits into patches; see
[`../patches/README.md`](../patches/README.md).

Images are built for 30 MHz / 115200 (`KIANV_CPU_FREQ`, `KIANV_BAUDRATE` in `pins.env`). A shuttle
with a different KianV clock (for example the 16 MHz GF chips) needs its own build.

## KianV Linux: programs and SDK

The programs in `/root` come from [`linux/apps`](../linux/apps) through the Buildroot external tree in
[`linux/br2-external`](../linux/br2-external); `build-ulinux.sh` passes it to Buildroot as `BR2_EXTERNAL` and
rebuilds them on every run. To add your own, see [`linux/README.md`](../linux/README.md).

| Variable              | Effect                                                                                   |
| --------------------- | ---------------------------------------------------------------------------------------- |
| `BUILD_SDK=1`         | Also build the SDK (`kianv-ulinux-sdk.tar.gz`: toolchain, `elf2flt`, QEMU; about 176 MB) |
| `KIANRISCV_DIR=<dir>` | Use an existing kianRiscV checkout instead of `build/kianRiscV`                          |

To build a single program without building the image, use the SDK from a release: [linux-apps.md](linux-apps.md).
