/* SUSTAIN — mip chains for the material textures.
 *
 * WHY THIS REPLACED THE CHEAP VERSION.
 *
 * The first attempt at level-of-detail faded a sample toward its texture's
 * MEAN COLOUR once the footprint passed one texel per pixel. That killed the
 * aliasing, but it also destroyed the material: fading to a single colour
 * throws away the mid-frequencies along with the high ones, so canyon walls
 * that should read as banded rock went flat and featureless. Azure spotted it
 * immediately — "you removed most of the textures?"
 *
 * A mip chain removes only what is actually aliasing. Each level is a
 * box-filtered half-size copy, so sampling the level whose texel size matches
 * the screen footprint preserves every frequency the screen can still resolve
 * and discards exactly those it cannot.
 *
 * Levels are blended (trilinear), not selected. A hard level switch would put
 * a visible band across the terrain where the LOD changes — and that band
 * MOVES with the camera, which is precisely the kind of travelling edge this
 * demo may not have.
 *
 * Cost: levels 1..3 of four 128x128 textures = 4 x 10.5 KB = 42 KB, which fits
 * inside one 64 KB field scratch slot (field_scratch.h). Level 0 stays in
 * flash and is never copied.
 */

#include "mip.h"
#include "rgb565.h"
#include "hot.h"

#include <math.h>
#include "fastmath.h"

/* Level 1 is 64x64, level 2 is 32x32, level 3 is 16x16. */
/* One contiguous chain per texture: 64x64 + 32x32 + 16x16 = 5376 entries.
 * This originally carried a spurious [MIP_LEVELS] dimension, tripling it to
 * 129 KB — which is most of why the first firmware link overflowed RAM. The
 * levels live end-to-end inside one chain, they are not separate arrays. */
static uint16_t g_store[MIP_COUNT][64 * 64 + 32 * 32 + 16 * 16];
static int      g_ready = 0;

static mip_t g_mip[MIP_COUNT];

/* Box-filter a level down by two. RGB565 channels are unpacked, averaged and
 * repacked — averaging the packed words directly would bleed channels into
 * each other. */
static void halve(const uint16_t *src, int sw, int sh, uint16_t *dst)
{
    const int dw = sw / 2, dh = sh / 2;
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            const uint16_t a = src[(y * 2)     * sw + x * 2];
            const uint16_t b = src[(y * 2)     * sw + x * 2 + 1];
            const uint16_t c = src[(y * 2 + 1) * sw + x * 2];
            const uint16_t d = src[(y * 2 + 1) * sw + x * 2 + 1];
            const int r = (rgb565_r8(a) + rgb565_r8(b) + rgb565_r8(c) + rgb565_r8(d)) >> 2;
            const int g = (rgb565_g8(a) + rgb565_g8(b) + rgb565_g8(c) + rgb565_g8(d)) >> 2;
            const int bl= (rgb565_b8(a) + rgb565_b8(b) + rgb565_b8(c) + rgb565_b8(d)) >> 2;
            dst[y * dw + x] = rgb565_pack(r, g, bl);
        }
    }
}

void mip_build(int slot, const uint8_t *raw, int w)
{
    if (slot < 0 || slot >= MIP_COUNT) return;
    mip_t *m = &g_mip[slot];
    m->l0 = (const uint16_t *)raw;
    m->w0 = w;

    uint16_t *p = g_store[slot];
    int sw = w;
    const uint16_t *src = m->l0;

    for (int lv = 0; lv < MIP_LEVELS; lv++) {
        const int dw = sw / 2;
        halve(src, sw, sw, p);
        m->l[lv] = p;
        m->w[lv] = dw;
        src = p;
        p  += dw * dw;
        sw  = dw;
    }
}

void mip_build_all(const uint8_t *surface_cold, const uint8_t *surface_hot,
                   const uint8_t *wall_cold, const uint8_t *wall_hot, int w)
{
    if (g_ready) return;
    mip_build(MIP_SURFACE_COLD, surface_cold, w);
    mip_build(MIP_SURFACE_HOT,  surface_hot,  w);
    mip_build(MIP_WALL_COLD,    wall_cold,    w);
    mip_build(MIP_WALL_HOT,     wall_hot,     w);
    g_ready = 1;
}

/* Bilinear within a level. */
static void SUSTAIN_HOT(sample_level)(const uint16_t *t, int w, float u, float v,
                         int *r, int *g, int *b)
{
    const int mask = w - 1;
    const float uf = ffloor(u), vf = ffloor(v);
    const float fu = u - uf, fv = v - vf;
    const int u0 = (int)uf & mask, v0 = (int)vf & mask;
    const int u1 = (u0 + 1) & mask, v1 = (v0 + 1) & mask;

    const uint16_t p00 = t[v0 * w + u0], p10 = t[v0 * w + u1];
    const uint16_t p01 = t[v1 * w + u0], p11 = t[v1 * w + u1];

#define CH(F)                                                              \
    ({ float a = (float)F(p00), b_ = (float)F(p10),                         \
              c = (float)F(p01), d = (float)F(p11);                         \
       float tp = a + (b_ - a) * fu, bt = c + (d - c) * fu;                 \
       (int)(tp + (bt - tp) * fv); })
    *r = CH(rgb565_r8);
    *g = CH(rgb565_g8);
    *b = CH(rgb565_b8);
#undef CH
}

void SUSTAIN_HOT(mip_sample)(int slot, float u_tex, float v_tex, float texels_per_pixel,
                int *r, int *g, int *b)
{
    const mip_t *m = &g_mip[slot];

    /* Level whose texels are about one screen pixel across. */
    float lod = (texels_per_pixel > 1.0f) ? flog2(texels_per_pixel) : 0.0f;
    if (lod > (float)MIP_LEVELS) lod = (float)MIP_LEVELS;

    const int   lv = (int)lod;
    const float fr = lod - (float)lv;

    /* Texture coordinates are in level-0 texels; each level halves them. */
    const float s0 = 1.0f / (float)(1 << lv);

    int ar, ag, ab;
    if (lv == 0) sample_level(m->l0, m->w0, u_tex, v_tex, &ar, &ag, &ab);
    else         sample_level(m->l[lv - 1], m->w[lv - 1],
                              u_tex * s0, v_tex * s0, &ar, &ag, &ab);

    if (fr <= 0.01f || lv >= MIP_LEVELS) { *r = ar; *g = ag; *b = ab; return; }

    /* Blend into the next level down — trilinear, so no travelling LOD band. */
    const float s1 = s0 * 0.5f;
    int br, bg, bb;
    sample_level(m->l[lv], m->w[lv], u_tex * s1, v_tex * s1, &br, &bg, &bb);

    *r = ar + (int)((br - ar) * fr);
    *g = ag + (int)((bg - ag) * fr);
    *b = ab + (int)((bb - ab) * fr);
}
