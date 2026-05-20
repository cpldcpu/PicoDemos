/* Fluid portraits — scene 3 (0:55 – 1:30, MODE_160).
 *
 * Dye-in-flowing-fluid in true-colour 160×120, scaled to 320×240 by the
 * VGA backend.
 *
 * Sim is a deliberately *cheap* approximation, not full Navier-Stokes:
 *   - Density field (uint8_t per cell) is the dye amount visible to the
 *     viewer. Two buffers, ping-ponged each frame.
 *   - Velocity field is procedural — sum-of-sines that looks like a
 *     curl-noise flow, evaluated per cell per frame from a precomputed
 *     256-entry sin LUT. No pressure solve, no incompressibility. Much
 *     cheaper than full Stam fluid and the dye still reads as wet.
 *   - Semi-Lagrangian advect with nearest-neighbour sample + 2/256
 *     decay per frame for natural dissipation.
 *   - Every PORTRAIT_INJECT_MS we cycle to the next of three portrait
 *     "stamps" and additively add its luminance map to the density.
 *     Each portrait dissolves into the flow over ~6 seconds.
 *   - Density (0..255) → RGB565 via a 256-entry colour LUT through
 *     deep-magenta-black → teal → cyan → magenta → warm-white.
 *
 * Performance budget at 160×120 = 19,200 cells:
 *   - Advect: ~5 inner-loop instructions per cell, plus 4 LUT sin lookups
 *     ≈ 30 cycles per cell ≈ 0.6 M cycles/frame ≈ 2 ms @ 300 MHz.
 *   - Render: 19,200 LUT reads ≈ 60 K cycles ≈ 0.2 ms.
 *   - Plenty of headroom for the audio ISR. */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define FB_W                  VGA_160_W                  /* 160 */
#define FB_H                  VGA_160_H                  /* 120 */
#define P_W                   ASSET_PORTRAIT_A_W         /* 96  */
#define P_H                   ASSET_PORTRAIT_A_H         /* 96  */
#define P_X0                  ((FB_W - P_W) / 2)         /* 32  */
#define P_Y0                  ((FB_H - P_H) / 2)         /* 12  */

#define SCENE_LEN_MS          30000
#define FADE_IN_MS             2000
#define FADE_OUT_MS            3000
#define FADE_OUT_AT           (SCENE_LEN_MS - FADE_OUT_MS)

/* Slot length matches the total stamp lifecycle (fi + hold + fo below)
 * so portraits run back-to-back with no dead black gap between them.
 * As one fades out, the next fades in. */
#define PORTRAIT_INJECT_MS     5000
#define PORTRAIT_FADE_MS       1500   /* unused; kept for backward-compat */

/* --- buffers ------------------------------------------------------- */

/* Density fields live in the shared scene scratch (see scene_scratch.h)
 * so we share 76 KB with the other MODE_160 scene (reaction-diffusion)
 * and the MODE_320 backdrop-cache scenes. */
#include "../scene_scratch.h"
#define density_a (g_scratch.fluid.a)
#define density_b (g_scratch.fluid.b)
static uint8_t *density_back  = density_a;
static uint8_t *density_front = density_b;

/* RGB565 lookup for density values. */
static uint16_t color_lut[256];

/* Pre-extracted luminance maps for the three portraits (sampled from the
 * portrait's quantised palette once at init, then memcpy-style cheap at
 * runtime). */
static uint8_t portrait_lum[3][P_W * P_H];

/* Q15 sin table — 256 entries covering 0..2π. Inner loop uses this to
 * avoid sinf() calls during advect. */
static int16_t sin_q15[256];
static inline int16_t s256(int idx) { return sin_q15[idx & 0xFF]; }
static inline int16_t c256(int idx) { return sin_q15[(idx + 64) & 0xFF]; }

/* --- init helpers -------------------------------------------------- */

static void build_color_lut(void)
{
    static const uint8_t anchors[][3] = {
        {  10,   4,  28 },   /* near-black, hint of magenta */
        {  20,  80, 120 },   /* deep teal                   */
        {  40, 210, 210 },   /* bright cyan                 */
        { 220,  70, 180 },   /* magenta                     */
        { 255, 220, 160 },   /* warm white                  */
    };
    const int N    = (int)(sizeof anchors / sizeof anchors[0]);
    const int seg_w = 256 / (N - 1);
    for (int i = 0; i < 256; i++) {
        int seg = i / seg_w;
        if (seg >= N - 1) seg = N - 2;
        int sub = i - seg * seg_w;
        const uint8_t *c0 = anchors[seg];
        const uint8_t *c1 = anchors[seg + 1];
        int r = c0[0] + (c1[0] - c0[0]) * sub / seg_w;
        int g = c0[1] + (c1[1] - c0[1]) * sub / seg_w;
        int b = c0[2] + (c1[2] - c0[2]) * sub / seg_w;
        color_lut[i] = rgb565_pack(r, g, b);
    }
}

