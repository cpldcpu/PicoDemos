/* Scene 6 — Spacetime collapse / event-horizon crossing (3:50–4:37, MODE_HIRES).
 *
 * Deliberately a DIFFERENT visual language from the photographic lensing
 * scene before it: the "rubber-sheet" curvature of spacetime rendered as a
 * glowing wireframe gravity well. A polar grid (rings + radial spokes) is
 * dimpled into a funnel that DEEPENS as we cross the horizon; the camera
 * sinks toward the throat, the grid spirals, and at the singularity it all
 * plunges to a point and whites out into the rebirth scene.
 *
 * Pure vector rendering (additive glowing lines) — no panorama, no lens
 * LUT — so it reads as geometry/energy, not "another warped starfield."
 */

#include "scene.h"
#include "vga.h"
#include "rgb565.h"
#include "fx_common.h"
#include <stdint.h>
#include <math.h>

#define SCENE_LEN_MS  47000
#define W   VGA_HIRES_W
#define H   VGA_HIRES_H
#define NR  16                 /* rings   */
#define NS  48                 /* spokes  */
#define RMAX 140.0f
#define FOCAL 220.0f
#define CXS 160.0f
#define CYS 120.0f
#define DSTARS 70

static float   vsx[NR*NS], vsy[NR*NS], vbr[NR*NS];
static uint8_t vvis[NR*NS];
static struct { int16_t x, y; uint8_t b; } star[DSTARS];

/* Anti-aliased additive line: step the major axis, splat the two pixels
 * straddling the true minor coordinate with coverage weights. Cheap (2
 * additive writes per step) and smooths the wireframe nicely. */
static void aline(uint16_t *fb, float x0, float y0, float x1, float y1, int r, int g, int b)
{
    float dx = x1-x0, dy = y1-y0;
    float adx = dx<0?-dx:dx, ady = dy<0?-dy:dy;
    int n = (int)(adx >= ady ? adx : ady); if (n < 1) n = 1;
    float xs = dx/n, ys = dy/n, x = x0, y = y0;
    int major_x = adx >= ady;
    for (int i = 0; i <= n; i++, x += xs, y += ys) {
        if (major_x) {
            int ix = (int)floorf(x), iy = (int)floorf(y);
            int w1 = (int)((y - iy) * 256), w0 = 256 - w1;
            if ((unsigned)ix < W) {
                if ((unsigned)iy     < H) fx_add160(&fb[iy*W+ix],     (r*w0)>>8,(g*w0)>>8,(b*w0)>>8);
                if ((unsigned)(iy+1) < H) fx_add160(&fb[(iy+1)*W+ix], (r*w1)>>8,(g*w1)>>8,(b*w1)>>8);
            }
        } else {
            int ix = (int)floorf(x), iy = (int)floorf(y);
            int w1 = (int)((x - ix) * 256), w0 = 256 - w1;
            if ((unsigned)iy < H) {
                if ((unsigned)ix     < W) fx_add160(&fb[iy*W+ix],   (r*w0)>>8,(g*w0)>>8,(b*w0)>>8);
                if ((unsigned)(ix+1) < W) fx_add160(&fb[iy*W+ix+1], (r*w1)>>8,(g*w1)>>8,(b*w1)>>8);
            }
        }
    }
}

static void spacetime_init(void)
{
    uint32_t s = 0x51ee77a3u;
    for (int k = 0; k < DSTARS; k++) {
        s ^= s<<13; s ^= s>>17; s ^= s<<5; int sx = s % W;
        s ^= s<<13; s ^= s>>17; s ^= s<<5; int sy = s % H;
        s ^= s<<13; s ^= s>>17; s ^= s<<5; int sb = 50 + (s & 120);
        star[k].x=(int16_t)sx; star[k].y=(int16_t)sy; star[k].b=(uint8_t)sb;
    }
}

/* Ring-index colour: cool cyan rim → hot violet-white throat. */
static void ring_color(int k, int *r, int *g, int *b)
{
    float u = 1.0f - (float)k / (NR-1);          /* 1 at throat, 0 at rim */
    *r = (int)(40 + 175*u);
    *g = (int)(110 + 90*u);
    *b = (int)(150 + 105*u);
}

