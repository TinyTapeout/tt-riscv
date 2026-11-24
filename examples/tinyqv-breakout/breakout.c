/* Breakout for TinyQV "Asteroids" (ttsky25a #495): vga_gfx (peripheral 9, Mike Bell) for video,
 * game_pmod (peripheral 3, Mike Bell) for input, PiecewiseOrionSynth (peripheral 33, Toivo
 * Henningsson) for sound.
 *
 * 256x192, 2 bpp. The CPU is slow (RV32EC, code executed from QSPI flash), so the game
 * never redraws the screen: it draws only what changed (ball, paddle, broken bricks, HUD)
 * at the start of vertical blank, then runs the game logic for the next frame.
 * Integer only; multiplies use the core's mul16 instruction.
 * No gamepad -> demo mode: the paddle follows the ball with a random aiming error. */
#include <gamepad.h>
#include "sound.h"
#include "vga_timing.h"

#define W 256
#define H 192

/* Layout */
#define HUD_Y 1
#define WALL_Y 12
#define FIELD_TOP 14
#define FIELD_L 2
#define FIELD_R 254
#define BR_ROWS 6
#define BR_COLS 14
#define BR_TOP 30
#define BR_CW 18
#define BR_CH 8
#define PY 178
#define PH 4
#define PW 28
#define BS 3

/* Colours: index 1..3 -> RRGGBB. Bit 0 (blue LSB) is kept 0: uo_out[6] carries audio. */
#define COL_WHITE 0x3e
#define COL_RED 0x30
#define COL_YELLOW 0x3c

static const uint8_t row_colour[BR_ROWS] = {2, 2, 3, 3, 1, 1};
static const uint8_t row_points[BR_ROWS] = {7, 7, 4, 4, 1, 1};

/* 2-bpp word of 16 pixels, all in colour c */
#define SOLID_PATTERN(c) (0x55555555u * (c))

/* Ball directions for 8 paddle zones at speed 2.5 px/frame (8.8 fixed point):
 * 60, 45, 30, 15 degrees from vertical. */
static const int16_t zone_vx[8] = {-554, -453, -320, -166, 166, 320, 453, 554};
static const int16_t zone_vy[8] = {-320, -453, -554, -618, -618, -554, -453, -320};

/* ---------------------------------------------------------------- state */
static uint8_t bricks[BR_ROWS][BR_COLS];
static uint8_t brick_col_at_x[FIELD_R - FIELD_L];
static int bricks_left;

static int px;               /* paddle x, pixels */
static int bx, by, vx, vy;   /* ball, 8.8 fixed point (top-left corner) */
static int speed;            /* ball speed in 1/8 of the zone table */
static int zone;             /* last paddle zone, for re-scaling on speed-up */
static int score, lives, level, hits, combo;
static bool demo;
static int demo_aim_offset;
static uint32_t rng = 7;

enum { ST_READY, ST_SERVE, ST_PLAY, ST_BALL_LOST, ST_CLEAR, ST_OVER };
static int state, timer;

/* What the screen shows; render() brings it up to date. */
static int shown_px = -1, shown_bx = -1, shown_by = -1;
static bool ball_visible;
static uint8_t erased_bricks[8];
static int erased_count;
static bool hud_dirty, bricks_dirty, field_dirty;
static const char *msg, *shown_msg;
#ifdef SHOW_LOAD
static int load_max, load_shown = -1, load_frames;
#endif
/* Brick columns: framebuffer word and pixel masks of each column (brick spans two words),
 * and the mask of all brick columns. Filled by init_masks(). */
static uint8_t col_word[BR_COLS];
static uint32_t col_mask_lo[BR_COLS], col_mask_hi[BR_COLS], brick_cols_mask[16];

