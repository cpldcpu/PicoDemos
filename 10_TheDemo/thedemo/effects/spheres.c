/* Raytraced spheres + floor — scene 5 (1:50 – 2:50, MODE_160 RGB565).
 *
 * A real raytracer: four spheres orbiting in front of the camera over
 * an infinite checker floor, lit by a single directional light, with
 * the AI envmap as the sky. Two spheres are matte tinted diffuse and
 * two are mirror-polished; mirror reflections RECURSE through the
 * scene (one bounce) so the mirrors actually reflect the floor and
 * the other spheres, not just the panorama.
 *
 * Originally rendered at full 320×240 MODE_320 (palette-indexed). At
 * 300 MHz on the M33 that was running ~15 fps and showing palette
 * banding on every gradient surface despite Bayer dither.
 *
 * Now: MODE_160 (160×120 RGB565). 4× fewer pixels → comfortable
 * vblank fit, and truecolor RGB means smooth gradients with no
 * palette tricks needed anywhere. Mirror-sphere reflections, floor
 * depth fog, diffuse shading: all directly computed in RGB.
 *
 * Per-pixel pipeline (one D = primary ray direction):
 *   intersect spheres (bbox-culled) + floor → nearest hit.
 *   shade by material:
 *     - DIFFUSE_*  : RGB = tint × (ambient + diffuse·max(0, N·L)) × shadow_factor.
 *     - MIRROR     : reflect D → recurse trace_ray once → return result.
 *     - FLOOR      : checker by tile parity, depth-blended toward
 *                    horizon fog colour, sphere-shadowed.
 *     - SKY (miss) : equirectangular envmap sample along D.
 *
 * On-screen banner across the top boasts the spec: "REALTIME RAYTRACER"
 * + spec line. Stamped after the raytrace pass in RGB565 directly.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include "../font8x8.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define NUM_SPHERES       4
#define FB_W              VGA_160_W            /* 160 */
#define FB_H              VGA_160_H            /* 120 */
#define ENVMAP_W          ASSET_ENVMAP_W       /* 256 — power of 2 for the yaw wrap */
#define ENVMAP_H          ASSET_ENVMAP_H       /* 128 */

#define SCENE_LEN_MS      37000
#define FADE_IN_MS         2000
#define FADE_OUT_MS        2500
#define FADE_OUT_AT       (SCENE_LEN_MS - FADE_OUT_MS)

typedef enum {
    MAT_DIFFUSE_A = 0,
    MAT_DIFFUSE_B,
    MAT_MIRROR,
} material_t;

#define FLOOR_Y_F         1.6f      /* plane y = +1.6, camera at y = 0 */

typedef struct {
    float x, y, z;
    float r;
    float r2;
    material_t mat;
} sphere_t;

typedef struct { uint8_t r, g, b; } rgb8_t;

static sphere_t spheres[NUM_SPHERES];

/* Envmap palette, pre-expanded to 8-bit-per-channel RGB so the inner
 * loop never has to unpack RGB565. Mirror-reflection envmap samples
 * return one of these triplets directly. */
static rgb8_t env_rgb[256];

/* Directional light, world-space, unit-length. */
static float light_x, light_y, light_z;

/* Camera state. */
static float cam_x, cam_z;
static float cam_yaw_cos, cam_yaw_sin;

/* Envmap yaw offset, Q4 fractional pixels (mod ENVMAP_W << 4). Used
 * for a slow panorama drift on top of the camera tracking shot. */
static int yaw_offset_q4 = 0;

/* Per-frame "fade-in/fade-out" multiplier, Q8. Multiplied into every
 * shaded RGB before pack to RGB565 — gives the scene fade-from-black
 * boundary effect without needing a palette. */
static int alpha_q8 = 256;

static const float tint_hue_base[2] = { 320.0f, 180.0f };
/* Diffuse-sphere tint colours, updated per frame as the HSV hue drifts. */
static rgb8_t tint_a, tint_b;

