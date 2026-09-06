/* The affine plane: rotozoom and Mode-7 are the same kernel.
 *
 * Every row samples the texture along a straight line, (u0, v0) stepping by
 * (du, dv) per pixel, and the RP2350 SIO interpolator does the address
 * arithmetic: one POP per pixel returns the texel offset (with the 256x256
 * wrap folded into the mask) and advances both accumulators. On the host the
 * identical calls go through QUICKSILVER's bit-exact emulator.
 *
 * A rotozoom is the case where every row has the same (du, dv). A Mode-7
 * floor is the case where the row's scale is camH * F / (y - horizon). One
 * formula covers both: push the horizon far above the screen and the scale
 * is constant across rows, which IS a rotozoom. So going from the rotozoom
 * to the floor is a sweep of the horizon row, and it is continuous.
 *
 * Rows above the horizon are sky: one colour per row from a gradient table.
 * Fog is a per-row stipple level (0..3 of every 4 pixels become sky) because
 * a per-pixel lerp is ~6 cycles and the whole line has ~9 per pixel.
 *
 * MEASURED on the device: 3,650 cycles a line with no fog, 4,400 with. The
 * estimate was 2,600 and had ignored the fog stipple and the store.
 */

#include "beam.h"
#include "arena.h"
#include "rgb565.h"
#include "tables.h"
#include "interp_compat.h"
#include "affine.h"
#include "dither.h"

#include <string.h>
#include <math.h>

#define TEX_W 256
#define TEX_H 256

typedef struct {
    int32_t  u0[PV_H], v0[PV_H], du[PV_H], dv[PV_H];   /* 16.16 */
    uint16_t sky[PV_H];        /* packed, for the fog stipple                */
    rgb8_t   sky8[PV_H];       /* eight-bit, so full-sky rows can be dithered */
    uint8_t  fog[PV_H];        /* 0..3 stipple, 4 = all sky                   */
} affine_p_t;

static affine_p_t P[2];
static uint16_t  *s_tex;

/* ------------------------------------------------------------ texture ---- */

static uint32_t s_rng = 0x2545F491u;
static inline uint32_t rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }

void affine_texture_generate(int style)
{
    uint16_t *t = (uint16_t *)ARENA(ARENA_TEXTURE_OFF);
    s_tex = t;
    s_rng = 0x2545F491u + (uint32_t)style * 7919u;

    /* value noise, 32x32 lattice, bilinear */
    static uint8_t lat[32 * 32];
    for (int i = 0; i < 32 * 32; i++) lat[i] = (uint8_t)(rnd() >> 24);

    for (int y = 0; y < TEX_H; y++) {
        for (int x = 0; x < TEX_W; x++) {
            const int lx = x >> 3, ly = y >> 3, fx = x & 7, fy = y & 7;
            const int n00 = lat[(ly & 31) * 32 + (lx & 31)],       n10 = lat[(ly & 31) * 32 + ((lx + 1) & 31)];
            const int n01 = lat[((ly + 1) & 31) * 32 + (lx & 31)], n11 = lat[((ly + 1) & 31) * 32 + ((lx + 1) & 31)];
            const int n0 = n00 + ((n10 - n00) * fx >> 3), n1 = n01 + ((n11 - n01) * fx >> 3);
            const int n = n0 + ((n1 - n0) * fy >> 3);                     /* 0..255 */

            int r, g, b;
            if (style == AFFINE_TEX_FLOOR) {
                /* 64-px cells, a lit rim, thin traces on a 16-px grid */
                const int cx = x & 63, cy = y & 63;
                const int rim = (cx < 2 || cy < 2 || cx >= 62 || cy >= 62);
                const int trace = ((x & 15) == 7 && (cy & 8)) || ((y & 15) == 7 && (cx & 8));
                const int base = 24 + (n >> 3);
                r = base / 2; g = base * 2 / 3; b = base;
                if (rim)   { r = 90; g = 140; b = 180; }
                if (trace) { r = 40 + (n >> 2); g = 200; b = 230; }
                const int h = ((x >> 6) * 73 + (y >> 6) * 151) & 7;
                if (h == 3 && !rim) { r += 80; g += 40; b += 10; }
            } else {
                /* AFFINE_TEX_GRID: a wide dark grid for the finale, warmer */
                const int cx = x & 127, cy = y & 127;
                const int rim = (cx < 3 || cy < 3);
                const int base = 18 + (n >> 4);
                r = base; g = base / 2; b = base / 3;
                if (rim) { r = 220; g = 120; b = 40; }
                if (((x ^ y) & 127) < 2) { r = 255; g = 200; b = 120; }
            }
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            t[y * TEX_W + x] = rgb565_pack(r, g, b);
        }
    }
}

/* ------------------------------------------------------------- camera ---- */

