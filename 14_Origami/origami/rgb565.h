/* Canonical RGB565 bit layout for fb160 + asset palettes.
 *
 * Default pico_scanvideo_dpi pin mapping on the Pimoroni VGA Demo Base:
 *
 *   GP0..GP4   = Red    LSB..MSB   (5 bits)
 *   GP5        = (gap, no DAC pin)
 *   GP6..GP10  = Green  LSB..MSB   (5 bits)
 *   GP11..GP15 = Blue   LSB..MSB   (5 bits)
 *
 * The PIO shifts the 16-bit framebuffer value straight to GP0..GP15 in
 * one cycle, so the framebuffer layout must match the pin order. That
 * means R at the LSB end, B at the MSB end, with a gap at bit 5.
 *
 *   [ 15 14 13 12 11 | 10  9  8  7  6 | 5 | 4  3  2  1  0 ]
 *   [    blue (5)    |   green (5)    | _ |    red (5)    ]
 *
 * This is NOT "standard RGB565" (which is R at MSB, B at LSB, 6 bits of
 * green). The hardware physically can't do 6 bits of green — only 5
 * GPIO pins are wired for the green DAC ladder — so we drop the LSB of
 * green and call it "RGB555-with-gap" everywhere, but the framebuffer
 * type stays uint16_t for convenience.
 *
 * Everything that creates a pixel for fb160 (effects, asset packer,
 * blend math, gradient LUTs) must use these macros so the bit order
 * stays consistent end-to-end. Host SDL also decodes via these macros
 * so preview matches device. */

#ifndef THEDEMO_RGB565_H
#define THEDEMO_RGB565_H

#include <stdint.h>

/* Pack 8-bit RGB into a 16-bit pixel. Low bits of each channel are
 * discarded (truncate-to-zero, not round). */
static inline uint16_t rgb565_pack(int r, int g, int b)
{
    if (r < 0) r = 0; else if (r > 255) r = 255;
    if (g < 0) g = 0; else if (g > 255) g = 255;
    if (b < 0) b = 0; else if (b > 255) b = 255;
    return (uint16_t)(((b & 0xF8) << 8) | ((g & 0xF8) << 3) | (r >> 3));
}

/* Unpack a 16-bit pixel into 8-bit channels (low bits zero, no
 * replication). Returned values are in [0, 248] in steps of 8. */
static inline int rgb565_r8(uint16_t p) { return ( p        & 0x1F) << 3; }
static inline int rgb565_g8(uint16_t p) { return ((p >> 6 ) & 0x1F) << 3; }
static inline int rgb565_b8(uint16_t p) { return ((p >> 11) & 0x1F) << 3; }

/* 5-bit channel accessors (for code that wants the raw values without
 * the << 3 expansion). */
static inline int rgb565_r5(uint16_t p) { return ( p        & 0x1F); }
static inline int rgb565_g5(uint16_t p) { return ((p >> 6 ) & 0x1F); }
static inline int rgb565_b5(uint16_t p) { return ((p >> 11) & 0x1F); }

#endif
