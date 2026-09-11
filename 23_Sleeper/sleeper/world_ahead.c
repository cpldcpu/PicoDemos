/* world_ahead.c -- the driver's view, the camera at sleeper height, and the
 * one look upward. PLANNING §7's second and third systems.
 *
 * The plane is HELION's SIO affine span with Phase's five inheritance traps
 * answered, each marked below:
 *
 *   (1) start below the horizon singularity -- the first textured row is
 *       PLANE_SKIP rows under the vanishing line, never at it;
 *   (2) clamp the far distance -- rows whose depth exceeds Z_FAR are the
 *       horizon, not the plane;
 *   (3) merge subpixel sleepers into their average tone -- a row that spans
 *       more than MERGE_TEXELS of V draws from tex_rail_avg, so the ties
 *       neither crawl backwards nor freeze;
 *   (4) reduce distance modulo the texture period before converting to
 *       16.16 -- v_texel is taken mod TEX_SIZE in float and only then
 *       shifted, so a 192-second run cannot overflow the accumulator;
 *   (5) keep the near rail edges stable -- the two rails are not in the
 *       texture at all. They are analytic spans, one pair of edges per row
 *       computed from the row's own depth, which is exactly stable frame to
 *       frame because the row's depth is.
 *
 * The rest of the texture -- ballast and sleepers every four texels, 32 to a
 * beat -- is the SIO plane, unchanged. -- Overscan
 */
#include "render.h"
#include "accelerator.h"
#include <math.h>

#define FOCAL      300.f
#define BEDW       4.5f      /* metres of ballast bed across the texture     */
#define GAUGE      0.7175f   /* metres from the centre line to a rail head   */
#define PLANE_SKIP 8         /* (1) rows under the vanishing line to skip    */
#define MERGE_TEXELS 3.f     /* (3)                                          */
#define POST_METRES (METRES_PER_BEAT * 4.f)          /* one post a bar       */
#define SIGNAL_METRES (METRES_PER_BEAT * 32.f)       /* one signal a phrase  */

typedef struct { float h, z_far; int horizon; } cam_t;

static float wrapf(float v, float m) { return v - floorf(v / m) * m; }

/* Screen position of a world point (x metres right of the centre line,
 * y metres above the rail plane) at depth z. */
static float proj_x(const cam_t *c, float wx, float z) { (void)c; return 160.f + wx * FOCAL / z; }
static float proj_y(const cam_t *c, float wy, float z) { return (float)c->horizon - (wy - c->h) * FOCAL / z; }

/* The plane. One span a row: the bed narrows to the vanishing point and what
 * is outside it is the shoulder, so U never wraps and there is never a
 * second track where there should be one. */
static void HOT(plane)(const cam_t *c, int y_from)
{
    const float travel = F.metres;
    const float texels_per_m = (float)TEX_SIZE / METRES_PER_BEAT;
    texture_config(TEX_BITS);
    for (int y = y_from; y < HEIGHT; y++) {
        const float z = c->h * FOCAL / (float)(y - c->horizon);
        if (z > c->z_far) continue;                                  /* (2) */
        const float z_next = c->h * FOCAL / (float)(y + 1 - c->horizon);
        const float half = (BEDW * 0.5f) * FOCAL / z;
        int x0 = (int)(160.f - half), x1 = (int)(160.f + half);
        if (x0 < 0) x0 = 0;
        if (x1 > WIDTH - 1) x1 = WIDTH - 1;
        if (x0 > x1) continue;
        const int shade = clampi((int)(15.f - z * 0.10f), 2, 15);
        const uint16_t *pal = tex_pal[shade];
        uint16_t *out = g_fb + (unsigned)y * WIDTH + x0;
        /* U runs 0..TEX_SIZE across the bed; the row's own depth sets du. */
        const float du_f = (float)TEX_SIZE / (2.f * half);
        const float u0_f = ((float)x0 - (160.f - half)) * du_f;
        /* (4): reduce before the shift, never after */
        const float v_f = wrapf((travel + z) * texels_per_m, (float)TEX_SIZE);
        const float dv_row = fabsf(z - z_next) * texels_per_m;       /* (3) */
        if (dv_row > MERGE_TEXELS) {
            uint32_t u = (uint32_t)(u0_f * 65536.f), du = (uint32_t)(du_f * 65536.f);
            for (int x = x0; x <= x1; x++) { *out++ = pal[tex_rail_avg[(u >> 16) & (TEX_SIZE - 1)]]; u += du; }
        } else {
            texture_span((int32_t)(u0_f * 65536.f), (int32_t)(v_f * 65536.f),
                         (int32_t)(du_f * 65536.f), 0);
            for (int x = x0; x <= x1; x++) *out++ = pal[tex_rail[texture_pop()]];
        }
        g_stats.spans++;
    }
}