/* --- helpers ----------------------------------------------------- */

static inline uint16_t pack565(int r, int g, int b)
{
    return rgb565_pack(r, g, b);
}

/* Scale an RGB triplet by the scene-fade alpha (Q8). */
static inline uint16_t pack565_faded(int r, int g, int b)
{
    r = (r * alpha_q8) >> 8;
    g = (g * alpha_q8) >> 8;
    b = (b * alpha_q8) >> 8;
    return pack565(r, g, b);
}

static void hsv_to_rgb(float h, float s, float v, rgb8_t *out)
{
    float c  = v * s;
    float hp = h / 60.0f;
    while (hp < 0)   hp += 6.0f;
    while (hp >= 6)  hp -= 6.0f;
    float x  = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m  = v - c;
    float rf = 0, gf = 0, bf = 0;
    if      (hp < 1) { rf = c; gf = x; }
    else if (hp < 2) { rf = x; gf = c; }
    else if (hp < 3) { gf = c; bf = x; }
    else if (hp < 4) { gf = x; bf = c; }
    else if (hp < 5) { rf = x; bf = c; }
    else             { rf = c; bf = x; }
    int ri = (int)((rf + m) * 255 + 0.5f);
    int gi = (int)((gf + m) * 255 + 0.5f);
    int bi = (int)((bf + m) * 255 + 0.5f);
    if (ri > 255) ri = 255; if (gi > 255) gi = 255; if (bi > 255) bi = 255;
    if (ri < 0)   ri = 0;   if (gi < 0)   gi = 0;   if (bi < 0)   bi = 0;
    out->r = (uint8_t)ri; out->g = (uint8_t)gi; out->b = (uint8_t)bi;
}

/* Equirectangular envmap sample, RGB. Direction may point anywhere
 * (mirror reflections aren't constrained to forward). yaw_offset_q4
 * lets the panorama drift independently of the camera tracking shot.
 * No Bayer trickery needed now — RGB565 truecolor smooths the lookups
 * between adjacent envmap texels naturally. */
static inline rgb8_t envmap_sample_dir(float dx, float dy, float dz)
{
    float phi   = atan2f(dx, dz);
    int   u_q4  = (int)((phi * (1.0f / (2.0f * 3.14159265f)) + 0.5f) * (ENVMAP_W * 16.0f));
    int   u_idx = ((u_q4 + yaw_offset_q4) >> 4) & (ENVMAP_W - 1);

    float xz  = sqrtf(dx * dx + dz * dz);
    float th  = atan2f(-dy, xz);
    int   v_q4 = (int)((th * (1.0f / 3.14159265f) + 0.5f) * (ENVMAP_H * 16.0f));
    int   v_idx = v_q4 >> 4;
    if (v_idx < 0)         v_idx = 0;
    if (v_idx >= ENVMAP_H) v_idx = ENVMAP_H - 1;

    return env_rgb[asset_envmap_data[v_idx * ENVMAP_W + u_idx]];
}

/* Shadow ray: P toward light, test all spheres. Returns 1 if blocked.
 * The light_* vector points FROM the surface TOWARD the light, so the
 * shadow ray direction is +light, not -light (an earlier sign flip
 * silently killed every shadow). */
static inline int sphere_shadow_blocked(float Px, float Py, float Pz, int excl_idx)
{
    float Sx = Px + light_x * 0.01f;
    float Sy = Py + light_y * 0.01f;
    float Sz = Pz + light_z * 0.01f;
    for (int i = 0; i < NUM_SPHERES; i++) {
        if (i == excl_idx) continue;
        float Cx = spheres[i].x - Sx;
        float Cy = spheres[i].y - Sy;
        float Cz = spheres[i].z - Sz;
        float tca = Cx*light_x + Cy*light_y + Cz*light_z;
        if (tca <= 0) continue;
        float c2 = Cx*Cx + Cy*Cy + Cz*Cz;
        float d2 = c2 - tca*tca;
        if (d2 < spheres[i].r2) return 1;
    }
    return 0;
}

