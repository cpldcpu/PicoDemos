/* Scene 4 — Doppler-beamed accretion disk (1:51–2:43, MODE_HIRES 320x240).
 *
 * The accretion disk rendered as a spinning tilted plane via a Mode-7
 * affine sample of the top-down disk texture, with relativistic DOPPLER
 * BEAMING (the approaching side brightens + blue-shifts, the receding side
 * dims + reddens — the real M87 / Interstellar look) and Keplerian inner
 * material spinning faster.
 *
 * Dynamic: a slow camera dolly DESCENDS toward the black hole (it grows and
 * sets up the plunge), the disk spins, bright hot-spots orbit in the plane
 * (correctly occluded by the sphere when they pass behind it), and the
 * photon ring is an asymmetric pulsing glow.
 *
 * Perf: the disk texture is downsampled once per scene entry to a 128²
 * UNPACKED 8-bit RGB buffer in scene scratch, so the per-pixel bilinear tap
 * is plain byte reads — no RGB565 unpack in the inner loop. Per-scanline
 * constants are hoisted and off-disk/hole pixels are culled on squared
 * radius before any sqrt/sample.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "fx_common.h"
#include "../scene_scratch.h"
#include <stdint.h>
#include <math.h>

#define SCENE_LEN_MS  52000
#define W             VGA_HIRES_W      /* 320 */
#define H             VGA_HIRES_H      /* 240 */
#define HZ            92               /* horizon row */
#define CAM_H         20.0f
#define FOCAL         140.0f
#define R_OUT         70.0f            /* disk outer radius (world) */
#define R_IN          9.0f             /* inner edge / event-horizon radius */
#define DTEX          128              /* downsampled texture size */
#define DTC           64.0f            /* texture centre */
#define TSCALE        (DTC / R_OUT)
#define BH_R_WORLD    11.0f
#define NHOT          5

static uint8_t *dtex;                  /* DTEX*DTEX interleaved RGB, in scratch */

#define DSTARS 90
static struct { int16_t x, y; uint8_t b; } dstar[DSTARS];

static const float HS_RAD[NHOT] = { 16.0f, 24.0f, 34.0f, 46.0f, 60.0f };
static const float HS_PH [NHOT] = { 0.0f, 1.9f, 3.4f, 5.0f, 2.4f };
static const float HS_SP [NHOT] = { 1.7f, 1.35f, 1.1f, 0.92f, 0.8f };

/* Bilinear sample of the unpacked 128² RGB texture → 8-bit RGB. */
static inline void dtap_bilin(float tu, float tv, int *r, int *g, int *b)
{
    int u0 = (int)tu, v0 = (int)tv;
    float fu = tu - u0, fv = tv - v0;
    const uint8_t *r0 = &dtex[((v0 & (DTEX-1)) * DTEX) * 3];
    const uint8_t *r1 = &dtex[(((v0+1) & (DTEX-1)) * DTEX) * 3];
    int a = (u0 & (DTEX-1)) * 3, c = ((u0+1) & (DTEX-1)) * 3;
    float w00=(1-fu)*(1-fv), w10=fu*(1-fv), w01=(1-fu)*fv, w11=fu*fv;
    *r = (int)(r0[a+0]*w00 + r0[c+0]*w10 + r1[a+0]*w01 + r1[c+0]*w11);
    *g = (int)(r0[a+1]*w00 + r0[c+1]*w10 + r1[a+1]*w01 + r1[c+1]*w11);
    *b = (int)(r0[a+2]*w00 + r0[c+2]*w10 + r1[a+2]*w01 + r1[c+2]*w11);
}

static void disk_init(void)
{
    /* Downsample the 256² RGB565 source to a 128² unpacked-RGB texture in
     * scratch (averaging 2×2 blocks). One-time, on scene entry. */
    const uint16_t *src = (const uint16_t *)asset_disk_tex_data;
    dtex = g_scratch.bg_cache;     /* 128*128*3 = 49152 B ≤ 76800 */
    for (int v = 0; v < DTEX; v++) {
        const uint16_t *s0 = &src[(2*v)   * 256];
        const uint16_t *s1 = &src[(2*v+1) * 256];
        uint8_t *d = &dtex[(v*DTEX) * 3];
        for (int u = 0; u < DTEX; u++) {
            uint16_t a=s0[2*u], b=s0[2*u+1], c=s1[2*u], e=s1[2*u+1];
            d[u*3+0] = (rgb565_r8(a)+rgb565_r8(b)+rgb565_r8(c)+rgb565_r8(e)) >> 2;
            d[u*3+1] = (rgb565_g8(a)+rgb565_g8(b)+rgb565_g8(c)+rgb565_g8(e)) >> 2;
            d[u*3+2] = (rgb565_b8(a)+rgb565_b8(b)+rgb565_b8(c)+rgb565_b8(e)) >> 2;
        }
    }
    uint32_t s = 0x2468ace0u;
    for (int k = 0; k < DSTARS; k++) {
        s ^= s<<13; s ^= s>>17; s ^= s<<5; int sx = s % W;
        s ^= s<<13; s ^= s>>17; s ^= s<<5; int sy = s % (HZ + 24);
        s ^= s<<13; s ^= s>>17; s ^= s<<5; int sb = 70 + (s & 150);
        dstar[k].x = (int16_t)sx; dstar[k].y = (int16_t)sy; dstar[k].b = (uint8_t)sb;
    }
}

