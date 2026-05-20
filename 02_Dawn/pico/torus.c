#include <string.h>

#include "torus.h"
#include "mathtab.h"

vec3_t       torus_verts[TORUS_VERTS];
vec3_t       torus_norms[TORUS_VERTS];
torus_poly_t torus_polys[TORUS_POLYS];
int          torus_poly_count = 0;

/* Build the polygon list once. Each polygon spans (r, c) to (r+1, c+1) on
 * the implicit torus grid. Winding [p1, p4, p3, p2] is from dawn_final.s. */
void torus_init_polys(void)
{
    int idx = 0;
    for (int r = 0; r < ROUN_SEG; r++) {
        const int base   = r * CIRC_SEG;
        const int next_r = ((r + 1) % ROUN_SEG) * CIRC_SEG;
        for (int c = 0; c < CIRC_SEG; c++) {
            const int next_c = (c + 1) % CIRC_SEG;
            const int p1 = base + c;
            const int p2 = base + next_c;
            const int p3 = next_r + next_c;
            const int p4 = next_r + c;
            torus_polys[idx].v[0] = (uint8_t)p1;
            torus_polys[idx].v[1] = (uint8_t)p4;
            torus_polys[idx].v[2] = (uint8_t)p3;
            torus_polys[idx].v[3] = (uint8_t)p2;
            idx++;
        }
    }
    torus_poly_count = idx;
}

/* Build one frame parametrically. Math follows web_port/torus.ts so the
 * 68k bit-twiddles are preserved, but with native ints. */
static inline int sin1024(int idx)
{
    return sin_tab[idx & SIN_TAB_MASK];
}

