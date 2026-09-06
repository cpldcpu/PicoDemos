/* SUSTAIN — the terrain family.
 *
 * Sea, canyon and tunnel are ONE family. They are not three effects that
 * dissolve into each other; they are three parameter sets of the same
 * function, so morphing between them lerps eleven floats and evaluates the
 * surface once. That is why 6 of the arc's 11 morphs are nearly free
 * (PLANNING.md §3.2), and why the intermediate states look like a real place
 * rather than an average of two places.
 *
 * The three shapes come from three parameters doing the work:
 *
 *   P_CHANNEL  walls rising either side of the camera's path.
 *              0 = open sea, high = slot canyon.
 *   P_GAP      how far above the floor the ceiling sits. Enormous = open sky;
 *              as it descends the canyon walls lean in and close overhead,
 *              and the world becomes a tunnel without changing renderer.
 *   P_RELIEF   which height map is driving it: relief_soft (ocean swell) to
 *              relief_hard (eroded rock strata).
 *
 * Enclosure being a parameter rather than a different renderer is the whole
 * trick. Flying into a tunnel is an intra-family morph.
 */

#include "../world.h"
#include "../tex.h"
#include "sky_common.h"
#include "../mip.h"
#include "../hot.h"
#include "../fastmath.h"
#include "assets.h"

#include <math.h>

/* ---- parameter slots ---------------------------------------------------- */
enum {
    P_RELIEF = 0,   /* 0 = relief_soft, 1 = relief_hard                      */
    P_RSCALE,       /* relief texels per world unit                          */
    P_AMP,          /* vertical amplitude of the relief                      */
    P_CHANNEL,      /* wall steepness away from the path (0 = open)          */
    P_CHAN_W,       /* half-width of the clear channel                       */
    P_GAP,          /* ceiling height above the floor (>= GAP_OPEN = sky)    */
    P_MAT,          /* 0 = surface_cold, 1 = surface_hot                     */
    P_MSCALE,       /* material texels per world unit                        */
    P_BRIGHT,       /* material brightness                                   */
    P_SKY,          /* 0 = cold sky, 1 = hot sky                             */
    P_FLOW,         /* how fast the surface animates                         */
    P_GLOW,         /* camera-attached light (enclosed fields need their own) */
    P_WSCALE,       /* wall-texture texels per world unit                     */
    P_WSHAPE,       /* cross-section: 0 = V, 1 = U, 2 = box/slot              */
    P_ASYM,         /* -1..1, leans the profile: one wall steeper than other  */
    P_MEANDER,      /* lateral amplitude — the channel snakes instead of      */
    P_MEANDER_K,    /*   running straight; K is its spatial frequency         */
};

/* Reference distance at which every cross-section exponent gives the same
 * height, so changing P_WSHAPE alters the PROFILE without also changing how
 * tall the walls are. Without it, morphing V -> box would read as the walls
 * growing rather than as the shape changing. */
#define WALL_REF 10.0f

/* Above this the ceiling is off-frame at any sane pitch, so we skip drawing
 * it. The threshold is high enough that the roof is already outside the
 * viewport when it engages — no pop, and the open-sky case costs nothing. */
#define GAP_OPEN 250.0f

/* Width of the cold->hot dissolve front, in units of relief. Larger = a
 * softer, wider front; 0 would be a hard threshold that crawls visibly. */
#define MAT_SOFT 1.6f

#define MS_MASK (ASSET_SURFACE_COLD_W - 1)

/* THE RELIEF MAPS LIVE IN SRAM.
 *
 * These are the single hottest reads in the demo: every ray step samples one
 * (or during a relief morph, both) bilinearly, which is four byte loads each.
 * Left in flash they are reached through the XIP cache, and at 64 KB apiece
 * the working set thrashes it — which is the likeliest reason the first
 * firmware measured ~1,500 cycles per sample when the arithmetic is worth a
 * fraction of that.
 *
 * Copied at half resolution: 128x128 instead of 256x256, 16 KB each instead
 * of 64 KB. The relief is deliberately low-frequency (PROMPTS.md asks for
 * "large soft rounded features, about 5 across"), so halving it costs almost
 * nothing visually and buys a 4x smaller working set on top of moving it out
 * of flash entirely. */
