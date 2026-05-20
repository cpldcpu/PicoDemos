/* Palette — 64-color procedural generation.
 *
 * Original: dawn_final.s:982-1023 (makedacols routine).
 * Each scheme is a (rMul, gMul, bMul) triple in [0..65535] applied to a
 * (i^0x3F) intensity curve. We precompute RGB-565 for the scanline path.
 */

#ifndef PALETTE_H
#define PALETTE_H

#include "dawn.h"

/* Selectable palette schemes — matches the original's colors1..colors5. */
typedef enum {
    PAL_SCHEME_1,  /* warm red/orange (main)        */
    PAL_SCHEME_2,  /* "by" text                     */
    PAL_SCHEME_3,  /* "azure" text (greenish)       */
    PAL_SCHEME_4,  /* finale (grayscale)            */
    PAL_SCHEME_5,  /* alternate (golden)            */
} pal_scheme_t;

/* Live palette in RGB-565 layout (R bits 0..4, G 6..10, B 11..15) ready
 * for pico_scanvideo. Read by the scanline callback on core 1.
 *
 * NOTE: writes to this array race with the scanline reader; cosmetic
 * tearing during scheme transitions only and lasts <1 ms. */
extern volatile uint16_t palette_rgb565[PAL_SIZE];

void palette_set_scheme(pal_scheme_t s);

#endif
