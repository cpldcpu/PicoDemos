/* qs_text.h — chrome bitmap text for QUICKSILVER's title/credits. Each set
 * glyph pixel samples the chrome matcap by its screen position, so letters look
 * like polished metal cut from the same reflection as the 3D scenes. A dark
 * drop-shadow pass keeps the text legible over busy backdrops. */

#ifndef QS_TEXT_H
#define QS_TEXT_H

#include "../vga.h"
#include "../rgb565.h"
#include "../font8x8.h"
#include "assets.h"
#include <math.h>
#include <stdint.h>

/* The chrome wordmark is now a delivered image (asset_title_logo, 320x80). */
#define QS_LOGO_H ASSET_TITLE_LOGO_H

static inline int qs_text_w(const char *s, int scale) { return (int)(8 * scale) * (int)__builtin_strlen(s); }

static inline void qs_block(uint16_t *fb, int bx, int by, int scale, uint16_t col)
{
    for (int py = 0; py < scale; py++) {
        int sy = by + py; if ((unsigned)sy >= VGA_HIRES_H) continue;
        uint16_t *row = fb + sy * VGA_HIRES_W;
        for (int px = 0; px < scale; px++) {
            int sx = bx + px; if ((unsigned)sx >= VGA_HIRES_W) continue;
            row[sx] = col;
        }
    }
}

/* Scale an rgb565 colour toward black by alpha (0..256) — used for fade
 * in/out of black-keyed art and chrome text. */
static inline uint16_t qs_fade(uint16_t c, int a)
{
    if (a >= 256) return c;
    if (a <= 0)   return 0;
    return rgb565_pack(rgb565_r8(c) * a >> 8, rgb565_g8(c) * a >> 8, rgb565_b8(c) * a >> 8);
}

/* Draw `s` at (x0,y0), each glyph pixel `scale`x, with a dark drop shadow and
 * bright chrome (matcap) fill. `tint` adds extra brightness (0..80). `alpha`
 * fades the whole thing toward black (0..256). */
static inline void qs_text_chrome_a(const char *s, int x0, int y0, int scale, int tint, int alpha)
{
    if (alpha <= 0) return;
    uint16_t *fb = vga_hires_back_buffer();
    int off = scale > 1 ? 2 : 1;

    /* pass 1: dark drop shadow (offset down-right) */
    int x = x0;
    uint16_t shadow = qs_fade(rgb565_pack(4, 6, 12), alpha);
    for (const char *p = s; *p; p++) {
        const uint8_t *g = font8x8_glyph(*p);
        for (int gy = 0; gy < 8; gy++) {
            uint8_t bits = g[gy];
            for (int gx = 0; gx < 8; gx++)
                if (bits & (0x80 >> gx))
                    qs_block(fb, x + gx*scale + off, y0 + gy*scale + off, scale, shadow);
        }
        x += 8 * scale;
    }
    /* pass 2: chrome fill — a bright vertical silver gradient keyed to the
     * glyph row (brightest band near the top, like a polished bevel) so the
     * letters always read, independent of the backdrop. */
    static const int lvl[8] = { 150, 205, 240, 230, 200, 170, 140, 110 };
    x = x0;
    for (const char *p = s; *p; p++) {
        const uint8_t *g = font8x8_glyph(*p);
        for (int gy = 0; gy < 8; gy++) {
            uint8_t bits = g[gy];
            int L = lvl[gy] + tint;
            uint16_t col = qs_fade(rgb565_pack(L * 82 / 100, L * 90 / 100, L > 240 ? 255 : L + 15), alpha);
            for (int gx = 0; gx < 8; gx++)
                if (bits & (0x80 >> gx))
                    qs_block(fb, x + gx*scale, y0 + gy*scale, scale, col);
        }
        x += 8 * scale;
    }
}

static inline void qs_text_chrome(const char *s, int x0, int y0, int scale, int tint)
{
    qs_text_chrome_a(s, x0, y0, scale, tint, 256);
}

/* Blit a delivered chrome image (artwork on black) at (x0,y0), black-keyed —
 * skip near-black pixels so the chrome + glow composite over the backdrop.
 * `alpha` (0..256) CROSS-FADES each lit pixel with whatever is already in the
 * framebuffer (the live backdrop), so the art dissolves in/out of the scene
 * instead of fading to/from black — a far cleaner reveal and logo hand-off. */
static inline void qs_img_keyed_a(const uint16_t *img, int iw, int ih, int x0, int y0, int alpha)
{
    if (alpha <= 0) return;
    uint16_t *fb = vga_hires_back_buffer();
    for (int ly = 0; ly < ih; ly++) {
        int sy = y0 + ly; if ((unsigned)sy >= VGA_HIRES_H) continue;
        const uint16_t *r = img + ly * iw;
        uint16_t *frow = fb + sy * VGA_HIRES_W;
        for (int lx = 0; lx < iw; lx++) {
            int sx = x0 + lx; if ((unsigned)sx >= VGA_HIRES_W) continue;
            uint16_t c = r[lx];
            if (rgb565_r8(c) + rgb565_g8(c) + rgb565_b8(c) < 30) continue;  /* black key */
            if (alpha >= 256) { frow[sx] = c; continue; }
            uint16_t bg = frow[sx];                 /* cross-fade with the live backdrop */
            frow[sx] = rgb565_pack(
                rgb565_r8(bg) + (((rgb565_r8(c) - rgb565_r8(bg)) * alpha) >> 8),
                rgb565_g8(bg) + (((rgb565_g8(c) - rgb565_g8(bg)) * alpha) >> 8),
                rgb565_b8(bg) + (((rgb565_b8(c) - rgb565_b8(bg)) * alpha) >> 8));
        }
    }
}

static inline void qs_img_keyed(const uint16_t *img, int iw, int ih, int x0, int y0)
{
    qs_img_keyed_a(img, iw, ih, x0, y0, 256);
}

/* The QUICKSILVER wordmark (320x80). `sweepx` is unused (chrome is baked in). */
static inline void qs_logo_blit_a(int x0, int y0, int alpha)
{
    qs_img_keyed_a((const uint16_t *)asset_title_logo_data, ASSET_TITLE_LOGO_W, ASSET_TITLE_LOGO_H, x0, y0, alpha);
}
static inline void qs_logo_blit(int x0, int y0, int sweepx)
{
    (void)sweepx;
    qs_logo_blit_a(x0, y0, 256);
}

/* The LATENT group logo (256x48), horizontally centred. */
static inline void qs_latent_blit_a(int y0, int alpha)
{
    qs_img_keyed_a((const uint16_t *)asset_latent_logo_data, ASSET_LATENT_LOGO_W, ASSET_LATENT_LOGO_H,
                   (VGA_HIRES_W - ASSET_LATENT_LOGO_W) / 2, y0, alpha);
}
static inline void qs_latent_blit(int y0)
{
    qs_latent_blit_a(y0, 256);
}

#endif
