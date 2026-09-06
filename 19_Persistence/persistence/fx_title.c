/* The beam.
 *
 * The demo opens with its own subject. A single bright line sweeps down an
 * almost dark screen, and where it passes it leaves the title burned into the
 * phosphor: white-hot right behind the beam, cooling over the next sixty rows
 * into copper, and holding. Three sweeps, three lines of text.
 *
 * This is the effect that only makes sense on a machine with no framebuffer.
 * There is nothing to burn into and nothing to decay; the "afterglow" is the
 * beam's own row index minus this row's, evaluated as the line is generated.
 * The picture is made of the fact that rows are drawn in order and at a known
 * time -- which is the one thing this architecture has and a framebuffer
 * throws away.
 */

#include "beam.h"
#include "rgb565.h"
#include "tables.h"
#include "text.h"
#include "copper.h"
#include "song.h"

#include <string.h>

#define SWEEP_FRAMES 200          /* two bars per sweep */

typedef struct { const char *s; int y0, scale, reveal; } line_t;

static const line_t s_lines[] = {
    { "LATENT PRESENTS",            136, 3, 0 },
    { "PERSISTENCE",                190, 6, 1 },
    { "A DEMO WITH NO FRAMEBUFFER", 268, 2, 2 },
};
#define NLINES ((int)(sizeof s_lines / sizeof s_lines[0]))

typedef struct {
    uint16_t bg[PV_H];
    int16_t  beam;            /* row the beam is on, or -1 */
    uint8_t  sweep;           /* which sweep is running */
    uint8_t  hot;             /* 0..255 intensity of the beam itself */
} title_p_t;

static title_p_t P[2];

static void title_enter(void) {}

static void title_frame(uint32_t f, uint32_t local)
{
    title_p_t *p = &P[f & 1];

    copper_rows(p->bg, f, COPPER_DUSK);
    /* the copper builds over the intro rather than arriving at full strength */
    const int build = local < 400 ? (int)(64 + local * 191 / 400) : 255;
    for (int y = 0; y < PV_H; y++) {
        const uint16_t c = p->bg[y];
        p->bg[y] = rgb565_pack(rgb565_r8(c) * build >> 8, rgb565_g8(c) * build >> 8, rgb565_b8(c) * build >> 8);
    }

    const uint32_t sweep = local / SWEEP_FRAMES;
    const uint32_t phase = local % SWEEP_FRAMES;
    p->sweep = (uint8_t)(sweep > 3 ? 3 : sweep);
    /* the sweep takes 60% of its window, then the picture holds */
    const uint32_t travel = SWEEP_FRAMES * 3 / 5;
    p->beam = phase < travel ? (int16_t)(phase * (PV_H + 80) / travel - 40) : -1;
    p->hot  = 255;
}

/* The afterglow ramp: white at the beam, copper at 60 rows behind, and the
 * resting colour after that. Kept as a function of the row distance so the
 * whole effect is one expression and no state. */
static inline uint16_t glow(int age, int gy)
{
    /* resting colour: a vertical gradient down the glyph, amber to pale */
    const int base_r = 210 + gy * 5, base_g = 120 + gy * 14, base_b = 40 + gy * 12;
    if (age < 0) return 0;
    if (age > 60) return rgb565_pack(base_r, base_g, base_b);
    const int w = (60 - age) * 255 / 60;                 /* 255 at the beam */
    const int r = base_r + ((255 - base_r) * w >> 8);
    const int g = base_g + ((255 - base_g) * w >> 8);
    const int b = base_b + ((255 - base_b) * w >> 8);
    return rgb565_pack(r, g, b);
}

static void PV_HOT(title_line)(uint32_t f, uint16_t *px, int y)
{
    const title_p_t *p = &P[f & 1];
    pv_fill(px, 0, PV_W, p->bg[y]);

    for (int i = 0; i < NLINES; i++) {
        const line_t *L = &s_lines[i];
        const int gy = text_glyph_row(y, L->y0, L->scale);
        if (gy < 0) continue;
        int age;
        if (p->sweep > L->reveal) age = 1000;                       /* burned in earlier */
        else if (p->sweep < L->reveal) continue;                     /* not yet */
        else if (p->beam < 0) age = 1000;                            /* sweep finished */
        else if (y >= p->beam) continue;                             /* the beam has not reached it */
        else age = p->beam - y;
        text_row_centred(px, L->s, L->scale, gy, glow(age, gy));
    }

    /* the beam itself, over everything, with two dimmer rows of bloom */
    const int d = p->beam - y;
    if (p->beam >= 0 && d >= 0 && d < 3) {
        const int w = d == 0 ? 255 : (d == 1 ? 150 : 70);
        pv_fill(px, 0, PV_W, rgb565_pack(w, w, 200 + (w >> 2) > 255 ? 255 : 200 + (w >> 2)));
    }
}

const scene_t fx_title = { "title", title_enter, title_frame, title_line, NULL, NULL };