#define RELIEF_SRAM_W 128
#define RS_MASK (RELIEF_SRAM_W - 1)
static uint8_t g_relief_soft[RELIEF_SRAM_W * RELIEF_SRAM_W];
static uint8_t g_relief_hard[RELIEF_SRAM_W * RELIEF_SRAM_W];

static void copy_relief(uint8_t *dst, const uint8_t *src, int sw)
{
    const int step = sw / RELIEF_SRAM_W;
    for (int y = 0; y < RELIEF_SRAM_W; y++)
        for (int x = 0; x < RELIEF_SRAM_W; x++) {
            /* Box-average the source block rather than point-sampling: a
             * height map that is point-decimated gains high-frequency steps
             * exactly where we are trying to remove them. */
            int acc = 0;
            for (int j = 0; j < step; j++)
                for (int i = 0; i < step; i++)
                    acc += src[(y * step + j) * sw + (x * step + i)];
            dst[y * RELIEF_SRAM_W + x] = (uint8_t)(acc / (step * step));
        }
}

/* Relief, 0..1, blended between the two maps by P_RELIEF. Both maps are
 * sampled bilinearly — a stair-stepped height field cannot morph smoothly. */
static float SUSTAIN_HOT(relief)(const float *p, float x, float z)
{
    /* Half the source resolution now lives in SRAM, so the world-space scale
     * halves with it and the terrain is unchanged. */
    const float s = p[P_RSCALE] * 0.5f;
    const float u = x * s, v = z * s;

    const float soft = tex_gray_bilin(g_relief_soft, RS_MASK, RS_MASK,
                                      RELIEF_SRAM_W, u, v);
    const float m = p[P_RELIEF];
    if (m <= 0.001f) return soft * (1.0f / 255.0f);

    const float hard = tex_gray_bilin(g_relief_hard, RS_MASK, RS_MASK,
                                      RELIEF_SRAM_W, u, v);
    return (soft + (hard - soft) * m) * (1.0f / 255.0f);
}

/* Walls rising either side of the path. Squared beyond the channel edge so
 * the foot of the wall is soft and the top steepens — a linear ramp reads as
 * a folded sheet, this reads as erosion. */
/* Sine without sinf(), for the meander.
 *
 * walls() runs once per ray sample — tens of thousands of times per frame —
 * and libm's sinf() does range reduction and a polynomial every call. The
 * meander only needs a smooth periodic wobble, not accuracy, so this uses the
 * classic parabolic approximation: two parabolas stitched at the zero
 * crossings, accurate to about 5% of amplitude, which is invisible in a
 * quantity whose whole job is "bend the canyon a bit".
 *
 * Kept in float rather than fixed point so the host and device paths stay
 * bit-comparable and the audit remains a meaningful check. */
static inline float fast_sin_turns(float turns)
{
    turns -= ffloor(turns);                  /* 0..1 = one full period */
    const float xx = turns * 2.0f - 1.0f;    /* -1..1 */
    const float a  = xx < 0.0f ? -xx : xx;
    /* 4x(1-|x|) is a parabola matching sin's shape over a half period. */
    const float par = 4.0f * xx * (1.0f - a);
    /* One Newton-ish refinement pass tightens it from ~5% to ~1%. */
    return par * (1.0f + 0.225f * (a - 1.0f) * -1.0f) * 0.9f;
}

static float walls(const float *p, float x, float z)
{
    if (p[P_CHANNEL] <= 0.0f) return 0.0f;

    /* The channel need not run straight. Meandering its centre with z makes
     * the canyon snake, so the view ahead opens and closes as the camera is
     * carried around each bend — variety that costs two parameters. */
    const float cx = p[P_MEANDER] *
                     fast_sin_turns(z * p[P_MEANDER_K] * 0.159154943f);
    const float dx = x - cx;

    const float d = fabsf(dx) - p[P_CHAN_W];
    if (d <= 0.0f) return 0.0f;

    /* Cross-section profile. Blending powers of the normalised distance gives
     * a continuum from a V (linear) through a U (quadratic) to a near-vertical
     * slot (quartic) — and because it is a blend rather than a powf(), the
     * shape morphs continuously and costs three multiplies. */
    const float u  = d * (1.0f / WALL_REF);
    const float u2 = u * u;
    const float u4 = u2 * u2;
    const float sh = p[P_WSHAPE];
    float prof;
    if (sh <= 1.0f) prof = u  + (u2 - u ) * sh;
    else            prof = u2 + (u4 - u2) * (sh - 1.0f);

    /* Asymmetry: lean the profile so one wall is steeper than the other. A
     * perfectly symmetric channel is what made every shot read as the same
     * U-shaped trough. */
    float k = p[P_CHANNEL];
    k *= 1.0f + (dx > 0.0f ? p[P_ASYM] : -p[P_ASYM]);
    if (k < 0.0f) k = 0.0f;

    return k * prof * WALL_REF * 0.2f;
}

