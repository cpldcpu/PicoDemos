/* title.c — QUICKSILVER opening. The nano-banana title backdrop, a slow
 * interpolator-zoom shimmer over it, and the chrome wordmark (matcap text). */

#include "../interp_compat.h"
#include "../vga.h"
#include "../rgb565.h"
#include "../scene.h"
#include "assets.h"
#include "qs_text.h"
#include "qs_fx.h"

#include <math.h>

/* one channel, 2x2 bilinear blend (linear interpolation), weights 0..256 */
static inline int title_bilerp(int a00, int a10, int a01, int a11, int fx, int fy)
{
    int top = a00 * (256 - fx) + a10 * fx;
    int bot = a01 * (256 - fx) + a11 * fx;
    return (top * (256 - fy) + bot * fy) >> 16;
}

static void title_blit_bg(uint32_t t_ms)
{
    uint16_t *fb = vga_hires_back_buffer();
    const uint8_t  *src = asset_title_bg_data;
    const uint16_t *pal = asset_title_bg_pal;
    /* Slow parallax drift + brightness shimmer. The drift is FRACTIONAL and
     * the backdrop is sampled with 2x2 linear interpolation (the RP2350
     * interpolator's signature BLEND), so the image glides sub-pixel-smoothly
     * instead of stepping a whole pixel at a time. */
    float t = t_ms * 0.001f;
    float dyf = 6.0f * sinf(t * 0.5f);
    float dxf = 4.0f * sinf(t * 0.37f);
    int sh = (int)(8 * sinf(t * 1.3f));

    for (int y = 0; y < VGA_HIRES_H; y++) {
        float syf = (float)y + dyf;
        int   iy  = (int)floorf(syf);
        int   fy  = (int)((syf - iy) * 256.0f);
        int   y0  = iy < 0 ? 0 : (iy >= VGA_HIRES_H ? VGA_HIRES_H - 1 : iy);
        int   y1  = iy + 1 < 0 ? 0 : (iy + 1 >= VGA_HIRES_H ? VGA_HIRES_H - 1 : iy + 1);
        const uint8_t *r0 = src + y0 * VGA_HIRES_W;
        const uint8_t *r1 = src + y1 * VGA_HIRES_W;
        uint16_t *frow = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            float sxf = (float)x + dxf;
            int   ix  = (int)floorf(sxf);
            int   fx  = (int)((sxf - ix) * 256.0f);
            int   x0  = ix < 0 ? 0 : (ix >= VGA_HIRES_W ? VGA_HIRES_W - 1 : ix);
            int   x1  = ix + 1 < 0 ? 0 : (ix + 1 >= VGA_HIRES_W ? VGA_HIRES_W - 1 : ix + 1);
            uint16_t c00 = pal[r0[x0]], c10 = pal[r0[x1]];
            uint16_t c01 = pal[r1[x0]], c11 = pal[r1[x1]];
            int rr = title_bilerp(rgb565_r8(c00), rgb565_r8(c10), rgb565_r8(c01), rgb565_r8(c11), fx, fy);
            int gg = title_bilerp(rgb565_g8(c00), rgb565_g8(c10), rgb565_g8(c01), rgb565_g8(c11), fx, fy);
            int bb = title_bilerp(rgb565_b8(c00), rgb565_b8(c10), rgb565_b8(c01), rgb565_b8(c11), fx, fy);
            int d = sh + qs_dither(x, y);   /* break 8bpp/5-bit banding */
            frow[x] = rgb565_pack(rr + d, gg + d, bb + d);
        }
    }
}

/* fade ramp: 0 before `in`, ramps 0..256 over `in`..`in+rise`, holds 256,
 * then ramps 256..0 over `out-fall`..`out` (out<=0 means never fade out). */
static int ramp(float t, float in, float rise, float out, float fall)
{
    if (t < in) return 0;
    int a = 256;
    if (t < in + rise) a = (int)((t - in) / rise * 256.0f);
    if (out > 0.0f && t > out - fall) {
        int b = (int)((out - t) / fall * 256.0f);
        if (b < a) a = b;
    }
    if (a < 0) a = 0;
    if (a > 256) a = 256;
    return a;
}

static void title_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    title_blit_bg(t_ms);

    float t = t_ms * 0.001f;

    /* Stage 1 (0..7s): the LATENT group brand reveals, centred and alone.
     * Stage 2 (6.5s..end): it cross-fades into the QUICKSILVER wordmark +
     * tagline. The two logos never share the screen crowded — they hand off. */

    /* --- stage 1: LATENT brand, vertically centred --- */
    int la = ramp(t, 0.3f, 0.8f, 7.0f, 1.0f);     /* fade in, hold, fade out  */
    if (la > 0) {
        int ly = 86;                               /* logo (48h) + gap + text  */
        qs_latent_blit_a(ly, la);
        const char *g = "PRESENTS";
        int gw = qs_text_w(g, 1);
        qs_text_chrome_a(g, (VGA_HIRES_W - gw) / 2, ly + ASSET_LATENT_LOGO_H + 14, 1, 35, la);
    }

    /* --- stage 2: QUICKSILVER wordmark + tagline, vertically centred --- */
    int wa = ramp(t, 6.5f, 1.0f, 0.0f, 0.0f);     /* fade in, then hold       */
    if (wa > 0) {
        int settle = t < 7.5f ? (int)((7.5f - t) * 24) : 0;
        int wy = 72;
        qs_logo_blit_a(0, wy + settle, wa);
        const char *sub = "RP2350 INTERPOLATOR";
        int sw = qs_text_w(sub, 1);
        if (t > 7.6f) {
            int sa = ramp(t, 7.6f, 0.8f, 0.0f, 0.0f);
            qs_text_chrome_a(sub, (VGA_HIRES_W - sw) / 2, wy + QS_LOGO_H + 10, 1, 50, sa);
        }
    }
}

const effect_t fx_title = {
    .name  = "title",
    .mode  = MODE_HIRES,
    .init  = NULL,
    .frame = title_frame,
    .done  = NULL,
};
