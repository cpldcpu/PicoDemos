/* SUSTAIN — the monolith family.
 *
 * CROSS-FAMILY MORPH (2). The cave's forest of spires condenses: its
 * columns merge into a few enormous smooth masses that the camera has to fly around.
 *
 * The reason this is a separate family rather than another parameter set is
 * MATERIAL, not shape. Everything so far has been rock, lit by height and a
 * camera glow. The monolith is polished — shaded by a matcap (an orthographic
 * reflection probe indexed by surface normal), which is a completely different
 * lighting model and cannot be reached by interpolating rock parameters. It is
 * also what finally puts matcap_cold/matcap_warm to use.
 *
 * Placement: the arc puts this at 169.1 s, the structural boundary where the
 * track climbs out of its breakdown into the second high. The morph is the
 * expensive kind (both families evaluated), so it lands while the music is
 * still thin and the camera is moving fast enough to cover a coarser walk.
 */

#include "../world.h"
#include "../tex.h"
#include "sky_common.h"
#include "assets.h"

#include <math.h>

enum {
    M_SCALE = 0,   /* spatial frequency of the masses (low = enormous)      */
    M_AMP,         /* how tall they stand                                   */
    M_ROUND,       /* 0 = craggy, 1 = smoothly domed                        */
    M_CHANNEL,     /* surrounding tube, inherited from the cave            */
    M_CHAN_W,
    M_GAP,
    M_WARM,        /* 0 = matcap_cold, 1 = matcap_warm                      */
    M_BRIGHT,
    M_SKY,
    M_GLOW,
    M_DRIFT,       /* slow evolution, so the masses are not frozen          */
};

#define MC_MASK (ASSET_MATCAP_COLD_W - 1)

static float masses(const float *p, float x, float z)
{
    const float k = p[M_SCALE];
    /* Phase offsets for the same reason as the cave: sin(x*k) is odd about
     * x = 0 and the camera flies along x ~ 0, so without them the whole field
     * is mirror-symmetric down the centre of the frame. */
    float a = sinf(x * k + 0.417f) * sinf(z * k + 1.913f);
    a = a * 0.66f + 0.34f * sinf(x * k * 0.487f - 2.1f) * sinf(z * k * 0.561f + 0.7f);

    /* ROUND lifts the field toward a smooth dome; low values keep the sharper
     * lobes so the morph out of the cave has somewhere to come from. */
    const float r = p[M_ROUND];
    const float smooth = 0.5f + 0.5f * a;          /* 0..1, gentle */
    const float sharp  = a > 0.0f ? a * a : 0.0f;  /* isolated crowns */
    return (sharp + (smooth - sharp) * r) * p[M_AMP];
}

static float tube(const float *p, float x)
{
    const float d = fabsf(x) - p[M_CHAN_W];
    if (d <= 0.0f || p[M_CHANNEL] <= 0.0f) return 0.0f;
    return p[M_CHANNEL] * d * d * 0.02f;
}

static float mono_h(const float *p, float x, float z, float t)
{
    return masses(p, x, z + t * p[M_DRIFT]) + tube(p, x);
}

static float mono_ceil(const float *p, float x, float z, float t, float floor_h)
{
    (void)floor_h;
    const float zz = z + t * p[M_DRIFT];
    return tube(p, x) + p[M_GAP] - masses(p, x * 0.91f + 11.7f, zz) * 0.45f;
}

static void mono_shade(const float *p, float x, float z, float h, float t,
                       int up, float dist, float foot, int *r, int *g, int *b)
{
    (void)h; (void)foot;

    /* Surface normal from the height gradient. A matcap is indexed by normal,
     * so this is the one field that genuinely needs one — everything else has
     * been lit by height alone. */
    const float e  = 0.9f;
    const float zz = z + t * p[M_DRIFT];
    const float h0 = masses(p, x, zz) + tube(p, x);
    const float hx = (masses(p, x + e, zz) + tube(p, x + e)) - h0;
    const float hz = (masses(p, x, zz + e)  + tube(p, x))     - h0;

    float nx = -hx / e, nz = -hz / e, ny = 1.0f;
    const float inv = 1.0f / sqrtf(nx * nx + ny * ny + nz * nz);
    nx *= inv; nz *= inv;
    if (!up) { nx = -nx; nz = -nz; }

    /* Matcap lookup: the normal's lateral components index an orthographic
     * reflection probe, which is what gives a polished rather than a matte
     * read. Same pair rule as everywhere else — cold and warm probes share
     * framing and highlight placement, so this lerp recolours. */
    const float u = (0.5f + nx * 0.5f) * (float)ASSET_MATCAP_COLD_W;
    const float v = (0.5f - nz * 0.5f) * (float)ASSET_MATCAP_COLD_H;

    int cr, cg, cb;
    tex_rgb_bilin(asset_matcap_cold_data, MC_MASK, MC_MASK,
                  ASSET_MATCAP_COLD_W, u, v, &cr, &cg, &cb);
    const float w = p[M_WARM];
    if (w > 0.001f) {
        int wr, wg, wb;
        tex_rgb_bilin(asset_matcap_warm_data, MC_MASK, MC_MASK,
                      ASSET_MATCAP_WARM_W, u, v, &wr, &wg, &wb);
        cr += (int)((wr - cr) * w);
        cg += (int)((wg - cg) * w);
        cb += (int)((wb - cb) * w);
    }

    float k = p[M_BRIGHT];
    if (p[M_GLOW] > 0.001f) {
        const float f = 1.0f - 1.0f / (1.0f + dist * 0.16f);
        k *= 1.0f + p[M_GLOW] * (1.0f - f);
    }

    *r = (int)(cr * k);
    *g = (int)(cg * k);
    *b = (int)(cb * k);
}

static void mono_sky(const float *p, float u, float v, float t,
                     int *r, int *g, int *b)
{
    (void)t;
    sustain_sky(p[M_SKY], u, v, r, g, b);
}

const field_family_t fam_mono = {
    .name  = "monolith",
    .h     = mono_h,
    .ceil  = mono_ceil,
    .shade = mono_shade,
    .sky   = mono_sky,
    .prepare = NULL,
};

/* Tube parameters match the cave's, so the morph changes what the world is
 * MADE OF without also changing how enclosed it is — one event, not two. */
/*                  SCALE   AMP  ROUND CHANNEL CHAN_W   GAP  WARM BRIGHT  SKY GLOW DRIFT */
const field_t f_monolith = {
    "monolith", &fam_mono,
    {  0.055f, 16.0f, 0.35f,  5.00f,   7.4f, 26.0f, 0.75f, 1.05f, 0.45f, 0.5f, 2.2f,
     [P_UNI_MOTES] = 0.85f, [P_UNI_WARM] = 0.70f }
};

/* Cooling and settling, on the way back out to the sea. */
const field_t f_monolith_cool = {
    "monolith_cool", &fam_mono,
    {  0.048f, 13.0f, 0.72f,  3.20f,  11.0f, 60.0f, 0.20f, 1.10f, 0.12f, 0.3f, 1.5f,
     [P_UNI_MOTES] = 0.55f, [P_UNI_WARM] = 0.20f }
};