static void update_spheres(uint32_t t_ms)
{
    float t = t_ms * 0.001f;

    /* Materials alternate around the orbit so two mirrors are never
     * adjacent — diffuse → mirror → diffuse → mirror. */
    static const material_t mats   [NUM_SPHERES] = {
        MAT_DIFFUSE_A, MAT_MIRROR, MAT_DIFFUSE_B, MAT_MIRROR
    };

    /* Single shared orbit, equal spacing → spheres never collide:
     * the minimum pair separation is √2·R, which exceeds the sum of
     * radii by a comfortable margin. Independent y-bob keeps the
     * formation from looking rigid. */
    const float ORB_R = 1.7f;
    const float ORB_W = 0.45f;
    const float ORB_Z = 4.8f;
    static const float fr_base[NUM_SPHERES] = { 1.05f, 1.05f, 1.05f, 1.05f };

    for (int i = 0; i < NUM_SPHERES; i++) {
        float a   = t * ORB_W + i * (3.14159265f * 0.5f);
        float bob = -0.25f + sinf(t * 0.9f + i * 2.1f) * 0.35f;
        spheres[i].x   = cosf(a) * ORB_R;
        spheres[i].y   = bob;
        spheres[i].z   = sinf(a) * ORB_R * 0.85f + ORB_Z;
        spheres[i].r   = fr_base[i] + sinf(t * 1.1f + i * 0.7f) * 0.10f;
        spheres[i].r2  = spheres[i].r * spheres[i].r;
        spheres[i].mat = mats[i];
    }

    /* Light direction. light_y negative ⇒ light from above (the y-axis
     * is positive-down in our screen-down convention). */
    float lx = sinf(t * 0.18f) * 0.7f;
    float ly = -0.7f - sinf(t * 0.11f) * 0.15f;
    float lz = -0.5f + cosf(t * 0.13f) * 0.4f;
    float lm = 1.0f / sqrtf(lx*lx + ly*ly + lz*lz);
    light_x = lx * lm;
    light_y = ly * lm;
    light_z = lz * lm;

    /* Camera orbits the cluster centre (0, 0, 4.8) and always looks
     * AT it — the spheres never leave the frame. */
    const float target_x = 0.0f;
    const float target_z = 4.8f;
    float cam_orbit_a = t * 0.17f;
    cam_x = sinf(cam_orbit_a) * 1.8f;
    cam_z = cosf(cam_orbit_a) * 0.8f + 1.2f;
    float yaw = atan2f(target_x - cam_x, target_z - cam_z);
    cam_yaw_cos = cosf(yaw);
    cam_yaw_sin = sinf(yaw);

    /* Animated diffuse tints — HSV hue drifts so the matte balls
     * breathe colour. */
    float hue_a = tint_hue_base[0] + t * 8.0f;
    while (hue_a >= 360.0f) hue_a -= 360.0f;
    hsv_to_rgb(hue_a, 0.75f, 1.0f, &tint_a);
    float hue_b = tint_hue_base[1] + t * 8.0f;
    while (hue_b >= 360.0f) hue_b -= 360.0f;
    hsv_to_rgb(hue_b, 0.75f, 1.0f, &tint_b);
}

/* --- raytrace ---------------------------------------------------- */

/* Trace one ray and return its colour as 8-bit RGB. Called for primary
 * rays and recursively (one bounce) for mirror reflections — that's
 * how a mirror sphere reflects the floor checker and other spheres,
 * not just the sky. excl_sph skips the originating sphere on
 * recursion to avoid floating-point self-intersection. */
