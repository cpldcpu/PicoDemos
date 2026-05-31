/* Shared truecolor (MODE_HIRES, 320x240 RGB565) helpers for ORIGAMI scenes:
 * paper material setup, background fills, antialiased font8x8 text, and beat
 * helpers. Header-only static inlines (one TU per effect on pico + host).
 *
 * Everything is paper: warm pastels, flat colour fields, soft shadows. The
 * poly3d engine antialiases polygon/shadow edges; text is AA'd here via
 * bilinear glyph-coverage sampling.
 */

#ifndef ORIGAMI_FX_H
#define ORIGAMI_FX_H

#include <stdint.h>
#include <math.h>
#include <string.h>
#include "vga.h"
#include "rgb565.h"
#include "poly3d.h"
#include "../font8x8.h"

/* ---- timing / beat (Marimba Seedbox: 117.5 BPM) ---------------------- */
#define OG_BEAT_MS   510.6f
#define OG_BAR_MS    2042.6f
static inline float og_beat_pulse(uint32_t t){ return expf(-3.2f*(fmodf((float)t,OG_BEAT_MS)/OG_BEAT_MS)); }
static inline float og_bar_pulse (uint32_t t){ return expf(-2.0f*(fmodf((float)t,OG_BAR_MS )/OG_BAR_MS )); }

static inline int   og_clampi(int v,int lo,int hi){return v<lo?lo:(v>hi?hi:v);}
static inline float og_clampf(float v,float lo,float hi){return v<lo?lo:(v>hi?hi:v);}
static inline float og_smooth(float t){ t=og_clampf(t,0,1); return t*t*(3.0f-2.0f*t); }

/* ---- paper materials (lit RGB; engine darkens per face by Lambert) --- */
static inline void og_materials(void)
{
    p3_set_material(P3_MAT_WHITE,   246,243,233);
    p3_set_material(P3_MAT_CREAM,   242,228,194);
    p3_set_material(P3_MAT_SKYBLUE, 154,202,233);
    p3_set_material(P3_MAT_CORAL,   243,151,134);
    p3_set_material(P3_MAT_SAGE,    176,206,150);
    p3_set_material(P3_MAT_MUSTARD, 231,189, 92);
}

/* ---- background fills (RGB565) --------------------------------------- */
static inline void og_clear_rgb(int r, int g, int b)
{
    uint16_t *fb = vga_hires_back_buffer();
    uint16_t c = rgb565_pack(r,g,b);
    for (int i = 0; i < VGA_HIRES_W*VGA_HIRES_H; i++) fb[i] = c;
}

