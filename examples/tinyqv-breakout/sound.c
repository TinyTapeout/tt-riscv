#include <stdint.h>
#include <stdbool.h>
#include <gpio.h>
#include "sound.h"

#define PERI_IDX_PWL_SYNTH 33

/* Per-channel 16-bit registers, at byte offsets from the channel base. */
enum { R_PERIOD = 0, R_AMP = 2, R_SLOPE_R = 4, R_SLOPE_F = 6, R_PWM = 8, R_MODE = 10, R_SWEEP_PA = 12, R_SWEEP_WS = 14 };
/* Channels, one per effect so they never cut each other off. Jingles share the wall channel. */
enum { CH_PADDLE, CH_BRICK, CH_WALL, CH_BALL_LOST, NUM_CHANNELS };

static void synth_write(int ch, int reg, int v) {
    *(volatile uint16_t *)(PERI_BASE_ADDRESS(PERI_IDX_PWL_SYNTH) + (ch << 4) + reg) = v;
}

/* With no Audio Pmod, uio[7] is the QSPI Pmod's PSRAM B select, and sound on it would make PSRAM B
 * answer reads meant for PSRAM A. A stacked Audio Pmod holds that select high instead. So: write and
 * read back a few bytes of PSRAM B (uio[7] is its select until audio is routed there). If they stick,
 * PSRAM B is reachable, so there is no Audio Pmod and uio[7] stays PSRAM B's select. The patterns
 * differ from the address nibbles the bus last carried, so a floating bus cannot pass the test. */
static bool psram_b_present(void) {
    volatile uint8_t *b = (volatile uint8_t *)0x1800000;
    static const uint32_t off[4] = {0x000000, 0x000101, 0x2aaaaa, 0x555555};
    static const uint8_t pat[4] = {0xa5, 0x5a, 0xc3, 0x3c};
    for (int inv = 0; inv < 2; inv++) {
        uint8_t x = inv ? 0xff : 0;
        for (int i = 0; i < 4; i++) b[off[i]] = pat[i] ^ x;
        for (int i = 0; i < 4; i++)
            if (b[off[i]] != (uint8_t)(pat[i] ^ x)) return false;
    }
    return true;
}

/* The synth's left PWM output goes to uio[7] for an Audio Pmod (only when PSRAM B is not reachable,
 * see above) and to uo_out[6] (TinyVGA B0). The palette never sets B0; on a monitor the ~1 MHz PWM
 * shows as faint blue vertical streaks that shift with the sound, an old-CRT look kept on purpose. */
void sound_init(void) {
    if (!psram_b_present()) set_audio_func_sel(8 + 6); /* audio_select | PWL left on uio[7] */
    set_gpio_func(6, PERI_IDX_PWL_SYNTH);
    for (int ch = 0; ch < NUM_CHANNELS; ch++) synth_write(ch, R_AMP, 0);
}

static const uint16_t mantissa[12] = {1001, 887, 780, 679, 583, 493, 408, 327, 252, 180, 112, 49};

/* f_period for semitone n counted from C0 (C4 = 48 = 261.6 Hz at a 64 MHz clock). */
static int note_period(int n) {
    int m = n + 1, oct = 0;
    while (m >= 12) { m -= 12; oct++; }
    if (oct > 7) oct = 7;
    int mant = mantissa[m];
    if (oct >= 6) mant &= ~0u << (oct - 5);
    return mant | ((7 - oct) << 10);
}

/* sweep_pa: period sign(1 = falling period = rising pitch)/rate, amp target/rate */
#define SWEEP(pdown, prate, atarget, arate) (((pdown) << 12) | ((prate) << 8) | ((atarget) << 4) | (arate))

static void voice(int ch, int note, int amp, int slope_r, int slope_f, int pwm, int mode, int sweep) {
    synth_write(ch, R_SWEEP_PA, 0);
    synth_write(ch, R_AMP, 0);
    synth_write(ch, R_SWEEP_WS, 0);
    synth_write(ch, R_SLOPE_R, slope_r);
    synth_write(ch, R_SLOPE_F, slope_f);
    synth_write(ch, R_PWM, pwm);
    synth_write(ch, R_MODE, mode);
    synth_write(ch, R_PERIOD, note_period(note));
    synth_write(ch, R_AMP, amp);
    synth_write(ch, R_SWEEP_PA, sweep);
}

#define NOTE_C4 48
void sound_paddle(void) { voice(CH_PADDLE, NOTE_C4 + 7, 52, 192, 192, 0, 0, SWEEP(0, 0, 0, 9)); }
void sound_wall(void) { voice(CH_WALL, NOTE_C4 + 24, 36, 0, 0, 0, 0, SWEEP(0, 0, 0, 8)); }
void sound_brick(int height, int combo) {
    if (combo > 12) combo = 12;
    voice(CH_BRICK, NOTE_C4 + 12 + 2 * height + combo, 48, 192, 192, 128, 0, SWEEP(0, 0, 0, 10));
}
void sound_ball_lost(void) { voice(CH_BALL_LOST, NOTE_C4 + 12, 60, 0, 192, 0, 0, SWEEP(0, 8, 0, 13)); }

/* Jingles: {note, frames}, note 0 = end. */
typedef struct { uint8_t note, frames; } tune_step_t;
static const tune_step_t tune_new_game[] = {{60, 7}, {64, 7}, {67, 7}, {72, 14}, {0, 0}};
static const tune_step_t tune_level_clear[] = {{67, 5}, {72, 5}, {76, 5}, {79, 5}, {84, 16}, {0, 0}};
static const tune_step_t tune_game_over[] = {{60, 12}, {55, 12}, {52, 12}, {48, 24}, {0, 0}};
static const tune_step_t *const tunes[] = {tune_new_game, tune_level_clear, tune_game_over};

static const tune_step_t *tune_pos;
static int frames_left;

void sound_play_tune(enum tune t) {
    tune_pos = tunes[t];
    frames_left = 0;
}

void sound_tick(void) {
    if (!tune_pos) return;
    if (frames_left > 0) { frames_left--; return; }
    if (tune_pos->note == 0) {
        synth_write(CH_WALL, R_SWEEP_PA, SWEEP(0, 0, 0, 7));
        tune_pos = 0;
        return;
    }
    voice(CH_WALL, tune_pos->note, 44, 16, 16, 0, 0, SWEEP(0, 0, 0, 11));
    frames_left = tune_pos->frames - 1;
    tune_pos++;
}
