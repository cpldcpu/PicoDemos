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

/* Draw `s` at (x0,y0), each glyph pixel `scale`x, with a dark drop shadow and
 * bright chrome (matcap) fill. `tint` adds extra brightness (0..80). */
static inline void qs_text_chrome(const char *s, int x0, int y0, int scale, int tint)
{
    uint16_t *fb = vga_hires_back_buffer();
    const uint16_t *env = (const uint16_t *)asset_envmap_data;
    int off = scale > 1 ? 2 : 1;

    /* pass 1: dark drop shadow (offset down-right) */
    int x = x0;
    for (const char *p = s; *p; p++) {
        const uint8_t *g = font8x8_glyph(*p);
        for (int gy = 0; gy < 8; gy++) {
            uint8_t bits = g[gy];
            for (int gx = 0; gx < 8; gx++)
                if (bits & (0x80 >> gx))
                    qs_block(fb, x + gx*scale + off, y0 + gy*scale + off, scale, rgb565_pack(4, 6, 12));
        }
        x += 8 * scale;
    }
    /* pass 2: chrome fill — a bright vertical silver gradient keyed to the
     * glyph row (brightest band near the top, like a polished bevel) so the
     * letters always read, independent of the backdrop. */
    static const int lvl[8] = { 150, 205, 240, 230, 200, 170, 140, 110 };
    (void)env;
    x = x0;
    for (const char *p = s; *p; p++) {
        const uint8_t *g = font8x8_glyph(*p);
        for (int gy = 0; gy < 8; gy++) {
            uint8_t bits = g[gy];
            int L = lvl[gy] + tint;
            uint16_t col = rgb565_pack(L * 82 / 100, L * 90 / 100, L > 240 ? 255 : L + 15);
            for (int gx = 0; gx < 8; gx++)
                if (bits & (0x80 >> gx))
                    qs_block(fb, x + gx*scale, y0 + gy*scale, scale, col);
        }
        x += 8 * scale;
    }
}

/* Blit the delivered chrome wordmark image (asset_title_logo, 320x80) with its
 * top-left at (x0,y0), black-keyed (the artwork is the wordmark on black, so we
 * skip near-black pixels and let the chrome + amber rim glow composite over the
 * backdrop). `sweepx` is unused now that the chrome is baked into the art. */
static inline void qs_logo_blit(int x0, int y0, int sweepx)
{
    (void)sweepx;
    uint16_t *fb = vga_hires_back_buffer();
    const uint16_t *logo = (const uint16_t *)asset_title_logo_data;
    for (int ly = 0; ly < ASSET_TITLE_LOGO_H; ly++) {
        int sy = y0 + ly; if ((unsigned)sy >= VGA_HIRES_H) continue;
        const uint16_t *lrow = logo + ly * ASSET_TITLE_LOGO_W;
        uint16_t *frow = fb + sy * VGA_HIRES_W;
        for (int lx = 0; lx < ASSET_TITLE_LOGO_W; lx++) {
            int sx = x0 + lx; if ((unsigned)sx >= VGA_HIRES_W) continue;
            uint16_t c = lrow[lx];
            if (rgb565_r8(c) + rgb565_g8(c) + rgb565_b8(c) < 30) continue;  /* black key */
            frow[sx] = c;
        }
    }
}

#endif
