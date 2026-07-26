/* SUSTAIN — the continuous world runner.
 *
 * Replaces QUICKSILVER's scene.c. That file found the active timeline entry,
 * called done() on the outgoing effect, switched screen mode, called init() on
 * the incoming one, and painted a uniform glint over the seam. Every one of
 * those steps is a cut. None of them exist here.
 *
 * What this file does instead, every frame:
 *   1. evaluate the camera spline at t          (C1 by construction)
 *   2. evaluate the blend schedule at t         (never more than 2 fields live)
 *   3. hand both to one renderer                (which never sees a boundary)
 *
 * There is no per-frame branch anywhere in here that can produce a
 * discontinuity, which is the point: the rule is enforced by the shape of the
 * code, not by remembering to obey it.
 */

#include "world.h"
#include "scene.h"
#include "vga.h"
#include "render.h"
#include "field_scratch.h"
#include "hot.h"

#include <math.h>
#include <stdio.h>

/* ------------------------------------------------------------- blend ----- */

static const field_t *g_fa = NULL;   /* outgoing / current */
static const field_t *g_fb = NULL;   /* incoming (NULL when not morphing) */
static float          g_w  = 0.0f;   /* 0 = pure A, 1 = fully B */
static float          g_t_sec = 0.0f;

/* Set when g_fa and g_fb share a family: the morph then lerps parameters and
 * evaluates ONCE, into this block. See world.h for why that is both cheaper
 * and more correct than averaging two evaluated surfaces. */
static int   g_same_family = 0;
static float g_p[FIELD_MAX_PARAMS];

/* The parameter block every sampling call should read. Points at the
 * blended block during an intra-family morph, at the field's own otherwise. */
static const float *g_pa = NULL;

/* Smoothstep the morph weight. A linear ramp gives the eye a corner at each
 * end of the morph — a soft "start" and "stop" it can latch onto, which is
 * exactly the boundary the demo is refusing to provide. Smoothstep has zero
 * derivative at both ends, so the morph eases in and out of the surrounding
 * still world with no perceptible seam. */
static inline float smoothstep01(float u)
{
    if (u <= 0.0f) return 0.0f;
    if (u >= 1.0f) return 1.0f;
    return u * u * (3.0f - 2.0f * u);
}

/* Resolve g_pa / g_same_family once per frame from the current blend state. */
static void resolve_params(void)
{
    g_same_family = 0;
    if (!g_fa) { g_pa = NULL; return; }

    if (!g_fb) {                       /* settled: one field, its own params */
        g_pa = g_fa->params;
        return;
    }

    if (g_fa->family == g_fb->family) {
        g_same_family = 1;
        for (int i = 0; i < FIELD_MAX_PARAMS; i++)
            g_p[i] = g_fa->params[i] + (g_fb->params[i] - g_fa->params[i]) * g_w;
        g_pa = g_p;
    } else {
        g_pa = g_fa->params;           /* cross-family: both sides evaluated */
    }
}

/* Which arc node pair is live at t, and how far between them. */
static void blend_at(uint32_t t_ms)
{
    /* Before the first node: pure field 0. */
    if (arc_count == 0) { g_fa = g_fb = NULL; g_w = 0.0f; resolve_params(); return; }
    if (t_ms <= arc[0].t_ms || arc_count == 1) {
        g_fa = arc[0].field; g_fb = NULL; g_w = 0.0f; resolve_params(); return;
    }

    for (int i = 0; i + 1 < arc_count; i++) {
        uint32_t t0 = arc[i].t_ms, t1 = arc[i + 1].t_ms;
        if (t_ms < t0 || t_ms >= t1) continue;

        uint32_t m = arc[i + 1].morph_ms;
        if (m > t1 - t0) m = t1 - t0;          /* clamp: morphs cannot overlap */
        uint32_t morph_start = t1 - m;

        if (m == 0 || t_ms < morph_start) {
            /* Settled: one field, no blend cost. */
            g_fa = arc[i].field; g_fb = NULL; g_w = 0.0f;
        } else {
            g_fa = arc[i].field;
            g_fb = arc[i + 1].field;
            g_w  = smoothstep01((float)(t_ms - morph_start) / (float)m);
        }
        resolve_params();
        return;
    }

    /* Past the last node: hold it. The demo ends by camera move, not by
     * running out of arc. */
    g_fa = arc[arc_count - 1].field; g_fb = NULL; g_w = 0.0f;
    resolve_params();
}

