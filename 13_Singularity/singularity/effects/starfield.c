/* Scene 3 — Relativistic starfield warp (1:14–1:51, MODE_HIRES 320x240).
 *
 * A fly-through starfield accelerating to relativistic speed. Stars rush
 * radially out from the vanishing point, streaking longer as speed climbs
 * (β ramps up), and the headlight/Doppler effect blue-shifts and bright-
 * ens the on-rushing stars. Reads as a warp/hyperspace dive toward the
 * black hole.
 *
 * Classic 3D-projection warp (x,y,z; z decreases each frame; project x/z,
 * y/z) so there is real forward motion, with the relativistic colour +
 * streak length layered on top. No per-pixel transcendentals.
 */

#include "scene.h"
#include "vga.h"
#include "rgb565.h"
#include "fx_common.h"
#include "../scene_scratch.h"
#include <stdint.h>
#include <math.h>

#define SCENE_LEN_MS  37000
#define NS            STARFIELD_STARS
#define W             VGA_HIRES_W
#define H             VGA_HIRES_H
#define CX            160.0f
#define CY            120.0f
#define FOCAL         150.0f
#define ZNEAR         0.10f
#define ZFAR          6.0f
#define star (g_scratch.stars.s)

static uint32_t rng = 0x99aa55ffu;
static inline uint32_t xs(void){ rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
static inline float frnd(void){ return (xs() & 0xffffff)/(float)0x1000000; }

static void seed(int i, float z)
{
    /* Spread across a box wider than the frustum so streaks enter from the
     * edges; keep clear of the dead-centre so nothing sits on the vanishing
     * point. */
    float a = frnd() * 6.2831853f;
    float r = 0.15f + frnd() * 2.2f;
    star[i].x = cosf(a) * r;
    star[i].y = sinf(a) * r;
    star[i].z = z;
    star[i].mag = (uint8_t)(110 + (xs() & 145));
}

static void starfield_init(void)
{
    rng = 0x99aa55ffu;
    for (int i = 0; i < NS; i++) seed(i, ZNEAR + frnd() * (ZFAR - ZNEAR));
}

static inline void add_clip(uint16_t *fb, int x, int y, int r, int g, int b)
{
    if ((unsigned)x < W && (unsigned)y < H) fx_add160(&fb[y*W + x], r, g, b);
}

static void starfield_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;
    int A = fx_scene_alpha(t_into, SCENE_LEN_MS, 2000, 2500);
    float p = t_into / (float)SCENE_LEN_MS;
    float beta = p*p*(3.0f - 2.0f*p);                 /* eased 0→1 */
    /* Hard acceleration + beat-ish surges for ACTION. */
    float surge = 0.5f + 0.5f * sinf(t_into * 0.0042f);
    float speed = 0.03f + beta * 0.30f + 0.05f * beta * surge;
    float streakmul = 2.5f + beta * 6.0f;             /* exaggerated motion-blur */
    float dopp  = fx_clampf(beta * 1.1f, 0.0f, 1.0f); /* blueshift amount */
    /* Slow camera roll so the warp spirals — extra motion. */
    float roll  = t_into * 0.00018f;
    float rollp = (t_into > 16) ? (t_into - 16) * 0.00018f : roll;
    float cr = cosf(roll),  sr = sinf(roll);
    float crp = cosf(rollp), srp = sinf(rollp);

    uint16_t *fb = vga_hires_back_buffer();
    for (int i = 0; i < W*H; i++) fb[i] = 0;          /* black space */

    for (int i = 0; i < NS; i++) {
        float z = star[i].z;
        z -= speed;
        if (z < ZNEAR) { seed(i, ZFAR); continue; }
        star[i].z = z;
        float zp = z + speed * streakmul;             /* draw a long streak back */

        float bx = star[i].x, by = star[i].y;
        /* current (rolled) and previous (less-rolled, further) positions */
        float inv  = FOCAL / z;
        float invp = FOCAL / zp;
        float sx  = CX + (bx*cr  - by*sr ) * inv;
        float sy  = CY + (bx*sr  + by*cr ) * inv;
        float pxq = CX + (bx*crp - by*srp) * invp;
        float pyq = CY + (bx*srp + by*crp) * invp;
        if (sx < -60 || sx > W+60 || sy < -60 || sy > H+60) continue;

        /* Brightness ∝ closeness; closer = much brighter (sense of speed). */
        float br = star[i].mag * fx_clampf(1.2f / z, 0.0f, 4.0f);
        int lum = (int)(br); if (lum > 255) lum = 255;
        int r = (int)(lum * (1.0f - 0.55f*dopp));
        int g = (int)(lum * (1.0f - 0.18f*dopp));
        int b = lum;
        r=(r*A)>>8; g=(g*A)>>8; b=(b*A)>>8;

        /* Streak from previous to current projected position. */
        float dx = sx - pxq, dy = sy - pyq;
        float len = fabsf(dx) > fabsf(dy) ? fabsf(dx) : fabsf(dy);
        int steps = (int)len; if (steps < 1) steps = 1; if (steps > 40) steps = 40;
        float stepx = (sx - pxq) / steps, stepy = (sy - pyq) / steps;
        float fx = pxq, fy = pyq;
        for (int s = 0; s <= steps; s++) {
            int wgt = 40 + (s * 215) / steps;          /* tail dim → head bright */
            add_clip(fb, (int)fx, (int)fy, (r*wgt)>>8, (g*wgt)>>8, (b*wgt)>>8);
            fx += stepx; fy += stepy;
        }
        /* Bright head + halo. */
        int ix = (int)sx, iy = (int)sy;
        add_clip(fb, ix, iy, r, g, b);
        add_clip(fb, ix+1, iy, r>>1, g>>1, b>>1);
        add_clip(fb, ix-1, iy, r>>1, g>>1, b>>1);
        add_clip(fb, ix, iy+1, r>>1, g>>1, b>>1);
        add_clip(fb, ix, iy-1, r>>1, g>>1, b>>1);
    }
}

static void starfield_done(void) {}

const effect_t fx_starfield_real = {
    .name = "starfield", .mode = MODE_HIRES,
    .init = starfield_init, .frame = starfield_frame, .done = starfield_done,
};