/* ordered-dither matrix (4x4 Bayer), breaks RGB565 gradient banding */
static const uint8_t OG_BAYER[4][4] = {{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};

/* vertical sky gradient: top (tr,tg,tb) -> horizon (hr,hg,hb), dithered so
 * the 5-bit-per-channel steps don't show as bands. */
static inline void og_sky_grad(int tr,int tg,int tb, int hr,int hg,int hb)
{
    uint16_t *fb = vga_hires_back_buffer();
    for (int y = 0; y < VGA_HIRES_H; y++) {
        int u = y * 256 / VGA_HIRES_H;
        int r = tr+((hr-tr)*u>>8), g = tg+((hg-tg)*u>>8), b = tb+((hb-tb)*u>>8);
        uint16_t *row = &fb[y*VGA_HIRES_W];
        const uint8_t *brow = OG_BAYER[y&3];
        for (int x = 0; x < VGA_HIRES_W; x++) {
            int o = ((int)brow[x&3] - 8) >> 1;       /* -4..+3 */
            row[x] = rgb565_pack(r+o, g+o, b+o);
        }
    }
}
/* default pastel paper sky */
static inline void og_sky(void){ og_sky_grad(139,192,228, 223,233,236); }

/* ---- antialiased font8x8 text ---------------------------------------- */
static inline int og_text_w(const char *s, int scale){ int n=0; while(s[n])n++; return n*8*scale; }

static inline int og_glyph_bit(const uint8_t *g, int ix, int iy)
{ return ((unsigned)ix<8 && (unsigned)iy<8) ? ((g[iy]>>(7-ix))&1) : 0; }

static inline int og_glyph_cov(const uint8_t *g, float u, float v)
{
    int iu=(int)floorf(u), iv=(int)floorf(v);
    float fu=u-iu, fv=v-iv;
    float c = og_glyph_bit(g,iu,iv)*(1-fu)*(1-fv) + og_glyph_bit(g,iu+1,iv)*fu*(1-fv)
            + og_glyph_bit(g,iu,iv+1)*(1-fu)*fv  + og_glyph_bit(g,iu+1,iv+1)*fu*fv;
    return (int)(c*256.0f);
}
static inline void og_blend(uint16_t *p, int r,int g,int b,int cov)
{
    if (cov <= 0) return;
    if (cov > 256) cov = 256;
    int cr=rgb565_r8(*p), cg=rgb565_g8(*p), cb=rgb565_b8(*p);
    *p = rgb565_pack(cr+(((r-cr)*cov)>>8), cg+(((g-cg)*cov)>>8), cb+(((b-cb)*cov)>>8));
}

/* Crisp text (nearest, hard pixel blocks — the right look for an 8x8 pixel
 * font) with a soft, slightly translucent drop shadow. The polygons are
 * antialiased; an axis-aligned bitmap font has no edges to smooth, so it is
 * drawn sharp. fr/fg/fb fill, sr/sg/sb shadow. */
static inline void og_text_rgb(const char *s, int x0, int y0, int scale,
                               int fr,int fg,int fb, int sr,int sg,int sb)
{
    uint16_t *fbp = vga_hires_back_buffer();
    uint16_t fill = rgb565_pack(fr,fg,fb);
    uint16_t shad = rgb565_pack(sr,sg,sb);
    int ow = scale > 1 ? 2 : 1;             /* outline width (px) */
    /* pass 0: a solid 1-2px OUTLINE in the shadow colour (reads over any
     * background — busy fields, sky, page); pass 1: crisp fill on top. Both
     * solid, so thin small text stays sharp. */
    for (int pass = 0; pass < 2; pass++) {
        for (int ci = 0; s[ci]; ci++) {
            const uint8_t *g = font8x8_glyph(s[ci]);
            int cx = x0 + ci*8*scale;
            for (int row = 0; row < 8; row++) {
                uint8_t bits = g[row];
                if (!bits) continue;
                for (int col = 0; col < 8; col++) {
                    if (!((bits >> (7-col)) & 1)) continue;
                    int bx = cx + col*scale, by = y0 + row*scale;
                    for (int dy = 0; dy < scale; dy++) {
                        for (int dx = 0; dx < scale; dx++) {
                            int px = bx + dx, py = by + dy;
                            if (pass == 1) {
                                if ((unsigned)px < VGA_HIRES_W && (unsigned)py < VGA_HIRES_H)
                                    fbp[py*VGA_HIRES_W + px] = fill;
                            } else {
                                /* stamp the outline ring around this cell */
                                for (int oy = -ow; oy <= ow; oy++)
                                    for (int ox = -ow; ox <= ow; ox++) {
                                        int xx = px+ox, yy = py+oy;
                                        if ((unsigned)xx < VGA_HIRES_W && (unsigned)yy < VGA_HIRES_H)
                                            fbp[yy*VGA_HIRES_W + xx] = shad;
                                    }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* warm-white text with a soft brown shadow — the demo's default */
static inline void og_text(const char *s, int x0, int y0, int scale)
{ og_text_rgb(s, x0, y0, scale, 250,247,240, 70,64,58); }
static inline void og_text_centred(const char *s, int y0, int scale)
{ og_text(s, (VGA_HIRES_W - og_text_w(s,scale))/2, y0, scale); }

/* ---- vector logotype (the "ORIGAMI" wordmark) -----------------------
 * Big headings drawn as thick geometric strokes instead of an enlarged 8x8
 * bitmap: each letter is a few line segments, thickened into quads and drawn
 * through the engine's ANTIALIASED fill (p3_fill_convex). Angular, clean,
 * scalable — a cut-paper wordmark. Covers the letters of ORIGAMI. */
typedef struct { const float *seg; int n; } og_glyphv;   /* seg = 4 floats/segment, normalised 0..1 */

static inline og_glyphv og_vletter(char c)
{
    static const float O[]={0.13f,0.08f,0.13f,0.92f, 0.87f,0.08f,0.87f,0.92f, 0.13f,0.08f,0.87f,0.08f, 0.13f,0.92f,0.87f,0.92f};
    static const float R[]={0.17f,0.04f,0.17f,0.96f, 0.17f,0.09f,0.74f,0.09f, 0.80f,0.11f,0.80f,0.47f, 0.17f,0.50f,0.78f,0.50f, 0.46f,0.50f,0.86f,0.96f};
    static const float I[]={0.50f,0.04f,0.50f,0.96f};
    static const float G[]={0.13f,0.10f,0.13f,0.90f, 0.13f,0.08f,0.86f,0.08f, 0.13f,0.92f,0.88f,0.92f, 0.88f,0.50f,0.88f,0.92f, 0.55f,0.51f,0.88f,0.51f};
    static const float A[]={0.10f,0.96f,0.50f,0.04f, 0.90f,0.96f,0.50f,0.04f, 0.27f,0.62f,0.73f,0.62f};
    static const float M[]={0.08f,0.96f,0.08f,0.04f, 0.92f,0.96f,0.92f,0.04f, 0.08f,0.05f,0.50f,0.64f, 0.92f,0.05f,0.50f,0.64f};
    switch (c) {
        case 'O': return (og_glyphv){O,4}; case 'R': return (og_glyphv){R,5};
        case 'I': return (og_glyphv){I,1}; case 'G': return (og_glyphv){G,5};
        case 'A': return (og_glyphv){A,3}; case 'M': return (og_glyphv){M,4};
        default:  return (og_glyphv){0,0};
    }
}

static inline void og_logo_rgb(const char *s, int cx, int topY, int letterH,
                               int fr,int fg,int fb, int sr,int sg,int sb)
{
    float lw  = 0.78f*letterH;
    float adv = lw + 0.22f*letterH;
    float th  = 0.155f*letterH;
    int n = 0; while (s[n]) n++;
    float startx = cx - (n*adv - (adv-lw)) * 0.5f;
    int ofs = letterH > 40 ? 3 : 2;
    for (int pass = 0; pass < 2; pass++) {
        for (int ci = 0; ci < n; ci++) {
            og_glyphv gv = og_vletter(s[ci]);
            float bx = startx + ci*adv;
            for (int k = 0; k < gv.n; k++) {
                float ax = bx + gv.seg[k*4+0]*lw, ay = topY + gv.seg[k*4+1]*letterH;
                float cxx= bx + gv.seg[k*4+2]*lw, cyy= topY + gv.seg[k*4+3]*letterH;
                float dx = cxx-ax, dy = cyy-ay, L = sqrtf(dx*dx+dy*dy); if (L<1e-3f) L=1;
                float ex = dx/L, ey = dy/L;          /* extend ends to join corners */
                ax -= ex*th*0.5f; ay -= ey*th*0.5f; cxx += ex*th*0.5f; cyy += ey*th*0.5f;
                float nx = -ey*th*0.5f, ny = ex*th*0.5f;
                float o = pass==0 ? (float)ofs : 0.0f;
                float px[4] = { ax+nx+o, cxx+nx+o, cxx-nx+o, ax-nx+o };
                float py[4] = { ay+ny+o, cyy+ny+o, cyy-ny+o, ay-ny+o };
                if (pass==0) p3_fill_convex(px,py,4, sr,sg,sb);
                else         p3_fill_convex(px,py,4, fr,fg,fb);
            }
        }
    }
}
/* warm-white wordmark, soft brown shadow, centred */
static inline void og_logo_centred(const char *s, int topY, int letterH)
{ og_logo_rgb(s, VGA_HIRES_W/2, topY, letterH, 250,247,240, 78,70,62); }

#endif