/* (5) The rails, analytic. Two spans a row, from the row's own depth. */
static void HOT(rails)(const cam_t *c, int y_from, float offset_m, uint16_t head)
{
    for (int y = y_from; y < HEIGHT; y++) {
        const float z = c->h * FOCAL / (float)(y - c->horizon);
        if (z > c->z_far) continue;
        const float k = FOCAL / z;
        int w = (int)(0.055f * k); if (w < 1) w = 1; if (w > 2) w = 2;
        for (int s = -1; s <= 1; s += 2) {
            const int x = (int)(160.f + ((float)s * GAUGE + offset_m) * k);
            hspan(y, x - w, x + w, head);
        }
    }
}

/* Catenary posts as 3D quads with a wire, one a bar; a signal a phrase. */
static void posts(const cam_t *c)
{
    const float travel = F.metres;
    const int k0 = (int)floorf((travel + 6.f) / POST_METRES);
    float prev_x = 0.f, prev_y = 0.f; int have_prev = 0;
    for (int k = k0; k < k0 + 24; k++) {
        const float z = (float)k * POST_METRES - travel;
        if (z < 6.f) { have_prev = 0; continue; }
        if (z > c->z_far) break;
        const float bx = proj_x(c, 2.6f, z), by = proj_y(c, 0.f, z);
        const float tx = proj_x(c, 2.6f, z), ty = proj_y(c, 6.4f, z);
        int w = (int)(0.22f * FOCAL / z); if (w < 1) w = 1;
        rect((int)tx - w, (int)ty, (int)bx + w, (int)by, RGB(0x0C, 0x0E, 0x16));
        /* the bracket over the track */
        hspan((int)ty + 1, (int)(proj_x(c, -0.2f, z)), (int)tx, RGB(0x0A, 0x0C, 0x12));
        if (have_prev) {
            line_a((int)prev_x, (int)prev_y, (int)tx, (int)ty + 2, RGB(0x14, 0x16, 0x1E), 26);
            /* the contact wire over the centre line */
            line_a((int)proj_x(c, 0.f, z + POST_METRES), (int)(prev_y + 5.f),
                   (int)proj_x(c, 0.f, z), (int)(ty + 7.f), RGB(0x10, 0x12, 0x18), 22);
        }
        prev_x = tx; prev_y = ty + 2.f; have_prev = 1;
    }
    /* one signal a phrase: green on the line, red in the yard */
    const int s0 = (int)floorf((travel + 8.f) / SIGNAL_METRES);
    for (int s = s0; s < s0 + 3; s++) {
        const float z = (float)s * SIGNAL_METRES - travel;
        if (z < 8.f || z > c->z_far) continue;
        const int red = (F.world == WORLD_YARD) || (F.world == WORLD_TERMINUS);
        const float sx = proj_x(c, -3.1f, z), sy = proj_y(c, 3.4f, z);
        vspan((int)sx, (int)sy, (int)proj_y(c, 0.f, z), RGB(0x05, 0x06, 0x0A));
        const float r = 4.f + 90.f / z;
        light_t L = { sx, sy, 0.f, 0.f, r, red ? C_RED : C_GREEN, red ? C_RED : C_GREEN, 20, 2, 0 };
        light_draw(&L);
    }
}

/* CUT_POINTS: from the cut, a second rail pair diverging and a third
 * crossing it. The switch is a fixed distance ahead and the divergence grows
 * with depth, which is what a set of points looks like from the cab. */
static void points(const cam_t *c, int y_from)
{
    const float z_switch = 14.f;
    for (int y = y_from; y < HEIGHT; y++) {
        const float z = c->h * FOCAL / (float)(y - c->horizon);
        if (z > c->z_far || z < z_switch) continue;
        const float d = (z - z_switch) * 0.055f;
        if (d > 3.4f) continue;
        const float k = FOCAL / z;
        int w = (int)(0.075f * k); if (w < 1) w = 1; if (w > 4) w = 4;
        for (int s = -1; s <= 1; s += 2) {
            int x = (int)(160.f + ((float)s * GAUGE + d) * k);
            hspan(y, x - w, x + w, RGB(0x50, 0x58, 0x64));
            x = (int)(160.f + ((float)s * GAUGE - d) * k);
            hspan(y, x - w, x + w, RGB(0x44, 0x4C, 0x56));
        }
    }
}

