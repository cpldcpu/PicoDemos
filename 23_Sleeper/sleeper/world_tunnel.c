/* world_tunnel.c -- the tunnel, and the kaleidoscope that reuses its grid.
 *
 * HELION's polar tunnel with Phase's three corrections:
 *
 *   - no spin and no sine wobble. HELION's tube rolled and breathed because
 *     it was a sun's corona; a tunnel is bored straight and the train does
 *     not roll in it. What is left is the rings coming at the camera, which
 *     is the whole point of the shot;
 *   - the lamps light the concrete near them. A whole-palette flash flattens
 *     the tube and, worse, is a frame-wide luminance step that reads to a
 *     discontinuity detector exactly like a cut. Each lamp is a light at its
 *     own projected position, on the same projection the ahead world uses,
 *     so the two cut together;
 *   - the centre singularity is covered, always: by the exit disc when
 *     CUT_EXIT is set and by the bore's dark plug otherwise. The seam where
 *     the angle wraps is handled by unwrap(), kept from HELION.
 *
 * The polar grid is centred on the production's match point rather than the
 * screen centre, which gives the tunnel the off-centre vanishing point the
 * rest of the film has and lets the dream fold its kaleidoscope around the
 * same hub without a second table. -- Overscan
 */
#include "render.h"
#include "accelerator.h"
#include <math.h>

typedef struct { int32_t u, v; } uv_t;
static uv_t g_polar[31][41];

void tunnel_grid_init(void)
{
    for (int y = 0; y <= 30; y++)
        for (int x = 0; x <= 40; x++) {
            const float dx = (float)(x * 8) - MATCH_X, dy = (float)(y * 8) - MATCH_Y;
            const float r = sqrtf(dx * dx + dy * dy);
            g_polar[y][x].u = (int32_t)((atan2f(dy, dx) * (128.f / 6.28318530718f)) * 65536.f);
            g_polar[y][x].v = (int32_t)(fminf(128.f, 1600.f / (r + 8.f)) * 65536.f);
        }
}

static int unwrap(int delta)
{
    const int turn = 128 * 65536;
    if (delta > turn / 2) delta -= turn;
    if (delta < -turn / 2) delta += turn;
    return delta;
}

/* One pass over the 40x30 cell grid, eight pixels a span. `fold` folds the
 * angle into a sixth and mirrors it, which is the only difference between
 * the tunnel and the kaleidoscope. */
static void HOT(polar_pass)(const uint8_t *tex, const uint16_t pal16[16][256], float vscale, float travel, int shade_bias)
{
    texture_config(TEX_BITS);
    const int32_t tv = (int32_t)(travel * 65536.f);
    const int32_t vs = (int32_t)(vscale * 256.f);
    for (int y = 0; y < HEIGHT; y++) {
        const int gy = y >> 3, f = y & 7;
        for (int gx = 0; gx < 40; gx++) {
            const uv_t a = g_polar[gy][gx], b = g_polar[gy + 1][gx];
            const uv_t c = g_polar[gy][gx + 1], d = g_polar[gy + 1][gx + 1];
            int32_t u = a.u + unwrap(b.u - a.u) * f / 8, v = a.v + (b.v - a.v) * f / 8;
            int32_t u2 = c.u + unwrap(d.u - c.u) * f / 8, v2 = c.v + (d.v - c.v) * f / 8;
            int32_t du = unwrap(u2 - u) / 8;
            int32_t dv = (int32_t)(((int64_t)(v2 - v) * vs) >> 8) / 8;
            const int32_t v0 = (int32_t)(((int64_t)v * vs) >> 8) + tv;
            texture_span(u * 3, v0, du * 3, dv);
            /* Shade by the polar depth itself, not by a Manhattan distance
             * from the centre: HELION's metric drew a diamond across the
             * tube, which on a bored tunnel reads as a dartboard. v is the
             * depth in texels and it is already in hand. */
            const int shade = clampi(15 - (int)((uint32_t)v >> 19) + shade_bias, 1, 15);
            uint16_t *out = g_fb + (unsigned)y * WIDTH + gx * 8;
            const uint16_t *pal = pal16[shade];
            for (int x = 0; x < 8; x++) *out++ = pal[tex[texture_pop()]];
        }
        g_stats.spans++;
    }
}

void HOT(world_tunnel_draw)(void)
{
    /* variant 1 is the rings tightening: the same tube, more of it a second */
    const float vscale = F.variant ? 1.7f : 1.0f;
    polar_pass(tex_ring, tex_pal, vscale, F.metres * 4.f, 0);

    /* the lamps: one a bar, each lighting the concrete near it, on the same
     * projection the ahead world uses so a cut between them lands */
    const float travel = F.metres;
    const float post = METRES_PER_BEAT * 2.f;   /* a lamp every two beats  */
    const int k0 = (int)floorf((travel + 2.f) / post);
    for (int k = k0; k < k0 + 10; k++) {
        const float z = (float)k * post - travel;
        if (z < 2.f || z > 120.f) continue;
        const float r = 2.6f * 300.f / z;                  /* the bore radius */
        const float lx = MATCH_X, ly = MATCH_Y - r * 0.82f;
        const float glow = 700.f / z;
        light_t L = { lx, ly, 0.f, 0.f, glow < 4.f ? 4.f : glow > 52.f ? 52.f : glow,
                      C_FLUO, C_FLUO_H, 20, (int16_t)(z < 26.f ? 3 : 1), 0 };
        light_draw(&L);
    }

    if (F.flags & CUT_EXIT) {
        /* The exit, and the one shot in the film that went over budget on the
         * board. It was a disc with a halo of the same radius over it, and a
         * halo is O(r^2) with an integer divide a pixel above radius 63: at
         * the end of the shot that was a 500-pixel halo over the whole page
         * and a 20.8 ms frame against a 15.0 ms budget.
         *
         * The glow is a property of the disc's *edge*, not of its area, so it
         * is drawn as a ring of small halos around the rim -- the same trick
         * as the moon's path on the water. Cost is now proportional to the
         * circumference and capped, the picture is the same white disc with
         * a soft edge, and nothing about the shot was shortened or slowed.
         * -- Overscan */
        const float u = (float)F.since / (float)(2u * BAR_SAMPLES);
        const float k = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
        const float r = 3.f + k * k * 252.f;      /* 252 covers the far corner */
        int n = 6 + (int)(r * 0.16f); if (n > 26) n = 26;
        for (int i = 0; i < n; i++) {
            const float a = (float)i * (6.28318530718f / (float)n);
            halo(MATCH_X + fcos(a) * r, MATCH_Y + fsin(a) * r * 0.88f, 20.f, RGB(0xE0, 0xE8, 0xFF), 13);
        }
        disc(MATCH_X, MATCH_Y, r, RGB(0xF8, 0xFC, 0xFF));
    } else {
        /* the centre is a singularity in the polar map; it is never shown */
        disc(MATCH_X, MATCH_Y, 4.f, RGB(0x02, 0x02, 0x04));
        halo(MATCH_X, MATCH_Y, 9.f, RGB(0x00, 0x00, 0x04), 20);
    }
}

/* The kaleidoscope no longer folds this grid: round two rebuilt it out of
 * the passing train's own window lights (world_dream.c, note 11), which
 * keeps Phase's "only the journey's shapes" on the merits rather than on the
 * technicality that ballast is a shape. The grid stays where it belongs. */
