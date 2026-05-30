/* Scene 1 — Curl-noise nebula (0:18–0:50, MODE_HIRES 320x240).
 *
 * ~6000 dust particles advected by a curl-noise flow + inward pull, over a
 * growing warm proto-star core; the cloud heats (violet→gold) and collapses
 * as it spins faster, with a pre-ignition flicker. Arc: diffuse cool drift →
 * accelerating vortex → tight hot collapse.
 *
 * Perf (tuned for 60 fps on the M33): NO per-pixel divides (the radial
 * vignette + core glow are 1-D LUTs in r²), NO per-particle transcendentals
 * (the flow is a coarse 40×30 grid computed once per frame and bilinearly
 * sampled; colour comes from a per-particle hue + global warmth).
 */

#include "scene.h"
#include "vga.h"
#include "rgb565.h"
#include "fx_common.h"
#include "../scene_scratch.h"
#include <stdint.h>
#include <math.h>

#define SCENE_LEN_MS  32000
#define NP            NEBULA_PARTICLES
#define W             VGA_HIRES_W
#define H             VGA_HIRES_H
#define CX            160.0f
#define CY            120.0f
#define GW            40            /* flow grid */
#define GH            30
#define CELL          8             /* W/GW == H/GH == 8 px */
#define R2MAX         40000.0f      /* (160² + 120²) */
#define part (g_scratch.nebula.p)

static float    flowx[GW*GH], flowy[GW*GH];
static uint8_t  vig_lut[256];       /* vignette intensity 0..22 by r² */
static uint16_t glow_lut[256];      /* core-glow shape, Q8 (0..256) by r² */

