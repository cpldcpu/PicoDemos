#include "voxel.h"
#include "chunky.h"
#include "mathtab.h"

/* 256×256 (64 KB each). Held in BSS — generated once at startup, read
 * during the voxel scene. After the scene we leave them allocated — RAM
 * budget allows it and avoiding a free/realloc keeps the code simple. */
static uint8_t voxel_height[VOXEL_DIM * VOXEL_DIM];
static uint8_t voxel_color [VOXEL_DIM * VOXEL_DIM];

void voxel_init(void)
{
    /* Two overlapping sine bumps — direct transliteration of
     * dawn_final.s:1499-1545. Centers at (224, 224) and (31, 31), >> 4 /
     * >> 5 on the squared distance so each ray sees ~6 radial ripples
     * out to the far map edge. */
    const int center_a = 255 - 31;        /* = 224 */
    const int center_b = 31;

    for (int y = 0; y < VOXEL_DIM; y++) {
        for (int x = 0; x < VOXEL_DIM; x++) {
            const int idx = y * VOXEL_DIM + x;

            int dx1 = x - center_a;
            int dy1 = y - center_a;
            int dsq1 = (dx1*dx1 + dy1*dy1) >> 4;
            int h1 = sin_lookup(dsq1) >> 5;

            int dx2 = x - center_b;
            int dy2 = y - center_b;
            int dsq2 = (dx2*dx2 + dy2*dy2) >> 5;
            int h2 = sin_lookup(dsq2) >> 5;

            int combined = h1 + h2;
            int final_h = (combined >> 7) + 64;
            if (final_h < 0) final_h = 0;
            if (final_h > 255) final_h = 255;
            voxel_height[idx] = (uint8_t)final_h;

            int slope = combined - 31;
            int color = (slope >> 6) + 32;
            color ^= 0x3F;
            voxel_color[idx] = (uint8_t)(color & 0x3F);
        }
    }
}

/* Shade table from dawn_final.s:1547-1563. The asm precomputes a 65×256
 * byte table written sequentially while iterating d7 (outer) from 64
 * down and d6 (inner) from 255 down. The lookup index is
 * `(color_byte << 8) | dist_byte`, so the value at offset (c, d) was
 * written when d7_outer = 64-c and d6_inner = 255-d — i.e. the formula
 * baked in is:
 *
 *     max(0, (64 - c) - ((255 - d) & 0x7F) >> 3)
 *
 * Two inversions matter:
 *   - `64 - c` un-inverts the XOR-with-0x3F that voxel_init applies
 *     to the colormap (asm line 1540: eor #$3f,d5). Without it, steep
 *     terrain would come out dark and flat bright.
 *   - `(255 - d) & 0x7F` flips the distance direction: in the ray walk,
 *     `dist` counts down 99→0 from near to far. The asm wants near=bright
 *     so atten = (127 - dist)/8, giving small atten near, large atten far. */
static inline uint8_t shade(uint8_t color_byte, uint8_t dist_byte)
{
    int v = (64 - (color_byte & 0x3F)) - (((255 - dist_byte) & 0x7F) >> 3);
    if (v < 0) v = 0;
    if (v > 0x3F) v = 0x3F;
    return (uint8_t)v;
}

/* Column-based raycaster. Each column walks 99 voxel steps "into" the
 * scene; for each step, if its projected height exceeds the running
 * skyline we paint vertical pixels until we catch up, then continue.
 *
 * The x-step (`x_step`) is NOT constant across columns — dawn_final.s:568
 * does `addq #1,d1` outside .ilop, so the ray direction fans from -80
 * (sharp-left) at col 0, through 0 (straight) at col 80, to +79
 * (sharp-right) at col 159. That fan is what makes it a perspective
 * sweep instead of a parallel shear. */
void voxel_render(int frame_count)
{
    const int y_pos_anim = frame_count & 0xFF;
    int x_step = -80;
    int x_pos = (128 - 80) * 256;

    for (int col = 0; col < SCREEN_W; col++) {
        int y_ptr = y_pos_anim;
        int x_ptr = x_pos;
        int d4 = 64;                /* running height/screen-row counter */
        int dist = 99;              /* distance counter */
        int screen_row = SCREEN_H - 1;

        while (dist >= 0 && screen_row >= 0) {
            /* 256×256 map indexed by (x_ptr_int_byte, y_ptr_byte). asm
             * packs these as d3 = (x_hi:y_lo) and reads from base+32768
             * with signed 16-bit addressing — equivalent to `& 0xFFFF`.
             * Our layout is row-major in y, so the index is transposed
             * vs the asm; the wave generation is y/x-symmetric so this
             * doesn't change what we read. */
            const int hx = (x_ptr >> 8) & VOXEL_MASK;
            const int hy = y_ptr & VOXEL_MASK;
            const int hidx = hy * VOXEL_DIM + hx;

            const uint8_t h = voxel_height[hidx];
            const uint8_t c = voxel_color [hidx];
            const uint8_t shaded = shade(c, (uint8_t)dist);

            while (d4 < h && screen_row >= 0) {
                chunky[screen_row * SCREEN_W + col] = shaded;
                screen_row--;
                d4++;
            }
            if (d4 > 0) d4--;

            x_ptr += x_step;
            y_ptr++;
            dist--;
        }
        x_pos += 256;
        x_step++;
    }
}
