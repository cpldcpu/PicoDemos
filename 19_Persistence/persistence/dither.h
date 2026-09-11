/* Ordered dithering, for a machine whose DAC has five bits a channel.
 *
 * The Pimoroni board's ladder is 5 bits per channel, so an eight-bit gradient
 * quantises to 32 steps and every smooth ramp in the demo -- the copper, the
 * sky above the horizon, the plasma -- lands as visible bands. At 640 wide the
 * bands are wide too.
 *
 * The fix is the one the scene has used since the Amiga: perturb each pixel by
 * a small amount that depends on its position, so the quantiser rounds up on
 * some pixels of a band and down on others, and the eye averages them back to
 * the value that was asked for. A 4x4 Bayer matrix, scaled to plus or minus
 * half a quantisation step, is enough; anything larger reads as noise.
 *
 * It costs nothing per pixel. A row of flat colour is still a fill -- just a
 * fill of a four-pixel repeating pattern instead of one colour -- and the
 * plasma dithers by choosing between four pre-built palettes with a pointer,
 * which is free.
 */

#ifndef PV_DITHER_H
#define PV_DITHER_H

#include <stdint.h>
#include "rgb565.h"

/* 4x4 Bayer, recentred and halved: a five-bit channel steps by 8 in eight-bit
 * terms, so the perturbation wants to span about +-4. */
static const int8_t pv_bayer[4][4] = {
    { -4,  0, -3,  1 },
    {  2, -2,  3, -1 },
    { -3,  1, -4,  0 },
    {  3, -1,  2, -2 },
};

static inline uint16_t pv_pack_dither(int r, int g, int b, int x, int y)
{
    const int d = pv_bayer[y & 3][x & 3];
    return rgb565_pack(r + d, g + d, b + d);
}

/* Fill [x0, x1) with one colour, dithered. The pattern repeats every four
 * pixels, so this is two alternating words. */
static inline void pv_fill_dither(uint16_t *px, int x0, int x1, int r, int g, int b, int y)
{
    if (x0 >= x1) return;
    uint16_t c[4];
    for (int i = 0; i < 4; i++) c[i] = pv_pack_dither(r, g, b, i, y);

    /* head, to a four-pixel boundary */
    while ((x0 & 3) && x0 < x1) { px[x0] = c[x0 & 3]; x0++; }

    const uint32_t w0 = (uint32_t)c[0] | ((uint32_t)c[1] << 16);
    const uint32_t w1 = (uint32_t)c[2] | ((uint32_t)c[3] << 16);
    uint32_t *w = (uint32_t *)(px + x0);
    int quads = (x1 - x0) >> 2;
    while (quads--) { w[0] = w0; w[1] = w1; w += 2; }

    for (int x = x0 + (((x1 - x0) >> 2) << 2); x < x1; x++) px[x] = c[x & 3];
}

/* A row of eight-bit colour, one entry per scanline. */
typedef struct { uint8_t r, g, b; } rgb8_t;

static inline void pv_fill_row_dither(uint16_t *px, const rgb8_t *c, int y)
{
    pv_fill_dither(px, 0, 640, c->r, c->g, c->b, y);
}

#endif