static float SUSTAIN_HOT(terrain_h)(const float *p, float x, float z, float t)
{
    /* The surface drifts rather than sitting still: sampling the relief at a
     * slowly moving offset makes the sea live without a second texture. */
    const float dz = z + t * p[P_FLOW];
    return (relief(p, x, dz) - 0.5f) * p[P_AMP] + walls(p, x, z);
}

static float SUSTAIN_HOT(terrain_ceil)(const float *p, float x, float z, float t,
                                       float floor_h)
{
    const float gap = p[P_GAP];
    if (gap >= GAP_OPEN) return WORLD_NO_CEILING;

    /* THE ROOF IS DERIVED FROM THE FLOOR, NOT SAMPLED AGAIN.
     *
     * This used to take its own relief sample at offset coordinates — a second
     * bilinear tap on every ray step through every enclosed section, which is
     * why on-device profiling showed the tunnel running 3.5x slower than the
     * open canyon.
     *
     * But the intent was always that the ceiling carry the same geology as the
     * ground it closed over, and that is exactly what inverting the floor's
     * own relief gives. Subtracting the wall term first isolates the relief
     * from the channel profile, so the roof mirrors the ground's bumps while
     * the walls still rise on both sides. Same picture, one tap instead of
     * two. */
    const float w = walls(p, x, z);
    const float r = floor_h - w;                 /* relief-driven part alone */
    (void)t;
    return floor_h + gap - r * 0.6f;
}

/* ---- distance LOD ------------------------------------------------------- *
 *
 * Without this the ray walk samples the material far finer than one texel per
 * screen pixel on distant and steeply foreshortened surfaces, and the result
 * shimmers. That shimmer is not merely ugly: it is NOISE IN THE CHANNEL THE
 * AUDIT LISTENS TO. Retuning the texture scales up drove cut_detect.py's
 * median frame delta from 0.47 to 1.94, which quadrupled the size a real
 * discontinuity would have to reach before being flagged. Aliasing erodes the
 * referee.
 *
 * The proper fix is a mip chain. This is the cheap continuous approximation:
 * fade the sample toward the texture's own mean colour once texel density
 * passes one per pixel — i.e. exactly at Nyquist, where the detail has stopped
 * carrying information and started carrying noise. It costs one lerp, needs no
 * extra memory, and unlike discrete mip levels it cannot band or pop, which
 * matters more here than sharpness: a moving LOD boundary is a moving edge,
 * and this demo may not have those.
 */
static int g_mip_ready = 0;

static void ensure_mips(void)
{
    if (g_mip_ready) return;
    mip_build_all(asset_surface_cold_data, asset_surface_hot_data,
                  asset_wall_cold_data, asset_wall_hot_data,
                  ASSET_SURFACE_COLD_W);
    copy_relief(g_relief_soft, asset_relief_soft_data, ASSET_RELIEF_SOFT_W);
    copy_relief(g_relief_hard, asset_relief_hard_data, ASSET_RELIEF_HARD_W);
    g_mip_ready = 1;
}

/* Cold->hot mix at a point, keyed on the local relief. See the long note at
 * the call site in terrain_shade for why this is a height-keyed dissolve
 * rather than a uniform lerp. */
static float mat_edge(const float *p, float x, float dz)
{
    const float m = p[P_MAT];
    if (m <= 0.001f) return 0.0f;
    float edge = m * (1.0f + MAT_SOFT) - MAT_SOFT * relief(p, x, dz);
    if (edge <= 0.0f) return 0.0f;
    if (edge >= 1.0f) return 1.0f;
    return edge * edge * (3.0f - 2.0f * edge);
}