void torus_build_frame(int inner_step_in, int amplitude, int phase)
{
    /* The morph code in the original uses 16-bit angle accumulators with a
     * sine table of 1024 entries, then a >>1 to halve before indexing. We
     * mask to 1024 directly. */
    const int inner_step = (inner_step_in >> 1) & SIN_TAB_MASK;
    const int phase_off  = (phase >> 1) & SIN_TAB_MASK;
    const int cos_base   = (256 + phase_off) & SIN_TAB_MASK;
    const int outer_step = 1024 / ROUN_SEG;   /* 128 */
    const int inner_advance = 1024 / CIRC_SEG; /* 32 */

    int v_idx = 0;
    int d5 = 0;

    for (int outer = ROUN_SEG - 1; outer >= 0; outer--) {
        const int idx = d5 & SIN_TAB_MASK;
        const int sin_outer = sin1024(idx);
        const int cos_outer = sin1024(cos_base + idx);
        d5 = (d5 + outer_step) & 0xFFFF;

        /* Major radius components: web_port uses muls16(sin_outer, 4*384)
         * with sin_outer in [-32255..32255] giving roughly ±1.95M, then a
         * swap (high word). High word of (sin_outer * 1536) is just
         * (sin_outer * 1536) >> 16 — that's the term used for X/Z scale. */
        const int major_x_high = (sin_outer * (4 * 384)) >> 16;
        const int major_z_high = (cos_outer * (2 * 384)) >> 16;

        /* The 68k builds a4 = (cos_outer scaled in high word) | (major_x +
         * 1500 in low word). We just keep the two scaled values; the +1500
         * offset becomes the torus major radius bias. */
        const int major_x = major_x_high + 1500;
        const int major_z_scale = major_z_high;

        int a3 = 0;
        int d2 = (d5 >> 4) & 0xFFFF;

        for (int inner = CIRC_SEG - 1; inner >= 0; inner--) {
            const int sin_sample = sin1024(a3);
            a3 = (a3 + inner_step) & SIN_TAB_MASK;

            /* prod = (sin_sample + 32768) * amplitude; high word then
             * complement+halve to derive a normalized morph factor. */
            const int d0inner = (sin_sample + 32768) & 0xFFFF;
            const uint32_t prod = (uint32_t)d0inner * (uint32_t)(amplitude & 0xFFFF);
            uint32_t prod_high = (prod >> 16) & 0xFFFF;
            prod_high = (~prod_high) & 0xFFFF;
            prod_high >>= 1;
            const int d4 = (int)prod_high;

            /* Both X/Z paths take the HIGH 16 bits of a 32-bit product.
             * The TS port (web_port/torus.ts:180-198) does this with a
             * `swap32` + low-16 read; we just shift directly. Earlier the
             * z extraction took the LOW 16 bits by mistake — that fed
             * essentially random z values into projection, which made the
             * torus collapse after a few frames as backface culling
             * misclassified every quad. */
            const int d0temp_high = ((major_x       * d4) << 1) >> 16;
            const int d1temp_high = ((major_z_scale * d4) << 1) >> 16;

            const int sin_edge = sin1024(d2);
            const int cos_edge = sin1024(256 + d2);
            d2 = (d2 + inner_advance) & 0xFFFF;

            const int dx = d0temp_high * sin_edge;
            const int dy = d0temp_high * cos_edge;

            const int x_word = (int16_t)(dx >> 16);
            const int y_word = (int16_t)(dy >> 16);
            const int z_word = (int16_t)d1temp_high;

            /* POSITION_SCALE = 1/8 in the web port — we keep raw int values
             * and the projection's zoom constant absorbs the scale. Divide
             * here so the projection math doesn't overflow. */
            torus_verts[v_idx].x = (int16_t)(x_word >> 3);
            torus_verts[v_idx].y = (int16_t)(y_word >> 3);
            torus_verts[v_idx].z = (int16_t)(z_word >> 3);
            v_idx++;
        }
    }

    /* Compute vertex normals: sum face normals over each polygon's verts,
     * then normalize each accumulator to length ~128 (matches the assembly
     * which used a normalization LUT keyed by length²). On RP2040 we just
     * do an integer sqrt — division is fast on M0+. */
    int32_t accx[TORUS_VERTS] = {0};
    int32_t accy[TORUS_VERTS] = {0};
    int32_t accz[TORUS_VERTS] = {0};

    for (int p = 0; p < torus_poly_count; p++) {
        const int i1 = torus_polys[p].v[0];
        const int i4 = torus_polys[p].v[1];
        const int i3 = torus_polys[p].v[2];

        const int ax = torus_verts[i3].x - torus_verts[i4].x;
        const int ay = torus_verts[i3].y - torus_verts[i4].y;
        const int az = torus_verts[i3].z - torus_verts[i4].z;
        const int bx = torus_verts[i1].x - torus_verts[i4].x;
        const int by = torus_verts[i1].y - torus_verts[i4].y;
        const int bz = torus_verts[i1].z - torus_verts[i4].z;

        const int32_t nx = ay * bz - az * by;
        const int32_t ny = az * bx - ax * bz;
        const int32_t nz = ax * by - ay * bx;

        for (int k = 0; k < 4; k++) {
            const int vi = torus_polys[p].v[k];
            accx[vi] += nx;
            accy[vi] += ny;
            accz[vi] += nz;
        }
    }

    for (int i = 0; i < TORUS_VERTS; i++) {
        /* Quick & dirty length via float — runs once per scene change, not
         * per frame. Keeping it readable beats the LUT-and-sqrt dance the
         * original needed at 14 MHz. */
        const double fx = (double)accx[i];
        const double fy = (double)accy[i];
        const double fz = (double)accz[i];
        double len = fx*fx + fy*fy + fz*fz;
        if (len < 1.0) len = 1.0;
        const double inv = 128.0 / __builtin_sqrt(len);
        int nx = (int)(fx * inv);
        int ny = (int)(fy * inv);
        int nz = (int)(fz * inv);
        if (nx > 127) nx = 127; else if (nx < -128) nx = -128;
        if (ny > 127) ny = 127; else if (ny < -128) ny = -128;
        if (nz > 127) nz = 127; else if (nz < -128) nz = -128;
        torus_norms[i].x = (int16_t)nx;
        torus_norms[i].y = (int16_t)ny;
        torus_norms[i].z = (int16_t)nz;
    }
}
