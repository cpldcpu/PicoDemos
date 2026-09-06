/* liquid.c — QUICKSILVER "Liquid Metal". The scene appears TWICE, in two
 * deliberately different moods (keyed off the scene start: plasma < 40 s, bump
 * >= 40 s — both before the 100 s split chrome/mode7 use):
 *
 *   FIRST (0:27) — a PLASMA riser: an intensity field is computed on a coarse
 *     grid and bilinearly upscaled to full 320x240 with the interpolator's BLEND
 *     unit (HW linear interp), mapped through a smooth COOL-SILVER ramp (matching
 *     the mode7 mercury plain it flows out of) and motion-blurred. This is the
 *     demo's showcase of interp0 BLEND mode (address-gen + CLAMP are shown
 *     elsewhere). Soft, flowing, accelerating.
 *
 *   SECOND (0:54) — a real 2D BUMP-MAPPED mercury surface lit by a moving light
 *     (the classic Amiga emboss look). A smooth mercury height map (asset
 *     conduit_bump) is copied to SRAM and sampled as ONE layer gently DEFORMED by
 *     a travelling-sine domain warp, so it flows like fluid. Per pixel we take the
 *     surface gradient (analytic, from 4 bilinear texels), perturb the to-light
 *     vector by it, and look up a radial light response (broad diffuse pool +
 *     sharp specular sheen) — bumps facing the light catch it, the pool sweeps as
 *     the light moves. No transcendentals / no BLEND in the hot loop, just SRAM.
 *
 * Builds identically on host (emulator) and RP2350 (raw SIO).
 */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "../scene_scratch.h"
#include "assets.h"
#include "qs_fx.h"

#include <math.h>

#define GW 41            /* coarse grid: (GW-1)=40 divides 320 -> 8px cells */
#define GH 31            /* (GH-1)=30 divides 240 -> 8px cells              */
#define CELL 8

/* --- bump-map (breakdown) tables -------------------------------------------
 * s_light is the radial light response indexed by (r2 >> LCURVE_SHIFT), where r2
 * is the squared screen distance from the perturbed light centre. 512 entries
 * over a shift of 10 cover r2 up to ~5.2e5 (a corner-to-corner reach), so the
 * whole surface is lit with a gradient toward the light. */
#define LCURVE_N      512
#define LCURVE_SHIFT  10
#define BMP_W         256              /* conduit_bump tile is 256x256          */
#define BMP_MASK      255

static uint8_t  s_grid[GW * GH];
static uint16_t s_chrome[256];         /* plasma metal+sheen gradient (build)   */
static uint16_t s_steel[256];          /* clean steel ramp (bump path)          */
static uint8_t  s_light[LCURVE_N];     /* radial moving-light response          */
static uint8_t *s_bump;                /* 256x256 mercury height, in SRAM       */

