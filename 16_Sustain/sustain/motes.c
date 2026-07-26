/* Drifting motes. See motes.h for why they exist and why they cannot cut. */

#include "motes.h"
#include "rgb565.h"

#include <math.h>

#define W 320
#define H 240
#define FOCAL 260.0f

/* Motes live on a coarse world lattice so they stay put as the camera moves
 * (rather than swimming with it, which instantly reads as dirt on the lens).
 * One cell is CELL units square; we consider the cells around and ahead of the
 * camera only. */
#define CELL        34.0f
#define PER_CELL    5
#define CELLS_AHEAD 7
#define CELLS_SIDE  2

/* Cheap deterministic hash -> [0,1). */
static inline float h31(int a, int b, int c)
{
    /* All arithmetic UNSIGNED. Multiplying signed ints by large constants
     * overflows, which is undefined behaviour — gcc flagged it under
     * -Waggressive-loop-optimizations, and UB in a hash is the kind of thing
     * that behaves on one compiler and produces garbage on the next. */
    uint32_t x = (uint32_t)a * 374761393u
               ^ (uint32_t)b * 668265263u
               ^ (uint32_t)c * 2147483647u;
    x ^= x >> 13; x *= 1274126177u; x ^= x >> 16;
    return (float)(x & 0xFFFFFF) * (1.0f / 16777216.0f);
}

static inline void plot_add(uint16_t *fb, int x, int y, int r, int g, int b)
{
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    uint16_t p = fb[y * W + x];
    int pr = rgb565_r8(p) + r, pg = rgb565_g8(p) + g, pb = rgb565_b8(p) + b;
    fb[y * W + x] = rgb565_pack(pr, pg, pb);
}

void motes_draw(uint16_t *fb, const camera_t *cam, float t,
                const int *lo, const int *hi, float horizon_y,
                float density, float warm)
{
    if (density <= 0.001f) return;

    const float fx = sinf(cam->yaw), fz = cosf(cam->yaw);
    const float rx = cosf(cam->yaw), rz = -sinf(cam->yaw);

    const int cx0 = (int)floorf(cam->x / CELL);
    const int cz0 = (int)floorf(cam->z / CELL);

    for (int dz = -1; dz <= CELLS_AHEAD; dz++) {
        for (int dx = -CELLS_SIDE; dx <= CELLS_SIDE; dx++) {
            const int ix = cx0 + dx, iz = cz0 + dz;

            for (int k = 0; k < PER_CELL; k++) {
                /* A stable per-mote random draw decides whether this mote is
                 * live at the current density. Comparing against a FIXED draw
                 * (rather than picking randomly each frame) means motes fade in
                 * and out in a fixed order as density changes, instead of
                 * flickering — flicker would be per-frame noise, which is
                 * exactly what the audit must not be fed. */
                const float live = h31(ix, iz, k * 7 + 3);
                if (live > density) continue;

                const float jx = h31(ix, iz, k);
                const float jz = h31(ix, iz, k + 101);
                const float jy = h31(ix, iz, k + 211);
                const float ph = h31(ix, iz, k + 307) * 6.2831853f;

                /* Slow, per-mote drift so the field breathes rather than
                 * sitting rigid. */
                const float wx = (ix + jx) * CELL + sinf(t * 0.23f + ph) * 2.4f;
                const float wz = (iz + jz) * CELL + cosf(t * 0.19f + ph) * 2.4f;
                const float wy = cam->y - 6.0f + jy * 15.0f
                               + sinf(t * 0.61f + ph) * 1.6f;

                const float ddx = wx - cam->x, ddz = wz - cam->z;
                const float zc = ddx * fx + ddz * fz;
                if (zc < 1.2f || zc > 190.0f) continue;

                const float xc = ddx * rx + ddz * rz;
                const int sx = (int)(W * 0.5f + xc * FOCAL / zc);
                if (sx < 0 || sx >= W) continue;

                const int sy = (int)(horizon_y - (wy - cam->y) * FOCAL / zc);
                /* Occlusion: only in the open gap this column left unpainted. */
                if (sy <= hi[sx] || sy >= lo[sx]) continue;

                /* Twinkle, and fade with distance and at the far clip so a
                 * mote never pops into existence. */
                float a = (1.0f - zc / 190.0f);
                a *= a;
                a *= 0.55f + 0.45f * sinf(t * 2.1f + ph * 3.0f);
                if (a <= 0.0f) continue;

                const int base = (int)(150.0f * a);
                const int r = (int)(base * (0.55f + 0.45f * warm));
                const int g = (int)(base * (0.80f - 0.18f * warm));
                const int b = (int)(base * (1.00f - 0.55f * warm));

                plot_add(fb, sx, sy, r, g, b);
                /* A faint cross so a mote reads as a light rather than a
                 * stray pixel, without costing a real sprite. */
                const int hr = r >> 2, hg = g >> 2, hb = b >> 2;
                plot_add(fb, sx - 1, sy, hr, hg, hb);
                plot_add(fb, sx + 1, sy, hr, hg, hb);
                plot_add(fb, sx, sy - 1, hr, hg, hb);
                plot_add(fb, sx, sy + 1, hr, hg, hb);
            }
        }
    }
}
