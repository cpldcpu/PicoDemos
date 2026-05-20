/* Voxel landscape — scene 2 (0:20 – 0:55, MODE_320).
 *
 * Z-OUTER, COLUMN-INNER, FIXED-POINT INTEGER RAYMARCH.
 *
 * The original implementation iterated columns-outer with a per-step
 * float divide (screen_dist / z) and per-step float mul-add to compute
 * world position. On the M33 with FPU each of those was 3–14 cycles —
 * tolerable, but at 320×~120 ray steps per frame the cost added up,
 * starving the audio-decoder ISR's CPU budget on hardware.
 *
 * This version flips the loop: for each z step we do ONE divide (in
 * init, baked into proj_table[]) plus a handful of float ops to set up
 * the inner row, then iterate 320 columns with pure integer 16.16
 * fixed-point arithmetic — two adds and two shifts per column.
 *
 *   for z in front-to-back (precomputed z_table[]):
 *     compute world (wx, wy) at leftmost column          (a few float ops)
 *     compute (dx, dy) world step per column             (a few float ops)
 *     proj_q16 = proj_table[zi]                          (precomputed divide)
 *     for x in 0..320:
 *       if ybuf[x] > 0:
 *         h  = heightmap[(wy>>16 & mask)*W + (wx>>16 & mask)]
 *         dh = h - cam_h_i
 *         screen_y = horizon_i - (int)((int64)dh * proj_q16 >> 16)
 *         if screen_y < ybuf[x]: fill column, update ybuf[x]
 *       wx += dx;  wy += dy            <-- pure 32-bit integer adds
 *
 * Linear interpolation of world position across columns is geometrically
 * exact when we use *perspective* rays (camera-space Z = forward
 * distance, screen offset u = (x - W/2) / SD is linear in x).
 *
 * Sky fill happens in a separate pass after the raymarch — once per
 * column for whatever height remains unfilled.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MAP_W      512
#define MAP_H      512
#define MAP_MASK   511

#define FOV_DEG    70.0f
#define MAX_Z      300.0f
#define HORIZON_Y  90

#define SKY_BASE        224
#define SKY_COUNT       32

/* Terrain palette layout (slots 0..223) is 32 base colours × 7 shade
 * bands. The asset is packed with reserved=224 so its bytes are 0..31;
 * the renderer ORs in a `shade<<5` derived from depth to give depth
 * fog. Shade 0 = nearest (full colour); shade 6 = deepest (heavily
 * mixed with the horizon haze). */
#define TERRAIN_BASE_COLORS  32
#define SHADE_LEVELS          7

/* Number of z-steps in the precomputed table. With LOD growth (~1.005×
 * each step) starting at z=1.5, ~180 steps reach z≈290 which is just
 * inside MAX_Z=300. We size FAR_STEPS to that. */
#define FAR_STEPS    180

/* Per-Y sky-gradient — dithered between adjacent slots:
 *   sky_lo_lut[y]   = lower sky-slot index (relative to SKY_BASE)
 *   sky_frac_lut[y] = fractional position toward the next slot, Q4 (0..15)
 * Bayer-4 dither at draw time picks lo or hi based on threshold < frac.
 * Smooths the 32-band sky banding into perceptually continuous gradient. */
static uint8_t sky_lo_lut  [VGA_320_H];
static uint8_t sky_frac_lut[VGA_320_H];

/* Precomputed per-z depth fog band — also dithered between adjacent
 * shade levels so the depth-fog rolls in smoothly instead of stepping
 * in 7 visible bands. Each entry is already shifted (<<5) into the
 * upper palette-index bits for direct OR into the base colour. */
static uint8_t shade_lo_z   [FAR_STEPS + 1];
static uint8_t shade_hi_z   [FAR_STEPS + 1];
static uint8_t shade_frac_z [FAR_STEPS + 1];   /* Q4 0..15 */

/* Bayer 4×4 ordered dither — values 0..15 directly comparable against Q4 frac. */
static const uint8_t bayer4[16] = {
     0,  8,  2, 10,
    12,  4, 14,  6,
     3, 11,  1,  9,
    15,  7, 13,  5,
};

/* World-space "focal length" — SD = (W/2) / tan(fov/2). Set in init. */
static float screen_dist;

/* Precomputed per-z tables.
 *   z_table[i]    = actual z (forward distance) for step i
 *   proj_table[i] = (int)(SD / z * 65536), used for height-to-screen
 *                   projection without a runtime divide. */
