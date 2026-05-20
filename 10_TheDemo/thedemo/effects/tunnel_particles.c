/* Tunnel + particles forming endcard text —
 * scene 7 (3:10 – 3:40, MODE_320).
 *
 * Two layered effects, blended into one MODE_320 framebuffer:
 *
 *   1. Tunnel.  Classic raster effect — for each screen pixel we look
 *      up a precomputed (angle, distance) → texture (u, v) and sample
 *      the tunnel_tex asset. Adding a per-frame phase offset to angle
 *      and distance scrolls the texture in two axes ("flying through
 *      a wallpaper tube"). A slow twist on the angle term gives the
 *      whole tube a corkscrew motion.
 *
 *   2. Particles.  500 particles spawned at random positions, each
 *      drifting outward in their own direction at first. In the last
 *      ~10 seconds of the scene they're slowly attracted toward target
 *      positions sampled from the asset_endcard_mask shape — they
 *      converge to spell out "SLOP" via accumulated screen sparkle.
 *
 *   3. Final 4 seconds: particles fully formed into the text shape,
 *      tunnel dims away.
 *
 * Palette: keep the tunnel_tex's own 256-colour palette for the tube,
 * and reserve a handful of slots near the top for the particle colour
 * and any accent. The 8bpp tunnel_tex was packed with reserved=0, so
 * we just overwrite slots 248..255 for particles. Acceptable since the
 * quantizer rarely assigns frequent texels to the very last few slots.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define FB_W                VGA_320_W
#define FB_H                VGA_320_H
#define TEX_W               ASSET_TUNNEL_TEX_W
#define TEX_H               ASSET_TUNNEL_TEX_H

#define SCENE_LEN_MS        35000
#define FADE_IN_MS           2000
#define FADE_OUT_MS          3000
#define FADE_OUT_AT         (SCENE_LEN_MS - FADE_OUT_MS)

/* Particles start scattering at scene start and begin migrating toward
 * their endcard-mask targets at this offset. Scaled to keep
 * convergence in the last ~40 % of the scene as the scene length
 * shifts. */
#define PARTICLE_CONVERGE_AT  21000

#define PARTICLE_DIM        247        /* dim trailing tail / deep-tunnel sparkles */
#define PARTICLE_COLOUR     248        /* primary warm-white particle paint */
#define PARTICLE_GLOW       249        /* hot core for near / convergence-arrival flash */

#define NUM_PARTICLES       800

/* Internal rendering grid. Each row of the ray-vs-ellipse cast produces
 * one fb scanline that is then line-doubled into the 240-row output.
 * Horizontal is full 320 — vertical doubling alone isn't visible as the
 * "chunky" pattern the previous half-res render produced, but adding
 * full horizontal resolution is well within the cycle budget on
 * RP2350's FPU. */
#define INT_W  VGA_320_W          /* 320 — full horizontal */
#define INT_H  (VGA_320_H / 2)    /* 120 — half vertical, line-doubled */

/* --- particles ---------------------------------------------------- */

/* Particles live in *tunnel 3-space* so they fly through the tube with
 * the same forward motion the tunnel itself has — no longer an
 * unrelated drifting layer:
 *
 *   - x_q4, y_q4 are signed offsets from the tunnel centre in Q4 of
 *     pixel-equivalent units. Stay constant per particle.
 *   - z_q4 is a depth value, Q4. Each frame z decreases (particle
 *     approaches viewer). When too close, respawn deep in the tube.
 *   - Screen projection: sx = cx + x*FOCAL/z, sy = cy + y*FOCAL/z.
 *     Particles spread radially outward as they approach the viewer,
 *     exactly matching the tunnel's perspective.
 *
 *   On convergence (last ~10 s), we freeze the depth and ease the
 *   screen position toward an endcard-mask target. */
