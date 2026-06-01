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

static void title_blit_bg(uint32_t t_ms)
{
    uint16_t *fb = vga_hires_back_buffer();
    const uint8_t  *src = asset_title_bg_data;
    const uint16_t *pal = asset_title_bg_pal;
    /* slow vertical parallax drift + brightness shimmer so it's not static */
    float t = t_ms * 0.001f;
    int dy = (int)(6.0f * sinf(t * 0.5f));
    int sh = (int)(8 * sinf(t * 1.3f));
    for (int y = 0; y < VGA_HIRES_H; y++) {
        int sy = y + dy; if (sy < 0) sy = 0; else if (sy >= VGA_HIRES_H) sy = VGA_HIRES_H - 1;
        const uint8_t *srow = src + sy * VGA_HIRES_W;
        uint16_t *frow = fb + y * VGA_HIRES_W;
        for (int x = 0; x < VGA_HIRES_W; x++) {
            uint16_t c = pal[srow[x]];
            int d = sh + qs_dither(x, y);   /* break 8bpp/5-bit banding */
            frow[x] = rgb565_pack(rgb565_r8(c) + d, rgb565_g8(c) + d, rgb565_b8(c) + d);
        }
    }
}

static void title_frame(uint32_t t_ms, uint32_t t_global)
{
    (void)t_global;
    title_blit_bg(t_ms);

    float t = t_ms * 0.001f;
    int settle = t < 1.0f ? (int)((1.0f - t) * 36) : 0;

    /* group logo above the wordmark — LATENT presents QUICKSILVER */
    if (t > 0.3f) {
        qs_latent_blit(14);
        const char *g = "PRESENTS";
        int gw = qs_text_w(g, 1);
        qs_text_chrome(g, (VGA_HIRES_W - gw) / 2, 66, 1, 35);
    }

    /* specular glint sweeps left→right across the wordmark, repeating */
    int sweepx = (int)(fmodf(t * 220.0f, (float)(VGA_HIRES_W + 120))) - 60;
    qs_logo_blit(0, 92 + settle, sweepx);

    /* tagline sits below the wordmark */
    if (t > 1.2f) {
        const char *sub = "RP2350 INTERPOLATOR";
        int sw = qs_text_w(sub, 1);
        qs_text_chrome(sub, (VGA_HIRES_W - sw) / 2, 92 + QS_LOGO_H + 6, 1, 50);
    }
}

const effect_t fx_title = {
    .name  = "title",
    .mode  = MODE_HIRES,
    .init  = NULL,
    .frame = title_frame,
    .done  = NULL,
};