/* --------------------------------------------------- world sampling ------ */

/* Every sampler has the same three-case shape:
 *   settled            -> one evaluation
 *   intra-family morph -> one evaluation, against lerped parameters
 *   cross-family morph -> two evaluations, results lerped
 * Only the last case costs double, and only two morphs in the arc take it. */

float SUSTAIN_HOT(world_height)(float x, float z)
{
    if (!g_fa) return 0.0f;
    float a = g_fa->family->h(g_pa, x, z, g_t_sec);
    if (!g_fb || g_same_family) return a;
    float b = g_fb->family->h(g_fb->params, x, z, g_t_sec);
    return a + (b - a) * g_w;
}

/* Terrain height averaged over a disc, for the camera to ride. Radius is a few
 * world units — wide enough to average out ripples the camera should glide
 * over, narrow enough that it still climbs a canyon floor. Weighted centre-
 * heavy so the camera tracks the ground under it rather than lagging behind
 * the average of its surroundings. */
float world_height_smooth(float x, float z)
{
    /* ANISOTROPIC ON PURPOSE: wide along the track, narrow across it.
     *
     * A symmetric disc is wrong here. The jerk this smooths out comes from the
     * ALONG-TRACK profile — ridges the camera flies over — so the kernel needs
     * reach in z. But reaching equally far in x samples the canyon WALLS,
     * whose height climbs quadratically away from the path; in the narrow
     * tunnel (half-width ~6) a radius-5 disc pulled the average up by several
     * units and pushed the camera through the ceiling. cut_detect.py caught it
     * as 26 discontinuities the moment the pace increased.
     *
     * So: sample far in z, barely in x. Smooths what actually causes the bob,
     * and stays inside the channel where the camera lives. */
    static const float oz[4] = {  9.0f, -9.0f, 18.0f, -18.0f };
    static const float ox[2] = {  1.6f, -1.6f };

    float acc  = world_height(x, z) * 3.0f;
    float wsum = 3.0f;
    for (int i = 0; i < 4; i++) {
        const float w = (i < 2) ? 1.5f : 0.75f;   /* nearer samples count more */
        acc  += world_height(x, z + oz[i]) * w;
        wsum += w;
    }
    for (int i = 0; i < 2; i++) {
        acc  += world_height(x + ox[i], z) * 0.75f;
        wsum += 0.75f;
    }
    return acc / wsum;
}

float SUSTAIN_HOT(world_ceiling)(float x, float z, float floor_h)
{
    if (!g_fa) return WORLD_NO_CEILING;
    float a = g_fa->family->ceil(g_pa, x, z, g_t_sec, floor_h);
    if (!g_fb || g_same_family) return a;
    float b = g_fb->family->ceil(g_fb->params, x, z, g_t_sec, floor_h);

    /* Lerping toward WORLD_NO_CEILING would sweep a phantom ceiling down from
     * a million units up. When only one side is enclosed, ease the OPEN side
     * from the enclosed side's own height instead, so the roof grows out of
     * the geometry rather than falling out of the sky. */
    if (a >= WORLD_NO_CEILING && b >= WORLD_NO_CEILING) return WORLD_NO_CEILING;
    if (a >= WORLD_NO_CEILING) a = b + (b - floor_h);
    if (b >= WORLD_NO_CEILING) b = a + (a - floor_h);
    return a + (b - a) * g_w;
}