static void SUSTAIN_HOT(terrain_shade)(const float *p, float x, float z, float h, float t,
                          int up, float dist, float foot, int *r, int *g, int *b)
{
    const float s  = p[P_MSCALE];
    const float dz = z + t * p[P_FLOW];

    /* HEIGHT-KEYED DISSOLVE, not a uniform lerp.
     *
     * The cold and hot surface textures have uncorrelated structure
     * (tools/check_pair.py measures r = 0.009), so a uniform lerp shows BOTH
     * at half strength through the middle of the transition — the definition
     * of a crossfade, and the one thing this demo may not do.
     *
     * Keying the blend on the local relief fixes it without touching the art:
     * the hot material appears first in the low channels and creeps up the
     * crests as m rises, so any given point flips over quickly while the FRONT
     * moves slowly. The two textures are never both half-visible in the same
     * place. It is also the more physical reading — heat pooling in the seams
     * and rising — and it gives the transformation a direction, which a
     * uniform fade never has. */
    const float edge = mat_edge(p, x, dz);

    ensure_mips();

    /* Texels per screen pixel: the footprint the renderer measured, scaled by
     * how many texels a world unit spans. This picks the mip level. */
    const float tpp_m = s * foot * dist * (1.0f / 260.0f);

    /* ---- top-down projection (floors, sea, gentle slopes) ---- */
    int cr, cg, cb;
    mip_sample(MIP_SURFACE_COLD, x * s, dz * s, tpp_m, &cr, &cg, &cb);
    if (edge > 0.001f) {
        int hr, hg, hb;
        mip_sample(MIP_SURFACE_HOT, x * s, dz * s, tpp_m, &hr, &hg, &hb);
        cr += (int)((hr - cr) * edge);
        cg += (int)((hg - cg) * edge);
        cb += (int)((hb - cb) * edge);
    }

    /* ---- TRIPLANAR: swap in the wall projection on steep faces ----
     *
     * A top-down projection is correct for a sea and for a canyon floor, but
     * on a near-vertical face it stretches the texture into long vertical
     * streaks — the most visible defect in the build before this. So the
     * surface gradient decides the projection: flat ground keeps the top-down
     * sample, steep faces sample the wall pair keyed on HEIGHT, and the two
     * cross-fade over the intervening slopes.
     *
     * The gradient costs two extra height evaluations, but this runs once per
     * painted span rather than per pixel, so it is a per-column cost, not a
     * per-pixel one. */
    /* EARLY OUT ON FLATNESS.
     *
     * The gradient below costs two more field evaluations, and the wall
     * projection after it costs up to four mip lookups — all of it wasted on
     * ground that is not steep. `foot` is already in hand from the renderer:
     * it is how fast the surface climbs along the view ray, so a small value
     * means the surface is close to face-on and no wall texture will be
     * blended in. Testing it first skips the whole branch across every flat
     * floor and open sea in the demo.
     *
     * On-device profiling showed shading at 57% of the frame, and this is the
     * cheapest way to stop paying it where it buys nothing. */
    float wmix = 0.0f, slope = 0.0f, hx = 0.0f, hz = 0.0f;
    if (foot > 1.25f) {
        const float e = 0.7f;
        hx = terrain_h(p, x + e, z, t) - h;
        hz = terrain_h(p, x, z + e, t) - h;
        slope = sqrtf(hx * hx + hz * hz) * (1.0f / e);

        /* Smoothstep 0.55 -> 1.5 in gradient terms (about 29 to 56 degrees). A
         * hard threshold would put a visible contour line along the terrain
         * where the projection switches — a moving edge, which is exactly what
         * this demo must not produce. */
        wmix = (slope - 0.55f) * (1.0f / 0.95f);
        if (wmix < 0.0f) wmix = 0.0f;
        if (wmix > 1.0f) wmix = 1.0f;
        wmix = wmix * wmix * (3.0f - 2.0f * wmix);
    }

    if (wmix > 0.002f) {
        /* Sample along whichever horizontal axis runs ALONG the wall, with
         * height as the vertical coordinate — that is what puts the strata
         * bands level instead of smeared. */
        const float ws = p[P_WSCALE];

        /* TRUE TRIPLANAR: BLEND the two projections, never choose between
         * them.
         *
         * This line used to be
         *     wu = (fabsf(hx) > fabsf(hz)) ? z : x;
         * — a hard binary switch on which gradient component was larger. On
         * any curved wall the two components are comparable over wide areas,
         * so adjacent spans flipped between sampling along X and along Z: two
         * unrelated texture lookups alternating across the surface. That was
         * the vertical "combing" artefact, and because it is a branch rather
         * than a sampling-rate problem it appeared at ALL distances, including
         * right in front of the camera. Distance LOD could never have fixed it.
         *
         * Weighting by the gradient components instead makes the projection
         * continuous: where the wall faces X it is sampled along Z, where it
         * faces Z it is sampled along X, and in between it is a smooth mix of
         * both. No branch, so nothing to alternate. */
        const float ax = fabsf(hx), az = fabsf(hz);
        float wz = ax / (ax + az + 1.0e-6f);   /* faces X -> runs along Z */
        wz = wz * wz * (3.0f - 2.0f * wz);     /* bias toward the dominant axis */

        /* Use the MEASURED footprint, not a slope guess. `foot` comes from the
         * renderer as the height change along the ray between march steps, so
         * it captures grazing incidence directly — a wall seen near edge-on
         * has a huge footprint even a metre away, which is precisely the case
         * distance-based LOD is blind to and which produced the vertical
         * combing on the canyon walls. */
        /* Wall LOD combines BOTH footprint measures, and needs to be far more
         * aggressive than the floor's.
         *
         * `foot` is the along-ray rate: how fast the surface climbs between
         * successive march samples in THIS column. But the striping is a
         * COLUMN-TO-COLUMN artefact — at grazing incidence, neighbouring
         * screen columns strike the wall at world positions far apart, so the
         * horizontal footprint is large even where the along-ray one is not.
         * `slope` (the terrain gradient) captures that second axis. Disabling
         * the wall texture entirely proved the striping is textural, and this
         * is the amount of blur it actually takes to kill it while keeping
         * some material on the near walls. */
        /* Walls need the footprint scaled by slope as well: the striping is a
         * COLUMN-TO-COLUMN artefact, and at grazing incidence neighbouring
         * columns strike the wall far apart. With a real mip chain this only
         * selects a coarser LEVEL — it no longer erases the material. */
        const float tpp_w = ws * foot * (1.0f + slope * 1.4f) * dist * (1.0f / 260.0f);
        const float hv    = h * ws;

        /* When one axis clearly dominates, the other projection contributes
         * less than a quantisation step — so sample once instead of twice.
         * The blend is still continuous because the test sits outside the
         * band where both contribute. */
        int wr, wg, wb;
        if (wz > 0.97f) {
            mip_sample(MIP_WALL_COLD, z * ws, hv, tpp_w, &wr, &wg, &wb);
        } else if (wz < 0.03f) {
            mip_sample(MIP_WALL_COLD, x * ws, hv, tpp_w, &wr, &wg, &wb);
        } else {
            int ar_, ag_, ab_, br_, bg_, bb_;
            mip_sample(MIP_WALL_COLD, z * ws, hv, tpp_w, &ar_, &ag_, &ab_);
            mip_sample(MIP_WALL_COLD, x * ws, hv, tpp_w, &br_, &bg_, &bb_);
            wr = br_ + (int)((ar_ - br_) * wz);
            wg = bg_ + (int)((ag_ - bg_) * wz);
            wb = bb_ + (int)((ab_ - bb_) * wz);
        }

        if (edge > 0.001f) {
            int qr, qg, qb;
            if (wz > 0.97f) {
                mip_sample(MIP_WALL_HOT, z * ws, hv, tpp_w, &qr, &qg, &qb);
            } else if (wz < 0.03f) {
                mip_sample(MIP_WALL_HOT, x * ws, hv, tpp_w, &qr, &qg, &qb);
            } else {
                int ar_, ag_, ab_, br_, bg_, bb_;
                mip_sample(MIP_WALL_HOT, z * ws, hv, tpp_w, &ar_, &ag_, &ab_);
                mip_sample(MIP_WALL_HOT, x * ws, hv, tpp_w, &br_, &bg_, &bb_);
                qr = br_ + (int)((ar_ - br_) * wz);
                qg = bg_ + (int)((ag_ - bg_) * wz);
                qb = bb_ + (int)((ab_ - bb_) * wz);
            }
            wr += (int)((qr - wr) * edge);
            wg += (int)((qg - wg) * edge);
            wb += (int)((qb - wb) * edge);
        }

        cr += (int)((wr - cr) * wmix);
        cg += (int)((wg - cg) * wmix);
        cb += (int)((wb - cb) * wmix);
    }

    /* Height keys the lighting: crests catch the light, troughs sit dark.
     * Ceilings are lit at a fraction — they face away from the sky, and
     * without this a tunnel reads as a corridor with two floors. */
    float k = p[P_BRIGHT] * (0.62f + 0.5f * (h / (p[P_AMP] + 0.001f)));
    if (!up) k *= 0.55f;

    /* Camera-attached glow. An open field is lit by its sky; once the ceiling
     * closes there is no sky left to light anything and the tunnel renders
     * black — cut_detect.py's rule-2 check caught exactly that, 294 frames of
     * it. So an enclosed field carries its own light, falling off with view
     * distance, which also gives the tunnel the depth cue it needs to read as
     * a tube rather than a flat wall. */
    if (p[P_GLOW] > 0.001f) {
        /* Saturating rolloff, not 1/(1+d). A plain reciprocal runs away as
         * dist -> 0 and blows the near wall to flat white, which destroys the
         * surface detail exactly where the viewer is closest to it. This form
         * is bounded by (1 + GLOW) however close the geometry gets. */
        const float f = 1.0f - 1.0f / (1.0f + dist * 0.16f);
        k *= 1.0f + p[P_GLOW] * (1.0f - f);
    }

    if (k < 0.0f) k = 0.0f;

    *r = (int)(cr * k);
    *g = (int)(cg * k);
    *b = (int)(cb * k);
}

