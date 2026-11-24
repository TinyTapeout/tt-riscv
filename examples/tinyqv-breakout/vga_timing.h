/* VGA timing from the vga_gfx peripheral (9) on TinyQV "Asteroids" (ttsky25a #495). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <peripherals/vga_gfx.h>
#include <mul.h>

/* Current VGA line 0..805 (0..767 visible, 768.. vertical blank). */
static inline int vga_scanline(void) {
    uint32_t hi, lo, hi2;
    do {
        asm volatile("lbu %0, 0x243(tp)" : "=r"(hi));
        asm volatile("lbu %0, 0x242(tp)" : "=r"(lo));
        asm volatile("lbu %0, 0x243(tp)" : "=r"(hi2));
    } while (hi != hi2);
    return mul32x16(hi, 48) + (lo & 63);
}

/* Wait for the start of the next vertical blank. */
static inline void vga_wait_vblank_start(void) {
    uint32_t hi;
    do {
        asm volatile("lbu %0, 0x243(tp)" : "=r"(hi));
    } while (hi == 16);
    vga_gfx_wait_for_vsync();
}
