/* SUSTAIN — the one renderer.
 *
 * Front-to-back per-column ray walk with two y-buffers (floor and ceiling),
 * sampling world_height()/world_ceiling()/world_shade() rather than any
 * particular field. Sea, canyon and tunnel are all this one traversal under
 * different parameters, which is what makes most of the arc's morphs free.
 *
 * Occlusion falls out of the walk: marching outward and only painting outside
 * the running y-buffers means far geometry cannot overwrite near geometry, so
 * the surface stays solid through a morph even when the two sides of a
 * cross-family blend disagree wildly about where the ground is.
 *
 * Dithering: static ordered Bayer 4x4, applied at the RGB565 pack. The output
 * is 5 bits per channel, so smooth gradients band hard, and a band is a
 * visible contour that SLIDES across the frame as the camera moves — exactly
 * the kind of hard edge the eye latches onto in a shot that is trying not to
 * have any. Note the pattern is fixed in screen space and NOT animated per
 * frame: temporal dithering would add a large delta to every frame, lift
 * cut_detect.py's noise floor by an order of magnitude, and could hide a real
 * cut inside its own noise.
 */

#include "render.h"
#include "world.h"
#include "rgb565.h"
#include "motes.h"
#include "overlay.h"
#include "hot.h"
#include "fastmath.h"

#ifdef PICO_BUILD
#include "pico/time.h"
#define PROF_NOW() ((uint32_t)time_us_32())
#else
#define PROF_NOW() (0u)
#endif

static render_prof_t g_rp;

void render_prof_take(render_prof_t *out)
{
    *out = g_rp;
    g_rp.sky_us = g_rp.march_us = g_rp.post_us = g_rp.frames = 0;
}

#include <math.h>
#include <string.h>

#define W  320
#define H  240

#define FOCAL   260.0f

/* Start the walk close. At 0.60 the nearest 0.6 world units were never
 * sampled, so when the camera skimmed a surface the near geometry flipped in
 * and out of view between frames — cut_detect.py caught it repeatedly as
 * isolated 20+ delta spikes in the bottom quarter of the frame. Fixing the
 * cause beats raising camera clearance at each site where it happened. */
#define Z_NEAR    0.28f
#define Z_FAR   300.0f
#define DZ0       0.42f
#define DZ_GROW   1.058f
#define MAX_STEPS  1200

/* Never let one march step paint more than this many rows. On a steep wall the
 * projected height changes enormously between consecutive z samples, so a
 * fixed step quantises the SILHOUETTE into tall blocks — adjacent columns
 * resolve the edge at different steps and the result is the vertical "combing"
 * along the canyon walls. It is a geometric artefact, not a texture one, which
 * is why texture LOD never touched it. When a step would overshoot, we halve
 * the step and re-evaluate closer in: an adaptive march that spends its samples
 * where the surface is steep and strides past where it is flat. */
#define MAX_SPAN     SUSTAIN_MAX_SPAN

/* COLUMN STRIDE — the largest single lever on render cost.
 *
 * The ray walk's expense is proportional to the number of COLUMNS marched, so
 * marching every second column and duplicating into its neighbour halves the
 * dominant cost outright. Horizontal detail drops to 160 across a 320 buffer
 * that is already pixel-doubled to 640 on output.
 *
 * That is a real quality cost and I would not spend it on a desktop target.
 * On an RP2350 at 5 fps it is the difference between a demo and a slideshow,
 * and a demo that does not run at speed is not a demo. Set to 1 to render at
 * full horizontal resolution for video capture. */
/* QUALITY PROFILE.
 *
 * The device and the video capture are allowed to differ, and saying so is
 * more honest than pretending one setting suits both. On an RP2350 the demo
 * has to buy frame rate with detail; a video render has no such constraint and
 * should show the demo as designed.
 *
 * Measured on hardware at 300 MHz (tunnel section, the worst case):
 *     FAST     COL_STEP 2, MAX_SPAN 8  ->  ~10 fps
 *     QUALITY  COL_STEP 1, MAX_SPAN 4  ->  ~3 fps
 *
 * MAX_SPAN 12 was tried and reverted: it bought 20% but put visible blocky
 * banding and vertical streaking on the tunnel ceiling. Frame rate is not
 * worth an artefact the eye lands on immediately.
 *
 * Build the video with -DSUSTAIN_QUALITY=1. */