static rgb8_t trace_ray(float Ox, float Oy, float Oz,
                        float Dx, float Dy, float Dz,
                        int excl_sph, int depth)
{
    int   hit_kind = 0;
    int   hit_sph  = -1;
    float t_best   = 1e30f;
    float Nx = 0, Ny = 0, Nz = 0;

    /* Sphere intersections — no bbox cull here, since recursive rays
     * can point anywhere. (The primary-ray caller still does a screen-
     * space bbox cull before invoking this to skip pure-background
     * pixels entirely.) */
    for (int i = 0; i < NUM_SPHERES; i++) {
        if (i == excl_sph) continue;
        float Lx = spheres[i].x - Ox;
        float Ly = spheres[i].y - Oy;
        float Lz = spheres[i].z - Oz;
        float tca = Lx*Dx + Ly*Dy + Lz*Dz;
        if (tca <= 0) continue;
        float l2 = Lx*Lx + Ly*Ly + Lz*Lz;
        float d2 = l2 - tca*tca;
        float r2 = spheres[i].r2;
        if (d2 > r2) continue;
        float thc = sqrtf(r2 - d2);
        float t   = tca - thc;
        if (t <= 1e-3f) continue;
        if (t < t_best) {
            t_best   = t;
            hit_kind = 1;
            hit_sph  = i;
            float Px = Ox + Dx * t;
            float Py = Oy + Dy * t;
            float Pz = Oz + Dz * t;
            float invR = 1.0f / spheres[i].r;
            Nx = (Px - spheres[i].x) * invR;
            Ny = (Py - spheres[i].y) * invR;
            Nz = (Pz - spheres[i].z) * invR;
        }
    }

    if (Dy > 1e-4f && Oy < FLOOR_Y_F) {
        float t_p = (FLOOR_Y_F - Oy) / Dy;
        if (t_p > 1e-3f && t_p < t_best) {
            t_best   = t_p;
            hit_kind = 2;
            hit_sph  = -1;
            Nx = 0; Ny = -1; Nz = 0;
        }
    }

    if (hit_kind == 0) {
        return envmap_sample_dir(Dx, Dy, Dz);
    }

    if (hit_kind == 1) {
        sphere_t *sp = &spheres[hit_sph];
        float Px = Ox + Dx * t_best;
        float Py = Oy + Dy * t_best;
        float Pz = Oz + Dz * t_best;
        if (sp->mat == MAT_MIRROR) {
            float dn = Dx*Nx + Dy*Ny + Dz*Nz;
            float Rx = Dx - 2.0f * dn * Nx;
            float Ry = Dy - 2.0f * dn * Ny;
            float Rz = Dz - 2.0f * dn * Nz;
            if (depth > 0) {
                return trace_ray(Px, Py, Pz, Rx, Ry, Rz, hit_sph, depth - 1);
            }
            return envmap_sample_dir(Rx, Ry, Rz);
        }
        /* Diffuse: ambient + diffuse term, optionally shadowed.
         * Smooth gradient in RGB — no palette stepping. */
        float ndl = Nx*light_x + Ny*light_y + Nz*light_z;
        if (ndl < 0) ndl = 0;
        if (ndl > 0 && sphere_shadow_blocked(Px, Py, Pz, hit_sph)) {
            ndl = 0;
        }
        const float ambient = 0.18f;
        float bright = ambient + (1.0f - ambient) * ndl;
        const rgb8_t *t = (sp->mat == MAT_DIFFUSE_A) ? &tint_a : &tint_b;
        rgb8_t out;
        out.r = (uint8_t)(t->r * bright);
        out.g = (uint8_t)(t->g * bright);
        out.b = (uint8_t)(t->b * bright);
        return out;
    }

    /* Floor: warm bone-light tile vs cool-dark tile, crossfaded with a
     * teal horizon fog to recede into the envmap. Sphere shadows tint
     * the lit floor toward the dark colour. */
    float Px = Ox + Dx * t_best;
    float Pz = Oz + Dz * t_best;
    int tile_x = (int)floorf(Px);
    int tile_z = (int)floorf(Pz);
    int is_dark = (tile_x ^ tile_z) & 1;
    static const rgb8_t tile_light = { 220, 200, 175 };
    static const rgb8_t tile_dark  = {  60,  35,  70 };
    static const rgb8_t fog_color  = {  45,  90, 110 };
    const rgb8_t *tile = is_dark ? &tile_dark : &tile_light;
    float fog01 = t_best * (1.0f / 18.0f);
    if (fog01 > 1.0f) fog01 = 1.0f;
    /* Linear mix tile_colour → fog_colour as fog01 goes 0 → 1. */
    float kt = 1.0f - fog01;
    int fr = (int)(tile->r * kt + fog_color.r * fog01);
    int fg = (int)(tile->g * kt + fog_color.g * fog01);
    int fb_ = (int)(tile->b * kt + fog_color.b * fog01);
    if (sphere_shadow_blocked(Px, FLOOR_Y_F, Pz, -1)) {
        /* Shadowed: darken toward 35 % brightness. */
        fr = (fr * 90) >> 8;
        fg = (fg * 90) >> 8;
        fb_ = (fb_ * 90) >> 8;
    }
    rgb8_t out = { (uint8_t)fr, (uint8_t)fg, (uint8_t)fb_ };
    return out;
}