static void liquid_init(void)
{
    /* interp0 BLEND: lane1 result = base0 + (base1-base0)*alpha/256, alpha =
     * 8 LSBs of accum1. Configure once; we just feed base0/base1/accum1. */
    interp_config c0 = interp_default_config();
    interp_config_set_blend(&c0, true);
    interp_set_config(interp0, 0, &c0);
    interp_config c1 = interp_default_config();
    interp_set_config(interp0, 1, &c1);

    /* plasma palette: an IRIDESCENT oil-on-mercury spectrum — indigo, cyan,
     * silver-lilac, pink-gold, cream — so the field shimmers through metallic
     * sheen colours (sharing the mode7 plain's purple/pink family) instead of
     * flat grey. Built by lerping 5 control points; smooth, so no ringed edges.
     * The frame loop MIRROR-cycles this ramp (0..255..0) and scrolls it, which
     * is why the colours FLOW — a seamless loop with no wrap seam. */
    static const uint8_t cp[5][3] = {
        {  36,  44, 110 },     /* 0.00  deep indigo-blue  */
        {  60, 140, 170 },     /* 0.25  cyan-teal         */
        { 170, 160, 196 },     /* 0.50  silver-lilac      */
        { 226, 150, 172 },     /* 0.75  pink-gold         */
        { 255, 232, 206 },     /* 1.00  pale cream        */
    };
    for (int i = 0; i < 256; i++) {
        float s = (i / 255.0f) * 4.0f;       /* 0..4 across the 5 control points */
        int   seg = (int)s; if (seg > 3) seg = 3;
        float f = s - seg;
        int r = (int)(cp[seg][0] + (cp[seg + 1][0] - cp[seg][0]) * f);
        int g = (int)(cp[seg][1] + (cp[seg + 1][1] - cp[seg][1]) * f);
        int b = (int)(cp[seg][2] + (cp[seg + 1][2] - cp[seg][2]) * f);
        s_chrome[i] = rgb565_pack(r, g, b);
    }

    /* clean steel ramp for the bump surface: deep blue-steel in shadow -> bright
     * silver -> white sheen, MONOTONIC (the embossing comes from the bump, not
     * from gradient bands — sheen rings would fight the relief). */
    for (int i = 0; i < 256; i++) {
        float s = i / 255.0f;
        float k = powf(s, 0.85f);
        int r = (int)(20 + 218 * k);
        int g = (int)(28 + 212 * k);
        int b = (int)(46 + 198 * k);                      /* cool in the shadows */
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        s_steel[i] = rgb565_pack(r, g, b);
    }

    /* radial light response: a broad diffuse pool (SIG) the whole surface sits
     * in, plus a tight specular core (SC) that reads as a wet mercury sheen. */
    for (int i = 0; i < LCURVE_N; i++) {
        float r2 = (float)(i << LCURVE_SHIFT);
        float diff = expf(-r2 / (2.0f * 150.0f * 150.0f));
        float spec = expf(-r2 / (2.0f *  38.0f *  38.0f));
        int v = 16 + (int)(176.0f * diff) + (int)(96.0f * spec);
        if (v < 0)   v = 0;
        if (v > 255) v = 255;
        s_light[i] = (uint8_t)v;
    }

    /* Copy the mercury height map into SRAM — per-pixel sampling from XIP flash
     * is what broke 60 fps in mode7; from RAM the bump loop is cheap. Lives in
     * g_scratch (only one scene is active, and the build path doesn't use it). */
    s_bump = (uint8_t *)g_scratch.bg_cache;
#if defined(ASSET_CONDUIT_BUMP_W)
    const uint8_t *src = asset_conduit_bump_data;
    for (int i = 0; i < BMP_W * BMP_W; i++) s_bump[i] = src[i];
#else
    for (int i = 0; i < BMP_W * BMP_W; i++) s_bump[i] = 0;
#endif

    /* Make the tile genuinely seamless (offset-blend): the source isn't perfectly
     * tileable, so a plain wrap leaves a height STEP the light catches as a hard
     * seam line. Cross-fade a half-shifted copy with a triangular weight (1 at
     * the centre, 0 at the edges), once per axis — this pushes both edge-seams
     * into a smooth interior, so the wrap is continuous. On an organic mercury
     * field the blended overlap just reads as extra swirls. We only blend within
     * a narrow MARGIN of each edge (the interior keeps full amplitude, so the
     * bold rounded blobs survive). Done here so the hot loop stays a branch-free
     * `& 255` wrap (no mirror fold). */
    {
        const int MARGIN = 40;                                 /* blend band px   */
        uint8_t tmp[BMP_W];
        for (int y = 0; y < BMP_W; y++) {                      /* horizontal pass */
            uint8_t *rp = s_bump + (y << 8);
            for (int x = 0; x < BMP_W; x++) tmp[x] = rp[x];
            for (int x = 0; x < BMP_W; x++) {
                int ed = (x <= 128 ? x : 256 - x);             /* dist to seam    */
                int a = ed * 256 / MARGIN; if (a > 256) a = 256;
                rp[x] = (uint8_t)((tmp[x] * a + tmp[(x + 128) & 255] * (256 - a)) >> 8);
            }
        }
        for (int x = 0; x < BMP_W; x++) {                      /* vertical pass   */
            for (int y = 0; y < BMP_W; y++) tmp[y] = s_bump[(y << 8) | x];
            for (int y = 0; y < BMP_W; y++) {
                int ed = (y <= 128 ? y : 256 - y);
                int a = ed * 256 / MARGIN; if (a > 256) a = 256;
                s_bump[(y << 8) | x] =
                    (uint8_t)((tmp[y] * a + tmp[(y + 128) & 255] * (256 - a)) >> 8);
            }
        }
    }
}

static inline int hw_blend(int a, int b, int alpha)
{
    interp_set_base(interp0, 0, (uint32_t)a);
    interp_set_base(interp0, 1, (uint32_t)b);
    interp_set_accumulator(interp0, 1, (uint32_t)alpha);
    return (int)interp_peek_lane_result(interp0, 1);
}

/* Per-frame domain-warp tables. ONE height layer is gently DEFORMED instead of
 * scrolling a second one past it — that reads more like flowing fluid and is
 * cheaper (4 texels/pixel, not 8). The warp is SEPARABLE: a travelling-sine
 * horizontal ripple that depends only on the row (s_wu_*), and a vertical ripple
 * that depends only on the column (s_wv_*). Because the warp is constant along a
 * row/column, the bilinear sub-pixel weight is too — so the smooth analytic
 * gradient (from the same 4 texels, no extra reads) still holds, and the warp
 * being a continuous float keeps the motion sub-pixel smooth (no stutter). */
