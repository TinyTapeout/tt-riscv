# Patches

`kianRiscV/` is a `git am` series on top of `KIANRISCV_COMMIT` from [`../pins.env`](../pins.env):
changes to the KianV SoC software itself (configuration, kernel, bootloader, Zephyr port).
`scripts/fetch-kianriscv.sh` clones the pinned commit and applies it.

Programs added to the Linux image are not patches: they are plain source in [`../linux/apps`](../linux/apps),
built by the Buildroot external tree in [`../linux/br2-external`](../linux/br2-external).

| Patch     | What                                                                                 | Status             |
| --------- | ------------------------------------------------------------------------------------ | ------------------ |
| 0001-0003 | uLinux for the 16 MiB PSRAM + GPIO SoC variant (ttsky25a and later), 30 MHz / 115200 | tested on ttsky25a |
| 0004      | Pin the Buildroot commit                                                             | build only         |
| 0005      | `make xip`: Linux kernel executing in place from SPI flash (experimental, opt-in)    | tested on ttsky25a |
| 0006      | Kernel AMOs never use rd == rs2 (ttsky25b regfile edition erratum)                   | tested on ttsky25b |

To refresh a patch: apply the series in a clone, change and commit there, then
`git format-patch --keep-subject --no-signature --zero-commit <KIANRISCV_COMMIT>..HEAD -o patches/kianRiscV -- . ':(exclude).github'`.
