#include "effects.h"
#include "chunky.h"

/* Glyph blobs verbatim from web_port effects.ts (which read them from
 * dawn_final.s:631-646). Stored as bytes here (big-endian word order
 * matches the original). */
static const uint8_t TEXT_DAWN_BYTES[] = {
    0x00,0x00, 0x00,0x00, 0xFE,0xFE, 0x82,0x82, 0x82,0xFE,
    0x7C,0x00, 0xFC,0xFE, 0x12,0x12, 0x12,0xFE,
    0xFC,0x00, 0x7E,0xFE, 0x80,0xE0, 0xE0,0x80,
    0xFE,0x7E, 0x00,0xFE, 0xFE,0x02, 0x02,0x02,
    0xFE,0xFC,
};
static const uint8_t TEXT_BY_BYTES[] = {
    0,0,0,0, 0x00,0x00, 0x10,0x38, 0x10,0x00, 0x00,0x00,
    0x00,0xFE, 0xFE,0x92, 0x92,0xFE, 0x6C,0x00,
    0x8E,0x9E, 0x90,0x90, 0xFE,0x7E, 0x00,0x00,
    0x00,0x00, 0x10,0x38, 0x10,0x00, 0x00,0x00,
};
static const uint8_t TEXT_AZURE_BYTES[] = {
    0,0,0,0, 0x00,0x7E, 0x7F,0x09, 0x7F,0x7E, 0x00,0x71,
    0x79,0x49, 0x4F,0x47, 0x00,0x3F, 0x7F,0x40,
    0x40,0x7F, 0x3F,0x00, 0x7F,0x7F, 0x09,0x7F,
    0x76,0x00, 0x7F,0x7F, 0x49,0x49, 0x41,0x00, 0,0,0,0,
};

static const uint8_t *glyph_for(text_id_t which, int *len_out)
{
    switch (which) {
    case TEXT_DAWN:  *len_out = sizeof(TEXT_DAWN_BYTES);  return TEXT_DAWN_BYTES;
    case TEXT_BY:    *len_out = sizeof(TEXT_BY_BYTES);    return TEXT_BY_BYTES;
    case TEXT_AZURE: *len_out = sizeof(TEXT_AZURE_BYTES); return TEXT_AZURE_BYTES;
    }
    *len_out = 0;
    return TEXT_DAWN_BYTES;
}

/* rnd accumulator — adds wobble to the text's screen position frame to
 * frame. dawn_final.s:600-620. */
static uint16_t rnd_state = 0;

void effects_text_render(text_id_t which)
{
    int glen = 0;
    const uint8_t *glyph = glyph_for(which, &glen);

    rnd_state = (uint16_t)(rnd_state + 5);
    int tm_index = rnd_state & 0x3;
    const int y_offset = (((rnd_state >> 2) & 0x3) + 40) * SCREEN_W;

    int glyph_idx = 0;
    /* 39 rows × 4 blocks × 8 bits = letter cells stacked on the chunky
     * buffer with additive (clamped) blend. */
    for (int row = 0; row <= 38; row++) {
        const uint8_t glyph_byte = (glyph_idx < glen) ? glyph[glyph_idx] : 0;
        glyph_idx++;

        for (int block = 0; block < 4; block++) {
            const int base = tm_index;
            tm_index = (tm_index + 1);
            if (tm_index > 160) tm_index = 0;

            int pointer = base + y_offset;
            int mask = glyph_byte;
            for (int bit = 0; bit < 8; bit++) {
                if (mask & 1) {
                    for (int step = 0; step < 8; step++) {
                        if (pointer >= 0 && pointer < SCREEN_BYTES) {
                            int v = chunky[pointer] + 8;
                            if (v > 50) v = 50;
                            chunky[pointer] = (uint8_t)v;
                        }
                        pointer += SCREEN_W;
                    }
                } else {
                    pointer += SCREEN_W * 8;
                }
                mask >>= 1;
            }
        }
    }
}

/* Fadeout state — 70 frames of blur with no rendering, per
 * dawn_final.s:499-509 (the fadeout subroutine). */
#define FADEOUT_FRAMES 70
static int fadeout_phase = -1;     /* -1 = inactive */

void effects_fadeout_start(void)
{
    fadeout_phase = 0;
}

bool effects_fadeout_active(void)
{
    return fadeout_phase >= 0;
}

void effects_fadeout_step(void)
{
    if (fadeout_phase < 0) return;
    chunky_blur_vertical();
    fadeout_phase++;
    if (fadeout_phase >= FADEOUT_FRAMES) {
        fadeout_phase = -1;
    }
}
