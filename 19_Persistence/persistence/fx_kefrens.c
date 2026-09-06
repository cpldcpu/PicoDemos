/* Kefrens bars, 640 wide.
 *
 * The Amiga original is the purest beam effect there is: one line buffer that
 * is never cleared, and every scanline draws one bar into it at a position
 * that depends on the row and the time, then shows the buffer. Because the
 * buffer keeps whatever the rows above drew, the bars hang down the screen
 * like a curtain and the whole picture is a side effect of the beam's order.
 * It cannot be drawn into a framebuffer any faster than this; it can only be
 * drawn slower.
 *
 * Here the buffer is cleared at row 0 so a frame is a pure function of f
 * (this demo seeks), and K bars are drawn per row with different phases. The
 * bar is a 24-pixel lit cylinder: a per-bar gradient strip, copied in.
 *
 * Per row: K bar copies (24 px each) plus one 640-pixel copy out. ~1,200
 * cycles. The cheapest full-screen kernel after the copper.
 */

#include "beam.h"
#include "arena.h"
#include "rgb565.h"
#include "tables.h"
#include "song.h"

#include <string.h>

#define BARS     4
#define BAR_W    24

typedef struct {
    int16_t  x[BARS][PV_H];          /* left edge of each bar per row, or -1 */
    uint16_t strip[BARS][BAR_W];     /* the bar's shaded cross-section       */
    uint16_t bg[PV_H];               /* row colour the buffer is reset to    */
    uint8_t  active;                 /* how many bars are in play            */
} kefrens_p_t;

static kefrens_p_t P[2];
static uint16_t   *s_line;           /* the persistent line buffer, in SMALL */

static void kefrens_enter(void)
{
    s_line = (uint16_t *)ARENA(ARENA_SMALL_OFF + SMALL_KEFRENS_LINE);
    memset(s_line, 0, PV_W * 2);
}

static void shade_strip(uint16_t *strip, int r, int g, int b)
{
    for (int i = 0; i < BAR_W; i++) {
        /* a cylinder lit from the upper left: cosine profile, hot at 1/3 */
        int s = pv_sin16((uint32_t)(i * 512 / BAR_W)) >> 7;         /* 0..255 across */
        int hi = 255 - (i - BAR_W / 3) * (i - BAR_W / 3) * 6; if (hi < 0) hi = 0;
        int rr = (r * s >> 8) + (hi >> 1), gg = (g * s >> 8) + (hi >> 1), bb = (b * s >> 8) + (hi >> 1);
        if (rr > 255) rr = 255;
        if (gg > 255) gg = 255;
        if (bb > 255) bb = 255;
        strip[i] = rgb565_pack(rr, gg, bb);
    }
}

static void kefrens_frame(uint32_t f, uint32_t local)
{
    kefrens_p_t *p = &P[f & 1];
    const uint32_t bar = pv_bar_of_frame(f);
    (void)bar;

    /* bars arrive one at a time over the first bars of the scene */
    int active = 1 + (int)(local / 150); if (active > BARS) active = BARS;
    p->active = (uint8_t)active;

    static const uint8_t col[BARS][3] = { {255, 120, 40}, {60, 200, 255}, {255, 60, 200}, {120, 255, 120} };
    for (int k = 0; k < BARS; k++) shade_strip(p->strip[k], col[k][0], col[k][1], col[k][2]);

    /* dark, slightly warmer toward the bottom so the curtain has a floor */
    for (int y = 0; y < PV_H; y++) p->bg[y] = rgb565_pack(4 + y / 40, 3 + y / 60, 10 + y / 30);

    /* position: two sines per bar, phase by bar, speed by bar */
    const int t = (int)f;
    for (int k = 0; k < BARS; k++) {
        const int spd = 5 + k * 2, amp = 200 + k * 25;
        for (int y = 0; y < PV_H; y++) {
            int a = pv_sin16((uint32_t)(y * 3 + t * spd + k * 256)) * amp >> 15;
            int b = pv_sin16((uint32_t)(y * 7 - t * (spd + 3) + k * 400)) * (amp / 2) >> 15;
            int x = 320 - BAR_W / 2 + a + b;
            if (x < 0) x = 0;
            if (x > PV_W - BAR_W) x = PV_W - BAR_W;
            p->x[k][y] = (int16_t)x;
        }
    }
}

/* Clear the persistent buffer once at the top of the frame. See beam.h: this
 * cannot be a `y == 0` test inside the line function, because the raster split
 * hands this kernel a band that does not start at row 0. */
void kefrens_line0(uint32_t f)
{
    if (!s_line) return;
    pv_fill(s_line, 0, PV_W, P[f & 1].bg[0]);
}

static void PV_HOT(kefrens_line)(uint32_t f, uint16_t *px, int y)
{
    const kefrens_p_t *p = &P[f & 1];
    uint16_t *lb = s_line;

    /* draw this row's bars into the persistent buffer */
    for (int k = 0; k < p->active; k++) {
        const uint16_t *s = p->strip[k];
        uint16_t *d = lb + p->x[k][y];
        for (int i = 0; i < BAR_W; i += 4) { d[i] = s[i]; d[i + 1] = s[i + 1]; d[i + 2] = s[i + 2]; d[i + 3] = s[i + 3]; }
    }

    /* and show the buffer as it stands after this row */
    const uint32_t *src = (const uint32_t *)lb;
    uint32_t *dst = (uint32_t *)px;
    for (int i = 0; i < PV_W / 2; i += 8) {
        dst[i]     = src[i];     dst[i + 1] = src[i + 1]; dst[i + 2] = src[i + 2]; dst[i + 3] = src[i + 3];
        dst[i + 4] = src[i + 4]; dst[i + 5] = src[i + 5]; dst[i + 6] = src[i + 6]; dst[i + 7] = src[i + 7];
    }
}

const scene_t fx_kefrens = { "kefrens", kefrens_enter, kefrens_frame, kefrens_line, NULL, kefrens_line0 };
