/* Plasma, at native 640x480.
 *
 * The oldest effect in the book, and the first one written here because it is
 * the cheapest per-pixel kernel that still touches every pixel with a lookup:
 *
 *     px = pal[ (xt[x] + s3[x + yo] + rt) & 255 ]
 *
 * xt[] is the sum of every x-only term, rebuilt per frame on core 0 so the
 * line loop pays one load for all of them. s3 indexed by (x + yo) is the one
 * term that couples x and y, which is what makes it look like plasma rather
 * than like curtains. rt collects the y-only terms.
 *
 * MEASURED, first version: 7,200 cycles a line, against an estimate of 4,500.
 * The estimate had forgotten the loop and the mask. Unrolling by eight and
 * writing pixel pairs as words brought it to 4,308.
 *
 * ------------------------------------------------------------------ colour --
 *
 * Two things were wrong with the first palette and they had one cause. It
 * swept all three channels through a full sine at full amplitude with the
 * phases a third of a cycle apart, which is a hue wheel: every colour in the
 * gamut, all at maximum chroma, all of the time.
 *
 * This one moves LUMINANCE through the full range and lets chroma vary by
 * about a sixth of it, around a hue that drifts slowly. The structure is
 * identical -- the same sines, the same lookup, the same cost -- but the
 * picture is now made of light and shade with a colour cast rather than of
 * colour.
 *
 * FOUR palettes, not one. A smooth ramp quantised to the DAC's five bits bands
 * badly; the four copies carry the four x-phases of an ordered dither, and the
 * row rotates which one starts it so the pattern runs diagonally (dither.h).
 * Choosing between them is a pointer, so the dithering costs nothing.
 */

#include "beam.h"
#include "arena.h"
#include "rgb565.h"
#include "tables.h"
#include "dither.h"

#include <string.h>

typedef struct {
    uint8_t  rowterm[PV_H];      /* y-only terms, one byte per row */
    uint16_t yo[PV_H];           /* per-row offset into s3          */
    uint16_t pal[4][256];        /* one per dither phase            */
} plasma_p_t;

static plasma_p_t P[2];
static uint8_t *s_xt;            /* 640 bytes in the arena, per parity */

static void plasma_enter(void)
{
    s_xt = (uint8_t *)ARENA(ARENA_SMALL_OFF + SMALL_PLASMA_XT);
    memset(s_xt, 0, 2 * PV_W);
}

static void build_palette(uint16_t pal[4][256], uint32_t f, int fade)
{
    const int t = (int)(f * 3);
    for (int i = 0; i < 256; i++) {
        const int a = (i * 4 + t) & 1023;
        /* luminance carries the shape, with a floor so the darks are dark and
         * the brights stop short of white */
        const int lum = 96 + (pv_sin16((uint32_t)a) * 76 >> 15);          /* 20..172 */
        /* chroma is small and its phases sit close together, so the whole
         * frame occupies one part of the wheel and drifts through it slowly */
        const int c1 = pv_sin16((uint32_t)((a + t / 3) & 1023)) * 34 >> 15;
        const int c2 = pv_sin16((uint32_t)((a + 210 + t / 3) & 1023)) * 30 >> 15;
        const int c3 = pv_sin16((uint32_t)((a + 420 + t / 3) & 1023)) * 38 >> 15;

        int r = lum + c1 + 14, g = lum + c2, b = lum + c3 + 22;
        r = (r * fade) >> 8; g = (g * fade) >> 8; b = (b * fade) >> 8;
        if (r < 0) r = 0;
        if (g < 0) g = 0;
        if (b < 0) b = 0;
        for (int p = 0; p < 4; p++) pal[p][i] = pv_pack_dither(r, g, b, p, 0);
    }
}

static void plasma_frame(uint32_t f, uint32_t local)
{
    plasma_p_t *p = &P[f & 1];
    uint8_t *xt = s_xt + (f & 1) * PV_W;

    const int fade = local < 120 ? (int)(local * 255 / 120) : 255;
    build_palette(p->pal, f, fade);

    const int a = (int)(f * 5), b = (int)(f * 3), c = (int)(f * 2), d = (int)(f * 7);
    for (int x = 0; x < PV_W; x++) {
        const int v = pv_sin8((x * 2 + a) & 1023) + pv_sin8((x * 3 + b * 2) & 1023) / 2;
        xt[x] = (uint8_t)(v * 2 / 3 + 128);
    }
    for (int y = 0; y < PV_H; y++) {
        const int v = pv_sin8((y * 3 + c) & 1023) + pv_sin8((y * 5 - d) & 1023) / 2;
        p->rowterm[y] = (uint8_t)(v * 2 / 3 + 128);
        p->yo[y] = (uint16_t)((y * 2 + (int)(pv_sin8((y + f * 2) & 1023)) + f * 4) & 1023);
    }
}

static void PV_HOT(plasma_line)(uint32_t f, uint16_t *px, int y)
{
    const plasma_p_t *p = &P[f & 1];
    const uint8_t *xt = s_xt + (f & 1) * PV_W;
    const uint8_t *s3 = pv_usin8_tab + p->yo[y];       /* tab is 2048 long: no mask */
    const int rt = p->rowterm[y];

    const uint16_t *pa = p->pal[(y + 0) & 3], *pb = p->pal[(y + 1) & 3];
    const uint16_t *pc = p->pal[(y + 2) & 3], *pd = p->pal[(y + 3) & 3];

    uint32_t *w = (uint32_t *)px;

    if (!g_lod) {
        for (int x = 0; x < PV_W; x += 8) {
            const uint32_t c0 = pa[(xt[x    ] + s3[x    ] + rt) & 255];
            const uint32_t c1 = pb[(xt[x + 1] + s3[x + 1] + rt) & 255];
            const uint32_t c2 = pc[(xt[x + 2] + s3[x + 2] + rt) & 255];
            const uint32_t c3 = pd[(xt[x + 3] + s3[x + 3] + rt) & 255];
            const uint32_t c4 = pa[(xt[x + 4] + s3[x + 4] + rt) & 255];
            const uint32_t c5 = pb[(xt[x + 5] + s3[x + 5] + rt) & 255];
            const uint32_t c6 = pc[(xt[x + 6] + s3[x + 6] + rt) & 255];
            const uint32_t c7 = pd[(xt[x + 7] + s3[x + 7] + rt) & 255];
            w[(x >> 1)    ] = c0 | (c1 << 16);
            w[(x >> 1) + 1] = c2 | (c3 << 16);
            w[(x >> 1) + 2] = c4 | (c5 << 16);
            w[(x >> 1) + 3] = c6 | (c7 << 16);
        }
    } else {
        for (int x = 0; x < PV_W; x += 8) {
            const uint32_t c0 = pa[(xt[x    ] + s3[x    ] + rt) & 255];
            const uint32_t c1 = pc[(xt[x + 2] + s3[x + 2] + rt) & 255];
            const uint32_t c2 = pa[(xt[x + 4] + s3[x + 4] + rt) & 255];
            const uint32_t c3 = pc[(xt[x + 6] + s3[x + 6] + rt) & 255];
            w[(x >> 1)    ] = c0 | (c0 << 16);
            w[(x >> 1) + 1] = c1 | (c1 << 16);
            w[(x >> 1) + 2] = c2 | (c2 << 16);
            w[(x >> 1) + 3] = c3 | (c3 << 16);
        }
    }
}

const scene_t fx_plasma = { "plasma", plasma_enter, plasma_frame, plasma_line, NULL, NULL };