static inline void blob(uint16_t *fb, int cx, int cy, int rad, int r, int g, int b)
{
    for (int dy = -rad; dy <= rad; dy++) {
        int yy = cy + dy; if ((unsigned)yy >= H) continue;
        for (int dx = -rad; dx <= rad; dx++) {
            int xx = cx + dx; if ((unsigned)xx >= W) continue;
            int d2 = dx*dx + dy*dy; if (d2 > rad*rad) continue;
            int f = 256 - (d2 * 256) / (rad*rad + 1);
            fx_add160(&fb[yy*W+xx], (r*f)>>8, (g*f)>>8, (b*f)>>8);
        }
    }
}

/* Draw hot-spot k if it belongs to the requested depth pass:
 * want_near=0 → far side (behind the BH plane, drawn before the sphere so
 * it occludes them); want_near=1 → near side (drawn after, in front). */
static void hotspot(uint16_t *fb, int k, float spin, float zc, int A, int want_near)
{
    float rr = HS_RAD[k];
    float th = spin * HS_SP[k] + HS_PH[k];
    float wx = rr * cosf(th);
    float wz = zc + rr * sinf(th);
    if (wz < 3.0f) return;
    int is_near = (wz < zc);
    if (is_near != want_near) return;
    float sxf = W*0.5f + wx * FOCAL / wz;
    float syf = HZ + CAM_H * FOCAL / wz;
    if (syf < HZ + 1) return;
    float beam = fx_clampf(1.0f - 0.7f*(wx/rr), 0.4f, 1.9f);
    int br = (int)(160.0f * beam); if (br > 255) br = 255;
    br = (br * A) >> 8;
    int sz = (int)fx_clampf(160.0f / wz, 1.0f, 6.0f);
    if (beam > 1.0f) blob(fb, (int)sxf, (int)syf, sz, (br*8)>>3, (br*9)>>3, (br*12)>>3);
    else             blob(fb, (int)sxf, (int)syf, sz, br, (br*6)>>3, (br*3)>>3);
}