static void build_sin_lut(void)
{
    for (int i = 0; i < 256; i++) {
        sin_q15[i] = (int16_t)(sinf(i * (2.0f * 3.14159265f / 256.0f)) * 32767.0f);
    }
}

/* Sample one portrait's 8bpp+palette source and store BT.601 luminance
 * per pixel. The portraits are mostly black background + bright subject,
 * so luminance acts as a soft alpha mask for the dye stamp. */
static void precompute_portrait_lum(int idx, const uint8_t *src, const uint16_t *pal)
{
    uint8_t *dst = portrait_lum[idx];
    for (int i = 0; i < P_W * P_H; i++) {
        uint16_t c = pal[src[i]];
        int r = rgb565_r8(c);
        int g = rgb565_g8(c);
        int b = rgb565_b8(c);
        /* BT.601 luminance: 0.299 R + 0.587 G + 0.114 B. */
        int lum = (r * 299 + g * 587 + b * 114) / 1000;
        if (lum > 255) lum = 255;
        dst[i] = (uint8_t)lum;
    }
}

/* --- sim ----------------------------------------------------------- */

/* Q4 sub-cell velocity. Output magnitudes peak around ±16 = ±1.0 cell
 * per frame. At lower magnitudes the bilinear advect interpolates
 * between neighbour densities, so even very small flows visibly move
 * the dye — the portrait stamps accumulate before being swept away.
 *
 * Two distinct curl recipes crossfade via a slow blend factor (a sine
 * of phase, cycling once over ~33 s — roughly the scene length). The
 * field therefore morphs continuously through the demo, never the
 * same pattern twice. */
static inline void velocity_at(int x, int y, int phase, int *vx_q4, int *vy_q4)
{
    /* Recipe A — gentle horizontal-leaning swirls (low spatial freq). */
    int ax = (x * 3 +  y * 2 +  phase    ) & 0xFF;
    int ay = (y * 3 -  x * 2 +  phase / 2) & 0xFF;
    int bx = (x * 1 -  y * 4 -  phase / 3) & 0xFF;
    int by = (y * 1 +  x * 4 -  phase / 3) & 0xFF;
    int va_x = (s256(ax) + c256(bx)) >> 12;
    int va_y = (c256(ay) + s256(by)) >> 12;

    /* Recipe B — tighter, more rotational swirls with reverse phase
     * direction so its swirl axes face the opposite way. */
    int cx = (x * 6 -  y * 1 +  phase * 2) & 0xFF;
    int cy = (y * 6 +  x * 1 +  phase * 2) & 0xFF;
    int dx = (x * 2 +  y * 7 -  phase    ) & 0xFF;
    int dy = (y * 2 -  x * 7 -  phase    ) & 0xFF;
    int vb_x = (s256(cx) + c256(dx)) >> 12;
    int vb_y = (c256(cy) + s256(dy)) >> 12;

    /* Blend factor 0..128: slow oscillation through phase. phase
     * advances ~1 per 16 ms; phase >> 3 reaches 256 (one full sine
     * cycle) after ~33 s — about one cycle over the scene. */
    int blend = (s256((phase >> 3) & 0xFF) + 32768) >> 9;  /* 0..128 */
    *vx_q4 = (va_x * (128 - blend) + vb_x * blend) >> 7;
    *vy_q4 = (va_y * (128 - blend) + vb_y * blend) >> 7;
}

/* Stamp the portrait as a *minimum* density floor (max with current),
 * not an additive contribution. This way the dye for a portrait pixel
 * stays at full lum × brightness even while the surrounding fluid is
 * trying to advect it away — the portrait reads as clear while it's
 * being stamped, then dissolves naturally once we stop stamping. */
static void inject_portrait(int idx, int brightness_q8)
{
    if (idx < 0 || idx > 2 || brightness_q8 <= 0) return;
    if (brightness_q8 > 256) brightness_q8 = 256;
    const uint8_t *lum = portrait_lum[idx];
    for (int y = 0; y < P_H; y++) {
        uint8_t *dst = density_back + (P_Y0 + y) * FB_W + P_X0;
        const uint8_t *src = lum + y * P_W;
        for (int x = 0; x < P_W; x++) {
            int v = (src[x] * brightness_q8) >> 8;
            if (v > dst[x]) dst[x] = (uint8_t)v;
        }
    }
}

/* --- effect lifecycle --------------------------------------------- */

static void fluid_init(void)
{
    build_sin_lut();
    build_color_lut();
    precompute_portrait_lum(0, asset_portrait_a_data, asset_portrait_a_pal);
    precompute_portrait_lum(1, asset_portrait_b_data, asset_portrait_b_pal);
    precompute_portrait_lum(2, asset_portrait_c_data, asset_portrait_c_pal);
    memset(density_a, 0, sizeof density_a);
    memset(density_b, 0, sizeof density_b);
    density_back  = density_a;
    density_front = density_b;
}

