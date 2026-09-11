/* sky.c -- Phase's painted plates, expanded through a per-frame tinted
 * palette. PLANNING §9 and Phase's critique:
 *
 *   "HELION DMA copies an already expanded RGB16 sky. Indexed flash plus a
 *    changing palette needs an explicit expansion path and measured cost.
 *    Two unrelated indexed palettes cannot morph moon geometry into sun
 *    geometry."
 *
 * So there is no DMA and no blend between plates. The night plate serves
 * bars 0-103 with its palette tinted from night, through the first blue at
 * 87, to the blue hour at 96-103; the dawn plate arrives on the cut at 104
 * and its cool tint warms out to nothing by 112. The moon stays and goes
 * paler because the tint is indexed by the source pixel's own luminance:
 * the ramp's top is where the moon lands, and the ramp's top is never far
 * from white.
 *
 * The dither is applied here, at the point where the tinted eight-bit
 * colour becomes RGB555 -- Phase's other rule. Eight palettes, one per value
 * of the 4x4 pattern's three dropped bits, mean the expansion loop is a
 * double indexed load and nothing else: no per-pixel arithmetic at all.
 * The palettes are rebuilt only when the tint changes, which is once a bar,
 * and the rebuild is a pure function of (plate, light level), so a seek
 * lands on the same page as a play-through. -- Overscan
 */
#include "render.h"
#include "assets.h"

static uint16_t g_pal[8][256];
static int g_key = -1;

int sky_is_dawn(void) { return F.bar >= 104u; }

/* A 32-entry target ramp, indexed by a stretched luminance so the plate's
 * sky body -- which lives in the bottom seventh of the eight-bit range --
 * gets the whole ramp rather than four entries of it. */
static void ramp_of(uint32_t out[32], const uint8_t a[3], const uint8_t b[3], const uint8_t c[3], int knee)
{
    for (int i = 0; i < 32; i++) {
        int r, g, bl;
        if (i <= knee) {
            r = a[0] + (b[0] - a[0]) * i / knee;
            g = a[1] + (b[1] - a[1]) * i / knee;
            bl = a[2] + (b[2] - a[2]) * i / knee;
        } else {
            int n = i - knee, d = 31 - knee;
            r = b[0] + (c[0] - b[0]) * n / d;
            g = b[1] + (c[1] - b[1]) * n / d;
            bl = b[2] + (c[2] - b[2]) * n / d;
        }
        out[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)bl;  /* packed 8-bit */
    }
}

void sky_palette(void)
{
    const int dawn = sky_is_dawn();
    const int key = (dawn << 12) | F.lightlv;
    if (key == g_key) return;
    g_key = key;

    const uint8_t *src = dawn ? sky_dawn_pal : sky_night_pal;

    /* How much of the target ramp to mix in, and which ramp. */
    uint32_t ramp[32];
    int mix = 0;                                          /* 0..32          */
    if (!dawn) {
        /* night -> first blue -> blue hour; song.c's light_of() is 0 to bar
         * 86, 4..36 for 87..95 and 40..124 for 96..103. */
        const uint8_t lo[3] = { 0x10, 0x18, 0x20 }, mid[3] = { 0x60, 0x80, 0xB0 }, hi[3] = { 0xE8, 0xEE, 0xF8 };
        ramp_of(ramp, lo, mid, hi, 18);
        mix = F.lightlv * 27 / 124;                       /* 0 .. 27 of 32  */
    } else {
        /* the dawn plate arrives cool and warms to itself by 112 */
        const uint8_t lo[3] = { 0x18, 0x28, 0x38 }, mid[3] = { 0x40, 0x58, 0x7C }, hi[3] = { 0xD0, 0xDC, 0xEC };
        ramp_of(ramp, lo, mid, hi, 18);
        mix = (255 - F.lightlv) * 17 / 125;               /* 17 at 104, 0 at 112 */
        if (mix < 0) mix = 0;
    }

    for (int i = 0; i < 256; i++) {
        int r = src[i * 3], g = src[i * 3 + 1], b = src[i * 3 + 2];
        if (mix) {
            const int L = (r * 77 + g * 150 + b * 29) >> 8;
            int u = L * 31 / 70; if (u > 31) u = 31;
            const uint32_t t = ramp[u];
            r = r + (((int)((t >> 16) & 255) - r) * mix >> 5);
            g = g + (((int)((t >> 8) & 255) - g) * mix >> 5);
            b = b + (((int)(t & 255) - b) * mix >> 5);
        }
        for (int d = 0; d < 8; d++) g_pal[d][i] = rgb(r + d, g + d, b + d);
    }
}

void HOT(sky_expand)(int y0, int y1, int plate_y0)
{
    if (y0 < 0) y0 = 0;
    if (y1 > HEIGHT) y1 = HEIGHT;
    const uint8_t *plate = sky_is_dawn() ? sky_dawn_idx : sky_night_idx;
    for (int y = y0; y < y1; y++) {
        int py = plate_y0 + (y - y0);
        if (py < 0) py = 0; if (py > SKY_H - 1) py = SKY_H - 1;
        const uint8_t *s = plate + (unsigned)py * SKY_W;
        const uint8_t *bay = bayer4 + ((y & 3) << 2);
        const uint16_t *p0 = g_pal[bay[0] >> 1], *p1 = g_pal[bay[1] >> 1];
        const uint16_t *p2 = g_pal[bay[2] >> 1], *p3 = g_pal[bay[3] >> 1];
        uint16_t *o = g_fb + (unsigned)y * WIDTH;
        for (int x = 0; x < WIDTH; x += 4) {
            o[x] = p0[s[x]]; o[x + 1] = p1[s[x + 1]];
            o[x + 2] = p2[s[x + 2]]; o[x + 3] = p3[s[x + 3]];
        }
        g_stats.spans++;
    }
}
