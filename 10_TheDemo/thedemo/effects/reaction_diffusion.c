/* Reaction-diffusion walking through three regimes —
 * scene 6 (2:50 – 3:10, MODE_160).
 *
 * Classic Gray-Scott:
 *
 *     ∂u/∂t = D_u · ∇²u  − u·v²  + F·(1 − u)
 *     ∂v/∂t = D_v · ∇²v  + u·v²  − (F + k)·v
 *
 * The earlier version reinjected the logo shape every frame, which
 * pinned the field and gave a near-static image. This version lets the
 * chemistry run free and instead:
 *
 *   - walks F/k through three regimes over the scene (small spots →
 *     coarse stripes → big drifting spots) so the pattern visually
 *     mutates,
 *   - drops fresh seed bursts every ~2.2 s so new structures keep
 *     spawning into the developed field,
 *   - uses a true zero-flux Neumann boundary (mirroring interior into
 *     the edge each step). Without this, interior cells adjacent to the
 *     boundary read a stale (u=255, v=0) seed neighbour every step and
 *     u saturates along the rim.
 *
 * Last 5 s blends in the dilated logo wireframe and fades out.
 *
 * Everything is integer 8-bit Q8 (a cell value of 200 means "0.78").
 * Per-frame cost @ 160×120 with 20 substeps: ~4 M cell updates ≈ a few
 * ms @ 300 MHz. Comfortable.
 */

#include "scene.h"
#include "vga.h"
#include "assets.h"
#include "rgb565.h"
#include <stdint.h>
#include <string.h>
#include <math.h>

#define W                 VGA_160_W                  /* 160 */
#define H                 VGA_160_H                  /* 120 */

#define SCENE_LEN_MS      20000
#define FADE_IN_MS         1500
#define FADE_OUT_MS        2500
#define FADE_OUT_AT       (SCENE_LEN_MS - FADE_OUT_MS)

/* Q8 fields. Two of each so we can ping-pong source / destination
 * without overwriting cells the rest of the row still needs to read.
 * Live in shared scene scratch — see scene_scratch.h. */
#include "../scene_scratch.h"
static uint8_t * const U[2] = { g_scratch.rd.u0, g_scratch.rd.u1 };
static uint8_t * const V[2] = { g_scratch.rd.v0, g_scratch.rd.v1 };
#define FIELD_BYTES (W * H)
static int     front = 0;     /* read from [front], write into [front ^ 1] */

/* RGB565 colour LUT keyed by v (0..255). */
static uint16_t color_lut[256];

static void build_color_lut(void)
{
    /* Anchors chosen so the lower half of the index range (where the
     * developed field actually lives) traverses the full deep→hot ramp,
     * not just sea→teal as before. The renderer also stretches v×2 to
     * push more of the field into the saturated upper anchors. */
    static const uint8_t anchors[][3] = {
        {   6,   2,  16 },   /* black-violet sea */
        {  40,  10,  70 },   /* deep purple */
        { 100,  20, 140 },   /* indigo */
        { 220,  40, 130 },   /* hot magenta */
        { 250, 140,  50 },   /* coral orange */
        { 255, 235, 180 },   /* warm white core */
    };
    const int N    = (int)(sizeof anchors / sizeof anchors[0]);
    const int seg_w = 256 / (N - 1);
    for (int i = 0; i < 256; i++) {
        int seg = i / seg_w;
        if (seg >= N - 1) seg = N - 2;
        int sub = i - seg * seg_w;
        const uint8_t *c0 = anchors[seg];
        const uint8_t *c1 = anchors[seg + 1];
        int r = c0[0] + (c1[0] - c0[0]) * sub / seg_w;
        int g = c0[1] + (c1[1] - c0[1]) * sub / seg_w;
        int b = c0[2] + (c1[2] - c0[2]) * sub / seg_w;
        color_lut[i] = rgb565_pack(r, g, b);
    }
}

/* 1bpp asset_logo_mask is 160×120 — sampled at our native cell grid. */
static inline int mask_bit(int x, int y)
{
    int byte_idx = y * (ASSET_LOGO_MASK_W / 8) + (x >> 3);
    return (asset_logo_mask_data[byte_idx] >> (7 - (x & 7))) & 1;
}

static inline int mask_bit_safe(int x, int y)
{
    if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return 0;
    return mask_bit(x, y);
}

/* Edge-detected wireframe of the logo, computed once. */
static uint8_t logo_wire[W * H];