typedef struct {
    int16_t x_q4, y_q4;       /* tunnel-space offset from centre */
    int16_t z_q4;             /* depth: smaller = nearer */
    int16_t tx, ty;           /* endcard target (screen pixels) */
    uint8_t frozen;           /* set once we switch to screen-space pull */
    int16_t sx_q4, sy_q4;     /* screen-space pos during convergence */
} particle_t;

static particle_t particles[NUM_PARTICLES];

#define FOCAL_INT       96         /* "focal length" — larger = tighter perspective */
#define Z_NEAR_Q4       24         /* closest before respawn (= 1.5) */
#define Z_FAR_Q4        4096       /* furthest spawn (= 256) */
#define Z_DRIFT_BASE_Q4 10         /* per-frame z decrease — 67% faster zoom */
#define XY_SPREAD_Q4    1600       /* ±100-px equivalent spread of (x,y) */

/* Per-particle speed jitter — each particle gets `base + (id & 7)` so 8
 * speed classes mix together, particles fly past each other rather than
 * all advancing in lockstep. */
static inline int z_drift_for(int idx) { return Z_DRIFT_BASE_Q4 + (idx & 7); }

/* Tiny PRNG so we don't depend on rand() across builds. */
static uint32_t prng_state = 0x12345678u;
static inline uint32_t prng_u32(void)
{
    prng_state ^= prng_state << 13;
    prng_state ^= prng_state >> 17;
    prng_state ^= prng_state << 5;
    return prng_state;
}

/* 1bpp mask read for the endcard target. The asset is 300×80; we
 * center it on the 320×240 framebuffer at (10, 130).
 *
 * PIL's '1'-mode packer pads each row up to the next whole byte, so
 * for a 300-pixel-wide image the row stride is 38 bytes (304 bits, 4
 * padding bits at the right edge), NOT 37. Computing the stride as
 * W/8 silently produces a scrambled mask that shifts left by one byte
 * each row. Always round up. */
#define ENDCARD_X0          ((FB_W - ASSET_ENDCARD_MASK_W) / 2)
#define ENDCARD_Y0          130
#define ENDCARD_MASK_STRIDE ((ASSET_ENDCARD_MASK_W + 7) / 8)

static inline int endcard_bit(int x, int y)
{
    if ((unsigned)x >= ASSET_ENDCARD_MASK_W) return 0;
    if ((unsigned)y >= ASSET_ENDCARD_MASK_H) return 0;
    int byte_idx = y * ENDCARD_MASK_STRIDE + (x >> 3);
    return (asset_endcard_mask_data[byte_idx] >> (7 - (x & 7))) & 1;
}

/* Pick a random pixel inside the endcard mask (the white pixels of the
 * 1bpp text). We sample rejection-style — pretty fast since the mask
 * is mostly white. */
static void pick_endcard_target(int *tx, int *ty)
{
    for (int tries = 0; tries < 64; tries++) {
        int x = (int)(prng_u32() % ASSET_ENDCARD_MASK_W);
        int y = (int)(prng_u32() % ASSET_ENDCARD_MASK_H);
        if (endcard_bit(x, y)) {
            *tx = ENDCARD_X0 + x;
            *ty = ENDCARD_Y0 + y;
            return;
        }
    }
    /* Fallback: centre of canvas. */
    *tx = FB_W / 2;
    *ty = ENDCARD_Y0 + ASSET_ENDCARD_MASK_H / 2;
}

/* Place a particle somewhere in the tunnel volume at the given depth.
 * (x, y) are random offsets from the tube centre — wide spread so
 * particles appear along the walls, not bunched at the centre. */
static void spawn_particle(int i, int z_init_q4)
{
    int rx = (int)(prng_u32() % (2 * XY_SPREAD_Q4)) - XY_SPREAD_Q4;
    int ry = (int)(prng_u32() % (2 * XY_SPREAD_Q4)) - XY_SPREAD_Q4;
    particles[i].x_q4 = (int16_t)rx;
    particles[i].y_q4 = (int16_t)ry;
    particles[i].z_q4 = (int16_t)z_init_q4;
    particles[i].frozen = 0;

    int tx, ty;
    pick_endcard_target(&tx, &ty);
    particles[i].tx = (int16_t)tx;
    particles[i].ty = (int16_t)ty;
}