void SUSTAIN_HOT(world_shade)(float x, float z, float h, int up, float dist, float foot,
                 int *r, int *g, int *b)
{
    int ar = 0, ag = 0, ab = 0;
    if (!g_fa) { *r = *g = *b = 0; return; }
    g_fa->family->shade(g_pa, x, z, h, g_t_sec, up, dist, foot, &ar, &ag, &ab);
    if (!g_fb || g_same_family) { *r = ar; *g = ag; *b = ab; return; }

    int br = 0, bg = 0, bb = 0;
    g_fb->family->shade(g_fb->params, x, z, h, g_t_sec, up, dist, foot, &br, &bg, &bb);
    *r = ar + (int)((br - ar) * g_w);
    *g = ag + (int)((bg - ag) * g_w);
    *b = ab + (int)((bb - ab) * g_w);
}

void world_sky(float u, float v, int *r, int *g, int *b)
{
    int ar = 0, ag = 0, ab = 0;
    if (!g_fa) { *r = *g = *b = 0; return; }
    g_fa->family->sky(g_pa, u, v, g_t_sec, &ar, &ag, &ab);
    if (!g_fb || g_same_family) { *r = ar; *g = ag; *b = ab; return; }

    int br = 0, bg = 0, bb = 0;
    g_fb->family->sky(g_fb->params, u, v, g_t_sec, &br, &bg, &bb);
    *r = ar + (int)((br - ar) * g_w);
    *g = ag + (int)((bg - ag) * g_w);
    *b = ab + (int)((bb - ab) * g_w);
}

/* Universal slots blend the same way whether or not the families match — the
 * whole point of reserving fixed indices. */
static float uni(int slot)
{
    if (!g_fa) return 0.0f;
    const float a = g_fa->params[slot];
    if (!g_fb) return a;
    const float b = g_fb->params[slot];
    return a + (b - a) * g_w;
}

float world_mote_density(void) { return uni(P_UNI_MOTES); }
float world_mote_warm(void)    { return uni(P_UNI_WARM); }

int         world_is_cross_family(void) { return g_fb && !g_same_family; }
float       world_blend_w(void)      { return g_w; }
const char *world_field_a_name(void) { return g_fa ? g_fa->name : "-"; }
const char *world_field_b_name(void) { return g_fb ? g_fb->name : "-"; }

uint32_t world_duration_ms(void)
{
    return arc_count ? arc[arc_count - 1].t_ms : 0;
}

/* ------------------------------------------------------------ camera ----- */

/* Cubic Hermite with tangents estimated from neighbouring keys, scaled by the
 * local time step. Plain uniform Catmull-Rom would be C1 only for evenly
 * spaced keyframes; our keys are not evenly spaced (the camera dwells in some
 * places and races through others, which is how this demo paces itself without
 * cuts), so the tangents are divided by the actual time span. Get this wrong
 * and the camera's velocity jumps at every keyframe — a jerk the eye reads as
 * a cut even though the position is continuous. */
static float hermite(float p0, float p1, float p2, float p3,
                     float dt0, float dt1, float dt2, float u)
{
    /* Tangents at p1 and p2 (per unit time), then scaled into this span. */
    float m1 = (dt0 + dt1) > 0.0f ? (p2 - p0) / (dt0 + dt1) : 0.0f;
    float m2 = (dt1 + dt2) > 0.0f ? (p3 - p1) / (dt1 + dt2) : 0.0f;
    m1 *= dt1;
    m2 *= dt1;

    float u2 = u * u, u3 = u2 * u;
    float h00 =  2.0f * u3 - 3.0f * u2 + 1.0f;
    float h10 =         u3 - 2.0f * u2 + u;
    float h01 = -2.0f * u3 + 3.0f * u2;
    float h11 =         u3 -        u2;
    return h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;
}