static void fluid_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    int phase = (int)(t_into >> 4);   /* slow drift of the curl pattern */

    /* Semi-Lagrangian advect with Q4 sub-cell sampling. Source position
     * is integer cell + 4-bit fractional; bilinear-interp the four
     * neighbouring densities. Decay (~0.8%/frame) gives a ~3 s tail. */
    for (int y = 0; y < FB_H; y++) {
        uint8_t *dst_row = density_back + y * FB_W;
        int y_q4 = y << 4;
        for (int x = 0; x < FB_W; x++) {
            int vx_q4, vy_q4;
            velocity_at(x, y, phase, &vx_q4, &vy_q4);

            int sx_q4 = (x << 4) - vx_q4;
            int sy_q4 = y_q4    - vy_q4;
            int sx    = sx_q4 >> 4;
            int sy    = sy_q4 >> 4;
            int fx    = sx_q4 & 0x0F;     /* 0..15 fractional */
            int fy    = sy_q4 & 0x0F;

            /* Clamp so the +1 neighbours stay in-bounds. */
            if (sx < 0) { sx = 0; fx = 0; }
            else if (sx >= FB_W - 1) { sx = FB_W - 2; fx = 15; }
            if (sy < 0) { sy = 0; fy = 0; }
            else if (sy >= FB_H - 1) { sy = FB_H - 2; fy = 15; }

            const uint8_t *r0 = density_front + sy       * FB_W + sx;
            const uint8_t *r1 = density_front + (sy + 1) * FB_W + sx;
            int d00 = r0[0], d10 = r0[1];
            int d01 = r1[0], d11 = r1[1];
            int d_top = d00 + (((d10 - d00) * fx) >> 4);
            int d_bot = d01 + (((d11 - d01) * fx) >> 4);
            int d     = d_top + (((d_bot - d_top) * fy) >> 4);

            d = (d * 255) >> 8;          /* very gentle decay — keeps trails alive ~3s */
            dst_row[x] = (uint8_t)d;
        }
    }

    /* Inject portrait dye onto the just-advected back buffer.
     * Each PORTRAIT_INJECT_MS-slot the next portrait gets stamped: its
     * brightness ramps 0→256 over the first PORTRAIT_FADE_IN_MS, holds
     * at full for PORTRAIT_HOLD_MS, then ramps 256→0 over
     * PORTRAIT_FADE_OUT_MS. The remaining ~2.5 s of the slot is pure
     * fluid: previous dye gets advected and decayed away before the
     * next portrait arrives. */
    if (t_into < FADE_OUT_AT) {
        uint32_t slot = t_into / PORTRAIT_INJECT_MS;
        uint32_t into = t_into - slot * PORTRAIT_INJECT_MS;
        /* fi 1500 → hold 1000 → fo 2500 = 5000 ms continuous stamp,
         * leaving 1000 ms of pure-fluid drift before the next slot's
         * fade-in begins. The slow fade-out means the previous portrait
         * is still ~40% visible when the next starts to emerge —
         * crossfades through the dye trail rather than going to black. */
        const uint32_t fi   = 1500, hold = 1000, fo = 2500;
        int bright = 0;
        if      (into < fi)             bright = (int)(into * 256 / fi);
        else if (into < fi + hold)      bright = 256;
        else if (into < fi + hold + fo) bright = 256 - (int)((into - fi - hold) * 256 / fo);
        inject_portrait((int)(slot % 3), bright);
    }

    /* Scene-level fade-in/fade-out: scale density before colour mapping
     * so the whole image dims to black at the scene boundaries. */
    int alpha_q8 = 256;
    if (t_into < FADE_IN_MS) {
        alpha_q8 = (int)(t_into * 256 / FADE_IN_MS);
    } else if (t_into >= FADE_OUT_AT) {
        uint32_t into = t_into - FADE_OUT_AT;
        alpha_q8 = (into >= FADE_OUT_MS) ? 0 : 256 - (int)(into * 256 / FADE_OUT_MS);
    }

    /* Render: density (back) → RGB565 framebuffer. */
    uint16_t *fb = vga_160_back_buffer();
    if (alpha_q8 == 256) {
        for (int i = 0; i < FB_W * FB_H; i++) {
            fb[i] = color_lut[density_back[i]];
        }
    } else {
        for (int i = 0; i < FB_W * FB_H; i++) {
            int d = (density_back[i] * alpha_q8) >> 8;
            fb[i] = color_lut[d];
        }
    }

    /* Swap. Next frame, what we just wrote becomes the source. */
    uint8_t *tmp = density_front;
    density_front = density_back;
    density_back  = tmp;
}

static void fluid_done(void)
{
    /* Leave the buffers cleared so a re-entry to this scene starts
     * dark. */
    memset(density_a, 0, sizeof density_a);
    memset(density_b, 0, sizeof density_b);
}

const effect_t fx_fluid_portraits_real = {
    .name  = "fluid_portraits",
    .mode  = MODE_160,
    .init  = fluid_init,
    .frame = fluid_frame,
    .done  = fluid_done,
};