static void build_wire(void)
{
    memset(logo_wire, 0, sizeof logo_wire);
    for (int y = 1; y < H - 1; y++) {
        for (int x = 1; x < W - 1; x++) {
            int here = mask_bit_safe(x, y);
            if (!here) continue;
            int n = mask_bit_safe(x, y - 1);
            int s = mask_bit_safe(x, y + 1);
            int e = mask_bit_safe(x + 1, y);
            int w = mask_bit_safe(x - 1, y);
            if (n == 0 || s == 0 || e == 0 || w == 0) {
                logo_wire[y * W + x] = 1;
            }
        }
    }
    /* One-pixel dilation so the wireframe reads as a clear stroke. */
    static uint8_t tmp[W * H];
    memcpy(tmp, logo_wire, sizeof tmp);
    for (int y = 1; y < H - 1; y++) {
        for (int x = 1; x < W - 1; x++) {
            int idx = y * W + x;
            if (tmp[idx]) {
                logo_wire[idx - 1] = 1;
                logo_wire[idx + 1] = 1;
                logo_wire[idx - W] = 1;
                logo_wire[idx + W] = 1;
            }
        }
    }
}

/* xorshift32 — used by seed() and the periodic bursts. */
static inline uint32_t xs32(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}

/* Drop `count` perturbed clusters into the live field. Clusters are 4×4
 * with strong centre values (u=50, v=160) — small 2×2 seeds at low v
 * die to the Q8 reaction-term truncation before they can grow. If
 * `logo_bias` is non-zero, ~60% of clusters only land if inside the
 * logo mask, biasing the developed centre of mass toward the
 * silhouette. */
static void seed_burst(uint32_t rng_seed, int count, int logo_bias)
{
    uint32_t s = rng_seed | 1u;
    uint8_t *u = U[front];
    uint8_t *v = V[front];
    for (int n = 0; n < count; n++) {
        int x = (int)(xs32(&s) % (W - 6)) + 3;
        int y = (int)(xs32(&s) % (H - 6)) + 3;
        if (logo_bias && (xs32(&s) & 0xFF) < 154 && !mask_bit(x, y)) continue;
        for (int dy = -2; dy <= 1; dy++)
        for (int dx = -2; dx <= 1; dx++) {
            int idx = (y + dy) * W + (x + dx);
            u[idx] =  80;
            v[idx] = 120;
        }
    }
}

static void seed(void)
{
    /* Background sea: u=255 (=1.0 Q8), v=0. */
    memset(U[0], 255, FIELD_BYTES);
    memset(U[1], 255, FIELD_BYTES);
    memset(V[0],   0, FIELD_BYTES);
    memset(V[1],   0, FIELD_BYTES);
    front = 0;
    seed_burst(0xDEADBEEFu, 250, /*logo_bias=*/1);
}

/* --- effect lifecycle --------------------------------------------- */

static void rd_init(void)
{
    build_color_lut();
    build_wire();
    seed();
}

/* Runtime-variable Gray-Scott parameters so update_params() can shift
 * them over the scene. Q8 throughout. At 8-bit precision we can't run
 * the canonical Pearson constants without losing the small-v reaction
 * term to truncation, so we use a halved set that demonstrably grows
 * patterns; periodic parameter shifts then force the developed field
 * to reorganise (which reads as motion). */
static int F_q8  = 5;
static int K_q8  = 8;
static int DU_q8 = 22;
static int DV_q8 = 10;

static inline int clamp_u8(int v)
{
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return v;
}

static void rd_step(void)
{
    const uint8_t *u_in  = U[front];
    const uint8_t *v_in  = V[front];
    uint8_t       *u_out = U[front ^ 1];
    uint8_t       *v_out = V[front ^ 1];

    const int F  = F_q8;
    const int K  = K_q8;
    const int DU = DU_q8;
    const int DV = DV_q8;
    const int FK = F + K;

    for (int y = 1; y < H - 1; y++) {
        for (int x = 1; x < W - 1; x++) {
            int idx = y * W + x;
            int u  = u_in[idx];
            int v  = v_in[idx];

            int lu = (u_in[idx - 1] + u_in[idx + 1] + u_in[idx - W] + u_in[idx + W]) - 4 * u;
            int lv = (v_in[idx - 1] + v_in[idx + 1] + v_in[idx - W] + v_in[idx + W]) - 4 * v;

            int uv2 = (u * v * v) >> 16;

            int du = ((DU * lu) >> 8) - uv2 + ((F  * (255 - u)) >> 8);
            int dv = ((DV * lv) >> 8) + uv2 - ((FK * v)         >> 8);

            u_out[idx] = (uint8_t)clamp_u8(u + du);
            v_out[idx] = (uint8_t)clamp_u8(v + dv);
        }
    }

    /* Zero-flux Neumann boundary: boundary cell = nearest interior cell.
     * The earlier "copy boundary unchanged from u_in" approach left the
     * edge frozen at the seed value (u=255, v=0) for the whole scene,
     * which made interior cells next to the edge see a stale neighbour
     * every step → u saturated and v starved at the rim. */
    memcpy(u_out,                u_out + W,           W);
    memcpy(u_out + (H - 1) * W,  u_out + (H - 2) * W, W);
    memcpy(v_out,                v_out + W,           W);
    memcpy(v_out + (H - 1) * W,  v_out + (H - 2) * W, W);
    for (int y = 0; y < H; y++) {
        u_out[y * W]             = u_out[y * W + 1];
        u_out[y * W + W - 1]     = u_out[y * W + W - 2];
        v_out[y * W]             = v_out[y * W + 1];
        v_out[y * W + W - 1]     = v_out[y * W + W - 2];
    }

    front ^= 1;
}