static void disk_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int A = fx_scene_alpha(t_into, SCENE_LEN_MS, 2500, 2500);
    float t = t_into * 0.001f;
    float app = t_into / (float)SCENE_LEN_MS;
    float zc = 96.0f - 52.0f * app;
    float spin = t * 0.85f;
    float cs = cosf(spin), sn = sinf(spin);
    float beam_amt = 0.6f + 0.4f * app;

    uint16_t *fb = vga_hires_back_buffer();

    /* Space above the horizon: clean gradient + soft stars. */
    for (int y = 0; y < HZ; y++) {
        int v = 9 - (y * 8) / HZ; if (v < 1) v = 1;
        uint16_t c = rgb565_pack((v*5)>>3, (v*6)>>3, v);
        uint16_t *row = &fb[y*W];
        for (int x = 0; x < W; x++) row[x] = c;
    }
    for (int k = 0; k < DSTARS; k++) {
        int sx = dstar[k].x, sy = dstar[k].y;
        if (sy >= HZ) continue;
        int b = (dstar[k].b * A) >> 8;
        int ix = sy*W + sx;
        fx_add160(&fb[ix], (b*9)>>4, (b*9)>>4, b);
        if (sx>0)   fx_add160(&fb[ix-1], b>>2,b>>2,(b*5)>>4);
        if (sx<W-1) fx_add160(&fb[ix+1], b>>2,b>>2,(b*5)>>4);
        if (sy>0)   fx_add160(&fb[ix-W], b>>2,b>>2,(b*5)>>4);
    }

    /* Disk plane (Mode-7). */
    const float R_OUT2 = R_OUT*R_OUT, R_IN2 = R_IN*R_IN;
    const uint16_t darkspace = rgb565_pack(1, 1, 3);
    for (int y = HZ; y < H; y++) {
        float pne  = (y - HZ) + 0.5f;
        float dist = CAM_H * FOCAL / pne;
        float invF = dist / FOCAL;
        float rz0  = dist - zc;
        float rz0sq = rz0 * rz0;
        float rxs  = rz0 * sn, rxc = rz0 * cs;
        int   fogA = (int)(fx_clampf(1.0f - dist*0.006f, 0.25f, 1.0f) * A);
        uint16_t *row = &fb[y*W];
        float rx0 = -(W * 0.5f) * invF;
        for (int x = 0; x < W; x++, rx0 += invF) {
            float rad2 = rx0*rx0 + rz0sq;
            if (rad2 > R_OUT2) { row[x] = darkspace; continue; }
            if (rad2 < R_IN2)  { row[x] = 0; continue; }
            float inv_rad = 1.0f / sqrtf(rad2);

            float rx = rx0*cs - rxs;
            float rz = rx0*sn + rxc;
            int r, g, b;
            dtap_bilin(DTC + rx*TSCALE, DTC + rz*TSCALE, &r, &g, &b);

            float vrel = fx_clampf(R_IN * 1.4f * inv_rad, 0.0f, 0.9f);
            float beam = fx_clampf(1.0f - beam_amt * vrel * (rx0 * inv_rad), 0.35f, 1.9f);
            float b3 = beam*beam*beam;
            r = (int)(r * b3); g = (int)(g * b3); b = (int)(b * b3);
            if (beam > 1.0f) b += (int)((255-b)*(beam-1.0f)*0.5f);
            else             r += (int)((255-r)*(1.0f-beam)*0.4f);

            row[x] = rgb565_pack((fx_clampi(r,0,255)*fogA)>>8,
                                 (fx_clampi(g,0,255)*fogA)>>8,
                                 (fx_clampi(b,0,255)*fogA)>>8);
        }
    }

    /* Far-side hot-spots — drawn first so the black hole occludes them. */
    for (int k = 0; k < NHOT; k++) hotspot(fb, k, spin, zc, A, 0);

    /* Black hole + living photon ring. */
    float cxf = W * 0.5f;
    float cyf = HZ + CAM_H * FOCAL / zc;
    float rbh = fx_clampf(BH_R_WORLD * FOCAL / zc, 12.0f, 70.0f);
    float pulse = 0.82f + 0.18f * sinf(t * 3.5f);
    int icx = (int)cxf, icy = (int)cyf, outer = (int)rbh + 7;
    for (int dy = -outer; dy <= outer; dy++) {
        int yy = icy + dy; if ((unsigned)yy >= H) continue;
        for (int dx = -outer; dx <= outer; dx++) {
            int xx = icx + dx; if ((unsigned)xx >= W) continue;
            float fdx = xx - cxf, fdy = yy - cyf;
            float rr = sqrtf(fdx*fdx + fdy*fdy);
            uint16_t *p = &fb[yy*W + xx];

            float covS = fx_clampf(rbh - rr + 0.5f, 0.0f, 1.0f);
            if (covS > 0.004f) {
                float k = 1.0f - covS;
                *p = rgb565_pack((int)(rgb565_r8(*p)*k),
                                 (int)(rgb565_g8(*p)*k),
                                 (int)(rgb565_b8(*p)*k));
            }
            float edge = 1.0f - fabsf(rr - rbh) / 5.0f;
            if (edge > 0.0f) {
                float invr = 1.0f / (rr + 1.0f);
                float appr = fx_clampf(-fdx * invr, 0.0f, 1.0f);
                float topf = fx_clampf(-fdy * invr, 0.0f, 1.0f);
                float bright = edge * edge * (0.45f + 0.85f*appr + 0.55f*topf) * pulse;
                int r = (int)(255 * bright), g = (int)(210 * bright), b = (int)(140 * bright);
                if (appr > 0.6f) b += (int)(80 * bright);
                fx_add160(p, (fx_clampi(r,0,255)*A)>>8,
                             (fx_clampi(g,0,255)*A)>>8,
                             (fx_clampi(b,0,255)*A)>>8);
            }
        }
    }

    /* Near-side hot-spots — drawn last so they stay in front of the sphere. */
    for (int k = 0; k < NHOT; k++) hotspot(fb, k, spin, zc, A, 1);
}

static void disk_done(void) {}

const effect_t fx_disk_real = {
    .name = "disk", .mode = MODE_HIRES,
    .init = disk_init, .frame = disk_frame, .done = disk_done,
};