static uint32_t xorshift(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

/* ---------------------------------------------------------------- drawing */
/* 3x5 font, drawn at 2x. Row 0 in bits 14..12, MSB = left column. */
static uint16_t glyph(char c) {
    static const uint16_t digits[10] = {
        075557, 026227, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717};
    if (c >= '0' && c <= '9') return digits[c - '0'];
    switch (c) {
    case 'A': return 025755;
    case 'C': return 034443;
    case 'D': return 065556;
    case 'E': return 074647;
    case 'G': return 034553;
    case 'L': return 044447;
    case 'M': return 057555;
    case 'O': return 025552;
    case 'R': return 065655;
    case 'S': return 034216;
    case 'T': return 072222;
    case 'V': return 055552;
    case 'P': return 065644;
    case 'Y': return 055222;
    default: return 0;
    }
}

static void draw_char(uint32_t *fb, char c, int x, int y, int colour) {
    uint32_t rows[10];
    uint16_t g = glyph(c);
    for (int r = 0; r < 5; r++) {
        int bits = (g >> (12 - 3 * r)) & 7;
        uint32_t w = 0;
        for (int col = 0; col < 3; col++)
            if (bits & (4 >> col)) w |= (uint32_t)(colour | (colour << 2)) << (4 * col);
        rows[2 * r] = rows[2 * r + 1] = w;
    }
    vga_gfx_simple_blit(fb, rows, x, y, 6, 10);
}

static int text_width(const char *s) {
    int n = 0;
    while (*s++) n += 8;
    return n - 2;
}

static void draw_text(uint32_t *fb, const char *s, int x, int y, int colour) {
    for (; *s; s++, x += 8)
        if (*s != ' ') draw_char(fb, *s, x, y, colour);
}

/* n as `digits` decimal digits, without division. */
static void draw_number(uint32_t *fb, int n, int digits, int x, int y, int colour) {
    static const int pow10[5] = {10000, 1000, 100, 10, 1};
    char s[6];
    for (int i = 0; i < digits; i++) {
        int p = pow10[5 - digits + i], d = 0;
        while (n >= p) { n -= p; d++; }
        s[i] = '0' + d;
    }
    s[digits] = 0;
    vga_gfx_box(0, fb, x, y, digits * 8, 10);
    draw_text(fb, s, x, y, colour);
}

static void brick_box(uint32_t *fb, int r, int c, int colour) {
    vga_gfx_box(colour, fb, FIELD_L + mul32x16(c, BR_CW) + 1, BR_TOP + (r << 3) + 1, BR_CW - 2, BR_CH - 2);
}

static void init_masks(void) {
    for (int c = 0; c < BR_COLS; c++) {
        int x = FIELD_L + mul32x16(c, BR_CW) + 1, sh = (x & 15) << 1;
        col_word[c] = x >> 4;
        col_mask_lo[c] = ~0u << sh;                     /* pixels x.. of the first word */
        col_mask_hi[c] = sh ? ~0u >> (32 - sh) : 0;     /* ..x+15 in the next word */
        brick_cols_mask[x >> 4] |= col_mask_lo[c];
        if (sh) brick_cols_mask[(x >> 4) + 1] |= col_mask_hi[c];
    }
}

/* All bricks, a row at a time: whole-word writes of the row's pattern, masked to the live
 * bricks (dead bricks and the gaps inside brick columns are cleared, walls left alone). */
static void draw_bricks(uint32_t *fb) {
    for (int r = 0; r < BR_ROWS; r++) {
        uint32_t live[16] = {0};
        for (int c = 0; c < BR_COLS; c++)
            if (bricks[r][c]) {
                live[col_word[c]] |= col_mask_lo[c];
                live[col_word[c] + 1] |= col_mask_hi[c];
            }
        uint32_t pat = SOLID_PATTERN(row_colour[r]);
        uint32_t *p = fb + ((BR_TOP + (r << 3) + 1) << 4);
        for (int l = 0; l < BR_CH - 2; l++, p += 16)
            for (int j = 0; j < 16; j++)
                if (brick_cols_mask[j]) p[j] = (p[j] & ~brick_cols_mask[j]) | (pat & live[j]);
    }
}

static void draw_field(uint32_t *fb) {
    vga_gfx_clear(0, fb);
    vga_gfx_box(1, fb, 0, WALL_Y, W, 2);
    vga_gfx_box(1, fb, 0, WALL_Y, 2, H - WALL_Y);
    vga_gfx_box(1, fb, FIELD_R, WALL_Y, 2, H - WALL_Y);
}

static void draw_hud(uint32_t *fb) {
    draw_number(fb, score, 5, 4, HUD_Y, 1);
    vga_gfx_box(0, fb, 96, HUD_Y, 40, 10);
    if (demo) draw_text(fb, "DEMO", 98, HUD_Y, 3);
    vga_gfx_box(0, fb, 206, HUD_Y, 46, 10);
    for (int i = 0; i < lives; i++) vga_gfx_box(2, fb, 246 - 9 * i, HUD_Y + 2, 6, 6);
}

#define MSG_Y 110
static void render(void) {
    uint32_t *fb = vga_gfx_front_buffer;

    if (field_dirty) {
        draw_field(fb);
        field_dirty = false;
        bricks_dirty = true;
        hud_dirty = true;
        shown_px = shown_bx = -1;
        shown_msg = 0;
    }
    if (bricks_dirty) {
        draw_bricks(fb);
        bricks_dirty = false;
        erased_count = 0;
    }
    for (int i = 0; i < erased_count; i++) brick_box(fb, erased_bricks[i] >> 4, erased_bricks[i] & 15, 0);
    erased_count = 0;

    int nbx = ball_visible ? bx >> 8 : -1, nby = by >> 8;
    bool paddle_dirty = shown_px != px;
    if (shown_bx >= 0 && (shown_bx != nbx || shown_by != nby)) {
        vga_gfx_box(0, fb, shown_bx, shown_by, BS, BS);
        if (shown_by + BS > PY && shown_by < PY + PH) paddle_dirty = true;
        shown_bx = -1;
    }
    if (paddle_dirty) {
        if (shown_px >= 0) vga_gfx_box(0, fb, shown_px, PY, PW, PH);
        vga_gfx_box(1, fb, px, PY, PW, PH);
        shown_px = px;
    }
    if (nbx >= 0 && nby < H && (shown_bx != nbx || shown_by != nby)) {
        vga_gfx_box(1, fb, nbx, nby, BS, BS);
        shown_bx = nbx;
        shown_by = nby;
    }

    if (msg != shown_msg) {
        vga_gfx_box(0, fb, FIELD_L, MSG_Y, FIELD_R - FIELD_L, 10);
        if (msg) draw_text(fb, msg, (W - text_width(msg)) >> 1, MSG_Y, 3);
        shown_msg = msg;
    }
    if (hud_dirty) {
        draw_hud(fb);
        hud_dirty = false;
#ifdef SHOW_LOAD
        load_shown = -1; /* also after a field redraw: that sets hud_dirty */
#endif
    }
#ifdef SHOW_LOAD
    /* Development: CPU load = VGA lines (of 806 per frame) spent from vblank start to the
     * end of the game step, worst case over the last 60 frames. Shown in red as "L nnn". */
    if (load_shown != load_max && load_frames == 0) {
        draw_text(fb, "L", 160, HUD_Y, 2);
        draw_number(fb, load_max, 3, 170, HUD_Y, 2);
        load_shown = load_max;
    }
#endif
}

/* ---------------------------------------------------------------- game */
/* Launch in the direction of `zone` at the current speed. */
static void set_velocity(void) {
    vx = mul32x16(zone_vx[zone], speed) >> 3;
    vy = mul32x16(zone_vy[zone], speed) >> 3;
}

/* New speed, same direction. */
static void rescale_velocity(void) {
    int sx = vx < 0, sy = vy < 0;
    set_velocity();
    if ((vx < 0) != sx) vx = -vx;
    if ((vy < 0) != sy) vy = -vy;
}

/* Aiming error of the demo player: -19..19 pixels, so it misses now and then. */
static int demo_aim_error(void) { return (int)(xorshift() & 31) + (int)(xorshift() & 7) - 19; }

static void new_level(void) {
    for (int r = 0; r < BR_ROWS; r++)
        for (int c = 0; c < BR_COLS; c++) bricks[r][c] = 1;
    bricks_left = BR_ROWS * BR_COLS;
    hits = 0;
    bricks_dirty = true;
}

static void serve(void) {
    state = ST_SERVE;
    timer = 40;
    ball_visible = true;
    speed = 8 + (level < 4 ? level : 4);
    combo = 0;
    demo_aim_offset = demo_aim_error();
}

static void new_game(void) {
    score = 0;
    lives = 3;
    level = 0;
    px = (W - PW) >> 1;
    field_dirty = true;
    msg = 0;
    new_level();
    serve();
    /* A player starts the game with a button press: the monitor may still be syncing to the
     * VGA signal at power-up. The demo starts right away. */
    state = ST_READY;
    if (!demo) msg = "PRESS START";
}

/* Index (r << 4 | c) of a live brick under any corner of the ball at (x, y), else -1. */
static int brick_at(int x, int y) {
    for (int k = 0; k < 4; k++) {
        int cx = x + ((k & 1) ? BS - 1 : 0), cy = y + ((k & 2) ? BS - 1 : 0);
        int ry = cy - BR_TOP;
        if (ry < 0 || ry >= BR_ROWS * BR_CH || cx < FIELD_L || cx >= FIELD_R) continue;
        int r = ry >> 3, c = brick_col_at_x[cx - FIELD_L];
        if (bricks[r][c]) return (r << 4) | c;
    }
    return -1;
}

static void hit_brick(int idx) {
    int r = idx >> 4, c = idx & 15;
    bricks[r][c] = 0;
    if (erased_count < 8) erased_bricks[erased_count++] = idx;
    score += row_points[r];
    bricks_left--;
    hits++;
    hud_dirty = true;
    sound_brick(BR_ROWS - 1 - r, combo);
    combo++;
    /* Speed-ups: after 4 and 12 hits, and on reaching the top two rows */
    int s = speed;
    if ((hits == 4 || hits == 12) && speed < 12) s++;
    if (r < 2 && speed < 11) s = 11;
    if (s != speed) { speed = s; rescale_velocity(); }
}

/* Buttons of every connected pad (the Pmod has two ports; either may be used). */
static gamepad_state_t pad_state(void) {
    gamepad_state_t g;
    g.state = 0;
    if (gamepad_1_present()) g.state |= gamepad_1_state().state;
    if (gamepad_2_present()) g.state |= gamepad_2_state().state;
    return g;
}

static void move_paddle(gamepad_state_t g) {
    int target = px;
    if (demo) {
        target = (bx >> 8) + 1 - (PW >> 1) + demo_aim_offset;
        int d = target - px;
        if (d > 4) d = 4;
        if (d < -4) d = -4;
        target = px + d;
    } else {
        int sp = g.b ? 5 : 3;
        if (g.left) target -= sp;
        if (g.right) target += sp;
    }
    if (target < FIELD_L) target = FIELD_L;
    if (target > FIELD_R - PW) target = FIELD_R - PW;
    px = target;
}

static void step_ball(void) {
    int oy = by >> 8;
    int nx = bx + vx, x = nx >> 8;
    if (x < FIELD_L) { nx = FIELD_L << 8; vx = -vx; sound_wall(); }
    else if (x > FIELD_R - BS) { nx = (FIELD_R - BS) << 8; vx = -vx; sound_wall(); }
    else {
        int b = brick_at(x, oy);
        if (b >= 0) { hit_brick(b); vx = -vx; nx = bx; }
    }
    bx = nx;

    int ny = by + vy, y = ny >> 8;
    if (y < FIELD_TOP) { ny = FIELD_TOP << 8; vy = -vy; sound_wall(); }
    else {
        int b = brick_at(bx >> 8, y);
        if (b >= 0) { hit_brick(b); vy = -vy; ny = by; }
    }
    y = ny >> 8;
    x = bx >> 8;
    /* Paddle: the ball's bottom edge crosses the paddle's top edge this frame */
    if (vy > 0 && oy + BS <= PY && y + BS > PY && x + BS > px && x < px + PW) {
        int rel = x + 1 - px + 2; /* ball centre - px: -1..PW -> 1..PW+2 */
        zone = rel >> 2;
        if (zone > 7) zone = 7;
        set_velocity();
        ny = (PY - BS) << 8;
        combo = 0;
        demo_aim_offset = demo_aim_error();
        sound_paddle();
    }
    by = ny;

    if ((by >> 8) >= H) {
        ball_visible = false;
        state = ST_BALL_LOST;
        timer = 70;
        sound_ball_lost();
    } else if (bricks_left == 0) {
        state = ST_CLEAR;
        timer = 90;
        ball_visible = false;
        msg = "CLEAR";
        sound_play_tune(TUNE_LEVEL_CLEAR);
    }
}

static void ball_on_paddle(void) {
    bx = (px + (PW >> 1) - 1) << 8;
    by = (PY - BS) << 8;
}

static void update(void) {
    bool pad = gamepad_1_present() || gamepad_2_present();
    if (demo == pad) { demo = !pad; hud_dirty = true; }
    static uint16_t held;
    gamepad_state_t g = pad_state(), pressed;
    pressed.state = g.state & ~held;
    held = g.state;
    bool a_or_start = pressed.a || pressed.start;

    move_paddle(g);
    switch (state) {
    case ST_READY:
        ball_on_paddle();
        if (demo || pressed.state) {
            msg = 0;
            state = ST_SERVE;
            sound_play_tune(TUNE_NEW_GAME);
        }
        break;
    case ST_SERVE:
        ball_on_paddle();
        if (--timer <= 0 || a_or_start) {
            zone = (xorshift() & 1) ? 2 : 5;
            set_velocity();
            state = ST_PLAY;
        }
        break;
    case ST_PLAY:
        step_ball();
        break;
    case ST_BALL_LOST:
        if (--timer <= 0) {
            lives--;
            hud_dirty = true;
            if (lives <= 0) {
                state = ST_OVER;
                timer = 180;
                msg = "GAME OVER";
                sound_play_tune(TUNE_GAME_OVER);
            } else
                serve();
        }
        break;
    case ST_CLEAR:
        if (--timer <= 0) {
            msg = 0;
            level++;
            new_level();
            serve();
        }
        break;
    case ST_OVER:
        if (--timer <= 0 || a_or_start) new_game();
        break;
    }
}

int main(void) {
    for (int x = 0, c = 0, n = 0; x < FIELD_R - FIELD_L; x++) {
        brick_col_at_x[x] = c;
        if (++n == BR_CW) { n = 0; c++; }
    }
    init_masks();

    gamepad_enable();
    vga_gfx_set_colour1(COL_WHITE);
    vga_gfx_set_colour2(COL_RED);
    vga_gfx_set_colour3(COL_YELLOW);
    vga_gfx_clear(0, vga_gfx_front_buffer);
    vga_gfx_start();
    sound_init();

    demo = !(gamepad_1_present() || gamepad_2_present());
    new_game();

    while (1) {
        vga_wait_vblank_start();
#ifdef SHOW_LOAD
        int s0 = vga_scanline();
#endif
        render();
        sound_tick();
        update();
#ifdef SHOW_LOAD
        int lines = vga_scanline() - s0;
        if (lines < 0) lines += 806;
        if (lines > load_max) load_max = lines;
        if (++load_frames == 60) load_frames = 0;
        if (load_frames == 1) load_max = lines;
#endif
    }
}
