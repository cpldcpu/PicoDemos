/* Two twisting columns.
 *
 * A hexagonal cross-section whose angle depends on the row, so the prism
 * appears to twist down the screen; the twist rate and the base angle move
 * with time. Because the section is convex, the front faces exactly tile the
 * column's width -- so there is no depth test, no overdraw and no sorting:
 * walk the six corners, keep the faces whose normal points at the viewer, and
 * fill between their screen x positions.
 *
 * Six corners, four to five visible faces, about 180 filled pixels a column.
 * ~2,300 cycles a line for two columns over a copper background.
 */

#include "beam.h"
#include "rgb565.h"
#include "tables.h"
#include "copper.h"
#include "song.h"

#define NCOL   2
#define NSIDE  6
#define SIDE_A (1024 / NSIDE)

typedef struct {
    uint16_t bg[PV_H];
    uint16_t ang[NCOL][PV_H];        /* base angle of the section, 0..1023 */
    int16_t  cx [NCOL][PV_H];
    int16_t  rad[NCOL][PV_H];
    uint8_t  shade[NCOL][PV_H];      /* vertical light falloff, 0..255      */
    uint8_t  hue;                    /* which of the two palettes leads     */
} tw_p_t;

static tw_p_t P[2];

static void twister_enter(void) {}

static void twister_frame(uint32_t f, uint32_t local)
{
    tw_p_t *p = &P[f & 1];
    copper_rows(p->bg, f, COPPER_BARS);

    /* The columns lean apart and sway; the twist rate breathes so the prism
     * alternately winds tight and unwinds. */
    const int twist = 6 + (pv_sin16(f * 3) * 5 >> 15);

    for (int c = 0; c < NCOL; c++) {
        const int dir  = c ? -1 : 1;
        const int base = c ? 428 : 212;                       /* screen x */
        for (int y = 0; y < PV_H; y++) {
            p->ang[c][y] = (uint16_t)((y * twist * dir + (int)f * 7 * dir + c * 300) & 1023);
            p->cx [c][y] = (int16_t)(base + ((pv_sin16((uint32_t)(y * 2 + f * 4 + c * 512)) * 46) >> 15));
            /* the column is fatter in the middle of the screen: a slight barrel */
            const int t = y - PV_H / 2;
            p->rad[c][y] = (int16_t)(96 - (t * t) / 900);
            /* light falls off toward the top and bottom */
            const int d = t < 0 ? -t : t;
            p->shade[c][y] = (uint8_t)(255 - d * 110 / (PV_H / 2));
        }
    }
    p->hue = (uint8_t)((local / 100) & 1);
}

static void PV_HOT(twister_line)(uint32_t f, uint16_t *px, int y)
{
    const tw_p_t *p = &P[f & 1];
    pv_fill(px, 0, PV_W, p->bg[y]);

    for (int c = 0; c < NCOL; c++) {
        const int cx = p->cx[c][y], rad = p->rad[c][y], a0 = p->ang[c][y];
        const int sh = p->shade[c][y];

        int xs[NSIDE + 1];
        for (int i = 0; i <= NSIDE; i++)
            xs[i] = cx + ((pv_cos16((uint32_t)(a0 + i * SIDE_A)) * rad) >> 15);

        for (int k = 0; k < NSIDE; k++) {
            const int na = a0 + k * SIDE_A + SIDE_A / 2;      /* this face's normal */
            const int nz = pv_sin16((uint32_t)na);
            if (nz >= 0) continue;                             /* facing away        */

            /* Lambert against a light up and to the left of the viewer, plus a
             * narrow specular where the normal points straight back at us. */
            const int nd = -nz;                                /* 0..32767, facing us */
            const int nx = pv_cos16((uint32_t)na);
            int lum = (nd * 3 / 5 + (nx > 0 ? nx * 2 / 5 : 0)) >> 7;   /* 0..255 */
            if (lum < 0) lum = 0;
            if (lum > 255) lum = 255;
            lum = (lum * sh) >> 8;
            const int spec = nd > 30000 ? (nd - 30000) * 255 / 2767 : 0;

            int r, g, b;
            if ((c ^ p->hue) & 1) { r = 60 + lum;      g = 30 + lum * 3 / 5; b = 20 + lum / 3; }
            else                  { r = 20 + lum / 3;  g = 45 + lum * 4 / 5; b = 70 + lum;     }
            r += spec; g += spec; b += spec;
            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;

            int x0 = xs[k], x1 = xs[k + 1];
            if (x0 > x1) { const int t = x0; x0 = x1; x1 = t; }
            pv_fill(px, x0, x1, rgb565_pack(r, g, b));
        }
    }
}

const scene_t fx_twister = { "twister", twister_enter, twister_frame, twister_line, NULL, NULL };
