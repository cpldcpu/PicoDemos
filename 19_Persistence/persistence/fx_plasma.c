/* Plasma, at native 640x480.
 *
 * The oldest effect in the book, and the first one here because it is the
 * cheapest per-pixel kernel that still touches every pixel with a lookup:
 *
 *     px = pal[ xt[x] + s3[x + yo] + rt ]
 *
 * xt[] is the sum of every x-only term, rebuilt per frame on core 0 so the
 * line loop pays one load for all of them. s3 indexed by (x + yo) is the one
 * term that couples x and y, which is what makes it look like plasma rather
 * than like curtains. rt collects the y-only terms.
 *
 * MEASURED, first version: 7,200 cycles a line, against an estimate of 4,500.
 * The estimate had forgotten the loop and the mask. This version removes both:
 * the palette is stored four times over (1,024 entries) so the byte sum never
 * needs masking, the row term is folded into the palette pointer, the loop is
 * unrolled by eight and writes pixel pairs as words. Two byte loads, an add,
 * a halfword load, and half a word store per pixel.
 *
 * At g_lod 1 it renders 320 pixels and writes pairs: half the cost, and the
 * picture is still a plasma.
 */

#include "beam.h"
#include "arena.h"
#include "rgb565.h"
#include "tables.h"

#include <string.h>

typedef struct {
    uint8_t  rowterm[PV_H];      /* y-only terms, one byte per row      */
    uint16_t yo[PV_H];           /* per-row offset into s3               */
    uint16_t pal[1024];          /* the palette x4, animated per frame   */
} plasma_p_t;

static plasma_p_t P[2];
static uint8_t *s_xt;            /* 640 bytes in the arena, per parity   */

static void plasma_enter(void)
{
    s_xt = (uint8_t *)ARENA(ARENA_SMALL_OFF + SMALL_PLASMA_XT);
    memset(s_xt, 0, 2 * PV_W);
}

/* A smooth, saturated palette: three phase-shifted sines through a warm and a
 * cold anchor, so the picture reads as light rather than as a hue wheel. */
static void build_palette(uint16_t *pal, uint32_t f, int fade)
{
    const int t = (int)(f * 3);
    for (int i = 0; i < 256; i++) {
        int r = 128 + (pv_sin8((i * 4 + t) & 1023));
        int g = 128 + (pv_sin8((i * 4 + 341 + t / 2) & 1023));
        int b = 128 + (pv_sin8((i * 4 + 682 + t / 3) & 1023));
        /* crush the lows a little so there is real dark in it */
        r = (r * r) >> 8; g = (g * g) >> 8; b = (b * b) >> 8;
        r = (r * fade) >> 8; g = (g * fade) >> 8; b = (b * fade) >> 8;
        uint16_t c = rgb565_pack(r, g, b);
        pal[i] = pal[i + 256] = pal[i + 512] = pal[i + 768] = c;
    }
}

static void plasma_frame(uint32_t f, uint32_t local)
{
    plasma_p_t *p = &P[f & 1];
    uint8_t *xt = s_xt + (f & 1) * PV_W;

    /* fade in over the first 2 s of its cue */
    int fade = local < 120 ? (int)(local * 255 / 120) : 255;
    build_palette(p->pal, f, fade);

    const int a = (int)(f * 5), b = (int)(f * 3), c = (int)(f * 2), d = (int)(f * 7);
    for (int x = 0; x < PV_W; x++) {
        int v = pv_sin8((x * 2 + a) & 1023) + pv_sin8((x * 3 + b * 2) & 1023) / 2;   /* -190..190 */
        xt[x] = (uint8_t)(v * 2 / 3 + 128);                                          /* 1..255   */
    }
    for (int y = 0; y < PV_H; y++) {
        int v = pv_sin8((y * 3 + c) & 1023) + pv_sin8((y * 5 - d) & 1023) / 2;
        p->rowterm[y] = (uint8_t)(v * 2 / 3 + 128);
        /* the diagonal coupling term drifts: rows slide across s3 */
        p->yo[y] = (uint16_t)((y * 2 + (int)(pv_sin8((y + f * 2) & 1023)) + f * 4) & 1023);
    }
}

static void PV_HOT(plasma_line)(uint32_t f, uint16_t *px, int y)
{
    const plasma_p_t *p = &P[f & 1];
    const uint8_t  *xt  = s_xt + (f & 1) * PV_W;
    const uint8_t  *s3  = pv_usin8_tab + p->yo[y];     /* tab is 2048 long: no mask */
    const uint16_t *pal = p->pal + p->rowterm[y];      /* row term folded in; max index 255+255+255 < 1024 */
    uint32_t *w = (uint32_t *)px;

    if (!g_lod) {
        for (int x = 0; x < PV_W; x += 8) {
            uint32_t c0 = pal[xt[x    ] + s3[x    ]];
            uint32_t c1 = pal[xt[x + 1] + s3[x + 1]];
            uint32_t c2 = pal[xt[x + 2] + s3[x + 2]];
            uint32_t c3 = pal[xt[x + 3] + s3[x + 3]];
            uint32_t c4 = pal[xt[x + 4] + s3[x + 4]];
            uint32_t c5 = pal[xt[x + 5] + s3[x + 5]];
            uint32_t c6 = pal[xt[x + 6] + s3[x + 6]];
            uint32_t c7 = pal[xt[x + 7] + s3[x + 7]];
            w[(x >> 1)    ] = c0 | (c1 << 16);
            w[(x >> 1) + 1] = c2 | (c3 << 16);
            w[(x >> 1) + 2] = c4 | (c5 << 16);
            w[(x >> 1) + 3] = c6 | (c7 << 16);
        }
    } else {
        for (int x = 0; x < PV_W; x += 8) {
            uint32_t c0 = pal[xt[x    ] + s3[x    ]];
            uint32_t c1 = pal[xt[x + 2] + s3[x + 2]];
            uint32_t c2 = pal[xt[x + 4] + s3[x + 4]];
            uint32_t c3 = pal[xt[x + 6] + s3[x + 6]];
            w[(x >> 1)    ] = c0 | (c0 << 16);
            w[(x >> 1) + 1] = c1 | (c1 << 16);
            w[(x >> 1) + 2] = c2 | (c2 << 16);
            w[(x >> 1) + 3] = c3 | (c3 << 16);
        }
    }
}

const scene_t fx_plasma = { "plasma", plasma_enter, plasma_frame, plasma_line, NULL, NULL };