static void terrain_sky(const float *p, float u, float v, float t,
                        int *r, int *g, int *b)
{
    (void)t;
    /* Shared with every other family — see sky_common.h. Two families each
     * computing "the same" sky independently would crossfade during a
     * cross-family morph, dissolving the largest area of the frame. */
    sustain_sky(p[P_SKY], u, v, r, g, b);
}

const field_family_t fam_terrain = {
    .name  = "terrain",
    .h     = terrain_h,
    .ceil  = terrain_ceil,
    .shade = terrain_shade,
    .sky   = terrain_sky,
    .prepare = NULL,
};

/* ---- the three fields --------------------------------------------------- */
/* Same family, same code path. Only these numbers differ, which is why the
 * morphs between them are single-evaluation. */

/*                RELIEF RSCALE   AMP CHANNEL CHAN_W    GAP   MAT MSCALE BRIGHT   SKY  FLOW  GLOW WSCALE */
const field_t f_sea = {
    "sea", &fam_terrain,
    {   0.00f, 0.42f,  3.2f,  0.00f, 999.0f, 999.0f, 0.0f, 2.60f, 1.15f, 0.0f, 1.1f, 0.00f, 5.00f, 1.0f,  0.00f,  0.0f, 0.00f,
     [P_UNI_MOTES] = 0.62f, [P_UNI_WARM] = 0.00f }
};

