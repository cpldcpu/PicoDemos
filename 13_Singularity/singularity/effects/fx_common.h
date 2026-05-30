/* Shared helpers for SINGULARITY effects.
 *
 * Header-only static inlines so each effect .c stays self-contained and
 * the host Makefile / CMake don't need an extra translation unit. All
 * pixel building goes through rgb565.h so the PIO-native bit order is
 * preserved end-to-end (see rgb565.h).
 */

#ifndef SINGULARITY_FX_COMMON_H
#define SINGULARITY_FX_COMMON_H

#include <stdint.h>
#include <math.h>
#include "vga.h"
#include "rgb565.h"
#include "../font8x8.h"

/* --- scalar helpers ------------------------------------------------- */

static inline int   fx_clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float fx_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* 0..256 ramps (Q8). */
static inline int fx_ramp_in(uint32_t t, uint32_t start, uint32_t dur)
{
    if (t <  start)       return 0;
    if (t >= start + dur) return 256;
    return (int)((t - start) * 256 / dur);
}
static inline int fx_ramp_out(uint32_t t, uint32_t start, uint32_t dur)
{
    if (t <  start)       return 256;
    if (t >= start + dur) return 0;
    return 256 - (int)((t - start) * 256 / dur);
}

/* Scene boundary fade: returns a Q8 multiplier that ramps 0→256 over the
 * first `fade_in` ms and 256→0 over the last `fade_out` ms of a scene of
 * length `len`. Multiply it into every RGB before packing. */
static inline int fx_scene_alpha(uint32_t t_into, uint32_t len,
                                 uint32_t fade_in, uint32_t fade_out)
{
    if (t_into < fade_in) return (int)(t_into * 256 / fade_in);
    if (t_into >= len - fade_out) {
        uint32_t into = t_into - (len - fade_out);
        return into >= fade_out ? 0 : 256 - (int)(into * 256 / fade_out);
    }
    return 256;
}

/* --- MODE_160 RGB565 pixel ops -------------------------------------- */

/* Alpha-blend an RGB triplet over an existing fb160 pixel (Q8 alpha). */
static inline void fx_blend160(uint16_t *p, int tr, int tg, int tb, int a_q8)
{
    if (a_q8 <= 0) return;
    uint16_t cur = *p;
    int cr = rgb565_r8(cur), cg = rgb565_g8(cur), cb = rgb565_b8(cur);
    int r = cr + (((tr - cr) * a_q8) >> 8);
    int g = cg + (((tg - cg) * a_q8) >> 8);
    int b = cb + (((tb - cb) * a_q8) >> 8);
    *p = rgb565_pack(r, g, b);
}

/* Additive splat (saturating) into an fb160 pixel — for glowing dust,
 * sparks, etc. */
static inline void fx_add160(uint16_t *p, int ar, int ag, int ab)
{
    uint16_t cur = *p;
    int r = rgb565_r8(cur) + ar;
    int g = rgb565_g8(cur) + ag;
    int b = rgb565_b8(cur) + ab;
    *p = rgb565_pack(r, g, b);
}

/* Stamp an 8×8-font string into fb160 with a 1-px drop shadow, at integer
 * `scale`, blended at Q8 alpha. fr/fg/fb = fill, sr/sg/sb = shadow. */
static inline void fx_text160(uint16_t *fb, const char *s, int x0, int y0,
                              int scale, int fr, int fg, int fb_,
                              int sr, int sg, int sb, int a_q8)
{
    if (a_q8 <= 0) return;
    for (int ci = 0; s[ci]; ci++) {
        const uint8_t *g = font8x8_glyph(s[ci]);
        int cx = x0 + ci * 8 * scale;
        for (int row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            for (int col = 0; col < 8; col++) {
                if (!((bits >> (7 - col)) & 1)) continue;
                int bx = cx + col * scale;
                int by = y0 + row * scale;
                for (int dy = 0; dy < scale; dy++) {
                    int yy = by + dy, sy = yy + scale;
                    for (int dx = 0; dx < scale; dx++) {
                        int xx = bx + dx, sx = xx + scale;
                        if ((unsigned)sx < VGA_HIRES_W && (unsigned)sy < VGA_HIRES_H)
                            fx_blend160(&fb[sy * VGA_HIRES_W + sx], sr, sg, sb, a_q8);
                        if ((unsigned)xx < VGA_HIRES_W && (unsigned)yy < VGA_HIRES_H)
                            fx_blend160(&fb[yy * VGA_HIRES_W + xx], fr, fg, fb_, a_q8);
                    }
                }
            }
        }
    }
}

/* Pixel width of an 8×8 string at integer scale (for centring). */
static inline int fx_text160_w(const char *s, int scale)
{
    int n = 0; while (s[n]) n++;
    return n * 8 * scale;
}

/* Cheap scrolling text: crisp bitmap fill with VERTICAL sub-pixel
 * anti-aliasing (each set pixel blends into the two rows it straddles at a
 * fractional Y). Smooth glide at any scroll speed, ~30× cheaper than the
 * bilinear-coverage glow path — ideal for many small lines per frame.
 * Use fx_text_glow for big static headings; this for scrollers. */
