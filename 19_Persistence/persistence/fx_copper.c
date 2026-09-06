/* Copper bars: the cheapest thing a beam can do, and the baseline every other
 * kernel is measured against. One colour per row, one dithered fill per line.
 *
 * Also the shared background helper: copper_rows() builds a per-row colour
 * table for a given style so the twister, the title and the credits can stand
 * on the same light without each owning a copy of this code.
 *
 * The palette is deliberately narrow. The first version put saturated orange,
 * cyan and magenta bars on a blue field and it read as a test card -- three
 * hues at full chroma, all competing. These are the same three positions in
 * the spectrum pulled most of the way toward the background and toward each
 * other, so the bars read as light falling on something rather than as ink.
 */

#include "beam.h"
#include "rgb565.h"
#include "tables.h"
#include "copper.h"
#include "dither.h"

typedef struct { rgb8_t row[PV_H]; } copper_p_t;
static copper_p_t P[2];

void copper_rows(rgb8_t *row, uint32_t f, int style)
{
    for (int y = 0; y < PV_H; y++) {
        int r, g, b;
        switch (style) {
        default:
        case COPPER_BARS: {
            /* Six bars drifting on sines over a slate field. Each bar adds a
             * warm, a cool or a violet cast rather than a colour: the chroma
             * is about a third of what it was, and the bars overlap into
             * something continuous instead of stacking into white. */
            r = 10; g = 13; b = 22;
            for (int k = 0; k < 6; k++) {
                const int cy = 240 + (pv_sin16((uint32_t)(f * (3 + k) + k * 170)) * 200 >> 15);
                int d = y - cy; if (d < 0) d = -d;
                int a = 44 - d; if (a < 0) a = 0;
                a = (a * a) >> 5;                                  /* 0..60 */
                switch (k % 3) {
                case 0: r += a * 3 / 2; g += a * 3 / 4; b += a / 3;     break;  /* amber  */
                case 1: r += a / 3;     g += a;         b += a * 5 / 4; break;  /* steel  */
                case 2: r += a;         g += a / 2;     b += a * 5 / 4; break;  /* violet */
                }
            }
            break;
        }
        case COPPER_DUSK: {
            /* A vertical gradient: night above, one warm band across the
             * middle, dark floor. It sits behind text, so it stays quiet. */
            const int t = y < 240 ? y : 479 - y;
            const int e = (t * t) >> 9;                            /* 0..112 */
            r = 12 + e; g = 8 + e / 3; b = 26 + e / 5;
            if (y > 240) { r = r * 3 / 5; g = g * 3 / 5; b = b * 3 / 5; }
            break;
        }
        }
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        row[y].r = (uint8_t)r; row[y].g = (uint8_t)g; row[y].b = (uint8_t)b;
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
    pv_fill_row_dither(px, &P[f & 1].row[y], y);
}

const scene_t fx_copper = { "copper", copper_enter, copper_frame, copper_line, NULL, NULL };
