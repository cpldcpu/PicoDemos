/* Credits, and the power-off.
 *
 * The scroller rides a sine, which on a beam-raced machine is not a scroll at
 * all: each row simply asks where the text is at this row and this time. The
 * whole "scroller" is one sine lookup per row and a few filled runs.
 *
 * Then the endcard, and then the demo ends the way the machine it runs on
 * ends: the picture collapses to a band, to a line, to a dot, and the
 * phosphor goes out. A CRT losing its deflection is the only ending that
 * belongs on a demo about the beam.
 */

#include "beam.h"
#include "rgb565.h"
#include "tables.h"
#include "text.h"
#include "copper.h"

#include <string.h>

/* ------------------------------------------------------------- scroller -- */

static const char *const s_credits[] = {
    "PERSISTENCE",
    "A LATENT PRODUCTION",
    "",
    "CODE AND DIRECTION",
    "PHOSPHOR",
    "",
    "MODEL",
    "CLAUDE FABLE 5.1",
    "",
    "MUSIC",
    "PHOSPHOR",
    "",
    "CRITIC AND PRODUCER",
    "AZURE",
    "",
    "NATIVE 640 BY 480",
    "NO FRAMEBUFFER",
    "31500 SCANLINES A SECOND",
    "RASPBERRY PI PICO 2",
    "",
    "GREETINGS TO EVERY GROUP",
    "THAT EVER RACED THE BEAM",
    "",
};
#define NCREDITS ((int)(sizeof s_credits / sizeof s_credits[0]))

#define LINE_H   44          /* rows between credit lines */
#define CR_SCALE 3

typedef struct {
    uint16_t bg[PV_H];
    int32_t  scroll;                 /* pixels the text has travelled up */
    int16_t  wave[PV_H];             /* per-row horizontal displacement   */
    uint8_t  fade;                   /* 255 = full                        */
} credits_p_t;

static credits_p_t P[2];

static void credits_enter(void) {}

static void credits_frame(uint32_t f, uint32_t local)
{
    credits_p_t *p = &P[f & 1];

    copper_rows(p->bg, f, COPPER_DUSK);
    p->scroll = (int32_t)local * 5 / 2;                /* 2.5 px a frame */

    for (int y = 0; y < PV_H; y++)
        p->wave[y] = (int16_t)((pv_sin16((uint32_t)(y * 2 + f * 4)) * 90) >> 15);

    /* fade the whole thing out over the last bar of the scene */
    p->fade = local > 700 ? (uint8_t)(local >= 800 ? 0 : 255 - (local - 700) * 255 / 100) : 255;
}

static void PV_HOT(credits_line)(uint32_t f, uint16_t *px, int y)
{
    const credits_p_t *p = &P[f & 1];
    pv_fill(px, 0, PV_W, p->bg[y]);
    if (!p->fade) return;

    /* Which credit line, if any, covers this row? The text starts below the
     * screen and rises, so row y shows glyph row ((y + scroll) - k*LINE_H). */
    const int32_t v = (int32_t)y + p->scroll - PV_H;
    if (v < 0) return;
    const int idx = (int)(v / LINE_H);
    if (idx >= NCREDITS) return;
    const int gy = text_glyph_row((int)(v - (int32_t)idx * LINE_H), 0, CR_SCALE);
    if (gy < 0) return;

    const char *s = s_credits[idx];
    if (!s[0]) return;

    /* the first line is the production and gets the bright colour */
    int r = idx == 0 ? 255 : 210, g = idx == 0 ? 220 : 150, b = idx == 0 ? 160 : 70;
    r = r * p->fade >> 8; g = g * p->fade >> 8; b = b * p->fade >> 8;

    const int w = text_width(s, CR_SCALE);
    text_row(px, (PV_W - w) / 2 + p->wave[y], s, CR_SCALE, gy, rgb565_pack(r, g, b));
}

const scene_t fx_credits = { "credits", credits_enter, credits_frame, credits_line, NULL, NULL };