/* --- palette ------------------------------------------------------ */

static uint8_t tex_r[256], tex_g[256], tex_b[256];

static void load_tex_palette_cache(void)
{
    for (int i = 0; i < 256; i++) {
        uint16_t c = asset_tunnel_tex_pal[i];
        tex_r[i] = (uint8_t)rgb565_r8(c);
        tex_g[i] = (uint8_t)rgb565_g8(c);
        tex_b[i] = (uint8_t)rgb565_b8(c);
    }
}

/* tunnel_tex is packed with reserved=224 — only 32 base colours. At
 * scene init we lay out 7 brightness ramps of those 32, so the palette
 * holds slot[i] = (shade << 5) | base where shade ∈ 0..6 and base ∈
 * 0..31. That occupies slots 0..223 (= 7 × 32) and leaves slots
 * 224..255 free — used for particle colours so they never alias deep-
 * fog texture pixels (which is what produced the phantom "white stars"
 * in the previous 8-shade layout). */
#define TEX_COLORS    32
#define SHADE_LEVELS  7

/* Brightness for each shade band, Q8. Index 0 = nearest, 6 = deepest.
 * uint16_t because 256 is "100%, no fade" — it would overflow a uint8. */
static const uint16_t shade_q8[SHADE_LEVELS] = {
    256, 215, 175, 138, 102,  68,  38
};

/* Bayer-4 ordered dither — values 0..15, directly comparable against the
 * Q4 fractional position between adjacent shade bands. Used by the
 * depth-fog pixel selector and the endcard mask reveal. */
static const uint8_t bayer4_tunnel[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5,
};

static void apply_palette(int alpha_q8)
{
    if (alpha_q8 < 0) alpha_q8 = 0;
    if (alpha_q8 > 256) alpha_q8 = 256;
    for (int s = 0; s < SHADE_LEVELS; s++) {
        int eff = (alpha_q8 * shade_q8[s]) >> 8;
        for (int i = 0; i < TEX_COLORS; i++) {
            uint8_t r = (uint8_t)((tex_r[i] * eff) >> 8);
            uint8_t g = (uint8_t)((tex_g[i] * eff) >> 8);
            uint8_t b = (uint8_t)((tex_b[i] * eff) >> 8);
            vga_320_palette_set(s * TEX_COLORS + i, r, g, b);
        }
    }
    /* Three particle slots — depth-graded so the flying motion reads
     * as a true 3D swarm: GLOW is the hot near-camera core, COLOUR is
     * the mid-distance "fill" body, DIM is the far-deep-tunnel sparkle
     * and motion-trail. All fade with the rest. */
    vga_320_palette_set(PARTICLE_GLOW,
        (uint8_t)((255 * alpha_q8) >> 8),
        (uint8_t)((250 * alpha_q8) >> 8),
        (uint8_t)((230 * alpha_q8) >> 8));
    vga_320_palette_set(PARTICLE_COLOUR,
        (uint8_t)((220 * alpha_q8) >> 8),
        (uint8_t)((205 * alpha_q8) >> 8),
        (uint8_t)((180 * alpha_q8) >> 8));
    vga_320_palette_set(PARTICLE_DIM,
        (uint8_t)((110 * alpha_q8) >> 8),
        (uint8_t)((120 * alpha_q8) >> 8),
        (uint8_t)((130 * alpha_q8) >> 8));
}

/* --- effect lifecycle --------------------------------------------- */

static void tunnel_init(void)
{
    load_tex_palette_cache();
    apply_palette(0);
    prng_state = 0x12345678u;
    for (int i = 0; i < NUM_PARTICLES; i++) {
        /* Stagger initial z so particles appear at varying depths at
         * t=0 rather than all spawning together. */
        int z = Z_NEAR_Q4 + (int)(prng_u32() % (Z_FAR_Q4 - Z_NEAR_Q4));
        spawn_particle(i, z);
    }
}