static inline void fx_text_scroll(uint16_t *fb, const char *s, int x0, float yf,
                                  int scale, int fr, int fg, int fb_, int a_q8)
{
    if (a_q8 <= 0) return;
    int iy = (int)floorf(yf);
    int w1 = (int)((yf - iy) * 256), w0 = 256 - w1;
    int aw0 = (a_q8 * w0) >> 8, aw1 = (a_q8 * w1) >> 8;
    for (int ci = 0; s[ci]; ci++) {
        const uint8_t *g = font8x8_glyph(s[ci]);
        int cx = x0 + ci * 8 * scale;
        for (int row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            if (!bits) continue;
            int by = iy + row * scale;
            for (int col = 0; col < 8; col++) {
                if (!((bits >> (7 - col)) & 1)) continue;
                int bx = cx + col * scale;
                for (int sk = 0; sk < scale; sk++) {
                    int px = bx + sk; if ((unsigned)px >= VGA_HIRES_W) continue;
                    for (int rk = 0; rk < scale; rk++) {
                        int yy = by + rk;
                        if ((unsigned)yy      < VGA_HIRES_H) fx_blend160(&fb[yy*VGA_HIRES_W+px],     fr,fg,fb_, aw0);
                        if ((unsigned)(yy+1)  < VGA_HIRES_H) fx_blend160(&fb[(yy+1)*VGA_HIRES_W+px], fr,fg,fb_, aw1);
                    }
                }
            }
        }
    }
}

/* --- anti-aliased glowing text (truecolor / MODE_HIRES) --------------- */

static inline int fx_glyph_bit(const uint8_t *g, int ix, int iy)
{
    if ((unsigned)ix >= 8 || (unsigned)iy >= 8) return 0;
    return (g[iy] >> (7 - ix)) & 1;
}

/* Bilinear coverage of the 8×8 glyph at source (u,v), returned 0..256.
 * Sampling between the 1-bit cells yields soft, anti-aliased edges instead
 * of hard staircase blocks. */
static inline int fx_glyph_cov(const uint8_t *g, float u, float v)
{
    int iu = (int)floorf(u), iv = (int)floorf(v);
    float fu = u - iu, fv = v - iv;
    float c = fx_glyph_bit(g, iu,   iv  ) * (1.0f-fu) * (1.0f-fv)
            + fx_glyph_bit(g, iu+1, iv  ) *       fu  * (1.0f-fv)
            + fx_glyph_bit(g, iu,   iv+1) * (1.0f-fu) *       fv
            + fx_glyph_bit(g, iu+1, iv+1) *       fu  *       fv;
    return (int)(c * 256.0f);
}

/* Draw a string into an fb160/fb_hires RGB565 buffer with:
 *   - anti-aliased glyph edges (bilinear coverage), and
 *   - a soft additive colored glow halo (the glyph sampled ~1.4× larger).
 * fill = letter colour, glow = halo colour, a_q8 = master alpha (0..256).
 * Width-aware via VGA_HIRES_W/H (used by the 320×240 truecolor scenes). */
static inline void fx_text_glow(uint16_t *fb, const char *s, int x0, int y0,
                                int scale, int fr, int fg, int fb_,
                                int gr, int gg, int gb, int a_q8)
{
    if (a_q8 <= 0) return;
    float invs = 1.0f / scale;
    int   pad  = scale * 2;                  /* halo reach */
    for (int ci = 0; s[ci]; ci++) {
        const uint8_t *g = font8x8_glyph(s[ci]);
        int cx = x0 + ci * 8 * scale;
        for (int oy = -pad; oy < 8*scale + pad; oy++) {
            int py = y0 + oy; if ((unsigned)py >= VGA_HIRES_H) continue;
            float vf = (oy + 0.5f) * invs;                 /* fill source v   */
            float vg = 4.0f + (vf - 4.0f) / 1.4f;          /* glow source v   */
            for (int ox = -pad; ox < 8*scale + pad; ox++) {
                int px = cx + ox; if ((unsigned)px >= VGA_HIRES_W) continue;
                float uf = (ox + 0.5f) * invs;
                float ug = 4.0f + (uf - 4.0f) / 1.4f;
                int covg = fx_glyph_cov(g, ug, vg);
                int covf = fx_glyph_cov(g, uf, vf);
                uint16_t *p = &fb[py * VGA_HIRES_W + px];
                /* glow first (additive halo), then crisp fill on top */
                if (covg > 4) {
                    int ga = (covg * a_q8) >> 8;           /* 0..256 */
                    fx_add160(p, (gr*ga*40)>>14, (gg*ga*40)>>14, (gb*ga*40)>>14);
                }
                if (covf > 4) fx_blend160(p, fr, fg, fb_, (covf * a_q8) >> 8);
            }
        }
    }
}

/* --- colour ramps --------------------------------------------------- */

/* Blackbody-ish ramp: maps t in [0,255] from deep red → orange → white.
 * Cheap piecewise-linear, good enough for a star surface / plasma. */
static inline void fx_blackbody(int t, int *r, int *g, int *b)
{
    t = fx_clampi(t, 0, 255);
    /* r rises fastest, then g, then b. */
    *r = fx_clampi(t * 3,            0, 255);
    *g = fx_clampi((t - 60)  * 2,    0, 255);
    *b = fx_clampi((t - 150) * 3,    0, 255);
}

#endif