/* -------------------------------------------------------------- endcard -- */
/* PRODUCTION / GROUP / YEAR, then the tube loses its deflection. */

typedef struct {
    uint16_t bg[PV_H];
    int16_t  y0, y1;          /* the visible band collapses to a line     */
    int16_t  x0, x1;          /* then the line collapses to a dot         */
    uint8_t  bright;          /* and the dot burns out                    */
    uint8_t  squash;          /* 0..255 vertical compression of the image */
} endcard_p_t;

static endcard_p_t E[2];

static void endcard_enter(void) {}

static void endcard_frame(uint32_t f, uint32_t local)
{
    endcard_p_t *p = &E[f & 1];
    copper_rows(p->bg, f, COPPER_DUSK);

    /* 200 frames: hold for 90, collapse vertically over 60, horizontally over
     * 30, then the dot fades for 20. */
    if (local < 90) { p->squash = 0; p->y0 = 0; p->y1 = PV_H; p->x0 = 0; p->x1 = PV_W; p->bright = 255; }
    else if (local < 150) {
        const int k = (int)(local - 90) * 255 / 60;
        p->squash = (uint8_t)k;
        const int h = (PV_H / 2) * (255 - k) / 255;
        p->y0 = (int16_t)(PV_H / 2 - h); p->y1 = (int16_t)(PV_H / 2 + h + 1);
        p->x0 = 0; p->x1 = PV_W; p->bright = 255;
    } else if (local < 180) {
        const int k = (int)(local - 150) * 255 / 30;
        p->squash = 255;
        p->y0 = (int16_t)(PV_H / 2 - 1); p->y1 = (int16_t)(PV_H / 2 + 2);
        const int w = (PV_W / 2) * (255 - k) / 255;
        p->x0 = (int16_t)(PV_W / 2 - w); p->x1 = (int16_t)(PV_W / 2 + w + 1);
        p->bright = 255;
    } else {
        const int k = (int)(local - 180);
        p->squash = 255;
        p->y0 = (int16_t)(PV_H / 2 - 1); p->y1 = (int16_t)(PV_H / 2 + 2);
        p->x0 = (int16_t)(PV_W / 2 - 2); p->x1 = (int16_t)(PV_W / 2 + 3);
        p->bright = (uint8_t)(k >= 20 ? 0 : 255 - k * 255 / 20);
    }
}

static void PV_HOT(endcard_line)(uint32_t f, uint16_t *px, int y)
{
    const endcard_p_t *p = &E[f & 1];

    if (y < p->y0 || y >= p->y1) { pv_black(px); return; }

    if (p->squash >= 255) {
        /* the collapsed line: white, and only as wide as the deflection left */
        const int v = p->bright;
        pv_black(px);
        pv_fill(px, p->x0, p->x1, rgb565_pack(v, v, (v * 5 / 4) > 255 ? 255 : v * 5 / 4));
        return;
    }

    /* Vertical squash: this row shows source row `sy`, the image compressing
     * toward the middle as the deflection fails. */
    const int mid = PV_H / 2;
    const int span = p->y1 - p->y0;
    const int sy = span > 0 ? mid + (y - mid) * PV_H / span : mid;

    pv_fill(px, 0, PV_W, p->bg[sy < 0 ? 0 : (sy >= PV_H ? PV_H - 1 : sy)]);

    static const struct { const char *s; int y0, scale; } lines[] = {
        { "PERSISTENCE", 180, 6 },
        { "LATENT",      262, 4 },
        { "2026",        318, 3 },
    };
    for (int i = 0; i < 3; i++) {
        const int gy = text_glyph_row(sy, lines[i].y0, lines[i].scale);
        if (gy < 0) continue;
        const int r = 240, g = 190 + gy * 6, b = 120 + gy * 12;
        text_row_centred(px, lines[i].s, lines[i].scale, gy, rgb565_pack(r, g > 255 ? 255 : g, b > 255 ? 255 : b));
    }
}

const scene_t fx_endcard = { "endcard", endcard_enter, endcard_frame, endcard_line, NULL, NULL };