#define BMP_USTEP 384                  /* texels-per-pixel in 8.8 (=1.5x zoom)  */
/* per-row/col warp origins in 8.8 fixed point; values stay < ~26k over the 13 s
 * scene, so int16 is ample (the running u_fp/v_fp accumulators are int). */
static int16_t s_wu_fp[VGA_HIRES_H];   /* per-row u origin: drift + ripple      */
static int16_t s_wv_fp[VGA_HIRES_W];   /* per-col v ripple                       */

/* BREAKDOWN — bump-mapped mercury under a moving light. */
static void liquid_render_bump(float t, float prog)
{
    const uint8_t *H = s_bump;

    /* The flow is LIVELY and continuous from the first frame; the final third
     * just intensifies it (the old arc gated ALL motion to the end, which read
     * as the effect being frozen at the start). */
    float rise = prog < 0.55f ? 0.0f : (prog - 0.55f) / 0.45f;
    float amp  = 6.0f + 4.0f * rise;            /* ripple depth (texels)         */
    float wsp  = 1.05f + 0.7f * rise;           /* ripple travel speed           */

    /* a gentle base drift carries the surface; the travelling ripple deforms ONE
     * layer so it flows (no second layer). Tables are O(W+H) in 8.8 fixed point;
     * the hot loop is then pure lookups, and the texture is sampled at 1.5x zoom
     * so a single layer still has rich, intricate relief. */
    float su = 7.0f * t, sv = 5.0f * t;
    for (int y = 0; y < VGA_HIRES_H; y++)
        s_wu_fp[y] = (int16_t)((su + amp * sinf(y * 0.045f + t * wsp)) * 256.0f);
    for (int x = 0; x < VGA_HIRES_W; x++)
        s_wv_fp[x] = (int16_t)((sv + amp * sinf(x * 0.052f - t * (wsp * 0.9f))) * 256.0f);

    /* the light roams a compound (never-stalling) path across the screen */
    float ls = t * (0.62f + 0.5f * rise);
    int Lx = 160 + (int)(150.0f * sinf(ls)          + 34.0f * sinf(t * 0.27f));
    int Ly = 120 + (int)( 96.0f * sinf(ls * 1.23f + 1.1f) + 22.0f * cosf(t * 0.19f));

    const int KG = 5;                  /* emboss gain (depth of the relief)     */
    uint16_t *fb = vga_hires_back_buffer();

    for (int y = 0; y < VGA_HIRES_H; y++) {
        uint16_t *row = fb + y * VGA_HIRES_W;
        int u_fp = s_wu_fp[y];                  /* u runs across the row in 8.8  */
        int yv   = y * BMP_USTEP;               /* v grows down the column       */
        int ly   = Ly - y;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int v_fp = yv + s_wv_fp[x];
            int u0 = (u_fp >> 8) & BMP_MASK, u1 = (u0 + 1) & BMP_MASK, wu = u_fp & 255;
            int c0 = (v_fp >> 8) & BMP_MASK, c1 = (c0 + 1) & BMP_MASK, wv = v_fp & 255;
            int wun = 256 - wu, wvn = 256 - wv;
            u_fp += BMP_USTEP;
            const uint8_t *r0 = H + (c0 << 8), *r1 = H + (c1 << 8);
            int h00 = r0[u0], h10 = r0[u1], h01 = r1[u0], h11 = r1[u1];

            /* sub-pixel-smooth surface gradient, analytic from the 4 texels */
            int gx = ((h10 - h00) * wvn + (h11 - h01) * wv) >> 8;
            int gy = ((h01 - h00) * wun + (h11 - h10) * wu) >> 8;

            /* perturb the to-light vector by the surface slope: bright where the
             * slope points toward the light (flip the sign for recessed bumps). */
            int ox = (Lx - x) - gx * KG;
            int oy = ly        - gy * KG;
            int r2 = ox * ox + oy * oy;
            int idx = r2 >> LCURVE_SHIFT;
            if (idx >= LCURVE_N) idx = LCURVE_N - 1;

            uint16_t c = s_steel[s_light[idx]];
            int d = qs_dither(x, y);
            row[x] = rgb565_pack(rgb565_r8(c) + d, rgb565_g8(c) + d, rgb565_b8(c) + d);
        }
    }
}