static void tunnel_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    int alpha = 256;
    if (t_into < FADE_IN_MS) {
        alpha = (int)(t_into * 256 / FADE_IN_MS);
    } else if (t_into >= FADE_OUT_AT) {
        uint32_t into = t_into - FADE_OUT_AT;
        alpha = (into >= FADE_OUT_MS) ? 0 : 256 - (int)(into * 256 / FADE_OUT_MS);
    }
    apply_palette(alpha);

    /* Real per-pixel raycast against an animated elliptical tube. The
     * camera flies through 3D space — translates in x/y, rolls in
     * yaw, and pushes forward along z — while the tube's cross-section
     * breathes via independently-modulated a, b ellipse axes AND
     * rotates around the z-axis (ellipse "spins" so the long axis
     * sweeps through the orientation circle).
     *
     * For each pixel:
     *   D    = ((sx, sy)/focal, +z)              ray direction in world
     *   D'   = R(-φ) · D                          rotate into ellipse frame
     *   cam' = R(-φ) · cam                        camera in ellipse frame
     *   Find smallest positive t such that
     *     (cam'_x + D'_x·t)² / a² + (cam'_y + D'_y·t)² / b² = 1
     *   z_hit = cam_z + t  (D_z = 1 in this parameterisation)
     *   u     = atan2(hit_y/b, hit_x/a)          azimuth (ellipse frame)
     *   v     = z_hit                            depth along axis
     *   shade = clamp(t / fog_range, 0, 1)       true depth fog
     *
     * Yaw and ellipse-φ are combined into one rotation since both act
     * on the same (fx_scr, fy_scr) → (D'_x, D'_y) transform. Net visual:
     * texture-on-tube-wall rotates with the camera while the ellipse
     * itself spins in world space — reads as a tube whose oval mouth
     * rolls past you as you fly through.
     *
     * Cost at 320×120: ≈ 90 cy/pixel × 38 400 = 3.5 M cy ≈ 12 ms at
     * 300 MHz. Within the 16.67 ms vblank budget. */
    float t = t_into * 0.001f;
    /* Ellipse axes. The b-centre is pulled below 1 so the resting state
     * is already a clearly squashed oval, not a near-circle. Amplitudes
     * are dialled back slightly from the previous "DRAMATIC" pass so
     * the minimum tube width can support the camera drift without
     * clipping the wall. */
    float a     = 1.00f + 0.30f * sinf(t * 0.40f);    /* 0.70 .. 1.30 */
    float b     = 0.70f + 0.22f * cosf(t * 0.27f);    /* 0.48 .. 0.92 */

    /* Camera drift is bounded by the smaller current ellipse axis with
     * a 0.5 safety factor so cam_x²/a² + cam_y²/b² stays well below 1
     * for any combination of (cam, rotation, a, b). Without this the
     * camera periodically clipped through the wall when the ellipse
     * was at its narrowest. */
    float min_axis = (a < b) ? a : b;
    float cam_amp  = min_axis * 0.55f;
    float cam_x = cam_amp * sinf(t * 0.31f + 0.7f);
    float cam_y = cam_amp * cosf(t * 0.23f);
    float cam_z = t * 1.8f;

    /* Ellipse rotation: glider-style multi-frequency sway, no constant
     * angular velocity. Three non-commensurate sinusoids combine to
     * give a motion that banks left, levels, banks right, holds —
     * never repeating, never rotating fully through 360°. Total
     * angular range ≈ ±55° (±0.97 rad). */
    float ellipse_phi = 0.55f * sinf(t * 0.21f)
                      + 0.28f * sinf(t * 0.39f + 1.3f)
                      + 0.14f * sinf(t * 0.73f + 0.7f);
    float yaw         = 0.40f * sinf(t * 0.13f);
    float total_rot   = yaw + ellipse_phi;
    float rcos  = cosf(total_rot);
    float rsin  = sinf(total_rot);
    float inv_a2 = 1.0f / (a * a);
    float inv_b2 = 1.0f / (b * b);

    /* Camera transformed into ellipse-aligned frame, once per frame. */
    float cam_xe =  rcos * cam_x + rsin * cam_y;
    float cam_ye = -rsin * cam_x + rcos * cam_y;
    float C_q    = cam_xe * cam_xe * inv_a2 + cam_ye * cam_ye * inv_b2 - 1.0f;

    /* Focal scaled with new INT_W so horizontal FOV is preserved
     * (same ±1.0 fx_scr range as the old half-res renderer). The
     * vertical step is doubled because each iy now spans two output
     * rows after line-doubling — preserves 4:3 aspect. */
    const float focal      = (float)(INT_W / 2);                     /* 160 */
    const float fx_step    = 1.0f / focal;
    const float fy_step    = 2.0f / focal;
    const float u_scale    = 256.0f / (2.0f * 3.14159265f);
    const float v_scale    = 256.0f / 4.0f;
    const float fog_range  = 6.0f;

    uint8_t *fb = vga_320_back_buffer();
    for (int iy = 0; iy < INT_H; iy++) {
        float fy_scr = (iy - INT_H * 0.5f) * fy_step;
        uint8_t *fb_row0 = &fb[(iy * 2)     * FB_W];
        uint8_t *fb_row1 = &fb[(iy * 2 + 1) * FB_W];
        const uint8_t *bayer_row = &bayer4_tunnel[(iy & 3) << 2];
        for (int ix = 0; ix < INT_W; ix++) {
            float fx_scr = (ix - INT_W * 0.5f) * fx_step;

            /* Rotate ray direction into ellipse frame (yaw + spin in
             * one rotation). */
            float Dx = rcos * fx_scr + rsin * fy_scr;
            float Dy = -rsin * fx_scr + rcos * fy_scr;

            /* Quadratic A t² + 2 B t + C = 0 (using "half-B" form). */
            float A_q = Dx * Dx * inv_a2 + Dy * Dy * inv_b2;
            float B_q = cam_xe * Dx * inv_a2 + cam_ye * Dy * inv_b2;
            float disc = B_q * B_q - A_q * C_q;

            float t_hit;
            if (A_q < 1e-5f || disc < 0.0f) {
                t_hit = fog_range;
            } else {
                t_hit = (-B_q + sqrtf(disc)) / A_q;
                if (t_hit < 0.0f)        t_hit = 0.0f;
                if (t_hit > fog_range)   t_hit = fog_range;
            }

            float hx = cam_xe + Dx * t_hit;
            float hy = cam_ye + Dy * t_hit;
            float hz = cam_z + t_hit;

            /* u: angle around the tube in ellipse frame (axes-normalised
             * so the texture sits flat against the wall regardless of
             * eccentricity). v: depth. */
            float ang = atan2f(hy / b, hx / a);
            int u_idx = (int)(ang * u_scale + 128.0f) & (TEX_W - 1);
            int v_idx = (int)(hz  * v_scale)        & (TEX_H - 1);

            uint8_t base = asset_tunnel_tex_data[v_idx * TEX_W + u_idx]; /* 0..31 */

            /* Depth shading: Bayer-4 dither between adjacent shade
             * bands so the 7-band fog reads continuously across the
             * full-res pixels. */
            float fog = t_hit * (1.0f / fog_range);
            if (fog > 1.0f) fog = 1.0f;
            int   fog_q4   = (int)(fog * (SHADE_LEVELS - 1) * 16.0f + 0.5f);
            int   shade_lo = fog_q4 >> 4;
            int   shade_hi = shade_lo + 1;
            int   shade_f  = fog_q4 & 0xF;
            if (shade_lo >= SHADE_LEVELS) { shade_lo = SHADE_LEVELS - 1; shade_f = 0; }
            if (shade_hi >= SHADE_LEVELS) { shade_hi = SHADE_LEVELS - 1; }
            int   shade    = (bayer_row[ix & 3] < shade_f) ? shade_hi : shade_lo;
            uint8_t c = (uint8_t)((shade << 5) | (base & 0x1F));

            /* 1×2 cell — full horizontal resolution, line-doubled. */
            fb_row0[ix] = c;
            fb_row1[ix] = c;
        }
    }

    /* Convergence weight: 0 at PARTICLE_CONVERGE_AT, 1.0 by scene end. */
    int converge_q8 = 0;
    if (t_into > PARTICLE_CONVERGE_AT) {
        converge_q8 = (int)(((t_into - PARTICLE_CONVERGE_AT) * 256)
                          / (SCENE_LEN_MS - PARTICLE_CONVERGE_AT));
        if (converge_q8 > 256) converge_q8 = 256;
    }

    /* Fade the endcard mask in as if pixels are physically "arriving"
     * one at a time — each mask pixel has its own deterministic random
     * threshold (a hash of its position), so the reveal is an organic
     * noise pattern instead of the obviously-regular Bayer grid that
     * was here before. Pixels just past their threshold glow brighter
     * for a short window (PARTICLE_GLOW slot) before settling to the
     * final PARTICLE_COLOUR — reads as a brief flash as the pixel lands.
     *
     * Convergence "runs hot": we stretch the mapping so most pixels
     * have arrived well before the scene ends, giving the text a few
     * seconds to sit fully visible while particles still finish their
     * journey. */
    if (converge_q8 > 0) {
        int conv_extended = converge_q8 + (converge_q8 >> 2);   /* up to 320 */
        if (conv_extended > 280) conv_extended = 280;
        for (int sy = 0; sy < ASSET_ENDCARD_MASK_H; sy++) {
            int dy = ENDCARD_Y0 + sy;
            if ((unsigned)dy >= FB_H) continue;
            uint8_t *row = fb + dy * FB_W;
            for (int sx = 0; sx < ASSET_ENDCARD_MASK_W; sx++) {
                if (!endcard_bit(sx, sy)) continue;
                /* xorshift-style hash for a stable random threshold. */
                uint32_t h = (uint32_t)sx * 374761393u
                           + (uint32_t)sy * 668265263u;
                h ^= h >> 13;
                h *= 1274126177u;
                h ^= h >> 16;
                int threshold = (int)(h & 0xFF);          /* 0..255, stable per pixel */
                int delta = conv_extended - threshold;
                if (delta < 0) continue;                  /* not yet arrived */
                int dx = ENDCARD_X0 + sx;
                if ((unsigned)dx >= FB_W) continue;
                /* "Arrival flash": for ~6 frames worth of conv_q8 growth
                 * after crossing the threshold, use the brighter glow
                 * slot, then settle into the regular fill colour. */
                row[dx] = (delta < 20) ? PARTICLE_GLOW : PARTICLE_COLOUR;
            }
        }
    }

    /* Update + stamp particles. Two stages:
     *   1. Tunnel-flight: decrement z, perspective-project into screen.
     *   2. Convergence (last ~12 s): freeze at viewer plane, ease
     *      screen pos toward endcard target. */
    /* Particles use the same camera position as the tube raycast so
     * they appear to fly through the same 3D space. cam_x/cam_y are in
     * tube-radius units; map to screen pixels by multiplying by the
     * tunnel's effective screen radius (~half-screen). */
    const int fb_cx = FB_W / 2 - (int)(cam_x * (FB_W / 2));
    const int fb_cy = FB_H / 2 - (int)(cam_y * (FB_H / 2));
    for (int i = 0; i < NUM_PARTICLES; i++) {
        particle_t *p = &particles[i];

        if (!p->frozen) {
            /* Fly forward with per-particle speed jitter for visual variety. */
            int z = p->z_q4 - z_drift_for(i);
            if (z < Z_NEAR_Q4) {
                spawn_particle(i, Z_FAR_Q4 - (int)(prng_u32() % 256));
                continue;
            }
            p->z_q4 = (int16_t)z;

            /* Perspective project. x_q4 (Q4 of pixel-equiv) × FOCAL_INT
             * gives Q4 of (pixel × focal); divide by z_q4 (also Q4)
             * yields plain pixels — the Q4 scales cancel. */
            int sx = fb_cx + ((int)p->x_q4 * FOCAL_INT) / z;
            int sy = fb_cy + ((int)p->y_q4 * FOCAL_INT) / z;

            if (converge_q8 > 0) {
                /* Transition: lock in the current projected screen
                 * position as the convergence start point, freeze depth
                 * so the particle doesn't keep accelerating outward. */
                p->frozen = 1;
                p->sx_q4 = (int16_t)(sx << 4);
                p->sy_q4 = (int16_t)(sy << 4);
            } else if ((unsigned)sx < FB_W && (unsigned)sy < FB_H) {
                /* Depth-graded stamp:
                 *   z < 200  ≈ <12 z-units   → 2×2 hot core
                 *   z < 800  ≈ <50 z-units   → single bright pixel
                 *   z < 2200                 → single warm pixel
                 *   else                     → dim sparkle + short trail
                 *
                 * Trail: project the particle ONE step deeper and stamp
                 * a dim pixel there. Since perspective moves particles
                 * outward as they approach, a "deeper" position projects
                 * closer to the centre — the trail visibly streaks
                 * outward from the vanishing point. */
                int tx = fb_cx + ((int)p->x_q4 * FOCAL_INT) / (z + 24);
                int ty = fb_cy + ((int)p->y_q4 * FOCAL_INT) / (z + 24);
                if ((unsigned)tx < FB_W && (unsigned)ty < FB_H &&
                    fb[ty * FB_W + tx] < PARTICLE_DIM) {
                    fb[ty * FB_W + tx] = PARTICLE_DIM;
                }

                if (z < 200) {
                    /* Hot 2×2 cluster near the camera — these read as
                     * sparks whooshing past. */
                    fb[sy * FB_W + sx] = PARTICLE_GLOW;
                    if (sx + 1 < FB_W)        fb[sy * FB_W + (sx + 1)]       = PARTICLE_COLOUR;
                    if (sy + 1 < FB_H)        fb[(sy + 1) * FB_W + sx]       = PARTICLE_COLOUR;
                    if (sx + 1 < FB_W && sy + 1 < FB_H)
                                              fb[(sy + 1) * FB_W + (sx + 1)] = PARTICLE_COLOUR;
                } else if (z < 800) {
                    fb[sy * FB_W + sx] = PARTICLE_GLOW;
                } else if (z < 2200) {
                    fb[sy * FB_W + sx] = PARTICLE_COLOUR;
                } else {
                    fb[sy * FB_W + sx] = PARTICLE_DIM;
                }
            }
        }

        if (p->frozen) {
            /* Ease toward endcard target in screen space. */
            int dx = ((int)p->tx << 4) - (int)p->sx_q4;
            int dy = ((int)p->ty << 4) - (int)p->sy_q4;
            int pull = converge_q8 < 32 ? 32 : converge_q8;   /* keep moving */
            p->sx_q4 = (int16_t)((int)p->sx_q4 + ((dx * pull) >> 11));
            p->sy_q4 = (int16_t)((int)p->sy_q4 + ((dy * pull) >> 11));
            int sx = (int)p->sx_q4 >> 4;
            int sy = (int)p->sy_q4 >> 4;
            if ((unsigned)sx < FB_W && (unsigned)sy < FB_H) {
                fb[sy * FB_W + sx] = PARTICLE_COLOUR;
            }
        }
    }
}

static void tunnel_done(void) { /* palette overwritten by next scene */ }

const effect_t fx_tunnel_particles_real = {
    .name  = "tunnel_particles",
    .mode  = MODE_320,
    .init  = tunnel_init,
    .frame = tunnel_frame,
    .done  = tunnel_done,
};