const field_t f_canyon = {
    "canyon", &fam_terrain,
    {   1.00f, 0.50f,  9.0f,  3.20f,  13.0f, 999.0f, 0.0f, 2.30f, 1.05f, 0.0f, 1.6f, 0.35f, 5.60f, 0.15f, -0.35f,  6.0f, 0.012f,
     [P_UNI_MOTES] = 0.55f, [P_UNI_WARM] = 0.15f }
};

/* GAP is what closes the roof; GLOW is what makes the closed roof visible.
 * They have to move together — every enclosed parameter set needs its own
 * light, or the morph into it fades to black. */
const field_t f_tunnel = {
    "tunnel", &fam_terrain,
    {   1.00f, 0.58f, 11.0f,  4.50f,   8.0f,  17.0f, 0.3f, 2.10f, 0.78f, 0.2f, 2.4f, 0.80f, 6.00f, 1.00f,  0.30f,  5.0f, 0.017f,
     [P_UNI_MOTES] = 0.80f, [P_UNI_WARM] = 0.45f }
};

/* CLEARANCE RULE for any enclosed parameter set:
 *     GAP - 0.3*AMP - camera_y  must stay comfortably positive.
 * The ceiling carries its own relief of +/-0.3*AMP, and the camera rides a
 * SMOOTHED floor that can sit a couple of units off the local one. The first
 * version of this field had GAP 12, AMP 13 and a camera 7 up — about 1 unit of
 * margin — and clipped through the roof during the morph. cut_detect.py caught
 * it as 14 discontinuities.
 *
 * A second tunnel parameter set: tighter, faster, hotter. Exists so the long
 * enclosed stretch (61.6 -> 124.8 s, the track's sustained drive) is not one
 * unchanging place for a full minute. The morph between the two tunnels is
 * intra-family and therefore nearly free — which is the point of the family
 * design: variety costs parameters, not frame budget. */
