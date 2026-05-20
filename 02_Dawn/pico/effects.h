/* Text and blur effects.
 *
 * Text glyphs are bitmap blobs from dawn_final.s (.dawn / .by / .azure).
 * Blur is the same chunky_blur_vertical() (chunky.c) the original
 * intersperses with rendering.
 */

#ifndef EFFECTS_H
#define EFFECTS_H

#include "dawn.h"

typedef enum {
    TEXT_DAWN,
    TEXT_BY,
    TEXT_AZURE,
} text_id_t;

void effects_text_render(text_id_t which);

/* Fadeout: 70 frames of blur with no new rendering on top. Returns true
 * when finished. Caller spins on this in a fadeout loop. */
void effects_fadeout_start(void);
bool effects_fadeout_active(void);
void effects_fadeout_step(void);   /* one frame */

#endif