void world_camera_at(uint32_t t_ms, camera_t *out)
{
    if (cam_key_count == 0) {
        out->x = out->y = out->z = out->yaw = out->pitch = 0.0f;
        return;
    }
    if (cam_key_count == 1 || t_ms <= cam_keys[0].t_ms) {
        out->x = cam_keys[0].x; out->y = cam_keys[0].y; out->z = cam_keys[0].z;
        out->yaw = cam_keys[0].yaw; out->pitch = cam_keys[0].pitch;
        return;
    }

    int i = cam_key_count - 2;
    for (int k = 0; k + 1 < cam_key_count; k++) {
        if (t_ms < cam_keys[k + 1].t_ms) { i = k; break; }
    }

    int i0 = i > 0 ? i - 1 : 0;
    int i1 = i;
    int i2 = i + 1;
    int i3 = (i + 2 < cam_key_count) ? i + 2 : cam_key_count - 1;

    float dt0 = (float)(cam_keys[i1].t_ms - cam_keys[i0].t_ms);
    float dt1 = (float)(cam_keys[i2].t_ms - cam_keys[i1].t_ms);
    float dt2 = (float)(cam_keys[i3].t_ms - cam_keys[i2].t_ms);
    if (dt1 <= 0.0f) dt1 = 1.0f;

    float u = (float)(t_ms - cam_keys[i1].t_ms) / dt1;
    if (u < 0.0f) u = 0.0f;
    if (u > 1.0f) u = 1.0f;

#define HRM(F) hermite(cam_keys[i0].F, cam_keys[i1].F, cam_keys[i2].F, \
                       cam_keys[i3].F, dt0, dt1, dt2, u)
    out->x     = HRM(x);
    out->y     = HRM(y);
    out->z     = HRM(z);
    out->yaw   = HRM(yaw);
    out->pitch = HRM(pitch);
#undef HRM
}

/* ------------------------------------------------- precompute scheduling - */

/* Slot the *current* field renders from; the other slot is free for the next
 * field to prepare into. This is the ping-pong that replaces QUICKSILVER's
 * single-scene g_scratch union (scene_scratch.h:5 — "only one scene is active
 * at a time"), which a morph makes false. */
static int g_slot_cur  = 0;
static int g_prepared_upto = -1;   /* arc index whose prepare() has been run */

/* How far ahead of a morph we run the incoming field's prepare(). Must be
 * comfortably more than one frame: doing it at the morph would stall, and a
 * dropped frame is a visible discontinuity that fails the rule-1 audit. */
#define PREPARE_LEAD_MS 2000u

static void schedule_prepare(uint32_t t_ms)
{
    for (int i = 1; i < arc_count; i++) {
        if (i <= g_prepared_upto) continue;

        uint32_t morph_start = arc[i].t_ms - arc[i].morph_ms;
        uint32_t lead = (morph_start > PREPARE_LEAD_MS)
                      ? morph_start - PREPARE_LEAD_MS : 0u;
        if (t_ms < lead) break;              /* not yet — nodes are ordered */

        if (arc[i].field && arc[i].field->family->prepare)
            arc[i].field->family->prepare(1 - g_slot_cur, arc[i].field->params);
        g_prepared_upto = i;
        break;                               /* at most one prepare per frame */
    }
}

/* ------------------------------------------------------------- runner ---- */