static uint32_t rng = 0x1234abcdu;
static inline uint32_t xs(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static inline float frnd(void){ return (xs() & 0xffffff) / (float)0x1000000; }

static void respawn(int i)
{
    float ang = frnd() * 6.2831853f;
    float rad = 150.0f + frnd() * 80.0f;
    part[i].x = CX + cosf(ang) * rad;
    part[i].y = CY + sinf(ang) * rad * 0.72f;
    part[i].life = (uint8_t)(120 + (xs() & 127));
}

static void nebula_init(void)
{
    rng = 0x1234abcdu;
    for (int i = 0; i < NP; i++) {
        part[i].x = frnd() * (W + 32) - 16.0f;
        part[i].y = frnd() * (H + 32) - 16.0f;
        part[i].life = (uint8_t)(40 + (xs() & 200));
    }
    /* Radial LUTs (constant) — replace two per-pixel divides in the clear. */
    for (int i = 0; i < 256; i++) {
        float r2 = (i / 255.0f) * R2MAX;
        vig_lut[i]  = (uint8_t)(22.0f / (1.0f + r2 * 0.000088f));
        glow_lut[i] = (uint16_t)(256.0f / (1.0f + r2 * 0.0011f));
    }
}

static void nebula_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int A = fx_scene_alpha(t_into, SCENE_LEN_MS, 2500, 3000);
    float t = t_into * 0.001f;
    float grow  = fx_clampf(t_into / (float)SCENE_LEN_MS, 0.0f, 1.0f);
    float pull  = 0.35f + grow * 1.55f;
    float swirl = 0.30f + grow * grow * 2.3f;
    float fmag  = 4.2f * (1.0f - 0.55f*grow) * (1.0f + 0.30f*sinf(t*1.7f));
    float fr    = 0.022f + 0.020f * grow;
    float warm  = fx_clampf(0.18f + 0.90f*grow, 0.0f, 1.0f);
    float flick = 1.0f + 0.18f * sinf(t * 5.5f) * grow;
    int   corei = (int)(grow * grow * 240.0f * flick * A) >> 8;

    /* Coarse flow field — 1200 cells, evaluated once (vs per particle). */
    float ph_x = 0.5f*t, ph_y = -0.3f*t;
    for (int gy = 0; gy < GH; gy++) {
        float ny = (gy * CELL) * fr + ph_y;
        float sny = sinf(ny), cny = cosf(ny);
        for (int gx = 0; gx < GW; gx++) {
            float nx = (gx * CELL) * fr + ph_x;
            flowx[gy*GW+gx] = -sinf(nx) * sny;
            flowy[gy*GW+gx] = -cosf(nx) * cny;
        }
    }

    uint16_t *fb = vga_hires_back_buffer();

    /* Clear: vignette + growing warm core, both from r² LUTs (no divides). */
    for (int y = 0; y < H; y++) {
        float dy = y - CY;
        float dy2 = dy*dy;
        for (int x = 0; x < W; x++) {
            float dx = x - CX;
            int idx = (int)((dx*dx + dy2) * (255.0f / R2MAX));
            if (idx > 255) idx = 255;
            int v  = vig_lut[idx];
            int gl = (corei * glow_lut[idx]) >> 8;
            fb[y*W + x] = rgb565_pack(((v*10)>>4)+gl, ((v*7)>>4)+((gl*168)>>8),
                                      ((v*18)>>4)+((gl*102)>>8));
        }
    }

    for (int i = 0; i < NP; i++) {
        float x = part[i].x, y = part[i].y;

        /* Bilinear sample of the coarse flow grid (no trig per particle). */
        float gxf = x * (1.0f/CELL), gyf = y * (1.0f/CELL);
        int gx = (int)gxf, gy = (int)gyf;
        if (gx < 0) gx = 0; else if (gx > GW-2) gx = GW-2;
        if (gy < 0) gy = 0; else if (gy > GH-2) gy = GH-2;
        float tx = gxf - gx, ty = gyf - gy;
        int o = gy*GW + gx;
        float vx = flowx[o]*(1-tx)*(1-ty) + flowx[o+1]*tx*(1-ty)
                 + flowx[o+GW]*(1-tx)*ty + flowx[o+GW+1]*tx*ty;
        float vy = flowy[o]*(1-tx)*(1-ty) + flowy[o+1]*tx*(1-ty)
                 + flowy[o+GW]*(1-tx)*ty + flowy[o+GW+1]*tx*ty;

        float rx = CX - x, ry = CY - y;
        float rl = 1.0f / (sqrtf(rx*rx + ry*ry) + 0.001f);
        rx *= rl; ry *= rl;
        x += vx*fmag + rx*pull - ry*swirl;
        y += vy*fmag + ry*pull + rx*swirl;

        if (--part[i].life == 0 ||
            (x-CX)*(x-CX) + (y-CY)*(y-CY) < 16.0f ||
            x < -16 || x > W+16 || y < -16 || y > H+16) {
            respawn(i);
            x = part[i].x; y = part[i].y;
        }
        part[i].x = x; part[i].y = y;

        int ix = (int)x, iy = (int)y;
        if ((unsigned)ix >= W || (unsigned)iy >= H) continue;

        /* Per-particle fixed hue + global warmth (no atan2/sin). */
        float hue = ((i * 733) & 1023) * (1.0f/1024.0f);
        float gd  = fx_clampf(0.45f*hue + warm, 0.0f, 1.0f);
        float dxp = x - CX, dyp = y - CY;
        float rw  = 0.45f + 1.7f / (1.0f + (dxp*dxp + dyp*dyp) * 0.0006f);
        int r = (int)((150.0f + 105.0f*gd) * rw * flick * A) >> 8;
        int g = (int)(( 90.0f +  95.0f*gd) * rw * A) >> 8;
        int b = (int)((230.0f - 150.0f*gd) * rw * A) >> 8;

        uint16_t *p = &fb[iy*W + ix];
        fx_add160(p, r, g, b);
        int rh=r>>1, gh=g>>1, bh=b>>1, rq=r>>2, gq=g>>2, bq=b>>2;
        if (ix>0)   fx_add160(p-1, rh,gh,bh);
        if (ix<W-1) fx_add160(p+1, rh,gh,bh);
        if (iy>0)   fx_add160(p-W, rh,gh,bh);
        if (iy<H-1) fx_add160(p+W, rh,gh,bh);
        if (ix>0&&iy>0)     fx_add160(p-W-1, rq,gq,bq);
        if (ix<W-1&&iy>0)   fx_add160(p-W+1, rq,gq,bq);
        if (ix>0&&iy<H-1)   fx_add160(p+W-1, rq,gq,bq);
        if (ix<W-1&&iy<H-1) fx_add160(p+W+1, rq,gq,bq);
    }
}

static void nebula_done(void) {}

const effect_t fx_nebula_real = {
    .name = "nebula", .mode = MODE_HIRES,
    .init = nebula_init, .frame = nebula_frame, .done = nebula_done,
};
