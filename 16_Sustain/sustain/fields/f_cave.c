/* SUSTAIN — the cave family.
 *
 * CROSS-FAMILY MORPH (1) of two (PLANNING.md §3.2). The tunnel's continuous
 * wall breaks apart into discrete drifting lumps: the surface the camera has
 * been flying through stops being a surface. Because this is a different
 * family from the terrain, world.c evaluates BOTH sides and lerps the results,
 * at roughly 2x cost — which is why the arc puts this morph at 124.8 s, exactly
 * where analyze_music.py measured the track dropping into its breakdown. The
 * expensive morph hides in the quiet, low-detail moment.
 *
 * WHY IT IS STILL A HEIGHTFIELD.
 * The obvious way to draw a cave is to splat sprites, but that would mean a
 * second renderer, and a second renderer means a moment where the demo swaps
 * from one to the other — a cut, however well disguised. So the cave is
 * expressed in the SAME language every other field speaks: a height and a
 * ceiling sampled along view rays. What breaks apart is not the renderer but
 * the field's continuity. The lumps are real geometry, occlude correctly, and
 * morph from the tunnel wall by ordinary interpolation.
 *
 * That constraint is the architecture paying off rather than getting in the
 * way: "the wall dissolves into particles" becomes a property of the world
 * function, not a special case in the drawing code.
 */

#include "../world.h"
#include "../tex.h"
#include "sky_common.h"
#include "assets.h"

#include <math.h>

enum {
    S_CELL = 0,   /* lump spacing in world units                            */
    S_SEP,        /* separation threshold: higher = smaller, more isolated  */
    S_AMP,        /* lump height                                            */
    S_DRIFT,      /* how fast the cave streams past                        */
    S_CHANNEL,    /* tube wall steepness (inherited from the tunnel shape)  */
    S_CHAN_W,     /* tube half-width                                        */
    S_GAP,        /* ceiling height above the floor                         */
    S_MSCALE,     /* material texels per world unit                         */
    S_BRIGHT,
    S_SKY,        /* sky grade, shared with the terrain family              */
    S_GLOW,       /* camera-attached light — enclosed, so it needs its own  */
    S_MAT,        /* 0 = cold material, 1 = hot                             */
};

#define MS_MASK (ASSET_SURFACE_COLD_W - 1)

/* Separated lumps from a product of sines, thresholded.
 *
 * A hashed point lattice would give more convincing irregularity, but it costs
 * a 3x3 cell search at every one of the millions of samples the ray walk takes
 * per frame — and this field is already paying 2x for being cross-family. Two
 * sines and a threshold produce rounded, separated blobs for a handful of
 * cycles, which is what the effect actually needs. Choose the cheap shape and
 * spend the budget on the morph.
 */
static float lumps(const float *p, float x, float z)
{
    const float k = 6.2831853f / p[S_CELL];

    /* NOTE THE PHASE OFFSETS. sin(x*k) is odd about x = 0, and the camera flies
     * along x ~ 0, so without them the entire cave is mirror-symmetric down
     * the centre of the frame — which is exactly how it first rendered. An
     * irrational phase kills the symmetry for free. */
    float a = sinf(x * k + 0.937f) * sinf(z * k + 2.113f);
    /* Two further incommensurate octaves so the lattice does not read as a
     * grid. Frequencies are deliberately non-integer ratios: rational ones
     * beat into a visible repeating cell. */
    a = a * 0.62f
      + 0.30f * sinf(x * k * 0.613f + 1.7f) * sinf(z * k * 0.531f - 0.9f)
      + 0.22f * sinf(x * k * 1.771f - 2.4f) * sinf(z * k * 1.523f + 0.4f);

    const float T = p[S_SEP];
    float b = (a - T) / (1.0f - T);
    if (b <= 0.0f) return 0.0f;
    return b * b * p[S_AMP];       /* squared: rounded tops, clean gaps */
}

static float tube(const float *p, float x)
{
    const float d = fabsf(x) - p[S_CHAN_W];
    if (d <= 0.0f || p[S_CHANNEL] <= 0.0f) return 0.0f;
    return p[S_CHANNEL] * d * d * 0.02f;
}

static float cave_h(const float *p, float x, float z, float t)
{
    /* The cave streams toward the camera — the drift is what makes it read as
     * suspended matter rather than as bumpy ground. */
    return lumps(p, x, z + t * p[S_DRIFT]) + tube(p, x);
}

static float cave_ceil(const float *p, float x, float z, float t, float floor_h)
{
    const float zz = z + t * p[S_DRIFT] * 1.13f;   /* upper stream drifts apart */
    (void)floor_h;
    return tube(p, x) + p[S_GAP] - lumps(p, x * 0.83f + 19.3f, zz);
}

static void cave_shade(const float *p, float x, float z, float h, float t,
                        int up, float dist, float foot, int *r, int *g, int *b)
{
    (void)foot;
    const float s  = p[S_MSCALE];
    const float zz = z + t * p[S_DRIFT];

    int cr, cg, cb;
    tex_rgb_bilin(asset_surface_cold_data, MS_MASK, MS_MASK,
                  ASSET_SURFACE_COLD_W, x * s, zz * s, &cr, &cg, &cb);
    const float m = p[S_MAT];
    if (m > 0.001f) {
        int hr, hg, hb;
        tex_rgb_bilin(asset_surface_hot_data, MS_MASK, MS_MASK,
                      ASSET_SURFACE_HOT_W, x * s, zz * s, &hr, &hg, &hb);
        cr += (int)((hr - cr) * m);
        cg += (int)((hg - cg) * m);
        cb += (int)((hb - cb) * m);
    }

    /* Lumps are lit by their own height: the crown of each catches the light
     * and the gaps between fall dark, which is what separates them visually
     * from a merely bumpy floor. */
    float k = p[S_BRIGHT] * (0.30f + 0.95f * (h / (p[S_AMP] + 0.001f)));
    if (!up) k *= 0.62f;
    if (p[S_GLOW] > 0.001f) {
        const float f = 1.0f - 1.0f / (1.0f + dist * 0.16f);
        k *= 1.0f + p[S_GLOW] * (1.0f - f);
    }
    if (k < 0.0f) k = 0.0f;

    *r = (int)(cr * k);
    *g = (int)(cg * k);
    *b = (int)(cb * k);
}

static void cave_sky(const float *p, float u, float v, float t,
                      int *r, int *g, int *b)
{
    (void)t;
    sustain_sky(p[S_SKY], u, v, r, g, b);
}

const field_family_t fam_cave = {
    .name  = "cave",
    .h     = cave_h,
    .ceil  = cave_ceil,
    .shade = cave_shade,
    .sky   = cave_sky,
    .prepare = NULL,
};

/* The tube parameters deliberately MATCH f_tunnel_deep's, so the only thing
 * that changes across the morph is that the wall stops being continuous. If
 * the enclosure changed at the same time, the eye would read two events at
 * once and call it a cut. */
/*                  CELL   SEP    AMP  DRIFT CHANNEL CHAN_W   GAP MSCALE BRIGHT  SKY  GLOW  MAT */
const field_t f_cave = {
    "cave", &fam_cave,
    {   4.2f, 0.46f,  5.4f,  6.5f,  5.00f,   7.4f, 19.0f, 0.70f, 0.95f, 0.35f, 0.9f, 0.55f,
     [P_UNI_MOTES] = 1.00f, [P_UNI_WARM] = 0.65f }
};
