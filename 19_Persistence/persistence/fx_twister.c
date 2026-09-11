/* Two twisting columns of polished metal.
 *
 * A hexagonal cross-section whose angle depends on the row, so the prism
 * appears to twist down the screen; the twist rate and the base angle move
 * with time. Because the section is convex, the front faces exactly tile the
 * column's width -- so there is no depth test, no overdraw and no sorting:
 * walk the six corners, keep the faces whose normal points at the viewer, and
 * fill between their screen x positions.
 *
 * ------------------------------------------------------------------ shading --
 *
 * The first version shaded each face with a Lambert term and a narrow
 * specular, which is correct and looks like painted cardboard. A flat face has
 * exactly one normal, so it reflects exactly one direction of the world -- and
 * that means a mirrored face can be shaded by a single lookup into a picture
 * of the world, indexed by its normal. That is a matcap, one dimension of it,
 * and it costs three table reads.
 *
 * `s_env` is that world: a bright sky above, a hard bright horizon line, a
 * dark warm floor below, and a moving hot spot for the light. As a face turns,
 * its colour sweeps the table, so the columns pick up a horizon that slides
 * across them and a highlight that runs down the twist. That is what makes
 * metal look like metal -- not the specular power, but the fact that it shows
 * you the room.
 *
 * Cost: six corners, four or five visible faces, about 180 filled pixels a
 * column. Measured at 3,553 cycles a line for two columns over a copper
 * background, which left plenty of room for this.
 */

#include "beam.h"
#include "rgb565.h"
#include "tables.h"
#include "copper.h"
#include "dither.h"
#include "song.h"

#define NCOL   2
#define NSIDE  6
#define SIDE_A (1024 / NSIDE)

typedef struct {
    rgb8_t   bg[PV_H];
    uint16_t ang[NCOL][PV_H];        /* base angle of the section, 0..1023 */
    int16_t  cx [NCOL][PV_H];
    int16_t  rad[NCOL][PV_H];
    uint8_t  shade[NCOL][PV_H];      /* vertical falloff, 0..255            */
    rgb8_t   env[256];               /* the room, indexed by face normal    */
} tw_p_t;

static tw_p_t P[2];

static void twister_enter(void) {}

/* Build the environment for this frame. Rotating it rather than the light is
 * what makes the highlight travel across the columns. */
static void build_env(rgb8_t *env, uint32_t f)
{
    const int spin = (int)(f * 2);
    for (int i = 0; i < 256; i++) {
        const int ang = ((i * 4) + spin) & 1023;
        /* height of the reflected ray: 255 straight up, 0 straight down */
        const int t = (pv_sin16((uint32_t)ang) + 32768) >> 8;

        int r, g, b;
        if (t > 160) {                                  /* sky */
            const int k = (t - 160) * 255 / 95;
            r = 70 + k / 3; g = 96 + k / 2; b = 130 + k / 2;
        } else if (t > 132) {                           /* the horizon line */
            const int k = (t - 132) * 255 / 28;
            r = 200 + k / 5; g = 206 + k / 6; b = 214 + k / 7;
        } else if (t > 104) {                           /* haze under it */
            const int k = (t - 104) * 255 / 28;
            r = 60 + k / 2; g = 54 + k / 2; b = 52 + k / 2;
        } else {                                        /* floor */
            const int k = t * 255 / 104;
            r = 20 + k / 5; g = 16 + k / 6; b = 14 + k / 8;
        }

        /* one hot spot, a narrow lobe centred where the light is */
        int d = ((ang - 190) & 1023); if (d > 512) d = 1024 - d;
        if (d < 44) {
            const int s = (44 - d) * (44 - d) * 255 / (44 * 44);
            r += s; g += s * 62 / 64; b += s * 56 / 64;
        }
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        env[i].r = (uint8_t)r; env[i].g = (uint8_t)g; env[i].b = (uint8_t)b;
    }
}

static void twister_frame(uint32_t f, uint32_t local)
{
    tw_p_t *p = &P[f & 1];
    copper_rows(p->bg, f, COPPER_BARS);
    build_env(p->env, f);

    /* The twist rate breathes, so the prism alternately winds tight and
     * unwinds instead of spinning at one speed. */
    const int twist = 6 + (pv_sin16(f * 3) * 5 >> 15);

    for (int c = 0; c < NCOL; c++) {
        const int dir  = c ? -1 : 1;
        const int base = c ? 428 : 212;
        for (int y = 0; y < PV_H; y++) {
            p->ang[c][y] = (uint16_t)((y * twist * dir + (int)f * 7 * dir + c * 300) & 1023);
            p->cx [c][y] = (int16_t)(base + ((pv_sin16((uint32_t)(y * 2 + f * 4 + c * 512)) * 46) >> 15));
            const int t = y - PV_H / 2;
            p->rad[c][y] = (int16_t)(96 - (t * t) / 900);          /* a slight barrel */
            const int d = t < 0 ? -t : t;
            p->shade[c][y] = (uint8_t)(255 - d * 90 / (PV_H / 2)); /* light falls off */
        }
    }
    (void)local;
}

static void PV_HOT(twister_line)(uint32_t f, uint16_t *px, int y)
{
    const tw_p_t *p = &P[f & 1];
    pv_fill_row_dither(px, &p->bg[y], y);

    for (int c = 0; c < NCOL; c++) {
        const int cx = p->cx[c][y], rad = p->rad[c][y], a0 = p->ang[c][y];
        const int sh = p->shade[c][y];

        int xs[NSIDE + 1];
        for (int i = 0; i <= NSIDE; i++)
            xs[i] = cx + ((pv_cos16((uint32_t)(a0 + i * SIDE_A)) * rad) >> 15);

        for (int k = 0; k < NSIDE; k++) {
            const int na = a0 + k * SIDE_A + SIDE_A / 2;      /* this face's normal */
            if (pv_sin16((uint32_t)na) >= 0) continue;         /* facing away        */

            const rgb8_t e = p->env[(na >> 2) & 255];
            /* One column is warm metal, the other cool, so the two read as
             * different objects lit by the same room. */
            int r, g, b;
            if (c) { r = e.r * 232 >> 8; g = e.g * 244 >> 8; b = e.b; }
            else   { r = e.r;            g = e.g * 240 >> 8; b = e.b * 206 >> 8; }
            r = (r * sh) >> 8; g = (g * sh) >> 8; b = (b * sh) >> 8;

            int x0 = xs[k], x1 = xs[k + 1];
            if (x0 > x1) { const int t = x0; x0 = x1; x1 = t; }
            pv_fill_dither(px, x0, x1, r, g, b, y);
        }
    }
}

const scene_t fx_twister = { "twister", twister_enter, twister_frame, twister_line, NULL, NULL };