void affine_rows(const affine_cam_t *cam, uint32_t parity)
{
    affine_p_t *p = &P[parity & 1];
    const float ca = cosf(cam->angle), sa = sinf(cam->angle);
    const float F = 320.0f;
    const int H = cam->horizon;

    for (int y = 0; y < PV_H; y++) {
        const int dy = y - H;
        if (dy <= 0) { p->fog[y] = 4; p->u0[y] = p->v0[y] = p->du[y] = p->dv[y] = 0; continue; }

        float z = cam->height * F / (float)dy;          /* depth of this row, texels */
        if (z > 1.0e6f) z = 1.0e6f;
        const float s = z / F;                           /* texels per pixel          */
        /* world(x) = cam + z*fwd + (x-320)*s*right;  right = (ca, sa), fwd = (-sa, ca) */
        const float ux = cam->x + z * (-sa) - 320.0f * s * ca;
        const float vy = cam->y + z * ( ca) - 320.0f * s * sa;
        p->u0[y] = (int32_t)(ux * 65536.0f);
        p->v0[y] = (int32_t)(vy * 65536.0f);
        p->du[y] = (int32_t)(s * ca * 65536.0f);
        p->dv[y] = (int32_t)(s * sa * 65536.0f);

        int fog = 0;
        if (cam->fog_near > 0.0f) {
            const float q = (z - cam->fog_near) / (cam->fog_far - cam->fog_near);
            if (q > 0.0f) fog = q >= 1.0f ? 4 : (int)(q * 4.0f);
        }
        p->fog[y] = (uint8_t)fog;
    }
}

void affine_sky(const rgb8_t *sky, uint32_t parity)
{
    affine_p_t *p = &P[parity & 1];
    memcpy(p->sky8, sky, sizeof p->sky8);
    for (int y = 0; y < PV_H; y++) p->sky[y] = rgb565_pack(sky[y].r, sky[y].g, sky[y].b);
}

/* Standard dusk gradient: dark above, lit at the horizon. Kept at eight bits
 * per channel because a sky is the single worst thing to quantise -- a smooth
 * vertical ramp across 480 rows lands as a staircase on a five-bit DAC. */
void affine_sky_dusk(rgb8_t *sky, int horizon, int warm)
{
    for (int y = 0; y < PV_H; y++) {
        int d = horizon - y; if (d < 0) d = 0; if (d > 320) d = 320;
        const int e = 320 - d;
        if (warm) { sky[y].r = (uint8_t)(24 + e * 5 / 12); sky[y].g = (uint8_t)(14 + e / 5); sky[y].b = (uint8_t)(28 + e / 7); }
        else      { sky[y].r = (uint8_t)(10 + e / 6);      sky[y].g = (uint8_t)(13 + e / 5); sky[y].b = (uint8_t)(30 + e * 2 / 5); }
    }
}

/* ------------------------------------------------------------- kernel ---- */

void qs_texmap_setup_interp0(void) { qs_texmap_setup(interp0, 1, 8, 8); }

void PV_HOT(affine_line_p)(uint32_t parity, uint16_t *px, int y)
{
    const affine_p_t *p = &P[parity & 1];
    const int fog = p->fog[y];
    const uint32_t skyc = p->sky[y];

    if (fog >= 4) { pv_fill_row_dither(px, &p->sky8[y], y); return; }

    interp_set_accumulator(interp0, 0, (uint32_t)p->u0[y]);
    interp_set_accumulator(interp0, 1, (uint32_t)p->v0[y]);
    qs_texmap_step(interp0, (uint32_t)p->du[y], (uint32_t)p->dv[y]);
    const uint8_t *tex = (const uint8_t *)s_tex;
    uint32_t *w = (uint32_t *)px;

    if (g_lod) {
        /* half resolution: two pops per pair, keep one texel */
        for (int x = 0; x < PV_W; x += 4) {
            uint32_t c0 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            (void)interp_pop_full_result(interp0);
            uint32_t c1 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            (void)interp_pop_full_result(interp0);
            if (fog >= 2) c1 = skyc;
            w[(x >> 1)]     = c0 | (c0 << 16);
            w[(x >> 1) + 1] = c1 | (c1 << 16);
        }
        return;
    }

    if (fog == 0) {
        for (int x = 0; x < PV_W; x += 4) {
            const uint32_t c0 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            const uint32_t c1 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            const uint32_t c2 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            const uint32_t c3 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            w[(x >> 1)]     = c0 | (c1 << 16);
            w[(x >> 1) + 1] = c2 | (c3 << 16);
        }
    } else {
        /* One of the four slots is replaced by sky per fog level, and which
         * one alternates by row so the thinning reads as a dither rather than
         * as vertical stripes. */
        const int odd = y & 1;
        for (int x = 0; x < PV_W; x += 4) {
            uint32_t c0 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            uint32_t c1 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            uint32_t c2 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            uint32_t c3 = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
            if (odd) { c0 = skyc; if (fog >= 2) c2 = skyc; if (fog >= 3) c1 = skyc; }
            else     { c2 = skyc; if (fog >= 2) c0 = skyc; if (fog >= 3) c3 = skyc; }
            w[(x >> 1)]     = c0 | (c1 << 16);
            w[(x >> 1) + 1] = c2 | (c3 << 16);
        }
    }
}
