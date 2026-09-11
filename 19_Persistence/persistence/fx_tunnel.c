/* The tunnel.
 *
 * A tunnel wants, per pixel, the angle around the axis and the depth along it:
 *
 *     u = atan2(dy, dx)          v = D / sqrt(dx*dx + dy*dy)
 *
 * The usual way to afford that is a lookup table of (u, v) for every pixel.
 * At 640x480 that table is 614 KB and the chip has 520, so the usual way is
 * not available -- the same wall the whole production is built against, one
 * level down. (The other usual way is quarter symmetry, which halves the table
 * and pins the axis to the centre of the screen. This one drifts, which is
 * only possible because there is no table.)
 *
 * So it is computed exactly every SPAN pixels and the interpolator walks the
 * pixels between: seed the accumulators with (u, v), set the step to the
 * difference over the span, and POP. A tunnel's curve over a span is near
 * enough a straight line that the error only shows at the very centre, where
 * the curvature is highest -- and the centre is behind the dark core anyway,
 * which is why the core is there.
 *
 * ------------------------------------------------------------ what it costs --
 *
 * First version: 11,027 cycles a line against a budget of 8,400. Every line in
 * the scene was late and the picture tore. Two things were wrong, and neither
 * was the one predicted:
 *
 *  - The pixel loop was seven instructions per pixel because it packed pairs
 *    into words and recomputed the destination index each time. Writing
 *    halfwords through a moving pointer -- the form 15_Quicksilver measured at
 *    3 to 4 cycles a pixel -- is three.
 *  - sqrtf compiles to the FPU's own instruction AND a fallback call to libm,
 *    because without -fno-math-errno the standard requires errno to be set for
 *    a negative argument. The call is never taken, but the compiler has to
 *    keep the live values somewhere it could survive one, so the fast path
 *    spills to the stack. The flag is in CMakeLists.txt with a note; it does
 *    not license reassociation the way -ffast-math would.
 *
 * Also: the dark core used to pop once per pixel and throw the results away.
 * The accumulators are re-seeded at every span, so it can just not.
 *
 * Then measured again with the two halves stubbed out separately, which is the
 * only reason the next decision was the right one:
 *
 *     whole kernel          9,048 cycles a line (mean over a frame)
 *     coordinates stubbed   5,368     -> the pixel loop is 5,400
 *     pixel loop stubbed    2,280     -> the coordinates are 2,000
 *
 * The pixel loop was already near the three-to-four cycles a pixel that
 * 15_Quicksilver measured for the same POP-and-store shape, so there was
 * nothing to win there; the coordinates were 49 cycles each, which is about
 * what a square root, a divide and an octant arctangent should cost. Neither
 * half was wrong. What was wrong was doing the second half 41 times a line
 * when 21 will do: SPAN went from 16 to 32.
 *
 * A note on the instrument, because it cost an hour. The first version of the
 * profiling harness reported the same number for all three builds, and the
 * reason was that PowerShell had not expanded the variable in -DPV_PROF=$prof,
 * so CMake cached the literal string and every build was the control. An
 * experiment whose three arms agree exactly is not a result, it is a broken
 * apparatus, and it should be read that way the first time.
 */

#include "beam.h"
#include "arena.h"
#include "rgb565.h"
#include "tables.h"
#include "interp_compat.h"
#include "affine.h"
#include "song.h"

#include <math.h>
#include <string.h>

#define SPAN     24
#define TEX_W    256

/* RGB565 in this repo's PIO bit order is R:0-4, G:6-10, B:11-15. Clearing the
 * low bit of each field before the shift keeps a halved colour from bleeding
 * out of one channel into the next. */
#define DIM_MASK 0x0841u
#define dim0(c) (c)
static inline uint16_t dim1(uint16_t c) { return (uint16_t)((c & (uint16_t)~DIM_MASK) >> 1); }
static inline uint16_t dim2(uint16_t c) { return dim1(dim1(c)); }

/* Depth shading, dithered.
 *
 * Three brightness levels applied per span put hard steps across the tube --
 * 24-pixel blocks, and a tunnel is exactly the shape that shows them, because
 * the bands follow the radius and the blocks do not. Halving again would cost
 * a level and still step.
 *
 * Instead there are five levels, and the two new ones are the checkerboard
 * between their neighbours: alternate pixels take the brighter and the darker
 * value, and the eye averages them to the level in between. It is the same
 * ordered dithering the gradients use (dither.h), on brightness rather than on
 * colour, and it costs nothing -- the phase is fixed for a whole run of
 * pixels, so it is chosen once per span by hoisting the loop rather than per
 * pixel by branching. */