const field_t f_tunnel_deep = {
    "tunnel_deep", &fam_terrain,
    {   1.00f, 0.66f, 11.5f,  5.00f,   7.4f,  19.0f, 0.55f, 2.00f, 0.72f, 0.35f, 3.2f, 0.85f, 6.40f, 2.00f, -0.15f,  7.5f, 0.029f,
     [P_UNI_MOTES] = 1.00f, [P_UNI_WARM] = 0.80f }
};

/* ---- intermediate parameter sets ---------------------------------------- *
 *
 * The critic's note was that too little happens per minute: four field states
 * across three minutes leaves long stretches where the world is merely being
 * flown through rather than changing. These exist to fix that, and they cost
 * almost nothing — an intra-family morph is a parameter lerp and one field
 * evaluation, so the world can be in continuous gentle motion between the big
 * structural morphs instead of settling and waiting.
 *
 * That is the family design finally paying for itself: variety is priced in
 * floats, not in frame budget.
 */

/* Sea, later: swell building, first hint of warmth, more motes. */
const field_t f_sea_rise = {
    "sea_rise", &fam_terrain,
    {   0.18f, 0.46f,  5.0f,  0.00f, 999.0f, 999.0f, 0.08f, 2.40f, 1.10f, 0.03f, 1.3f, 0.00f, 5.00f, 1.0f,  0.00f,  0.0f, 0.00f,
     [P_UNI_MOTES] = 0.70f, [P_UNI_WARM] = 0.05f }
};

/* Canyon, wider and shallower — a breath before the walls close in. */
const field_t f_canyon_wide = {
    "canyon_wide", &fam_terrain,
    {   0.85f, 0.44f,  7.4f,  1.90f,  19.0f, 999.0f, 0.0f, 2.40f, 1.12f, 0.0f, 1.4f, 0.20f, 5.20f, 1.10f,  0.45f, 11.0f, 0.008f,
     [P_UNI_MOTES] = 0.42f, [P_UNI_WARM] = 0.10f }
};

/* Canyon, narrowing and darkening as the roof starts to be implied. */
const field_t f_canyon_deep = {
    "canyon_deep", &fam_terrain,
    {   1.00f, 0.54f, 10.2f,  4.10f,  10.5f, 999.0f, 0.10f, 2.20f, 0.98f, 0.08f, 1.9f, 0.45f, 6.20f, 1.85f, -0.20f,  8.0f, 0.021f,
     [P_UNI_MOTES] = 0.65f, [P_UNI_WARM] = 0.25f }
};

/* Tunnel, opening out into a chamber — the roof lifts and the light returns
 * before the final constriction. A moment of relief in the middle of the
 * enclosed stretch, so the tunnel is not one unbroken squeeze. */
const field_t f_chamber = {
    "chamber", &fam_terrain,
    {   1.00f, 0.50f,  9.0f,  2.60f,  15.0f,  38.0f, 0.42f, 2.20f, 0.92f, 0.28f, 2.0f, 0.55f, 5.40f, 0.55f,  0.70f, 14.0f, 0.006f,
     [P_UNI_MOTES] = 0.95f, [P_UNI_WARM] = 0.55f }
};

/* THE RETURN. Deliberately the sea's parameters again, with only the faintest
 * residue of the heat it passed through — the closing vista has to be
 * recognisably the opening one or the return means nothing. Slightly higher
 * swell and a few more motes: the same place, after something happened. */
const field_t f_sea_return = {
    "sea_return", &fam_terrain,
    {   0.06f, 0.44f,  3.8f,  0.00f, 999.0f, 999.0f, 0.05f, 2.50f, 1.12f, 0.02f, 1.0f, 0.00f, 5.00f,
        1.0f,  0.00f,  0.0f, 0.00f,
     [P_UNI_MOTES] = 0.50f, [P_UNI_WARM] = 0.08f }
};