/* Velocity-field advection of both u and v fields. Gray-Scott alone
 * gives static spots once the chemistry settles — Q8 precision + the
 * α-regime parameters don't admit traveling waves. So we transport
 * the substrate underneath the chemistry along a spatially-varying
 * flow field, then let chemistry re-sharpen the smeared pattern. The
 * net result reads as a fluid carrying coral that keeps regrowing.
 *
 * Flow field: sin/cos shear — per row, an x-velocity that depends on
 * row index and time; per column, a y-velocity that depends on column
 * index and time. Adjacent rows have different x-velocities, so the
 * shear produces visible swirling rather than uniform translation.
 * Wrap-around sampling (toroidal) means the rotating substrate doesn't
 * lose density at the boundary the way clamp-to-edge would.
 *
 * Cost: H + W sin/cos calls precomputed once per frame, then one
 * nearest-neighbour read per cell. */
static int vx_table[H];
static int vy_table[W];

static void build_flow(float t_sec)
{
    /* ±0.3 px per frame max. Faster than this and chemistry can't
     * re-sharpen the smeared pattern between frames, so spots get
     * streaked into linear smudges and eventually wash out. */
    const float amp = 0.3f;
    for (int y = 0; y < H; y++) {
        vx_table[y] = (int)(sinf((float)y * 0.07f + t_sec * 0.6f) * amp * 256.0f);
    }
    for (int x = 0; x < W; x++) {
        vy_table[x] = (int)(cosf((float)x * 0.07f + t_sec * 0.5f) * amp * 256.0f);
    }
}

static void advect_field(uint8_t *dst, uint8_t *scratch)
{
    memcpy(scratch, dst, FIELD_BYTES);
    for (int y = 0; y < H; y++) {
        int vx = vx_table[y];
        int sy_base = (y << 8);
        for (int x = 0; x < W; x++) {
            int vy = vy_table[x];
            int sx = (((x << 8) - vx) >> 8);
            int sy = ((sy_base - vy) >> 8);
            /* Toroidal wrap. The %-with-positive-correction handles
             * negative values (C's % is sign-preserving on negatives). */
            sx %= W; if (sx < 0) sx += W;
            sy %= H; if (sy < 0) sy += H;
            dst[y * W + x] = scratch[sy * W + sx];
        }
    }
}

/* Smoothly interpolate F/k/DU/DV between regime points so the pattern
 * morphs over the scene rather than holding one steady-state. Each
 * regime point is held for HOLD_MS, with XFADE_MS lerps between. */
static void update_params(uint32_t t_into)
{
    /* Stay in the survival zone — Q8 quantisation makes anything below
     * the spots regime tend to extinction once advection is also
     * thinning the field. Small F/K shifts give visible morphing
     * without killing the chemistry. */
    static const struct { int F, K, DU, DV; } regimes[] = {
        { 5,  8, 22, 10 },    /* spots                          */
        { 6, 10, 24, 10 },    /* slightly leaner spots          */
        { 4,  7, 20, 12 },    /* slightly fatter spots          */
        { 5,  8, 22, 10 },    /* base for wireframe reveal      */
    };
    const int N_REG    = (int)(sizeof regimes / sizeof regimes[0]);
    const uint32_t PERIOD = 5000;   /* 5 s per segment, 4 segments = 20 s */

    uint32_t seg = t_into / PERIOD;
    if (seg >= (uint32_t)N_REG - 1) seg = N_REG - 2;
    uint32_t sub = t_into - seg * PERIOD;
    /* Hold first half, lerp second half — gives stable patterns then a
     * visible morph into the next regime. */
    int t_q8;
    if (sub < PERIOD / 2)      t_q8 = 0;
    else                       t_q8 = (int)(((sub - PERIOD / 2) * 256) / (PERIOD / 2));
    if (t_q8 > 256) t_q8 = 256;

    const int a = (int)seg, b = (int)seg + 1;
    F_q8  = regimes[a].F  + ((regimes[b].F  - regimes[a].F)  * t_q8 >> 8);
    K_q8  = regimes[a].K  + ((regimes[b].K  - regimes[a].K)  * t_q8 >> 8);
    DU_q8 = regimes[a].DU + ((regimes[b].DU - regimes[a].DU) * t_q8 >> 8);
    DV_q8 = regimes[a].DV + ((regimes[b].DV - regimes[a].DV) * t_q8 >> 8);
}

