/* Chunky framebuffer — 160×128, 6-bit palette indices.
 *
 * Original: line 379-480 (chunky2planar AGA conversion). We skip the c2p
 * pass entirely — the scanline callback reads chunky bytes directly and
 * looks up palette[idx] inline. See vga.c.
 */

#ifndef CHUNKY_H
#define CHUNKY_H

#include "dawn.h"

/* `chunky` points to the BACK buffer — what the engine renders into.
 * `chunky_scanout` points to the FRONT buffer — what core 1's scanline
 * reader displays. They get swapped by chunky_present(), which waits for
 * the next vblank and then atomically publishes the new front pointer.
 * Two backing buffers live in chunky.c. */
extern uint8_t *chunky;
extern uint8_t * volatile chunky_scanout;

/* Wait for vblank, then swap front/back buffers. Call once per rendered
 * frame, after the sequencer has finished painting. */
void chunky_present(void);

void chunky_clear(uint8_t idx);

static inline void chunky_setpixel(int x, int y, uint8_t idx)
{
    if ((unsigned)x < SCREEN_W && (unsigned)y < SCREEN_H)
        chunky[y * SCREEN_W + x] = idx & 0x3F;
}

/* Vertical 2-tap averaging blur with the original palette-pair lookup.
 * Caller passes blur_get() for each pair (or use the inline computer in
 * effects.c, which doesn't require a 8 KB table).
 *
 * Reference: dawn_final.s:653-670 (blur_it). */
void chunky_blur_vertical(void);

#endif
