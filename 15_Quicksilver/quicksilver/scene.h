/* Scene/effect interface for 20_TheDemo.
 *
 * An EFFECT is a single visual segment of the demo. The scene runner calls
 * its init(), drives frame() at the display refresh rate while the effect
 * is the active one, and calls done() before moving on.
 *
 * The TIMELINE is the demo's storyboard: an ordered array of {start_ms,
 * end_ms, effect*} entries. The runner reads the audio playback ms each
 * iteration, finds the entry whose interval contains it, and switches
 * effects (with a mode change if needed) at boundaries.
 *
 * Effects do NOT own scanvideo. They write into the framebuffer that
 * vga.h hands out for their declared mode, and the VGA backend takes
 * care of timing + composable-scanline emission.
 */

#ifndef THEDEMO_SCENE_H
#define THEDEMO_SCENE_H

#include <stdint.h>

typedef enum {
    MODE_320 = 0,   /* 320x240 chunky 8bpp palettized */
    MODE_160 = 1,   /* 160x120 RGB565 truecolor       */
    MODE_HIRES = 2, /* 320x240 RGB565 truecolor — full native resolution.
                     * Shares framebuffer bytes with the other modes (only
                     * one mode is live at a time; switches land on a black
                     * frame). Used by the cosmic per-pixel scenes. */
    /* Copper-style raster split: display scanlines [0, split_row) come
     * from fb160 RGB565, scanlines [split_row, 240) come from fb320 8bpp.
     * Lets one scene do truecolor work in the upper region (e.g. blended
     * metaballs) AND keep a high-res chunky-pixel layer in the lower
     * region (e.g. crisp 8bpp scroller text). The scene populates both
     * back buffers and presents both. */
    MODE_SPLIT_160_OVER_320 = 3,

    /* Beam-raced full-VGA: the scene registers a per-scanline generator (via
     * vga_set_race_fn) that core 1 runs straight into the 640-wide output line
     * — no framebuffer. Used by the full-VGA rotozoom and Mode-7. */
    MODE_RACE = 4,
} screen_mode_t;

typedef struct effect {
    const char     *name;
    screen_mode_t   mode;
    void          (*init)(void);
    void          (*frame)(uint32_t t_ms_into_scene, uint32_t t_ms_global);
    void          (*done)(void);
} effect_t;

typedef struct timeline_entry {
    uint32_t          start_ms;
    uint32_t          end_ms;
    const effect_t   *effect;
} timeline_entry_t;

/* The arc, defined in timeline.c. */
extern const timeline_entry_t  timeline[];
extern const int               timeline_count;

/* Per-entry transition style (QS_TR_*) used when LEAVING that entry — themed to
 * the scenes it joins. Parallel to timeline[]. Defined in timeline.c. */
extern const uint8_t           timeline_trans[];

/* scene.c: drives the timeline. Call from main loop. Returns true if the
 * demo is still running, false when past the end (caller can loop or
 * idle). */
int scene_runner_tick(uint32_t t_ms_global);

/* Returns the end_ms of the timeline entry currently active at t, i.e.
 * the start of the next scene. If t is past the last entry, returns
 * the last end_ms. Used by the host's "skip to next scene" hotkey. */
uint32_t scene_next_boundary_ms(uint32_t t_ms_global);

/* Returns the start_ms of either the current scene (if we're past its
 * first ~0.5 s) or the previous scene (if we're near the current
 * scene's start). This is the "rewind" companion to scene_next — press
 * once to restart current, again to actually go back. */
uint32_t scene_prev_boundary_ms(uint32_t t_ms_global);

/* Start/end ms of the currently-active timeline entry (for effects that want to
 * fade against their own duration, e.g. the credits fade-out). */
uint32_t scene_cur_start_ms(void);
uint32_t scene_cur_end_ms(void);

#endif
