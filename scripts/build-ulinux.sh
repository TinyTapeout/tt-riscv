#!/usr/bin/env bash
# Build the KianV uLinux image (16 MiB PSRAM variant): bootloader.bin, kianv.dtb, Image, with the
# programs from linux/apps in /root (added through the Buildroot external tree in linux/br2-external).
# Needs: build-essential, device-tree-compiler, cpio, rsync, bc, wget, unzip, file, and for the
# Rust programs rustup (the pinned nightly + rust-src are installed on demand).
# Usage: scripts/build-ulinux.sh [out-dir]   (default: out/kianv-ulinux)
# Env:   KIANRISCV_DIR  kianRiscV checkout to use (default: build/kianRiscV, fetched and patched)
#        BUILD_SDK=1    also build the Buildroot SDK (toolchain + host tools) into out-dir
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
. "$ROOT/pins.env"
OUT=${1:-$ROOT/out/kianv-ulinux}
mkdir -p "$OUT" && OUT=$(cd "$OUT" && pwd)  # absolute: the script changes directory
SRC=${KIANRISCV_DIR:-$ROOT/build/kianRiscV}
"$ROOT/scripts/fetch-kianriscv.sh" "$SRC"
# Our programs: Buildroot picks up linux/br2-external from the environment (BR2_EXTERNAL), and the
# Rust packages use the nightly pinned in pins.env.
export BR2_EXTERNAL="$ROOT/linux/br2-external"
export RUSTUP_TOOLCHAIN="$RUST_NIGHTLY" RUST_STD_TOOLCHAIN="$RUST_NIGHTLY"
if command -v rustup >/dev/null; then
  rustup toolchain install "$RUST_NIGHTLY" --profile minimal --component rust-src
else
  echo "warning: rustup not found, the Rust demos will be skipped" >&2
fi
# GCC 15+ defaults to C23, which the pinned Buildroot's host packages (e.g. gnulib in host-m4) do
# not build with, and GCC 14+ turns old C (implicit declarations, e.g. in host-libffi) into errors.
# Give Buildroot a host gcc that defaults to GNU C17 with -fpermissive; an explicit -std from a
# package still wins because it comes later on the command line.
if [ -z "${HOSTCC:-}" ] && [ "$(gcc -dumpversion | cut -d. -f1)" -ge 15 ]; then
  mkdir -p "$ROOT/build/hostcc"
  printf '#!/bin/sh\nexec %s -std=gnu17 -fpermissive "$@"\n' "$(command -v gcc)" > "$ROOT/build/hostcc/gcc"
  chmod +x "$ROOT/build/hostcc/gcc"
  export HOSTCC="$ROOT/build/hostcc/gcc"
  echo "host gcc $(gcc -dumpversion): building host tools with $HOSTCC (-std=gnu17 -fpermissive)"
fi
cd "$SRC/asic/os/ulinux_asic_kianv_soc"
# A restored CI cache brings back buildroot/dl and buildroot/output but not the Buildroot checkout
# itself. The Makefile would then run git in the parent repo, so recreate the pinned checkout in place.
if [ -d buildroot ] && [ ! -d buildroot/.git ]; then
  br_url=$(sed -n 's/^REPO_URL := //p' Makefile)
  br_sha=$(sed -n 's/^BUILDROOT_SHA := //p' Makefile)
  git -C buildroot init -q
  git -C buildroot remote add origin "$br_url"
  git -C buildroot fetch -q --depth 1 origin "$br_sha"
  git -C buildroot -c advice.detachedHead=false checkout -q -f FETCH_HEAD
fi
# Packages built from a local directory are not rebuilt when their source changes, and CI restores
# an old build tree from its cache: always rebuild our programs. Remove their build directories
# directly (that is all <pkg>-dirclean does): CI restores buildroot/output but not buildroot/.config,
# so `make <pkg>-dirclean` cannot run at this point.
rm -rf buildroot/output/build/tt-hello-7seg buildroot/output/build/tt-rust-nostd buildroot/output/build/tt-rust-std
# Likewise Buildroot does not rebuild an already built kernel when its patches change: rebuild it
# from scratch whenever the kernel patches or config differ from those of the cached build.
kpatches=bldroot/board/kianv/rv32ima/patches/linux
kinputs=$(cat "$kpatches"/* bldroot/board/kianv/rv32ima/linux.config | sha256sum | cut -d' ' -f1)
kstamp=buildroot/output/.tt-riscv-kernel-inputs
if [ "$(cat "$kstamp" 2>/dev/null)" != "$kinputs" ]; then
  echo "kernel patches or config changed: rebuilding the kernel"
  rm -rf buildroot/output/build/linux-[0-9]* "$kstamp"
fi
make all CPU_FREQ="$KIANV_CPU_FREQ" BAUDRATE="$KIANV_BAUDRATE"
# Fail if any of our kernel patches is missing from the kernel that was just built.
applied=$(echo buildroot/output/build/linux-[0-9]*/.applied_patches_list)
for p in "$kpatches"/*.patch; do
  grep -q "$(basename "$p")" "$applied" || { echo "error: $(basename "$p") not applied to the kernel" >&2; exit 1; }
done
echo "$kinputs" > "$kstamp"
# The bootloader copies a fixed IMAGE_SIZE, so it must be built after the Image.
make bootloader CPU_FREQ="$KIANV_CPU_FREQ" BAUDRATE="$KIANV_BAUDRATE"
mkdir -p "$OUT"
cp bootloader/bootloader.bin kianv.dtb buildroot/output/images/Image "$OUT/"
files="bootloader.bin kianv.dtb Image"
if [ "${BUILD_SDK:-0}" = 1 ]; then
  make -C buildroot sdk
  cp buildroot/output/images/riscv32-buildroot-linux-uclibc_sdk-buildroot.tar.gz "$OUT/kianv-ulinux-sdk.tar.gz"
  files="$files kianv-ulinux-sdk.tar.gz"
fi
# shellcheck disable=SC2086
(cd "$OUT" && sha256sum $files > SHA256SUMS && ls -l)
