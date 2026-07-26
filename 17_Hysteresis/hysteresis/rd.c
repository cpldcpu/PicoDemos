/* Gray-Scott in fixed point. See rd.h for why this is a source and not an
 * effect.
 *
 *   du/dt = Du*lap(u) - u*v^2 + F*(1-u)
 *   dv/dt = Dv*lap(v) + u*v^2 - (F+k)*v
 *
 * u and v are stored 0..255 for 0..1. The u*v^2 term is the whole character of
 * the system and also the thing most easily destroyed by quantisation: at 8
 * bits, v^2 for small v rounds to zero and the reaction simply stops. So the
 * term is computed at higher precision and only the state is 8-bit --
 * (U*V>>8)*V>>8 keeps sixteen bits of intermediate where it matters.
 *
 * Integer only, for the same reason as field.c: the host render and the device
 * render are diffed byte for byte, and that only means anything if neither
 * compiler is free to reassociate.
 */

#include "rd.h"
#include "field.h"
#include "hot.h"

#include <string.h>

#define RD_N (RD_W * RD_H)

/* Q12 STATE, not 8-bit. The first version stored u and v as uint8 and computed
 * only the u*v^2 product at higher precision, which is precisely the mistake the
 * header warns about. At the growth front v is around 0.067, so in 8-bit the
 * reaction term rounds to 1 and the kill term also rounds to 1 -- the front
 * cannot advance and the whole pattern decays. Measured: mean 0.31 at one
 * second, 0.00 by five.
 *
 * Q12 gives sixteen times the resolution exactly where the quadratic needs it,
 * and U*V still fits int32 before the shift. Cost is 4 x 19200 x 2 = 153.6 KB
 * against the ~200 KB the device has spare, which is affordable and was
 * measured rather than assumed. */
#define RD_ONE 4096

/* A uint8 half-resolution mirror of v, refreshed at the end of each step.
 *
 * field_step reads this once per cell to bias its react threshold, so it wants
 * a cheap 8-bit load rather than a Q12 shift in the inner loop. 19,200 bytes
 * to keep the hot path to one fetch. */
static uint8_t g_v8[RD_N];

static uint16_t g_u[2][RD_N];
static uint16_t g_v[2][RD_N];
static int      g_page;

void rd_reset(void)
{
    for (int i = 0; i < RD_N; i++) {   /* substrate everywhere, no product */
        g_u[0][i] = g_u[1][i] = RD_ONE;
        g_v[0][i] = g_v[1][i] = 0;
    }
    g_page = 0;

    /* A few nuclei. Deliberately asymmetric and off-centre: symmetric seeds
     * give a symmetric pattern, which is the mirrored-world mistake demo 16
     * made with sin(x*k) being odd about the flight path. */
    static const struct { int16_t x, y, r; } seed[4] = {
        { 52, 44, 5 }, { 106, 71, 4 }, { 74, 92, 3 }, { 122, 34, 4 },
    };
    for (int s = 0; s < 4; s++)
        rd_poke(seed[s].x, seed[s].y, seed[s].r);
}

void rd_poke(int cx, int cy, int radius)
{
    const int r2 = radius * radius;
    for (int j = -radius; j <= radius; j++) {
        const int y = cy + j;
        if (y < 1 || y >= RD_H - 1) continue;
        for (int i = -radius; i <= radius; i++) {
            const int x = cx + i;
            if (x < 1 || x >= RD_W - 1) continue;
            if (i * i + j * j > r2) continue;
            g_u[g_page][y * RD_W + x] = RD_ONE / 4;
            g_v[g_page][y * RD_W + x] = (RD_ONE * 7) / 8;
        }
    }
}

