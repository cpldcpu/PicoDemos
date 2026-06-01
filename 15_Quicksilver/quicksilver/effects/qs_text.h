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
#include "logo.h"
#include <math.h>
#include <stdint.h>

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

/* Blit the antialiased chrome wordmark stencil with its top-left at (x0,y0):
 * dark drop shadow then a bevelled silver gradient with a travelling specular
 * highlight at screen-x `sweepx`. Used by the title and the credits header. */
static inline void qs_logo_blit(int x0, int y0, int sweepx)
{
    uint16_t *fb = vga_hires_back_buffer();
    for (int ly = 0; ly < QS_LOGO_H; ly++) {        /* shadow */
        int sy = y0 + ly + 3; if ((unsigned)sy >= VGA_HIRES_H) continue;
        const uint8_t *mrow = &qs_logo_mask[ly * QS_LOGO_W];
        uint16_t *frow = fb + sy * VGA_HIRES_W;
        for (int lx = 0; lx < QS_LOGO_W - 3; lx++) {
            int m = mrow[lx]; if (m < 8) continue;
            int sx = x0 + lx + 3; if ((unsigned)sx >= VGA_HIRES_W) continue;
            uint16_t o = frow[sx];
            frow[sx] = rgb565_pack(rgb565_r8(o) * (255 - m) >> 8,
                                   rgb565_g8(o) * (255 - m) >> 8,
                                   rgb565_b8(o) * (255 - m) >> 8);
        }
    }
    for (int ly = 0; ly < QS_LOGO_H; ly++) {        /* chrome bevel + glint */
        int sy = y0 + ly; if ((unsigned)sy >= VGA_HIRES_H) continue;
        int lum = 140 + (int)(105.0f * sinf((ly + 0.5f) / QS_LOGO_H * 3.14159f));
        const uint8_t *mrow = &qs_logo_mask[ly * QS_LOGO_W];
        uint16_t *frow = fb + sy * VGA_HIRES_W;
        for (int lx = 0; lx < QS_LOGO_W; lx++) {
            int m = mrow[lx]; if (m < 8) continue;
            int sx = x0 + lx; if ((unsigned)sx >= VGA_HIRES_W) continue;
            int d = sx - sweepx; if (d < 0) d = -d;
            int L = lum + (d < 26 ? (26 - d) * 5 : 0);
            int cr = L * 82 / 100, cg = L * 90 / 100, cb = L > 240 ? 255 : L + 15;
            uint16_t o = frow[sx];
            frow[sx] = rgb565_pack(rgb565_r8(o) + (((cr - rgb565_r8(o)) * m) >> 8),
                                   rgb565_g8(o) + (((cg - rgb565_g8(o)) * m) >> 8),
                                   rgb565_b8(o) + (((cb - rgb565_b8(o)) * m) >> 8));
        }
    }
}

#endif