static float   z_table   [FAR_STEPS + 1];
static int32_t proj_table[FAR_STEPS + 1];

/* Camera state. */
static float cam_x, cam_y, cam_h, cam_yaw, cam_horizon;

static inline uint8_t height_at(int x, int y)
{
    return asset_voxel_height_data[(y & MAP_MASK) * MAP_W + (x & MAP_MASK)];
}

static void voxel_init(void)
{
    /* The asset is now packed with reserved=224 so its bytes are 0..31
     * (32 base colours). Cache those 32 base RGBs, then synthesise 7
     * depth-shaded bands by lerping each base colour toward the horizon
     * haze and dimming it slightly. Final slot index = (shade<<5)|base. */
    static const uint8_t sky_top[3]    = {  10,   4,  28 };
    static const uint8_t sky_bottom[3] = {  60, 120, 145 };
    static const uint8_t fog_rgb[3]    = {  60, 120, 145 };  /* matches horizon */

    uint8_t base_r[TERRAIN_BASE_COLORS];
    uint8_t base_g[TERRAIN_BASE_COLORS];
    uint8_t base_b[TERRAIN_BASE_COLORS];
    for (int i = 0; i < TERRAIN_BASE_COLORS; i++) {
        uint16_t p = asset_voxel_color_pal[i];
        base_r[i] = (uint8_t)rgb565_r8(p);
        base_g[i] = (uint8_t)rgb565_g8(p);
        base_b[i] = (uint8_t)rgb565_b8(p);
    }

    /* Fog blend amount (Q8) and dimming per shade band. Shade 0 = full
     * brightness, no fog. Shade 6 = mostly fog colour, somewhat dim. */
    /* uint16 because 256 ("100 % brightness, no fade") overflows uint8. */
    static const uint16_t fog_q8[SHADE_LEVELS] = {   0,  30,  70, 115, 160, 205, 235 };
    static const uint16_t dim_q8[SHADE_LEVELS] = { 256, 248, 232, 212, 188, 160, 132 };
    for (int s = 0; s < SHADE_LEVELS; s++) {
        int f = fog_q8[s];
        int d = dim_q8[s];
        for (int b = 0; b < TERRAIN_BASE_COLORS; b++) {
            int r = base_r[b] + ((int)(fog_rgb[0] - base_r[b]) * f >> 8);
            int g = base_g[b] + ((int)(fog_rgb[1] - base_g[b]) * f >> 8);
            int b2= base_b[b] + ((int)(fog_rgb[2] - base_b[b]) * f >> 8);
            r = (r * d) >> 8;
            g = (g * d) >> 8;
            b2= (b2* d) >> 8;
            if (r > 255) r = 255; if (g > 255) g = 255; if (b2 > 255) b2 = 255;
            vga_320_palette_set(s * TERRAIN_BASE_COLORS + b,
                                (uint8_t)r, (uint8_t)g, (uint8_t)b2);
        }
    }

    /* Sky gradient: deep magenta-black at zenith → warm dawn teal at horizon. */
    for (int i = 0; i < SKY_COUNT; i++) {
        int num = i;
        int den = SKY_COUNT - 1;
        uint8_t r = (uint8_t)(sky_top[0] + (sky_bottom[0] - sky_top[0]) * num / den);
        uint8_t g = (uint8_t)(sky_top[1] + (sky_bottom[1] - sky_top[1]) * num / den);
        uint8_t b = (uint8_t)(sky_top[2] + (sky_bottom[2] - sky_top[2]) * num / den);
        vga_320_palette_set(SKY_BASE + i, r, g, b);
    }

    /* Per-screen-row sky band + fractional position toward the next
     * band, in Q4. We compute (y * (SKY_COUNT-1) * 16) / (H-1) and split
     * into integer slot + 4-bit fractional remainder. */
    for (int y = 0; y < VGA_320_H; y++) {
        int q = (y * (SKY_COUNT - 1) * 16) / (VGA_320_H - 1);    /* Q4 */
        int s = q >> 4;
        int f = q & 0xF;
        if (s >= SKY_COUNT - 1) { s = SKY_COUNT - 1; f = 0; }
        sky_lo_lut  [y] = (uint8_t)s;
        sky_frac_lut[y] = (uint8_t)f;
    }

    float fov_rad = FOV_DEG * 3.14159265f / 180.0f;
    screen_dist   = (VGA_320_W * 0.5f) / tanf(fov_rad * 0.5f);

    /* Precompute z_table, proj_table, and the dithered depth-shade
     * indices once per scene. shade_lo/hi ramp from 0 (near) to
     * SHADE_LEVELS-1 (far) over the z range, with a Q4 fractional
     * position dithered per-pixel in the inner loop. */
    float z  = 1.5f;
    float dz = 1.0f;
    for (int i = 1; i <= FAR_STEPS; i++) {
        z_table[i]    = z;
        proj_table[i] = (int32_t)(screen_dist / z * 65536.0f);
        int q  = ((i - 1) * (SHADE_LEVELS - 1) * 16) / FAR_STEPS;     /* Q4 */
        int s  = q >> 4;
        int f  = q & 0xF;
        int sh = s + 1;
        if (s  >= SHADE_LEVELS)     { s = SHADE_LEVELS - 1; }
        if (sh >= SHADE_LEVELS)     { sh = SHADE_LEVELS - 1; f = 0; }
        shade_lo_z  [i] = (uint8_t)(s  << 5);    /* pre-shifted for OR-in */
        shade_hi_z  [i] = (uint8_t)(sh << 5);
        shade_frac_z[i] = (uint8_t)f;
        z  += dz;
        dz *= 1.005f;
    }

    cam_x = 256.0f;
    cam_y = 0.0f;
    cam_h = 90.0f;
    cam_yaw = 0.0f;
    cam_horizon = HORIZON_Y;
}