static void rd_frame(uint32_t t_into, uint32_t t_global)
{
    (void)t_global;

    update_params(t_into);

    /* Periodic seed bursts: every 2.2 s a fresh batch of clusters drops
     * into the live field. Skip the last 5 s (wireframe morph) so the
     * pattern can stabilise for the reveal. */
    static uint32_t last_burst_at = 0;
    static int      burst_idx     = 0;
    const uint32_t BURST_PERIOD = 1500;
    const uint32_t BURST_STOP   = 13000;
    if (t_into < BURST_STOP && t_into >= last_burst_at + BURST_PERIOD) {
        /* Mix of in-mask and free bursts so the silhouette stays
         * suggested without being a hard constraint. Seed varies per
         * burst so each one drops different positions. */
        uint32_t rng = 0x9E3779B9u ^ (uint32_t)(burst_idx * 0x85EBCA6Bu);
        int logo_bias = (burst_idx & 1);   /* alternate biased / free */
        seed_burst(rng, 80, logo_bias);
        burst_idx++;
        last_burst_at = t_into;
    }
    /* The very first frame of the scene also gets a burst flag reset so
     * re-entries to the scene work right. */
    if (t_into < BURST_PERIOD) last_burst_at = 0;

    /* Advect both u and v along a shear flow, then let chemistry
     * re-sharpen the warped pattern. Skip during the wireframe morph
     * so the logo emerges cleanly. U[front^1] / V[front^1] are stale
     * after the previous frame's last rd_step swap — perfect as
     * advection scratch with no BSS cost. */
    if (t_into < 13000) {
        build_flow(t_into * 0.001f);
        advect_field(U[front], U[front ^ 1]);
        advect_field(V[front], V[front ^ 1]);
    }

    /* 15 substeps/frame at halved D constants → ~7 effective steps of
     * standard-strength Gray-Scott per frame, plenty of evolution per
     * second without burning core1 budget. */
    for (int s = 0; s < 15; s++) rd_step();

    /* Wireframe-emerge morph for the last 5 s. Instead of overlaying a
     * solid stroke (which sat on top of the chemistry with no
     * interaction), the wire cells are turned into chemistry *sources*:
     * each frame they pull u down / v up, scaled by morph strength.
     * Reaction-diffusion then carries that signal outward via
     * Gray-Scott — the wire shape grows into the surrounding field
     * with a halo of chemistry around it, and the logo emerges
     * organically rather than being stamped on top. */
    const uint32_t WIRE_START   = 13000;
    const uint32_t WIRE_DUR     =  5000;
    int morph_q8 =
        (t_into < WIRE_START) ? 0 :
        (t_into >= WIRE_START + WIRE_DUR) ? 256 :
        (int)(((t_into - WIRE_START) * 256) / WIRE_DUR);
    if (morph_q8 > 0) {
        uint8_t *u = U[front];
        uint8_t *v = V[front];
        for (int i = 0; i < W * H; i++) {
            if (logo_wire[i]) {
                int nv = v[i] + (((220 - v[i]) * morph_q8) >> 8);
                int nu = u[i] + ((( 20 - u[i]) * morph_q8) >> 8);
                if (nv < 0) nv = 0; else if (nv > 255) nv = 255;
                if (nu < 0) nu = 0; else if (nu > 255) nu = 255;
                v[i] = (uint8_t)nv;
                u[i] = (uint8_t)nu;
            }
        }
    }

    int alpha = 256;
    if (t_into < FADE_IN_MS) {
        alpha = (int)(t_into * 256 / FADE_IN_MS);
    } else if (t_into >= FADE_OUT_AT) {
        uint32_t into = t_into - FADE_OUT_AT;
        alpha = (into >= FADE_OUT_MS) ? 0 : 256 - (int)(into * 256 / FADE_OUT_MS);
    }

    uint16_t *fb = vga_160_back_buffer();
    const uint8_t *v = V[front];

    for (int i = 0; i < W * H; i++) {
        /* Stretch v ×1.5 so the developed field (typically v in
         * [0, ~150]) reaches the orange/magenta/warm-white anchors
         * at the top of the LUT but with a visible gradient across
         * each structure rather than saturating to white. */
        int d = v[i] + (v[i] >> 1);
        if (d > 255) d = 255;
        d = (d * alpha) >> 8;
        fb[i] = color_lut[d];
    }
}

static void rd_done(void)
{
    seed();
}

const effect_t fx_reaction_diffusion_real = {
    .name  = "reaction_diffusion",
    .mode  = MODE_160,
    .init  = rd_init,
    .frame = rd_frame,
    .done  = rd_done,
};
