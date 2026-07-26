/* Screen modes for SUSTAIN.
 *
 * SUSTAIN has no scenes. Where QUICKSILVER had an effect_t table swapped at
 * timeline boundaries, this demo has one continuous world (world.h) sampled by
 * one renderer along one camera spline. What survives from the old header is
 * the screen-mode enum (vga.c/vga_sdl.c are reused verbatim and need it) and
 * the runner entry points main.c/main_host.c call — those are implemented in
 * world.c now.
 *
 * MODE_320 / MODE_160 / MODE_SPLIT are retained only because vga.c implements
 * them; SUSTAIN never leaves MODE_HIRES (and later MODE_RACE). Switching modes
 * costs a black frame, which this demo is not allowed to produce.
 */

#ifndef SUSTAIN_SCENE_H
#define SUSTAIN_SCENE_H

#include <stdint.h>

typedef enum {
    MODE_320 = 0,   /* 320x240 chunky 8bpp palettized  (unused by SUSTAIN) */
    MODE_160 = 1,   /* 160x120 RGB565 truecolor        (unused by SUSTAIN) */
    MODE_HIRES = 2, /* 320x240 RGB565 truecolor — SUSTAIN's mode */
    MODE_SPLIT_160_OVER_320 = 3,           /* (unused by SUSTAIN) */
    MODE_RACE = 4,  /* beam-raced full-VGA, no framebuffer */
} screen_mode_t;

/* Drives the world. Call from the main loop with the demo clock in ms.
 * Returns non-zero while the demo is running, 0 once past the end. */
int scene_runner_tick(uint32_t t_ms_global);

/* Host navigation hooks (SPACE / LEFT / R). SUSTAIN has no scene boundaries,
 * so these step by ARC NODE — the moment a morph completes — purely as an
 * authoring convenience. They are never used in a real playback. */
uint32_t scene_next_boundary_ms(uint32_t t_ms_global);
uint32_t scene_prev_boundary_ms(uint32_t t_ms_global);

uint32_t scene_cur_start_ms(void);
uint32_t scene_cur_end_ms(void);

#endif