static void voxel_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    /* --- camera animation -------------------------------------------
     * Faster forward push (2× the previous cruise) with multi-octave
     * lateral drift and banking yaw, giving the flight a feeling of
     * weaving rather than holding a stationary curve. */
    float t = t_into * 0.001f;
    cam_y   = t * 80.0f;
    cam_x   = 256.0f + sinf(t * 0.55f) * 100.0f
                     + sinf(t * 1.21f) *  28.0f;
    cam_yaw = sinf(t * 0.47f) * 0.55f
            + sinf(t * 1.13f) * 0.18f;

    static float cam_h_pos = 200.0f;
    static float cam_h_vel = 0.0f;
    float h_max = (float)height_at((int)cam_x, (int)cam_y);
    for (int i = 1; i <= 8; i++) {
        for (int dx = -4; dx <= 4; dx += 4) {
            float h = (float)height_at((int)cam_x + dx, (int)cam_y + i * 6);
            if (h > h_max) h_max = h;
        }
    }
    /* Bigger climb/dive amplitude — the spring eases between low passes
     * skimming the ridges and steeper rises over the valleys. */
    const float CLEARANCE = 65.0f;
    float target_h = h_max + CLEARANCE
                   + sinf(t * 0.41f) * 18.0f
                   + sinf(t * 0.93f) *  6.0f;
    const float DT       = 1.0f / 60.0f;
    const float SPRING_K = 2.6f;
    const float DAMPING  = 0.10f;
    float force = (target_h - cam_h_pos) * SPRING_K;
    cam_h_vel += force * DT;
    cam_h_vel *= (1.0f - DAMPING);
    cam_h_pos += cam_h_vel * DT;
    cam_h = cam_h_pos;
    cam_horizon = HORIZON_Y + (int)(cam_h_vel * 0.9f)
                            + (int)(sinf(t * 0.27f) * 10.0f);

    float cy = cosf(cam_yaw);
    float sy = sinf(cam_yaw);

    /* Snapshot frame-invariant integers used by the inner loop. */
    int   horizon_i = (int)cam_horizon;
    int   cam_h_i   = (int)cam_h;

    /* Perspective-ray setup. With forward-Z and screen offset
     *   u(x) = (x - W/2) / SD          (linear in x),
     * world position at forward distance z is
     *   wx = cam_x + z * (cy * u  - sy)
     *   wy = cam_y + z * (sy * u  + cy)
     * Since u is linear in x, wx and wy step linearly across columns
     * with deltas (z * cy / SD, z * sy / SD). Pull the
     * z-independent parts out of the z loop. */
    float u_left = -(VGA_320_W * 0.5f) / screen_dist;
    float u_step =  1.0f / screen_dist;
    float wx_const = cy * u_left - sy;     /* world-x offset at column 0, per unit z */
    float wy_const = sy * u_left + cy;     /* world-y offset at column 0, per unit z */
    float dx_const = cy * u_step;          /* world-x step per column, per unit z */
    float dy_const = sy * u_step;

    uint8_t *fb = vga_320_back_buffer();

    /* Per-column Y-buffer. uint8_t is enough since VGA_320_H = 240 < 256. */
    uint8_t ybuf[VGA_320_W];
    memset(ybuf, VGA_320_H, sizeof ybuf);

    /* --- z-outer raymarch --------------------------------------------- */
    for (int zi = 1; zi <= FAR_STEPS; zi++) {
        float   z         = z_table[zi];
        int32_t pf        = proj_table[zi];        /* 16.16 SD/z */
        uint8_t shade_lo  = shade_lo_z  [zi];      /* pre-shifted (shade<<5) */
        uint8_t shade_hi  = shade_hi_z  [zi];
        int     shade_f   = shade_frac_z[zi];      /* Q4 0..15 */

        if (z >= MAX_Z) break;

        /* Float compute once per z, then convert to 16.16 fixed-point
         * before the column-inner loop. */
        float wx0_f = cam_x + z * wx_const;
        float wy0_f = cam_y + z * wy_const;
        float dx_f  =        z * dx_const;
        float dy_f  =        z * dy_const;

        int32_t wx_fp = (int32_t)(wx0_f * 65536.0f);
        int32_t wy_fp = (int32_t)(wy0_f * 65536.0f);
        int32_t dx_fp = (int32_t)(dx_f  * 65536.0f);
        int32_t dy_fp = (int32_t)(dy_f  * 65536.0f);

        for (int x = 0; x < VGA_320_W; x++) {
            int cur_top = ybuf[x];
            if (cur_top > 0) {
                int iwx = (int)((uint32_t)wx_fp >> 16) & MAP_MASK;
                int iwy = (int)((uint32_t)wy_fp >> 16) & MAP_MASK;
                int idx = iwy * MAP_W + iwx;
                int h   = asset_voxel_height_data[idx];
                int dh  = h - cam_h_i;

                /* dh * pf can exceed int32 at small z. Use 64-bit
                 * intermediate; M33 has single-cycle 32×32→64. */
                int screen_y = horizon_i - (int)(((int64_t)dh * pf) >> 16);

                if (screen_y < cur_top) {
                    if (screen_y < 0) screen_y = 0;
                    /* Per-pixel Bayer-4 dither between adjacent shade
                     * bands. shade_lo/hi already pre-shifted; OR with
                     * the 32-color base index from the asset. */
                    uint8_t base    = asset_voxel_color_data[idx];
                    uint8_t lo_pix  = (uint8_t)(shade_lo | base);
                    uint8_t hi_pix  = (uint8_t)(shade_hi | base);
                    int bayer_col   = (x & 3) << 2;
                    int by          = screen_y;
                    uint8_t *p      = fb + by * VGA_320_W + x;
                    int rows        = cur_top - screen_y;
                    while (rows-- > 0) {
                        int thr = bayer4[bayer_col | (by & 3)];
                        *p = (thr < shade_f) ? hi_pix : lo_pix;
                        p += VGA_320_W;
                        by++;
                    }
                    ybuf[x] = (uint8_t)screen_y;
                }
            }
            wx_fp += dx_fp;
            wy_fp += dy_fp;
        }
    }

    /* --- sky fill (Bayer-4 dithered between adjacent sky slots) ------- */
    for (int x = 0; x < VGA_320_W; x++) {
        int top = ybuf[x];
        if (top > 0) {
            int bayer_col = (x & 3) << 2;
            uint8_t *p = fb + x;
            for (int y = 0; y < top; y++) {
                int lo  = sky_lo_lut [y];
                int frac= sky_frac_lut[y];
                int thr = bayer4[bayer_col | (y & 3)];
                int hi  = lo + 1;
                if (hi >= SKY_COUNT) hi = SKY_COUNT - 1;
                *p = (uint8_t)(SKY_BASE + ((thr < frac) ? hi : lo));
                p += VGA_320_W;
            }
        }
    }
}

static void voxel_done(void) { /* palette overwritten by next scene */ }

const effect_t fx_voxel_landscape_real = {
    .name  = "voxel_landscape",
    .mode  = MODE_320,
    .init  = voxel_init,
    .frame = voxel_frame,
    .done  = voxel_done,
};
