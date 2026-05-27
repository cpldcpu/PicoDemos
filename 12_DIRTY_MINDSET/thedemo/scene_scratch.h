/* Shared per-scene scratch memory for the DIRTY_MINDSET demo.
 *
 * To avoid blowing the 520 KB SRAM limit on the RP2350, heavy per-scene
 * buffers share the same physical memory space via this global union.
 * Only one scene is active at a time, meaning each scene must treat its
 * union member as uninitialised on init() and populate it from scratch.
 */

#ifndef DIRTY_MINDSET_SCENE_SCRATCH_H
#define DIRTY_MINDSET_SCENE_SCRATCH_H

#include <stdint.h>
#include "vga.h"

union scene_scratch_u {
    /* 320x240 background buffer (76,800 bytes) used for backdrops,
     * scrolling text buffers, and rotozoom caches. Reused across scenes. */
    uint8_t bg_cache[VGA_320_W * VGA_320_H];

    /* Scene 3: Text Matrix */
    struct {
        int16_t column_y[40];                        // Per-column scroll position (signed to avoid wrap-around locks)
        uint8_t column_speed[40];                    // Per-column speed
        uint16_t text_offsets[40];                   // Message data offsets
    } matrix;

    /* Scene 4: Dirty Logo (3D wireframe / vector engine) */
    struct {
        float px[128];
        float py[128];
        float pz[128];
        uint16_t lines_start[256];
        uint16_t lines_end[256];
    } logo;

    /* Scene 5: Reaction-Diffusion */
    struct {
        float grid_a[80 * 60];       // Chemical A concentration
        float grid_b[80 * 60];       // Chemical B concentration
        float grid_a_next[80 * 60];  // Double-buffer for A
        float grid_b_next[80 * 60];  // Double-buffer for B
    } rd;                            // 76,800 bytes total

    /* Scene 6: Mandelbrot Zoom */
    struct {
        uint8_t iter_cache[VGA_160_W * VGA_160_H];   // Iteration count cache
    } fractal;

    /* Scene 7: Greetings scroller */
    struct {
        uint8_t text_buffer[80 * 240];               // Scroll text render cache
    } greetings;
};

extern union scene_scratch_u g_scratch;

#endif