#ifndef SUSTAIN_QUALITY
#define SUSTAIN_QUALITY 0
#endif

#if SUSTAIN_QUALITY
#define COL_STEP          1
#define SUSTAIN_MAX_SPAN  4
#else
#define COL_STEP          2
#define SUSTAIN_MAX_SPAN  8
#endif

#define MIN_DZ       0.015f

/* Set to 0 to render undithered — used to re-measure cut_detect.py's noise
 * floor against a clean signal. */
#ifndef SUSTAIN_DITHER
#define SUSTAIN_DITHER 1
#endif

#if SUSTAIN_DITHER
/* Bayer 4x4, centred to +/-4 — half of the 8-unit step between adjacent
 * 5-bit levels, which is exactly the error the quantiser can make. */
static const int8_t BAYER[16] = {
    -4,  0, -3,  1,
     2, -2,  3, -1,
    -3,  1, -4,  0,
     3, -1,  2, -2,
};
#define DITHER_AT(x, y) BAYER[(((y) & 3) << 2) | ((x) & 3)]
#else
#define DITHER_AT(x, y) 0
#endif

/* Sample counter, host-only. The ray walk's cost is dominated by how many
 * field evaluations it performs, and that number is what decides whether this
 * can ever run on an RP2350 — so it gets measured, not estimated. */
#ifdef SUSTAIN_PROFILE
#include <stdio.h>
#include <time.h>
unsigned long g_prof_samples = 0, g_prof_frames = 0;
static clock_t g_prof_clk = 0, g_prof_t0 = 0;
#define RENDER_COUNT_SAMPLE() (g_prof_samples++)
#define RENDER_BEGIN()        (g_prof_t0 = clock())
#define RENDER_COUNT_FRAME()                                                  \
    do { g_prof_clk += clock() - g_prof_t0;                                   \
         if (++g_prof_frames % 3000 == 0)                                     \
             fprintf(stderr, "PROFILE %lu frames: %.2f ms/frame, "            \
                     "%.0f samples/frame\n", g_prof_frames,                   \
                     1000.0 * (double)g_prof_clk / CLOCKS_PER_SEC             \
                       / (double)g_prof_frames,                               \
                     (double)g_prof_samples / g_prof_frames);                 \
    } while (0)
#else
#define RENDER_COUNT_SAMPLE() ((void)0)
#define RENDER_COUNT_FRAME()  ((void)0)
#define RENDER_BEGIN()        ((void)0)
#endif

static inline uint16_t pack_dither(int r, int g, int b, int x, int y)
{
    const int d = DITHER_AT(x, y);
    return rgb565_pack(r + d, g + d, b + d);
}

/* SKY CACHE — the single biggest cost in the renderer before this existed.
 *
 * The sky is a panorama, so it depends on view azimuth as well as screen row,
 * and the first implementation rebuilt a full 240-row strip for EVERY column:
 * 76,800 bilinear panorama fetches per frame, more than the entire ray walk
 * (measured at ~51,000 samples). But azimuth changes by a fraction of a degree
 * between neighbouring columns, so almost all of that work was recomputing
 * nearly identical values.
 *
 * Instead the frame is divided into SKY_BUCKETS azimuth slices, each strip
 * built once, and columns interpolate between the two nearest. That is
 * 32 x 240 = 7,680 fetches per frame instead of 76,800 — a 10x reduction on
 * the dominant cost — and interpolating rather than snapping means no visible
 * banding at the slice boundaries. */
#define SKY_BUCKETS 16

static int16_t g_sky[SKY_BUCKETS + 1][H][3];   /* 0..255 fits easily */
static int g_sr[H], g_sg[H], g_sb[H];

/* Per-column silhouette rows, handed to the mote layer for occlusion. */
static int g_lo[W], g_hi[W];