#define TUN_TEXEL (*(const uint16_t *)(tex + interp_pop_full_result(interp0)))

#define TUN_LOOP(LO, HI) \
    do { \
        if (ph) { \
            for (int i = 0; i < SPAN; i += 4) { \
                d[i + 0] = HI(TUN_TEXEL); \
                d[i + 1] = LO(TUN_TEXEL); \
                d[i + 2] = HI(TUN_TEXEL); \
                d[i + 3] = LO(TUN_TEXEL); \
            } \
        } else { \
            for (int i = 0; i < SPAN; i += 4) { \
                d[i + 0] = LO(TUN_TEXEL); \
                d[i + 1] = HI(TUN_TEXEL); \
                d[i + 2] = LO(TUN_TEXEL); \
                d[i + 3] = HI(TUN_TEXEL); \
            } \
        } \
    } while (0)

typedef struct {
    int16_t  cx, cy;          /* the axis, in pixels                    */
    int32_t  uofs, vofs;      /* texture pan, 16.16                     */
    int32_t  depth;           /* D, the depth constant                  */
    int32_t  t[5];            /* shading thresholds, near to far         */
    uint16_t core;            /* colour of the dark core                */
} tun_p_t;

static tun_p_t P[2];
static uint16_t *s_tex;

/* atan(i/256) for i in 0..256, in units where a full circle is 1024. */
static uint8_t s_atan[257];

static void tunnel_enter(void)
{
    for (int i = 0; i <= 256; i++)
        s_atan[i] = (uint8_t)lrintf(atanf((float)i / 256.0f) * (1024.0f / 6.2831853f));

    /* The wall: panelled rings with lit seams and a few warm windows, in a
     * cold steel palette so the warm floor that follows reads as a change of
     * place rather than a change of effect. */
    s_tex = (uint16_t *)ARENA(ARENA_TEXTURE_OFF);
    uint32_t rng = 0x13579BDFu;
    for (int v = 0; v < 256; v++) {
        for (int u = 0; u < 256; u++) {
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            const int n = (int)((rng >> 24) & 63);

            /* Sixteen panels around and a ring every sixteen texels: fine
             * enough that the wall still reads as a wall at the edge of the
             * screen, where one texel is stretched across many pixels. The
             * first version used 32 and the panels came out as wedges the size
             * of a quadrant. */
            const int ring  = (v & 15);
            const int panel = ((u + ((v & 16) ? 8 : 0)) & 15);
            const int seam  = (ring < 1) || (panel < 1);
            const int rib   = (v & 63) < 3;

            int r, g, b;
            if (rib)       { r = 110 + n;     g = 165 + n;     b = 205 + n; }
            else if (seam) { r = 50 + n / 2;  g = 110 + n;     b = 150 + n; }
            else           { r = 22 + n / 2;  g = 46 + n / 2;  b = 72 + n / 2; }

            if (((u >> 4) * 17 + (v >> 4) * 29) % 23 == 0 && !seam) { r = 190; g = 115; b = 50; }
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            s_tex[v * TEX_W + u] = rgb565_pack(r, g, b);
        }
    }
}

static void tunnel_frame(uint32_t f, uint32_t local)
{
    tun_p_t *p = &P[f & 1];
    const float t = (float)local / 60.0f;

    /* The axis drifts, so the mouth of the tube rolls past rather than sitting
     * dead centre. Quarter symmetry would have forbidden this. */
    p->cx = (int16_t)(320 + 150.0f * sinf(t * 0.37f));
    p->cy = (int16_t)(240 + 90.0f * sinf(t * 0.53f + 1.0f));

    /* Depth constant, in texel-pixels: v = D / r, so D fixes how much of the
     * tube the screen shows. The first version had D = 55, which put the whole
     * visible range inside ONE texel of the wall -- every ring and panel edge
     * squeezed into a sliver and the picture read as radial smear with no tube
     * in it at all. At D = 1600 the edge of the screen sits about four texels
     * down the tube and the core about 250, so fifteen rings are on screen at
     * once, which is what a tunnel looks like. */
    const float fly  = 24.0f * t + 3.2f * t * t;          /* accelerating flight */
    const float roll = 0.9f * t + 0.10f * t * t;
    /* Wrapped to the texture's 256 texels so neither offset can grow without
     * bound; the sampler wraps anyway, but the 16.16 sum must not overflow. */
    p->vofs  = (int32_t)(fly * 65536.0f) & 0x00FFFFFF;
    p->uofs  = (int32_t)(roll * (1024.0f / 6.2831853f) * 65536.0f * 0.25f) & 0x00FFFFFF;
    p->depth = (int32_t)((3000.0f + 600.0f * sinf(t * 0.7f)) * 65536.0f);

    /* Hoisted out of the line loop: five divides per span over twenty-seven
     * spans and 480 rows is 64,800 divides a frame for numbers that do not
     * change within one. Each is the vmag at a given radius, so the bands are
     * at r = 96, 74, 54, 38 and 20 pixels. */
    p->t[0] = p->depth / 96;
    p->t[1] = p->depth / 74;
    p->t[2] = p->depth / 54;
    p->t[3] = p->depth / 38;
    p->t[4] = p->depth / 20;          /* inside this is the core */
    p->core = rgb565_pack(7, 11, 17);
}