void HYST_HOT(rd_step)(const rd_params_t *p)
{
    const uint16_t *su = g_u[g_page], *sv = g_v[g_page];
    uint16_t *du_ = g_u[g_page ^ 1], *dv_ = g_v[g_page ^ 1];

    const int32_t Du = p->du, Dv = p->dv;
    const int32_t F  = p->feed, K = p->kill;

    /* Edges are left alone rather than wrapped or clamped: a 1-cell inert
     * border costs 0.5% of the area and removes every bounds test from the
     * inner loop. The field this feeds is bordered anyway. */
    memcpy(du_, su, RD_W * sizeof *du_);
    memcpy(dv_, sv, RD_W * sizeof *dv_);
    memcpy(du_ + (RD_H - 1) * RD_W, su + (RD_H - 1) * RD_W, RD_W * sizeof *du_);
    memcpy(dv_ + (RD_H - 1) * RD_W, sv + (RD_H - 1) * RD_W, RD_W * sizeof *dv_);

    for (int y = 1; y < RD_H - 1; y++) {
        const int row = y * RD_W;
        du_[row] = su[row]; dv_[row] = sv[row];
        du_[row + RD_W - 1] = su[row + RD_W - 1];
        dv_[row + RD_W - 1] = sv[row + RD_W - 1];

        for (int x = 1; x < RD_W - 1; x++) {
            const int i = row + x;
            const int32_t U = su[i], V = sv[i];

            const int32_t lu = su[i - 1] + su[i + 1] + su[i - RD_W] + su[i + RD_W]
                             - 4 * U;
            const int32_t lv = sv[i - 1] + sv[i + 1] + sv[i - RD_W] + sv[i + RD_W]
                             - 4 * V;

            /* u*v^2, normalised so a full-scale u and v give full scale.
             * Two staged shifts rather than one big one: (U*V)>>8 keeps the
             * product in range, and the second multiply by V then >>8 keeps
             * the quadratic behaviour for small V that a single >>16 would
             * flatten to zero. */
            const int32_t uvv = (((U * V) >> 12) * V) >> 12;

            int32_t nu = U + ((Du * lu) >> 8) - uvv + ((F * (RD_ONE - U)) >> 12);
            int32_t nv = V + ((Dv * lv) >> 8) + uvv - (((F + K) * V) >> 12);

            du_[i] = (uint16_t)(nu < 0 ? 0 : nu > RD_ONE ? RD_ONE : nu);
            dv_[i] = (uint16_t)(nv < 0 ? 0 : nv > RD_ONE ? RD_ONE : nv);
        }
    }

    g_page ^= 1;

    {   /* refresh the 8-bit mirror */
        const uint16_t *v = g_v[g_page];
        for (int i = 0; i < RD_N; i++) g_v8[i] = (uint8_t)(v[i] >> 4);
    }
}

const uint8_t *rd_v8(void) { return g_v8; }

/* v, rescaled to 0..255 for the injector. */
void rd_read_v(uint8_t *out)
{
    const uint16_t *v = g_v[g_page];
    for (int i = 0; i < RD_N; i++) out[i] = (uint8_t)(v[i] >> 4);
}

void HYST_HOT(rd_inject)(uint8_t *dst, int16_t amp)
{
    if (!amp) return;
    const uint16_t *v = g_v[g_page];

    /* Bilinear 2x upsample. The RD grid is exactly half the field in both
     * axes, so the weights are fixed at 0, 1/2 -- no interpolation setup, just
     * two averages, and the result has no blocky edges to hand the feedback
     * loop as permanent structure. */
    for (int y = 0; y < FIELD_H; y++) {
        const int sy = y >> 1;
        const int sy1 = (sy + 1 < RD_H) ? sy + 1 : sy;
        const uint16_t *r0 = v + sy * RD_W;
        const uint16_t *r1 = v + ((y & 1) ? sy1 : sy) * RD_W;
        uint8_t *d = dst + y * FIELD_W;

        for (int x = 0; x < FIELD_W; x++) {
            const int sx = x >> 1;
            const int sx1 = (sx + 1 < RD_W) ? sx + 1 : sx;
            const int a = (x & 1) ? ((r0[sx] + r0[sx1]) >> 1) : r0[sx];
            const int b = (x & 1) ? ((r1[sx] + r1[sx1]) >> 1) : r1[sx];
            const int s = ((a + b) >> 1) >> 4;      /* Q12 -> 0..255 */

            const int o = d[x] + ((s * amp) >> 8);
            d[x] = (uint8_t)(o < 0 ? 0 : o > 255 ? 255 : o);
        }
    }
}
