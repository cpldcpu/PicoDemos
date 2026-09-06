/* Copper bars: the cheapest thing a beam can do, and the baseline every other
 * kernel is measured against. One colour per row, one word fill per line.
 *
 * Also the shared background helper: copper_rows() builds a per-row colour
 * table for a given style so the twister and the title can stand on the same
 * kind of light without owning a copy of this code.
 */

#include "beam.h"
#include "rgb565.h"
#include "tables.h"
#include "copper.h"

typedef struct { uint16_t row[PV_H]; } copper_p_t;
static copper_p_t P[2];

void copper_rows(uint16_t *row, uint32_t f, int style)
{
    for (int y = 0; y < PV_H; y++) {
        int r, g, b;
        switch (style) {
        default:
        case COPPER_BARS: {
            /* six bars drifting on sines, additive, over a deep blue */
            r = 6; g = 8; b = 24;
            for (int k = 0; k < 6; k++) {
                int cy = 240 + (pv_sin16((f * (3 + k) + k * 170) & 1023) * 200 >> 15);
                int d = y - cy; if (d < 0) d = -d;
                int a = 40 - d; if (a < 0) a = 0;                    /* triangle, 40 rows */
                a = (a * a) >> 4;                                    /* 0..100 */
                switch (k % 3) {
                case 0: r += a * 2;      g += a / 2; break;          /* orange   */
                case 1: r += a / 3;      g += a;     b += a * 2; break; /* cyan   */
                case 2: r += a * 2;      b += a * 2; break;          /* magenta  */
                }
            }
            break;
        }
        case COPPER_DUSK: {
            /* a vertical gradient: night above, ember at the horizon, dark floor */
            int t = y < 240 ? y : 479 - y;                           /* 0..239, peaks mid */
            int e = (t * t) >> 8;                                    /* 0..223 */
            r = 8 + (e * 3) / 4; g = 4 + e / 4; b = 24 + e / 8;
            if (y > 240) { r = r / 2; g = g / 2; b = b / 2; }
            break;
        }
        }
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        row[y] = rgb565_pack(r, g, b);
    }
}

static void copper_enter(void) {}

static void copper_frame(uint32_t f, uint32_t local)
{
    (void)local;
    copper_rows(P[f & 1].row, f, COPPER_BARS);
}

static void PV_HOT(copper_line)(uint32_t f, uint16_t *px, int y)
{
    pv_fill(px, 0, PV_W, P[f & 1].row[y]);
}

const scene_t fx_copper = { "copper", copper_enter, copper_frame, copper_line, NULL, NULL };