/* Exact (u, v) at one screen point, in 16.16 texture units. */
static inline void tun_point(const tun_p_t *p, int dx, int dy, int32_t *u, int32_t *v)
{
    const int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;

    int a;                                        /* 1024 units to the circle */
    if (ax >= ay) a = ax ? s_atan[(ay << 8) / ax] : 0;
    else          a = 256 - (ay ? s_atan[(ax << 8) / ay] : 0);
    if (dx < 0) a = 512 - a;
    if (dy < 0) a = -a;

    /* Clamped at the core radius, not at 3: inside the core the wall is not
     * drawn, and letting v run to D/3 would make the interpolated slope across
     * the neighbouring span enormous for pixels nobody sees. */
    const float r = sqrtf((float)(dx * dx + dy * dy));
    const int32_t vv = (int32_t)((float)p->depth / (r < 12.0f ? 12.0f : r));

    *u = (a << 14) + p->uofs;                     /* 1024 angle -> 256 texels, 16.16 */
    *v = vv + p->vofs;
}

static void PV_HOT(tunnel_line)(uint32_t f, uint16_t *px, int y)
{
    const tun_p_t *p = &P[f & 1];
    const int dy = y - p->cy;
    const uint8_t *const tex = (const uint8_t *)s_tex;
    const int span = g_lod ? SPAN * 2 : SPAN;
    const int ph = y & 1;                 /* the dither's phase for this row */

    int32_t u0, v0, u1, v1;
    tun_point(p, -p->cx, dy, &u0, &v0);

    for (int x = 0; x < PV_W; x += span) {
#if PV_PROF == 1
        u1 = u0 + (span << 16); v1 = v0;      /* stub the maths, keep the pixels */
#else
        tun_point(p, x + span - p->cx, dy, &u1, &v1);
#endif
        uint16_t *d = px + x;
#if PV_PROF == 2
        pv_fill(d, 0, span, 0x1234);          /* stub the pixels, keep the maths */
        u0 = u1; v0 = v1;
        continue;
#endif

        const int32_t vmag = v0 - p->vofs;
        if (vmag > p->t[4]) {
            /* The dark core. No pops at all: the accumulators are re-seeded at
             * every span, so nothing downstream depends on them advancing. */
            pv_fill(d, 0, span, p->core);
            u0 = u1; v0 = v1;
            continue;
        }

        interp_set_accumulator(interp0, 0, (uint32_t)u0);
        interp_set_accumulator(interp0, 1, (uint32_t)v0);

        if (g_lod) {
            /* Half horizontal resolution: step TWICE as far per pop and pop
             * half as often. Stepping normally and throwing every second
             * result away saves the texture load and nothing else, which is
             * most of the point missed. */
            qs_texmap_step(interp0, (uint32_t)(2 * (u1 - u0) / span), (uint32_t)(2 * (v1 - v0) / span));
            for (int i = 0; i < span; i += 2) {
                const uint16_t c = *(const uint16_t *)(tex + interp_pop_full_result(interp0));
                d[i] = c; d[i + 1] = c;
            }
            u0 = u1; v0 = v1;
            continue;
        }
        qs_texmap_step(interp0, (uint32_t)((u1 - u0) / span), (uint32_t)((v1 - v0) / span));

        if      (vmag <= p->t[0]) TUN_LOOP(dim0, dim0);
        else if (vmag <= p->t[1]) TUN_LOOP(dim0, dim1);
        else if (vmag <= p->t[2]) TUN_LOOP(dim1, dim1);
        else if (vmag <= p->t[3]) TUN_LOOP(dim1, dim2);
        else                      TUN_LOOP(dim2, dim2);

        u0 = u1; v0 = v1;
    }
}

static void tunnel_setup1(void) { qs_texmap_setup_interp0(); }

const scene_t fx_tunnel = { "tunnel", tunnel_enter, tunnel_frame, tunnel_line, tunnel_setup1, NULL };