/* --- text overlay ------------------------------------------------ */

/* Stamp an 8×8 font string with a 1-pixel drop shadow onto the RGB565
 * framebuffer. The fill/shadow RGB triplets are blended with the
 * existing framebuffer pixel at `alpha_q8` (0..256) — so when the
 * banner fades, it dissolves into whatever's actually behind it
 * (spheres, floor, sky) rather than into black. At alpha_q8 = 256 the
 * text is fully opaque; at 0 it's invisible. */
static inline void blend_pixel(uint16_t *p, int tr, int tg, int tb, int alpha_q8)
{
    uint16_t cur = *p;
    int cr = rgb565_r8(cur);
    int cg = rgb565_g8(cur);
    int cb = rgb565_b8(cur);
    /* lerp: out = cur + (text - cur) * alpha / 256 */
    int r = cr + (((tr - cr) * alpha_q8) >> 8);
    int g = cg + (((tg - cg) * alpha_q8) >> 8);
    int b = cb + (((tb - cb) * alpha_q8) >> 8);
    *p = pack565(r, g, b);
}

static void stamp_banner(uint16_t *fb, const char *s, int x0, int y0,
                         int fr, int fg, int fb_,
                         int sr, int sg, int sb,
                         int alpha_q8)
{
    if (alpha_q8 <= 0) return;
    for (int ci = 0; s[ci]; ci++) {
        const uint8_t *g = font8x8_glyph(s[ci]);
        int cx = x0 + ci * 8;
        if (cx >= FB_W) break;
        for (int row = 0; row < 8; row++) {
            uint8_t bits = g[row];
            int y = y0 + row;
            if ((unsigned)y >= FB_H) continue;
            for (int col = 0; col < 8; col++) {
                if (!((bits >> (7 - col)) & 1)) continue;
                int x = cx + col;
                if ((unsigned)x >= FB_W) continue;
                /* Drop shadow blended first (will be partly overwritten
                 * by the fill on the next-cell's pixel). */
                int sx = x + 1, sy = y + 1;
                if ((unsigned)sx < FB_W && (unsigned)sy < FB_H) {
                    blend_pixel(&fb[sy * FB_W + sx], sr, sg, sb, alpha_q8);
                }
                blend_pixel(&fb[y * FB_W + x], fr, fg, fb_, alpha_q8);
            }
        }
    }
}

/* --- effect lifecycle -------------------------------------------- */

static void spheres_init(void)
{
    /* Pre-expand the envmap palette to RGB triplets so per-pixel
     * envmap_sample_dir() doesn't need to unpack RGB565. */
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_envmap_pal[i];
        env_rgb[i].r = (uint8_t)rgb565_r8(c);
        env_rgb[i].g = (uint8_t)rgb565_g8(c);
        env_rgb[i].b = (uint8_t)rgb565_b8(c);
    }
}