/* CUT_MOUTH: the tunnel mouth growing over the shot's last two bars. */
static void mouth(const cam_t *c)
{
    const float u = (float)F.since / (float)(2u * BAR_SAMPLES);
    const float k = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
    /* It grows to fill the frame exactly at the cut into the tunnel and not
     * a beat before it: at 430 the last two seconds of the shot were black,
     * which is both a worse picture and a blank frame check.c is right to
     * refuse. */
    const float r = 4.f + k * k * 232.f;
    const float cx = 160.f, cy = (float)c->horizon + 18.f;
    disc(cx, cy, r, RGB(0x00, 0x00, 0x00));
    /* the arch ring: lit brick, the last thing the sky sees */
    for (float a = 0.f; a < 6.28f; a += 0.04f) {
        const float rr = r + 2.f;
        px((int)(cx + fcos(a) * rr), (int)(cy + fsin(a) * rr * 0.86f), RGB(0x48, 0x30, 0x18), 26);
        px((int)(cx + fcos(a) * (rr + 1.f)), (int)(cy + fsin(a) * (rr + 1.f) * 0.86f), RGB(0x20, 0x16, 0x0C), 22);
    }
}

static void bridge_girders(const cam_t *c)
{
    const float travel = F.metres;
    const int k0 = (int)floorf((travel + 5.f) / METRES_PER_BEAT);
    for (int k = k0; k < k0 + 40; k++) {
        const float z = (float)k * METRES_PER_BEAT - travel;
        if (z < 5.f) continue;
        if (z > c->z_far) break;
        const float y0 = proj_y(c, 6.8f, z), y1 = proj_y(c, 5.6f, z);
        if (y1 < 0.f || y1 - y0 < 1.5f) continue;   /* merged: not a girder  */
        rect(0, (int)y0, WIDTH - 1, (int)y1, RGB(0x02, 0x03, 0x05));
    }
    /* the parapets */
    for (int y = c->horizon + PLANE_SKIP; y < HEIGHT; y++) {
        const float z = c->h * FOCAL / (float)(y - c->horizon);
        if (z > c->z_far) continue;
        const float k = FOCAL / z;
        hspan(y, 0, (int)(160.f - 3.4f * k), RGB(0x03, 0x04, 0x06));
        hspan(y, (int)(160.f + 3.4f * k), WIDTH - 1, RGB(0x03, 0x04, 0x06));
    }
}

static void horizon_and_sky(const cam_t *c)
{
    /* Phase's plate, shifted so its horizon lands on this camera's. The moon
     * is where it was in the window: the same sky, the same night. */
    sky_expand(0, c->horizon, HORIZON - c->horizon);
    rect(0, c->horizon, WIDTH - 1, HEIGHT - 1, F.dawn > 0.35f ? RGB(0x18, 0x18, 0x20) : C_LAND);
    if (sky_is_dawn() && F.world == WORLD_COAST) {
        /* The sea to the right. It was a rectangle and read as one; a coast
         * seen from the cab is a shoreline that recedes with the track, so
         * it is a wedge that closes on the vanishing point, and its water is
         * the plate's own sea colour rather than a second one. */
        for (int y = c->horizon; y < c->horizon + 34; y++) {
            const int d = y - c->horizon;
            const int edge = WIDTH - 1 - (34 - d) * 3;
            if (edge < 200) continue;
            hspan(y, edge, WIDTH - 1, RGB(0x30, 0x48, 0x60));
        }
    }
    far_layer(c->horizon, F.dist * NEAR_PX_PER_UNIT * 0.25f, 2);
}

static void ahead_common(const cam_t *c)
{
    const int y0 = c->horizon + PLANE_SKIP;                          /* (1) */
    horizon_and_sky(c);
    if (F.world == WORLD_BRIDGE) bridge_girders(c);
    plane(c, y0);
    rails(c, y0, 0.f, RGB(0x44, 0x4C, 0x58));
    if (F.flags & CUT_POINTS) points(c, y0);
    posts(c);
    if (F.flags & CUT_MOUTH) mouth(c);
}