static void liquid_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    float t = t_ms * 0.001f;

    float dur = (scene_cur_end_ms() - scene_cur_start_ms()) * 0.001f;
    if (dur < 1.0f) dur = 1.0f;
    float prog = t / dur; if (prog > 1.0f) prog = 1.0f;
    /* Two LIQUID slots, both before 100 s, so we split them here: the FIRST
     * (0:27) is the soft plasma riser, the SECOND (0:54) the bump-mapped mercury
     * showpiece. (CHROME/MODE7 split on 100 s; liquid can't, hence 40 s.) */
    int plasma = scene_cur_start_ms() < 40000u;

    if (!plasma) {                      /* bump-mapped mercury — the showpiece */
        liquid_render_bump(t, prog);
        return;
    }

    /* plasma riser (0:27): a soft-edged field flowing out of the mode7 plain,
     * accelerating, with its IRIDESCENT palette scrolling so the colours flow. */
    float sc = 0.80f;                  /* finer, busier field                    */
    float ts = 1.5f + 1.4f * prog;     /* already fast, accelerating             */
    float cx = 10.0f, cy = 8.0f;       /* radial centres, deliberately            */
    float cx2 = 25.0f, cy2 = 19.0f;    /*   asymmetric, to kill the symmetry      */
    int   poff = (int)(t * 78.0f);     /* palette scroll => flowing iridescence   */

    /* Summed travelling sines, but DE-LATTICED so it doesn't read as a repeating
     * grid: (1) a DOMAIN WARP bends the coordinates before sampling, so the rows
     * go organic; (2) the frequencies are NON-HARMONIC (irrational-ish ratios) so
     * the field's repeat period is enormous; (3) TWO offset radial centres break
     * the mirror symmetry. The 0.16 scale keeps it mostly off the 0/255 rails. */
    for (int gy = 0; gy < GH; gy++) {
        for (int gx = 0; gx < GW; gx++) {
            float x = gx * sc, y = gy * sc;
            float wx = x + 1.6f * sinf(y * 0.41f + t * 0.70f * ts);   /* domain warp */
            float wy = y + 1.6f * sinf(x * 0.37f - t * 0.55f * ts);
            float d1 = sqrtf((wx-cx )*(wx-cx ) + (wy-cy )*(wy-cy ));
            float d2 = sqrtf((wx-cx2)*(wx-cx2) + (wy-cy2)*(wy-cy2));
            float f = sinf(wx * 0.93f + t * 1.30f * ts)
                    + sinf(wy * 1.17f - t * 0.90f * ts)
                    + sinf((wx * 0.71f + wy * 0.59f) + t * 1.70f * ts)
                    + sinf(d1 * 1.40f - t * 2.10f * ts)
                    + 0.85f * sinf(d2 * 1.07f + t * 1.90f * ts);
            int v = (int)((f * 0.16f + 0.5f) * 255.0f);
            s_grid[gy * GW + gx] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
        }
    }

    uint16_t *fb = vga_hires_back_buffer();
    for (int y = 0; y < VGA_HIRES_H; y++) {
        int gy = y / CELL, fy = (y % CELL) * (256 / CELL);
        const uint8_t *r0 = &s_grid[gy * GW];
        const uint8_t *r1 = &s_grid[(gy + 1 < GH ? gy + 1 : gy) * GW];
        uint16_t *row = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int gx = x / CELL, fx = (x % CELL) * (256 / CELL);
            int top = hw_blend(r0[gx], r0[gx + 1], fx);     /* HW bilinear: H */
            int bot = hw_blend(r1[gx], r1[gx + 1], fx);
            int val = hw_blend(top, bot, fy);               /* HW bilinear: V */
            int idx = (val + poff) & 511;                   /* scroll the palette... */
            if (idx >= 256) idx = 511 - idx;                /* ...mirrored => seamless loop */
            uint16_t c = s_chrome[idx];                     /* flowing iridescence */

            uint16_t o = row[x];                            /* motion blur trail */
            int d = qs_dither(x, y);
            int r = rgb565_r8(c) + (((rgb565_r8(o) - rgb565_r8(c)) * 96) >> 8) + d;
            int g = rgb565_g8(c) + (((rgb565_g8(o) - rgb565_g8(c)) * 96) >> 8) + d;
            int b = rgb565_b8(c) + (((rgb565_b8(o) - rgb565_b8(c)) * 96) >> 8) + d;
            row[x] = rgb565_pack(r, g, b);
        }
    }
}

const effect_t fx_liquid = {
    .name  = "liquid",
    .mode  = MODE_HIRES,
    .init  = liquid_init,
    .frame = liquid_frame,
    .done  = NULL,
};