static void spacetime_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    float p  = t_into / (float)SCENE_LEN_MS;
    float p2 = p*p;
    int   fin = fx_ramp_in(t_into, 0, 1500);
    float flash = (t_into > SCENE_LEN_MS - 1700)
                ? (t_into - (SCENE_LEN_MS - 1700)) / 1700.0f : 0.0f;

    float well  = 22.0f + 240.0f * p2;            /* well deepens */
    float th0   = t_into * 0.00026f;              /* grid spirals */
    float camY  = 42.0f - 16.0f * p;              /* sink toward the throat */
    float camZ  = -160.0f + 60.0f * p;            /* far back → approach the well */
    float pitch = 0.52f + 0.26f * p;
    float cosP = cosf(pitch), sinP = sinf(pitch);
    float twist = 1.6f * p;                        /* frame-drag spiral */

    uint16_t *fb = vga_hires_back_buffer();
    for (int i = 0; i < W*H; i++) fb[i] = 0;

    /* faint star background */
    for (int k = 0; k < DSTARS; k++) {
        int b = (star[k].b * fin) >> 8;
        fx_add160(&fb[star[k].y*W + star[k].x], (b*7)>>3, (b*7)>>3, b);
    }

    /* Project all grid vertices. */
    const float dR = RMAX / NR;
    for (int k = 0; k < NR; k++) {
        float r  = (k+1) * dR;
        float wy = -well / (r*0.045f + 0.22f);     /* funnel depth (Y up) */
        float spin = th0 + twist / (r*0.02f + 0.3f); /* inner rings drag faster */
        float bright = fx_clampf(0.5f + 1.4f*(1.0f - (float)k/(NR-1)), 0.4f, 2.0f);
        for (int j = 0; j < NS; j++) {
            float ang = j * (6.2831853f / NS) + spin;
            float wx = r * cosf(ang), wz = r * sinf(ang);
            float ex = wx, ey = wy - camY, ez = wz - camZ;
            /* pitch the camera down: cz forward, cyc vertical */
            float cz  = -ey*sinP + ez*cosP;
            float cyc =  ey*cosP + ez*sinP;
            int idx = k*NS + j;
            if (cz < 3.0f) { vvis[idx] = 0; continue; }
            float inv = FOCAL / cz;
            float sx = CXS + ex*inv, sy = CYS - cyc*inv;
            if (sx < -1500 || sx > 1500 || sy < -1500 || sy > 1500) { vvis[idx]=0; continue; }
            vsx[idx] = sx; vsy[idx] = sy;
            vbr[idx] = bright * fx_clampf(40.0f/cz, 0.25f, 1.8f);
            vvis[idx] = 1;
        }
    }

    /* Draw rings + spokes. */
    for (int k = 0; k < NR; k++) {
        int cr, cg, cb; ring_color(k, &cr, &cg, &cb);
        for (int j = 0; j < NS; j++) {
            int a = k*NS + j, bnxt = k*NS + (j+1)%NS;
            if (vvis[a] && vvis[bnxt]) {
                float br = (vbr[a]+vbr[bnxt])*0.5f * fin / 256.0f;
                aline(fb, vsx[a],vsy[a],vsx[bnxt],vsy[bnxt],
                      (int)(cr*br),(int)(cg*br),(int)(cb*br));
            }
            if (k < NR-1) {
                int c = (k+1)*NS + j;
                if (vvis[a] && vvis[c]) {
                    float br = (vbr[a]+vbr[c])*0.5f * fin / 256.0f;
                    aline(fb, vsx[a],vsy[a],vsx[c],vsy[c],
                          (int)(cr*br),(int)(cg*br),(int)(cb*br));
                }
            }
        }
    }

    if (flash > 0.0f) {
        int wv = (int)(flash * 255);
        for (int i = 0; i < W*H; i++) {
            uint16_t c = fb[i];
            int r = rgb565_r8(c) + (((255-rgb565_r8(c))*wv)>>8);
            int g = rgb565_g8(c) + (((255-rgb565_g8(c))*wv)>>8);
            int b = rgb565_b8(c) + (((255-rgb565_b8(c))*wv)>>8);
            fb[i] = rgb565_pack(r,g,b);
        }
    }
}

static void spacetime_done(void) {}

const effect_t fx_spacetime_real = {
    .name = "spacetime", .mode = MODE_HIRES,
    .init = spacetime_init, .frame = spacetime_frame, .done = spacetime_done,
};