static void spheres_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    int alpha = 256;
    if (t_into < FADE_IN_MS) {
        alpha = (int)(t_into * 256 / FADE_IN_MS);
    } else if (t_into >= FADE_OUT_AT) {
        uint32_t into = t_into - FADE_OUT_AT;
        alpha = (into >= FADE_OUT_MS) ? 0 : 256 - (int)(into * 256 / FADE_OUT_MS);
    }
    alpha_q8 = alpha;

    update_spheres(t_into);

    /* Slow envmap-yaw drift, Q4 fractional pixels. */
    yaw_offset_q4 = (int)(((uint64_t)t_into * (ENVMAP_W * 16) / SCENE_LEN_MS)
                          & ((ENVMAP_W * 16) - 1));

    /* Pre-project sphere screen-space bboxes for primary-ray cull.
     * focal = FB_W/2 for 90° FOV. */
    int  scr_cx [NUM_SPHERES];
    int  scr_cy [NUM_SPHERES];
    int  scr_r2 [NUM_SPHERES];
    int  scr_active[NUM_SPHERES];
    const float half_focal = FB_W * 0.5f;
    for (int i = 0; i < NUM_SPHERES; i++) {
        scr_active[i] = 0;
        float wx = spheres[i].x - cam_x;
        float wz = spheres[i].z - cam_z;
        float cx_view =  cam_yaw_cos * wx - cam_yaw_sin * wz;
        float cz_view =  cam_yaw_sin * wx + cam_yaw_cos * wz;
        if (cz_view <= 0.5f) continue;
        float inv_z = 1.0f / cz_view;
        int sx = (int)(FB_W * 0.5f + cx_view      * half_focal * inv_z);
        int sy = (int)(FB_H * 0.5f + spheres[i].y * half_focal * inv_z);
        int rr = (int)(spheres[i].r               * half_focal * inv_z) + 2;
        scr_cx[i] = sx;
        scr_cy[i] = sy;
        scr_r2[i] = rr * rr;
        scr_active[i] = 1;
    }

    uint16_t *fb = vga_160_back_buffer();
    const float inv_focal = 1.0f / half_focal;
    const float Ox = cam_x;
    const float Oy = 0.0f;
    const float Oz = cam_z;

    for (int py = 0; py < FB_H; py++) {
        float dy_f = (py - FB_H * 0.5f) * inv_focal;
        uint16_t *fb_row = fb + py * FB_W;
        for (int px = 0; px < FB_W; px++) {
            float dx_f = (px - FB_W * 0.5f) * inv_focal;
            float dz_f = 1.0f;

            float mag2 = dx_f * dx_f + dy_f * dy_f + dz_f * dz_f;
            float inv  = 1.0f / sqrtf(mag2);
            float Dvx = dx_f * inv;
            float Dvy = dy_f * inv;
            float Dvz = dz_f * inv;
            float Dx =  cam_yaw_cos * Dvx + cam_yaw_sin * Dvz;
            float Dy =  Dvy;
            float Dz = -cam_yaw_sin * Dvx + cam_yaw_cos * Dvz;

            /* Primary-ray bbox cull: if no sphere bbox contains this
             * pixel, take the cheap "floor or sky" path directly. */
            int may_hit = 0;
            for (int i = 0; i < NUM_SPHERES; i++) {
                if (!scr_active[i]) continue;
                int dpx = px - scr_cx[i];
                int dpy = py - scr_cy[i];
                if (dpx*dpx + dpy*dpy <= scr_r2[i]) { may_hit = 1; break; }
            }

            rgb8_t out;
            if (may_hit) {
                out = trace_ray(Ox, Oy, Oz, Dx, Dy, Dz, -1, 1);
            } else if (Dy > 1e-4f) {
                /* Pure floor or sky — inline the cheap path. */
                float t_p = FLOOR_Y_F / Dy;
                float Px = Ox + Dx * t_p;
                float Pz = Oz + Dz * t_p;
                int tile_x = (int)floorf(Px);
                int tile_z = (int)floorf(Pz);
                int is_dark = (tile_x ^ tile_z) & 1;
                static const rgb8_t tile_light = { 220, 200, 175 };
                static const rgb8_t tile_dark  = {  60,  35,  70 };
                static const rgb8_t fog_color  = {  45,  90, 110 };
                const rgb8_t *tile = is_dark ? &tile_dark : &tile_light;
                float fog01 = t_p * (1.0f / 18.0f);
                if (fog01 > 1.0f) fog01 = 1.0f;
                float kt = 1.0f - fog01;
                int fr = (int)(tile->r * kt + fog_color.r * fog01);
                int fg = (int)(tile->g * kt + fog_color.g * fog01);
                int fb_ = (int)(tile->b * kt + fog_color.b * fog01);
                if (sphere_shadow_blocked(Px, FLOOR_Y_F, Pz, -1)) {
                    fr = (fr * 90) >> 8;
                    fg = (fg * 90) >> 8;
                    fb_ = (fb_ * 90) >> 8;
                }
                out.r = (uint8_t)fr; out.g = (uint8_t)fg; out.b = (uint8_t)fb_;
            } else {
                out = envmap_sample_dir(Dx, Dy, Dz);
            }

            fb_row[px] = pack565_faded(out.r, out.g, out.b);
        }
    }

    /* Boast banner — typewriter intro, brief hold, fade out. Stays
     * subtle (top-left corner, one short line) and only present in
     * the first ~10 s of the scene; the rest of the time the spheres
     * have the screen to themselves.
     *
     *   0  .. 1.5 s  hidden (let the scene establish)
     *   1.5..  4 s   typewriter: one char every ~140 ms
     *     4 ..  8 s   hold at full brightness
     *     8 .. 11 s   fade out over 3 s
     *    11+         hidden until the end fade-out of the scene
     */
    static const char banner_str[] = "REALTIME RAYTRACER";
    const int banner_len = (int)(sizeof banner_str - 1);
    int   chars_visible = 0;
    int   text_q8       = 0;
    if (t_into >= 1500 && t_into < 4000) {
        chars_visible = (int)((t_into - 1500) * banner_len / 2500);
        if (chars_visible > banner_len) chars_visible = banner_len;
        text_q8 = 256;
    } else if (t_into >= 4000 && t_into < 8000) {
        chars_visible = banner_len;
        text_q8 = 256;
    } else if (t_into >= 8000 && t_into < 11000) {
        chars_visible = banner_len;
        text_q8 = 256 - (int)((t_into - 8000) * 256 / 3000);
    }
    /* Combine with the scene-level fade so the banner respects the
     * scene's own boundary fades too. */
    text_q8 = (text_q8 * alpha_q8) >> 8;
    if (text_q8 > 0 && chars_visible > 0) {
        /* Render only the visible prefix. The banner blends with the
         * existing fb pixel at text_q8 alpha, so the fade-out lerps
         * cleanly into the spheres/floor/sky behind it — not to black. */
        char buf[32];
        if (chars_visible > (int)sizeof(buf) - 1) chars_visible = (int)sizeof(buf) - 1;
        memcpy(buf, banner_str, chars_visible);
        buf[chars_visible] = 0;
        stamp_banner(fb, buf, 4, 4,
                     240, 235, 215,   /* warm-white fill */
                      10,   6,  20,   /* deep-plum shadow */
                     text_q8);
    }
}

static void spheres_done(void) { /* nothing to clean up */ }

const effect_t fx_spheres_real = {
    .name  = "spheres",
    .mode  = MODE_160,
    .init  = spheres_init,
    .frame = spheres_frame,
    .done  = spheres_done,
};
