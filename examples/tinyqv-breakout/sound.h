/* Breakout's sound effects and jingles on the PiecewiseOrionSynth (peripheral 33, Toivo Henningsson). */
#pragma once

enum tune { TUNE_NEW_GAME, TUNE_LEVEL_CLEAR, TUNE_GAME_OVER };

/* Route the synth to the output pins and silence all channels. */
void sound_init(void);
/* Advance the jingle sequencer; call once per frame. */
void sound_tick(void);
void sound_play_tune(enum tune t);

void sound_paddle(void);
void sound_wall(void);
/* height: brick row counted from the bottom row (0); combo: bricks hit since the paddle. */
void sound_brick(int height, int combo);
void sound_ball_lost(void);