int scene_runner_tick(uint32_t t_ms_global)
{
    /* The demo ends when the camera has flown out, not when a list runs out. */
    uint32_t end = world_duration_ms();
    if (end && t_ms_global >= end) return 0;

    g_t_sec = t_ms_global * 0.001f;

    blend_at(t_ms_global);
    schedule_prepare(t_ms_global);

    camera_t cam;
    world_camera_at(t_ms_global, &cam);

    /* cam_key.y is height ABOVE THE LOCAL FLOOR, not an absolute altitude.
     *
     * Absolute heights cannot work here: the same camera path has to skim a
     * sea of amplitude 3 and then thread a tunnel of amplitude 11, and any
     * fixed altitude that clears one clips through the other. Authoring keys
     * against one field and then morphing the world out from under them put
     * the camera inside the geometry, which cut_detect.py caught as 23
     * discontinuities of delta ~14 in the tunnel.
     *
     * Riding the floor also means the camera rises and falls WITH a morph
     * rather than being overtaken by it — the ground can heave into a canyon
     * and the flight stays legal throughout, with no keyframe edits.
     *
     * SMOOTHED IN SPACE, NOT TIME. Following the raw height makes the camera
     * bob over every ripple, which reads as jerk. The obvious fix — a temporal
     * low-pass on the followed height — would be wrong here: it makes the
     * camera depend on frame history, so seeking to a timestamp would no
     * longer reproduce the same frame, breaking --screenshot-at, --start-ms
     * and the determinism cut_detect.py relies on. Averaging the terrain over
     * a disc around the camera instead is a pure function of position: it
     * low-passes exactly the high-frequency detail that causes the bob, and
     * the same time always yields the same frame. */
    cam.y += world_height_smooth(cam.x, cam.z);

    /* SMOOTH CLEARANCE GUARD.
     *
     * The camera rides a SMOOTHED floor, so where the local surface rises above
     * that average it can come within touching distance — or pass through. That
     * has now caused three separate audit failures (87 s, 117 s, 122 s), each
     * of which I first "fixed" by hand-tuning a keyframe's height. That is
     * whack-a-mole: the real problem is that nothing enforces clearance, so
     * every future arc edit can reintroduce it.
     *
     * This enforces it. A hard clamp would be wrong — it is C0, and a kink in
     * the camera path is exactly the jerk the eye reads as a cut — so it uses a
     * smooth maximum, which is C-infinity: far from the surface it is the
     * identity, and it eases asymptotically as the surface approaches. The
     * camera can no longer touch the geometry, and no arc edit can make it. */
    {
        /* MARGIN is not just a safety distance. Skimming a crest at 20 u/s
         * makes near geometry sweep across the frame in a couple of frames —
         * legitimate motion, but it reads as a flash and it is ugly. Standing
         * further off calms the near field without slowing the camera down. */
        const float MARGIN = 4.0f;
        const float SOFT   = 2.6f;      /* width of the easing zone */
        const float k      = SOFT * SOFT;

        const float floor_h = world_height(cam.x, cam.z);
        const float fl = floor_h + MARGIN;
        cam.y = 0.5f * (cam.y + fl + sqrtf((cam.y - fl) * (cam.y - fl) + k));

        const float ce = world_ceiling(cam.x, cam.z, floor_h);
        if (ce < WORLD_NO_CEILING) {
            const float cl = ce - MARGIN;
            cam.y = 0.5f * (cam.y + cl - sqrtf((cam.y - cl) * (cam.y - cl) + k));
        }
    }

    /* SUSTAIN never changes screen mode mid-run: vga_set_mode() stops and
     * restarts scanvideo, which costs a black frame. Called once here, and
     * idempotent thereafter. */
    vga_set_mode(MODE_HIRES);

    render_world(vga_hires_back_buffer(), &cam, g_t_sec);
    return 1;
}

/* --------------------------------------- authoring navigation (host only) - */

uint32_t scene_next_boundary_ms(uint32_t t_ms_global)
{
    for (int i = 0; i < arc_count; i++)
        if (arc[i].t_ms > t_ms_global) return arc[i].t_ms;
    return world_duration_ms();
}

uint32_t scene_prev_boundary_ms(uint32_t t_ms_global)
{
    uint32_t prev = 0;
    for (int i = 0; i < arc_count; i++) {
        if (arc[i].t_ms + 500u >= t_ms_global) break;
        prev = arc[i].t_ms;
    }
    return prev;
}

uint32_t scene_cur_start_ms(void)
{
    return 0;
}

uint32_t scene_cur_end_ms(void)
{
    return world_duration_ms();
}