void HOT(world_ahead)(void)
{
    cam_t c = { 3.2f, 140.f, 112 };
    ahead_common(&c);
    if (F.world == WORLD_CITY || F.world == WORLD_YARD || F.world == WORLD_TERMINUS) {
        /* the yard's lamps, thrown along both shoulders */
        const float travel = F.metres;
        const int k0 = (int)floorf((travel + 8.f) / (METRES_PER_BEAT * 2.f));
        for (int k = k0; k < k0 + 12; k++) {
            const float z = (float)k * METRES_PER_BEAT * 2.f - travel;
            if (z < 8.f || z > c.z_far) continue;
            for (int s = -1; s <= 1; s += 2) {
                const float sx = proj_x(&c, (float)s * 7.5f, z), sy = proj_y(&c, 8.f, z);
                vspan((int)sx, (int)sy, (int)proj_y(&c, 0.f, z), RGB(0x05, 0x06, 0x0A));
                light_t L = { sx, sy, 0.f, 0.f, 3.f + 140.f / z, C_SODIUM, C_SODIUM_H, 15, 2, 0 };
                light_draw(&L);
            }
        }
    }
    if (F.flags & CUT_RAIN) { /* rain on the windscreen too */ }
}

void HOT(world_under)(void)
{
    /* The camera at sleeper height, looking down and forward. Same plane,
     * lower and closer: the sleepers are huge and they strobe, which is what
     * the shot is for. */
    /* 1.1 m, not the 0.4 m of PLANNING §7: at 0.4 m the nearest row is
     * seventy centimetres ahead, one texel covers fifteen pixels and the
     * ballast becomes a wall of rectangles. At 1.1 m a texel is five pixels
     * and the sleepers still strobe, which is what the shot is for. */
    cam_t c = { 1.1f, 30.f, 78 };
    sky_expand(0, c.horizon, HORIZON - c.horizon);
    /* Everything below the vanishing line, before the plane: the bed is
     * narrower than the screen for most of the shot and what is beside it
     * has to be the lineside, not whatever the page held last frame. */
    rect(0, c.horizon, WIDTH - 1, HEIGHT - 1, C_LAND);
    far_layer(c.horizon, F.dist * NEAR_PX_PER_UNIT * 0.25f, 3);
    plane(&c, c.horizon + PLANE_SKIP);
    rails(&c, c.horizon + PLANE_SKIP, 0.f, RGB(0x4C, 0x54, 0x60));
}

void HOT(world_up)(void)
{
    /* Looking up out of the city: the towers pass overhead as silhouettes
     * against the sky, their windows lights. */
    sky_expand(0, HORIZON, 0);
    sky_expand(HORIZON, HEIGHT, 60);
    const float off = F.dist * NEAR_PX_PER_UNIT;
    const float vel = -F.ppf;
    for (int side = 0; side < 2; side++) {
        const float sgn = side ? 1.f : -1.f;
        /* the wall's edge converges toward the centre as it goes up */
        for (int y = 0; y < HEIGHT; y++) {
            const float k = 1.f + (float)y * 0.0075f;      /* the perspective */
            const int edge = (int)(160.f + sgn * (44.f * k));
            if (side) hspan(y, edge, WIDTH - 1, RGB(0x02, 0x03, 0x05));
            else hspan(y, 0, edge, RGB(0x02, 0x03, 0x05));
        }
        /* floors sliding down the wall, at the shared spacing */
        const int S = RHYTHM / 2;
        const int first = (int)floorf(off / (float)S);
        for (int i = first; ; i++) {
            const float t = (float)(i * S) - off;
            if (t > 320.f) break;
            if (t < -40.f) continue;
            const int y = (int)(t * 0.75f);
            if (y < 0 || y >= HEIGHT) continue;
            const float k = 1.f + (float)y * 0.0075f;
            const int inner = (int)(160.f + sgn * (44.f * k)), outer = side ? WIDTH - 1 : 0;
            hspan_a(y, side ? inner : outer, side ? outer : inner, RGB(0x12, 0x15, 0x1E), 30);
            /* three lit windows a floor */
            for (int w = 0; w < 3; w++) {
                const uint32_t q = rhash(((uint32_t)i * 7u + (uint32_t)w) ^ (uint32_t)side);
                if ((q & 1u)) continue;
                const float wx = 160.f + sgn * (52.f * k + (float)w * 30.f);
                if (wx < 0.f || wx > WIDTH - 1.f) continue;
                rect((int)wx - 2, y - 3, (int)wx + 2, y + 3, C_FLUO_H);
                light_t L = { wx, (float)y, 0.f, vel * 0.30f, 9.f, C_FLUO, C_FLUO_H, 15, 2, 0 };
                light_draw(&L);
            }
        }
    }
    if (F.flags & CUT_RAIN) { /* the droplets are added by the dispatcher */ }
}