/* Build all azimuth slices for this frame. The row->v mapping is identical for
 * every slice, so it is hoisted out. */
static void build_sky_cache(float u0, float u1, float horizon_y)
{
    for (int bkt = 0; bkt <= SKY_BUCKETS; bkt++) {
        const float u = u0 + (u1 - u0) * ((float)bkt / (float)SKY_BUCKETS);
        for (int y = 0; y < H; y++) {
            float v = (horizon_y - (float)y) / (float)H;
            if (v < 0.0f) v = 0.0f;
            if (v > 1.0f) v = 1.0f;
            int rr, gg, bb;
            world_sky(u, v, &rr, &gg, &bb);
            g_sky[bkt][y][0] = (int16_t)rr;
            g_sky[bkt][y][1] = (int16_t)gg;
            g_sky[bkt][y][2] = (int16_t)bb;
        }
    }
}

/* Fill the working strip for one column by blending the two nearest slices. */
static void sky_for_column(float frac)
{
    float f = frac * (float)SKY_BUCKETS;
    int   b = (int)f;
    if (b < 0) b = 0;
    if (b >= SKY_BUCKETS) b = SKY_BUCKETS - 1;
    const int w = (int)((f - (float)b) * 256.0f);

    const int16_t (*a)[3] = g_sky[b];
    const int16_t (*c)[3] = g_sky[b + 1];
    for (int y = 0; y < H; y++) {
        g_sr[y] = a[y][0] + (((c[y][0] - a[y][0]) * w) >> 8);
        g_sg[y] = a[y][1] + (((c[y][1] - a[y][1]) * w) >> 8);
        g_sb[y] = a[y][2] + (((c[y][2] - a[y][2]) * w) >> 8);
    }
}

void SUSTAIN_HOT(render_world)(uint16_t *fb, const camera_t *cam, float t)
{
    RENDER_BEGIN();

    /* KEPT IN FLOAT DELIBERATELY. Rounding the horizon to an int makes the
     * WHOLE FRAME jump a pixel the instant pitch crosses an integer boundary.
     * cut_detect.py flagged exactly that as recurring ~1.0 delta spikes on the
     * first audit. In float, each column crosses its pixel boundary
     * independently and the step is dithered across the frame instead. */
    const float horizon_y = H * 0.5f + cam->pitch * FOCAL;

    const float fx = sinf(cam->yaw), fz = cosf(cam->yaw);
    const float rx = cosf(cam->yaw), rz = -sinf(cam->yaw);

    /* A cross-family morph evaluates both sides, so it costs ~2x for its
     * duration. Coarsen the walk while it holds — it is scheduled into fog and
     * bloom precisely so the loss is invisible (PLANNING.md §6.2). */
    const float grow = world_is_cross_family() ? DZ_GROW * 1.5f : DZ_GROW;

    /* Azimuth at the two frame edges, so the cache spans exactly the visible
     * arc. Unwrapped around the seam: a panorama wraps at 0/1 and snapping a
     * bucket across that seam would put a hard vertical edge in the sky. */
    const float sxL = (float)(0 - W / 2) * (1.0f / FOCAL);
    const float sxR = (float)(W - 1 - W / 2) * (1.0f / FOCAL);
    float uL = atan2f(fx + rx * sxL, fz + rz * sxL) * (0.5f / (float)M_PI);
    float uR = atan2f(fx + rx * sxR, fz + rz * sxR) * (0.5f / (float)M_PI);
    if (uR < uL) uR += 1.0f;
    const uint32_t t_sky0 = PROF_NOW();
    build_sky_cache(uL, uR, horizon_y);
    const uint32_t t_sky1 = PROF_NOW();

    for (int x = 0; x < W; x += COL_STEP) {
        const float sx   = (float)(x - W / 2) * (1.0f / FOCAL);
        const float dirx = fx + rx * sx;
        const float dirz = fz + rz * sx;

        sky_for_column((float)x / (float)(W - 1));

        int lo = H;      /* lowest unpainted row (floor grows up from here)  */
        int hi = -1;     /* highest painted row of ceiling (grows down)      */

        /* Colour of the previous march step, so a span can be shaded as a
         * GRADIENT between two samples rather than as one flat block.
         *
         * Without this, every span gets the single colour sampled at its own
         * step, and wherever the surface runs near-horizontal one step covers
         * many rows — producing the flat terraced steps visible on the chasm
         * floor. The geometry was never stepped; only the shading was. */
        int pr = 0, pg = 0, pb = 0, pfi = 0, have_prev = 0;
        int cr_ = 0, cg_ = 0, cb_ = 0, cfi = 0, have_cprev = 0;

        /* Previous step's floor/ceiling height, for measuring how fast the
         * surface climbs ALONG THE RAY. That rate is the footprint: a wall seen
         * near edge-on gains many units of height per unit of view distance, so
         * one screen pixel straddles a huge patch of texture. This is the
         * quantity plain distance-based LOD cannot see, and it is what produced
         * the vertical combing on the canyon walls. */
        float ph = 0.0f, pch = 0.0f;
        float phz = 0.0f, pchz = 0.0f;     /* distance of those samples */
        int   have_ph = 0, have_pch = 0;

        float z  = Z_NEAR;
        float dz = DZ0;

        for (int step = 0; step < MAX_STEPS && z < Z_FAR && lo > hi + 1; step++) {
            RENDER_COUNT_SAMPLE();
            const float px = cam->x + dirx * z;
            const float pz = cam->z + dirz * z;

            float fg = z * (1.0f / Z_FAR);
            if (fg > 1.0f) fg = 1.0f;
            fg = fg * fg * (3.0f - 2.0f * fg);
            const int fi = (int)(fg * 256.0f);

            /* ---- floor ---- */
            const float h = world_height(px, pz);

            /* Measure against the previous sample's actual DISTANCE, not the
             * current step size. The adaptive march halves dz and re-evaluates,
             * so dz is not the gap between the two samples being compared —
             * and an earlier version reset the baseline on every refinement,
             * which switched the grazing LOD off precisely on the steep faces
             * that need it most. That was why wall striping survived every
             * attempt to fix it. */
            float foot = 1.0f;
            if (have_ph) {
                const float gap = z - phz;
                if (gap > 1.0e-5f) {
                    const float dhdz = (h - ph) / gap;
                    foot = sqrtf(1.0f + dhdz * dhdz);  /* 1 head-on, big grazing */
                }
            }
            /* The along-ray gradient misses the OTHER grazing case: a nearly
             * FLAT surface seen from low altitude. There dh/dz is ~0, so the
             * measure above says "head-on" and applies no blur — yet a low
             * camera looking out to sea packs enormous distance into each
             * screen row, which is what streaked the water horizontally.
             *
             * For a plane at height H below the eye, z = H*FOCAL/(row-horizon),
             * so the world distance spanned by one row is z^2/(H*FOCAL). That
             * is the footprint, and it grows with the SQUARE of distance. */
            {
                float Hh = cam->y - h;
                if (Hh < 0.35f) Hh = 0.35f;
                const float graze = z * z / (Hh * FOCAL);
                if (graze > foot) foot = graze;
            }
            if (foot > 64.0f) foot = 64.0f;
            ph = h; phz = z; have_ph = 1;
            int sy = ifloor(horizon_y - (h - cam->y) * FOCAL / z + 0.5f);
            if (sy < lo) {
                /* Refine rather than paint a tall block. */
                if (lo - sy > MAX_SPAN && dz > MIN_DZ) {
                    z  -= dz * 0.5f;
                    dz *= 0.5f;
                    continue;             /* baseline survives; see foot above */
                }
                if (sy <= hi) sy = hi + 1;
                int r, g, b;
                world_shade(px, pz, h, 1, z, foot, &r, &g, &b);

                /* Shade the span as a gradient from this sample (at its top,
                 * the far end) to the previous sample (at its bottom, the near
                 * end). Fog is interpolated too, since the two ends are at
                 * different distances. */
                const int span = lo - sy;
                for (int y = sy; y < lo; y++) {
                    int rr = r, gg = g, bb = b, ff = fi;
                    if (have_prev && span > 1) {
                        const int u = ((y - sy) * 256) / (span - 1);
                        rr = r  + (((pr  - r)  * u) >> 8);
                        gg = g  + (((pg  - g)  * u) >> 8);
                        bb = b  + (((pb  - b)  * u) >> 8);
                        ff = fi + (((pfi - fi) * u) >> 8);
                    }
                    fb[y * W + x] = pack_dither(
                        rr + (((g_sr[y] - rr) * ff) >> 8),
                        gg + (((g_sg[y] - gg) * ff) >> 8),
                        bb + (((g_sb[y] - bb) * ff) >> 8), x, y);
                }
                lo = sy;
                pr = r; pg = g; pb = b; pfi = fi; have_prev = 1;
            }

            /* ---- ceiling ---- */
            const float ch = world_ceiling(px, pz, h);
            if (ch < WORLD_NO_CEILING) {
                float cfoot = 1.0f;
                if (have_pch) {
                    const float gap = z - pchz;
                    if (gap > 1.0e-5f) {
                        const float dcdz = (ch - pch) / gap;
                        cfoot = sqrtf(1.0f + dcdz * dcdz);
                        if (cfoot > 64.0f) cfoot = 64.0f;
                    }
                }
                pch = ch; pchz = z; have_pch = 1;
                int cy = ifloor(horizon_y - (ch - cam->y) * FOCAL / z + 0.5f);
                if (cy > hi) {
                    if (cy >= lo) cy = lo - 1;
                    int r, g, b;
                    world_shade(px, pz, ch, 0, z, cfoot, &r, &g, &b);

                    /* Same gradient treatment as the floor — the ceiling
                     * terraces identically when it runs near-horizontal. */
                    const int y0 = hi + 1, span = cy - hi;
                    for (int y = y0; y <= cy; y++) {
                        if (y < 0) continue;
                        int rr = r, gg = g, bb = b, ff = fi;
                        if (have_cprev && span > 1) {
                            const int u = ((cy - y) * 256) / (span - 1);
                            rr = r  + (((cr_ - r)  * u) >> 8);
                            gg = g  + (((cg_ - g)  * u) >> 8);
                            bb = b  + (((cb_ - b)  * u) >> 8);
                            ff = fi + (((cfi - fi) * u) >> 8);
                        }
                        fb[y * W + x] = pack_dither(
                            rr + (((g_sr[y] - rr) * ff) >> 8),
                            gg + (((g_sg[y] - gg) * ff) >> 8),
                            bb + (((g_sb[y] - bb) * ff) >> 8), x, y);
                    }
                    hi = cy;
                    cr_ = r; cg_ = g; cb_ = b; cfi = fi; have_cprev = 1;
                }
            }

            z  += dz;
            dz *= grow;
        }

        /* Whatever neither surface reached is sky. */
        for (int y = hi + 1; y < lo; y++)
            fb[y * W + x] = pack_dither(g_sr[y], g_sg[y], g_sb[y], x, y);

        g_lo[x] = lo; g_hi[x] = hi;

#if COL_STEP > 1
        /* Duplicate this column into the ones we skipped. */
        for (int d = 1; d < COL_STEP && x + d < W; d++) {
            for (int y = 0; y < H; y++)
                fb[y * W + x + d] = fb[y * W + x];
            g_lo[x + d] = lo; g_hi[x + d] = hi;
        }
#endif
    }

    const uint32_t t_march1 = PROF_NOW();

    /* Motes last, composited over the finished frame. They are drawn only in
     * the open gap each column left, so geometry occludes them without needing
     * a depth buffer. */
    motes_draw(fb, cam, t, g_lo, g_hi, horizon_y,
               world_mote_density(), world_mote_warm());

    /* Title, credits and the collapse, last of all. */
    overlay_draw(fb, t);

    {
        const uint32_t t_end = PROF_NOW();
        g_rp.sky_us   += t_sky1   - t_sky0;
        g_rp.march_us += t_march1 - t_sky1;
        g_rp.post_us  += t_end    - t_march1;
        g_rp.frames++;
    }
    RENDER_COUNT_FRAME();
}
