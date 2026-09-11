/* world_side.c -- the window view. PLANNING §7's first system, rebuilt for
 * round two against Phosphor's fourteen notes.
 *
 * The shape is unchanged: Phase's painted plate for rows 0..149, then far,
 * mid and near silhouettes at parallax 1/4, 1/2 and 1 from hashed procedural
 * profiles keyed on world distance, with lights at every layer's depth
 * carrying that depth's projected velocity. What changed is everything that
 * makes it read at speed, and all of it is one idea: **the frame has to show
 * the beat even when there is no lamp in it.**
 *
 *   - telegraph poles, one a beat, in every outdoor world (note 1);
 *   - a ballast band under everything, scrolling at twice the near layer, so
 *     it is stones at rest and streaks at cruise (note 2);
 *   - farms as clusters and villages as huddles, not a regular picket (3);
 *   - the other train as carriages with rectangular windows and gaps (4);
 *   - the tunnel wall two and a half metres away instead of nine, so its
 *     lamps are lines (5);
 *   - thin bridge posts with a top chord and a brace, reflected (6);
 *   - the station's board at full glyph size (7);
 *   - the sleep window as bar 60's composition dimmed to one lamp (8);
 *   - rain over the whole glass (9);
 *   - window grids and a street-lamp row in the city (10);
 *   - a crossing that is two lamps on a mast (12);
 *   - sodium off in daylight (13);
 *   - more platform (14).
 *
 * -- Overscan
 */
#include "render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PAR_FAR  0.25f
#define PAR_MID  0.5f
#define PAR_NEAR 1.0f
#define BASE_FAR  150
#define BASE_MID  168
#define BASE_NEAR 196
#define BALLAST_TOP 216

static float layer_off(float par) { return F.dist * NEAR_PX_PER_UNIT * par; }
static float layer_vel(float par) { return -F.ppf * par; }
static float wrapf(float v, float m) { return v - floorf(v / m) * m; }

/* --------------------------------------------------------------- profiles */

static int prof_soft(int wx, int cell, int amp, uint32_t seed)
{
    return profile1(wx, cell, amp, seed) + profile1(wx, cell / 3 + 1, amp / 3, seed ^ 0x9e37u);
}
/* Boxes: houses, sheds, buildings. One height per cell, no interpolation --
 * that is what makes a roofline instead of a hill. */
static int prof_box(int wx, int cell, int amp, uint32_t seed)
{
    int c = wx >= 0 ? wx / cell : -((-wx + cell - 1) / cell);
    uint32_t h = rhash((uint32_t)c * 2246822519u ^ seed);
    if ((h & 7u) == 0u) return 0;                     /* a gap in the row    */
    return (int)(amp / 3 + (h >> 8) % (unsigned)(amp * 2 / 3 + 1));
}

typedef struct {
    uint32_t seed;
    int   cell, amp, box;
    int   lamp_step;           /* screen pixels between lamps, 0 = none     */
    int   lamp_h;
    uint16_t core, glow;
    int   radius, intensity, core_r;
    int   windows;             /* lit windows per box cell, 0 = none        */
    int   grid;                /* note 10: draw them as aligned rows        */
    int   poles;               /* note 1: telegraph poles on the near layer */
    int   farms;               /* note 3: clustered farm lamps              */
} band_t;

static void HOT(band_fill)(int baseline, int floor_y, float off, const band_t *s, uint16_t col)
{
    const int o = (int)off;
    for (int x = 0; x < WIDTH; x++) {
        int h = s->box ? prof_box(x + o, s->cell, s->amp, s->seed)
                       : prof_soft(x + o, s->cell, s->amp, s->seed);
        vspan(x, baseline - h, floor_y, col);
    }
}

/* Note 10. Lit windows in aligned rows rather than scattered dots: a
 * building has floors, and two or three rows of them at the same height is
 * what says "building" at eleven pixels tall. */
static void band_windows(int baseline, float off, const band_t *s, uint16_t col)
{
    const int o = (int)off;
    const int dim = F.lightlv > 200 ? 1 : 0;
    for (int cx = -s->cell; cx < WIDTH + s->cell; cx += s->cell) {
        const int wx = ((cx + o) / s->cell) * s->cell;
        const int sx = wx - o;
        const int h = prof_box(wx, s->cell, s->amp, s->seed);
        if (h < 10) continue;
        const uint32_t g = rhash((uint32_t)wx * 374761393u ^ (s->seed + 5u));
        if (s->grid) {
            const int rows = 2 + (int)((g >> 3) & 1u);        /* two or three */
            const int step_y = (h - 4) / (rows + 1);
            if (step_y < 3) continue;
            for (int r = 1; r <= rows; r++) {
                const int wyy = baseline - 3 - r * step_y;
                for (int c = 0; c < s->cell / 5; c++) {
                    const uint32_t q = rhash(g + (uint32_t)(r * 31 + c) * 2654435761u);
                    if (q & 1u) continue;                     /* a dark one   */
                    const int wxx = sx + 2 + c * 5;
                    if (wxx < -2 || wxx > WIDTH + 2) continue;
                    rect(wxx, wyy, wxx + 1, wyy + (dim ? 1 : 2), (q & 2u) ? col : C_SODIUM_H);
                }
            }
        } else {
            for (int k = 0; k < s->windows; k++) {
                const uint32_t q = rhash(g + (uint32_t)k * 2654435761u);
                if (q & 1u) continue;
                const int wxx = sx + 2 + (int)((q >> 3) % (unsigned)(s->cell - 4));
                const int wyy = baseline - 4 - (int)((q >> 11) % (unsigned)(h - 6));
                rect(wxx, wyy, wxx + 1, wyy + 2, (q & 2u) ? col : C_SODIUM_H);
            }
        }
    }
}

static void band_lamps(int baseline, float off, float vel, const band_t *s)
{
    if (!s->lamp_step) return;
    const int inten = daylight_scale(s->core, s->intensity);
    const int S = s->lamp_step;
    const int first = (int)floorf(off / (float)S);
    for (int i = first; ; i++) {
        const float x = (float)(i * S) - off;
        if (x > WIDTH + 40.f) break;
        if (x < -40.f) continue;
        const uint32_t h = rhash((uint32_t)i * 2654435761u ^ s->seed);
        const float y = (float)(baseline - s->lamp_h) - (float)(h % 7u);
        if (s->lamp_h > 12) vspan((int)x, (int)y + 2, baseline, RGB(0x08, 0x0A, 0x10));
        if (inten <= 0) { if (s->core_r) disc(x, y, (float)s->core_r, RGB(0x0C, 0x0C, 0x10)); continue; }
        light_t L = { x, y, vel, 0.f, (float)s->radius, s->core, s->glow,
                      (int16_t)inten, (int16_t)s->core_r, 0 };
        light_draw(&L);
    }
}

/* ----------------------------------------------------- note 1: the poles */

/* Telegraph poles, one a beat, on the near layer. They are dark, so they
 * never streak; what they do is put the tempo on the screen in every frame,
 * including the frames with no lamp in them. They are also, literally, the
 * one visual sentence's crossbars standing in the real world, which is why
 * the wire between them sags the way the dream's rails converge. */
static void HOT(poles)(float off)
{
    const int S = (int)BEAT_PX;
    const int first = (int)floorf(off / (float)S) - 1;
    const uint16_t dark = RGB(0x04, 0x05, 0x0A);
    const int top = 58, foot = BASE_NEAR - 2;
    float prev_x = 0.f; int have = 0;
    for (int i = first; ; i++) {
        const float x = (float)(i * S) - off;
        if (x > WIDTH + (float)S) break;
        if (x < -(float)S - 20.f) { have = 0; continue; }
        const uint32_t h = rhash((uint32_t)i * 2654435761u ^ 0x7013u);
        const int t = top + (int)(h % 9u);
        rect((int)x - 1, t, (int)x + 1, foot, dark);
        hspan(t + 5, (int)x - 7, (int)x + 7, dark);
        hspan(t + 9, (int)x - 5, (int)x + 5, dark);
        if (have) {
            /* the wire: a sag between this pole and the last, four chords */
            const float dx = x - prev_x;
            const float sag = 11.f;
            int px = (int)prev_x, py = t + 6;
            for (int k = 1; k <= 4; k++) {
                const float u = (float)k * 0.25f;
                const int qx = (int)(prev_x + dx * u);
                const int qy = t + 6 + (int)(sag * 4.f * u * (1.f - u));
                line_a(px, py, qx, qy, RGB(0x0A, 0x0C, 0x14), 24);
                px = qx; py = qy;
            }
        }
        prev_x = x; have = 1;
    }
}

/* --------------------------------------------------- note 2: the ballast */

/* The bottom twenty-four rows, scrolling at twice the near layer. At rest
 * each stone is three pixels of (2,2,3) on (0,0,1); at cruise the same
 * stones are fifty-pixel streaks, because the length of a stone is the
 * distance it travels in a field. Speed lives here: it is the only part of
 * the frame that is close enough to smear on its own. */
static void HOT(ballast)(float off)
{
    for (int y = BALLAST_TOP; y < HEIGHT; y++) {
        const int k = (y - BALLAST_TOP) * 8 / (HEIGHT - BALLAST_TOP);
        hspan(y, 0, WIDTH - 1, rgb(2 + k, 2 + k, 8 + k * 2));
    }
    const float o = off * 2.f;
    const int len = 3 + (int)(F.ppf * 2.f);
    const uint16_t stone = RGB(0x20, 0x20, 0x2C);
    for (int y = BALLAST_TOP; y < HEIGHT; y++) {
        const int S = 22;
        const uint32_t rs = rhash((uint32_t)y * 2654435761u ^ 0x51A7u);
        const float ro = o + (float)(rs % 22u);
        const int first = (int)floorf(ro / (float)S);
        for (int i = first; ; i++) {
            const float x = (float)(i * S) - ro;
            if (x > WIDTH) break;
            if (x < -(float)len) continue;
            const uint32_t h = rhash((uint32_t)i * 374761393u ^ rs);
            if (h & 1u) continue;
            hspan_a(y, (int)x, (int)x + len - 1 + (int)(h % 3u), stone, (h & 2u) ? 30 : 19);
        }
    }

}

/* ------------------------------------------------------ note 3: the farms */

/* A farm is three to five lamps at different depths and heights -- a yard
 * lamp high, a window low, a barn door -- placed by the hash so a seek lands
 * on the same farm; a village is a dozen dim window dots in a huddle on the
 * far layer. Between them the poles carry the beat. The grid stays: farms
 * sit on a three-beat grid, so they still arrive on a beat. */
static void farms(float on, float of, float vn, float vf)
{
    const int S = (int)(BEAT_PX * 2.f);
    const int first = (int)floorf(on / (float)S) - 1;
    for (int i = first; ; i++) {
        const float bx = (float)(i * S) - on;
        if (bx > WIDTH + 110.f) break;
        if (bx < -110.f) continue;
        const uint32_t h = rhash((uint32_t)i * 2246822519u ^ 0xFA47u);
        const int n = 3 + (int)((h >> 2) & 3u);            /* three to five   */
        for (int k = 0; k < n; k++) {
            const uint32_t q = rhash(h + (uint32_t)k * 2654435761u);
            const float dx = (float)((int)(q % 190u) - 95);
            const float y = (float)(BASE_NEAR - 6 - (int)((q >> 8) % 34u));
            const int high = ((q >> 16) & 3u) == 0u;
            const uint16_t core = high ? C_SODIUM : C_FLUO_H;
            const int inten = daylight_scale(core, high ? 15 : 11);
            if (inten <= 0) continue;
            if (high) vspan((int)(bx + dx), (int)y + 2, BASE_NEAR, RGB(0x07, 0x08, 0x0E));
            light_t L = { bx + dx, y, vn, 0.f, high ? 13.f : 8.f,
                          core, high ? C_SODIUM_H : C_FLUO_H,
                          (int16_t)inten, (int16_t)(high ? 2 : 1), 0 };
            light_draw(&L);
        }
    }
    /* the village on the far layer: one a shot, a huddle of window dots */
    const int VS = (int)(BEAT_PX * 4.f);
    const int vfirst = (int)floorf(of / (float)VS);
    for (int i = vfirst; ; i++) {
        const float bx = (float)(i * VS) - of;
        if (bx > WIDTH + 60.f) break;
        if (bx < -60.f) continue;
        const uint32_t h = rhash((uint32_t)i * 668265263u ^ 0x1D0Cu);
        if ((h & 1u) == 0u) continue;
        for (int k = 0; k < 12; k++) {
            const uint32_t q = rhash(h + (uint32_t)k * 374761393u);
            const int x = (int)(bx + (float)((int)(q % 46u) - 23));
            const int y = BASE_FAR - 2 - (int)((q >> 8) % 7u);
            if (F.lightlv > 200) continue;
            px(x, y, (q & 4u) ? C_SODIUM_H : C_FLUO_H, 22);
            (void)vf;
        }
    }
}

/* ------------------------------------------------------------- the water */

static void HOT(water)(int surface, uint16_t sea)
{
    const float phase = F.dist * 0.02f;
    for (int y = surface; y < HEIGHT; y++) {
        const int d = y - surface;
        int src = surface - 1 - (d * d) / 26;
        if (src < 0) src = 0;
        const float amp = 1.f + (float)d * 0.10f;
        const int off = (int)(fsin((float)d * 0.31f + phase) * amp + fsin((float)d * 0.13f - phase * 0.7f) * amp * 0.6f);
        const uint16_t *s = g_fb + (unsigned)src * WIDTH;
        uint16_t *o = g_fb + (unsigned)y * WIDTH;
        const uint8_t *bay = bayer4 + ((y & 3) << 2);
        int fade = 12 + d / 6; if (fade > 24) fade = 24;
        for (int x = 0; x < WIDTH; x++) {
            int sx = x + off; if (sx < 0) sx = 0; if (sx > WIDTH - 1) sx = WIDTH - 1;
            o[x] = mixc(s[sx], sea, fade + (bay[x & 3] >> 3));
        }
        g_stats.spans++;
    }
}

/* ------------------------------------------ note 4: the other train */

/* Carriages, not a string of bulbs. The windows' cores are rectangles about
 * seven by four at this depth, in a row at a pitch wider than their own
 * halos so they stay separate; the carriage has a roof line one step above
 * the silhouette black; and every eight windows there is an inter-car gap
 * where the sky shows through, which is the thing that makes it a train
 * with a length rather than a lit bar. */
#define CAR_WINDOWS 8
static void HOT(passing_train_draw)(void)
{
    const float u = (float)F.since / (float)BAR_SAMPLES;
    if (u > 1.f) return;
    const float span = 1140.f, len = 760.f;
    const float front = 340.f - span * u;
    const float vel = -span / 90.f;               /* px per field over a bar */
    const int top = 96, bot = 168;
    const float pitch = (float)RHYTHM / 2.f;      /* 37 px, the shared spacing */
    const float car = pitch * (float)CAR_WINDOWS;
    const float gap = 13.f;
    /* the carriages, each its own body with sky between */
    for (float cx = front; cx < front + len; cx += car) {
        const int x0 = (int)cx, x1 = (int)(cx + car - gap);
        if (x1 < -8 || x0 > WIDTH + 8) continue;
        rect(x0, top, x1, bot, RGB(0x02, 0x03, 0x05));
        hspan(top, x0, x1, RGB(0x14, 0x18, 0x24));            /* the roof line */
        hspan(top + 1, x0, x1, RGB(0x08, 0x0A, 0x10));
        hspan(bot, x0, x1, RGB(0x0A, 0x0C, 0x12));            /* the skirt     */
    }
    for (float wx = front + 20.f; wx < front + len - 26.f; wx += pitch) {
        const float within = wx - front;
        if (fmodf(within, car) > car - gap - 12.f) continue;  /* in the gap    */
        if (wx < -50.f || wx > WIDTH + 50.f) continue;
        window_light(wx + 4.f, (float)(top + 24), vel, 0.f, 7, 4, 11.f,
                     C_FLUO, C_FLUO_H, 16, 4);
    }
    {
        light_t L = { front - 4.f, (float)(top + 42), vel, 0.f, 26.f,
                      RGB(0xFF, 0xFF, 0xF0), RGB(0xE0, 0xE8, 0xFF), 13, 4, 0 };
        light_draw(&L);
    }
}

/* --------------------------------------------------------- note 9: rain */

/* Forty-eight droplets over the whole glass. Each is a lens -- its interior
 * samples the page through a fixed offset, so the lights behind bend through
 * it -- with a one-pixel bright rim on its upper left where the glass turns
 * to the sky. They drift down and left with the distance, which means they
 * stand still when the train stands, and at 64 the passenger falls asleep
 * looking at motionless water on the window.
 *
 * Round one drew them in a column. The cause was one line: the x, the fall
 * rate and the radius were all taken from different shifts of the SAME
 * 32-bit hash, and `rnd01(h >> 15)` has seventeen bits left to divide by
 * 2^24 -- a number between 0 and 0.008. They are four independent hashes
 * now. -- Overscan */
static void HOT(droplet)(int cx, int cy, int r)
{
    for (int dy = -r; dy <= r; dy++) {
        const int y = cy + dy;
        if ((unsigned)y >= HEIGHT) continue;
        const int rem = r * r - dy * dy;
        if (rem < 0) continue;
        const int half = (int)sqrtf((float)rem);
        uint16_t *o = g_fb + (unsigned)y * WIDTH;
        for (int dx = -half; dx <= half; dx++) {
            const int x = cx + dx;
            if ((unsigned)x >= WIDTH) continue;
            int sx = cx - dx * 2, sy = cy - dy * 2 + r + r;
            if ((unsigned)sx >= WIDTH) sx = x;
            if ((unsigned)sy >= HEIGHT) sy = y;
            o[x] = g_fb[(unsigned)sy * WIDTH + (unsigned)sx];
        }
    }
    /* the rim: bright where the glass turns up-left, dark where it turns away */
    for (int k = 0; k < 10; k++) {
        const float a = 2.35f + (float)k * 0.16f;             /* the upper left */
        px(cx + (int)(fcos(a) * (float)r), cy + (int)(fsin(a) * (float)r), C_FLUO, 20);
    }
    for (int k = 0; k < 12; k++) {
        const float a = -0.9f + (float)k * 0.16f;             /* the lower right */
        px(cx + (int)(fcos(a) * (float)r), cy + (int)(fsin(a) * (float)r), RGB(0x00, 0x00, 0x08), 14);
    }
}

void HOT(rain_draw)(void)
{
    const float travel = F.dist * NEAR_PX_PER_UNIT;
    for (int i = 0; i < 48; i++) {
        const uint32_t h0 = rhash((uint32_t)i * 2654435761u ^ 0x9A11u);
        const uint32_t h1 = rhash((uint32_t)i * 374761393u ^ 0x5C2Bu);
        const uint32_t h2 = rhash((uint32_t)i * 668265263u ^ 0x2F19u);
        const uint32_t h3 = rhash((uint32_t)i * 2246822519u ^ 0xB4D7u);
        const float rate = 0.030f + rnd01(h0) * 0.045f;
        const float fall = travel * rate;
        const float y = wrapf(rnd01(h1) * (HEIGHT + 40.f) + fall, HEIGHT + 40.f) - 20.f;
        const float x = wrapf(rnd01(h2) * (WIDTH + 80.f) - fall * 0.55f, WIDTH + 80.f) - 40.f;
        droplet((int)x, (int)y, 2 + (int)(rnd01(h3) * 3.4f));
    }
    g_stats.spans += 48;
}

/* ------------------------------------------------------------- the roofs */

static void station_roof(void)
{
    const float off = layer_off(PAR_NEAR), vel = layer_vel(PAR_NEAR);
    rect(0, 0, WIDTH - 1, 30, RGB(0x03, 0x04, 0x06));
    rect(0, 31, WIDTH - 1, 38, F.dawn > 0.5f ? RGB(0x38, 0x50, 0x70) : RGB(0x10, 0x16, 0x24));
    const int S = RHYTHM;
    const int first = (int)floorf(off / (float)S);
    const int inten = daylight_scale(C_FLUO, 14);
    for (int i = first; ; i++) {
        const float x = (float)(i * S) - off;
        if (x > WIDTH + 30.f) break;
        if (x < -30.f) continue;
        rect((int)x - 12, 40, (int)x + 12, 43, inten ? C_FLUO : RGB(0x18, 0x1C, 0x24));
        if (!inten) continue;
        light_t L = { x, 42.f, vel, 0.f, 18.f, C_FLUO, C_FLUO_H, (int16_t)inten, 0, 0 };
        light_draw(&L);
    }
}

/* -------------------------------------------- note 5: inside the tunnel */

/* The wall is two and a half metres from the glass, not the nine the open
 * country is, so a lamp on it crosses at about a hundred pixels a field --
 * three and a half times the near layer. At that speed a lamp is not a
 * streaked disc, it is a line, and it stays a bright line only because a
 * tunnel lamp is far above the top of a five-bit channel: over-range 16.
 * Each lamp also lights the wall around it, a dim warm patch behind the
 * lining rather than a flash of the whole palette -- Phase's rule, and the
 * reason cut_check does not see the lamps as cuts. */
static void HOT(tunnel_wall)(void)
{
    rect(0, 0, WIDTH - 1, HEIGHT - 1, RGB(0x0C, 0x0C, 0x12));
    const float off = layer_off(PAR_NEAR) * 3.6f;         /* 2.5 m, not 9 m  */
    const float vel = -F.ppf * 3.6f;
    const int lamp_y = 62;

    /* Two lamps a beat, which is what a real tunnel gives at this speed:
     * lamps about ten metres apart and the train at 51.2 m/s is 5.1 lamps a
     * second, and a beat is 0.375 s. */
    const int LS = (int)(BEAT_PX * 3.6f / 2.f);
    const int lfirst = (int)floorf(off / (float)LS) - 1;

    /* The pools of light go on first and the lining is drawn over them. A
     * lamp two and a half metres away lights six metres of wall, which at
     * this focal length is seven hundred pixels -- so the pool is much wider
     * than the frame and the wall is never black between lamps, which is the
     * difference between a tunnel and a dark room with lights in it. It is
     * an ellipse of rows rather than a halo because a 350-pixel halo is a
     * quarter of a million divides and this is three hundred spans. */
    for (int i = lfirst; ; i++) {
        const float x = (float)(i * LS) - off;
        if (x > WIDTH + 700.f) break;
        if (x < -700.f) continue;
        /* Four nested ellipses rather than one: a single span per row gives
         * the pool a hard parabolic edge sweeping across the wall, which
         * reads as a shape rather than as light. Four passes at a quarter of
         * the alpha each is a four-step radial falloff for four hundred
         * spans, and at these alphas the steps are under one DAC level. */
        for (int pass = 0; pass < 4; pass++) {
            const float scale = 1.f - (float)pass * 0.24f;
            for (int y = 4; y < HEIGHT - 20; y += 2) {
                const float dy = (float)(y - lamp_y - 40) / (130.f * scale);
                const float k = 1.f - dy * dy;
                if (k <= 0.f) continue;
                const int half = (int)(350.f * scale * k);
                hspan_a(y, (int)x - half, (int)x + half, RGB(0x60, 0x50, 0x2C), 5);
                hspan_a(y + 1, (int)x - half, (int)x + half, RGB(0x60, 0x50, 0x2C), 5);
            }
        }
    }

    /* the lining: courses across, segment joints down every half beat */
    for (int y = 8; y < HEIGHT; y += 26) {
        hspan_a(y, 0, WIDTH - 1, RGB(0x30, 0x2C, 0x30), 24);
        hspan_a(y + 1, 0, WIDTH - 1, RGB(0x04, 0x04, 0x08), 26);
    }
    {
        const int S = (int)(BEAT_PX * 3.6f / 2.f);        /* a half beat      */
        const int first = (int)floorf(off / (float)S) - 1;
        for (int i = first; ; i++) {
            const float x = (float)(i * S) - off;
            if (x > WIDTH) break;
            if (x < -6.f) continue;
            vspan((int)x, 0, HEIGHT - 1, RGB(0x14, 0x14, 0x1C));
            vspan((int)x + 1, 0, HEIGHT - 1, RGB(0x04, 0x04, 0x08));
            /* the cable bracket at every joint */
            rect((int)x - 4, 190, (int)x + 4, 194, RGB(0x18, 0x18, 0x22));
        }
    }
    /* the cable run */
    hspan(196, 0, WIDTH - 1, RGB(0x22, 0x22, 0x2C));
    hspan(197, 0, WIDTH - 1, RGB(0x10, 0x10, 0x18));

    /* the lamps themselves: lines */
    for (int i = lfirst; ; i++) {
        const float x = (float)(i * LS) - off;
        if (x > WIDTH + 240.f) break;
        if (x < -240.f) continue;
        light_t L = { x, (float)lamp_y, vel, 0.f, 13.f, C_FLUO, C_FLUO_H, 20, 3, 16 };
        light_draw(&L);
    }
    ballast(off * 0.5f);
}

/* --------------------------------------------------- note 12: the crossing */

/* Two red lamps on a mast, alternating on the 8th for four beats, with the
 * barrier arm down. One red blob is a warning light; two alternating on a
 * mast with an arm is a level crossing. */
static void crossing(void)
{
    if (F.since >= 4u * BEAT_SAMPLES) return;
    const unsigned eighth = F.since / (STEP_SAMPLES * 2u);
    const float travelled = F.dist - (float)(song_distance(F.sample - F.since) >> 8);
    const float at = 250.f - travelled * NEAR_PX_PER_UNIT;
    if (at < -40.f) return;
    const float vel = layer_vel(PAR_NEAR);
    const int x = (int)at;
    /* the mast, the head and the barrier arm, all silhouette */
    rect(x - 1, 118, x + 1, BASE_NEAR, RGB(0x04, 0x05, 0x0A));
    hspan(126, x - 15, x + 15, RGB(0x04, 0x05, 0x0A));
    hspan(127, x - 15, x + 15, RGB(0x04, 0x05, 0x0A));
    for (int k = 0; k < 26; k++)                          /* the arm, down    */
        px(x + 4 + k * 3, 150 + k, RGB(0x06, 0x07, 0x0C), 30);
    for (int k = 0; k < 2; k++) {
        const float lx = at + (k ? 15.f : -15.f);
        const int on = ((int)eighth & 1) == k;
        /* a halo narrower than the gap between them, or the crossing is one
         * red blob that happens to move */
        light_t L = { lx, 132.f, vel, 0.f, on ? 9.f : 4.f, C_RED, C_RED,
                      (int16_t)(on ? 26 : 3), (int16_t)(on ? 3 : 1), 0 };
        light_draw(&L);
    }
}

/* -------------------------------------- notes 8 and 14: platform, station */

static void platform_edge(float on)
{
    rect(0, BASE_NEAR - 6, WIDTH - 1, HEIGHT - 1, RGB(0x0A, 0x0C, 0x10));
    hspan(BASE_NEAR - 6, 0, WIDTH - 1, RGB(0x20, 0x24, 0x2C));
    const int S = 12; const int first = (int)floorf(on / (float)S);
    for (int i = first; ; i++) {
        const float x = (float)(i * S) - on;
        if (x > WIDTH) break; if (x < -S) continue;
        rect((int)x, BASE_NEAR - 4, (int)x + 5, BASE_NEAR - 2, RGB(0x16, 0x18, 0x1E));
    }
}

/* Note 14: the platform is not just an edge and two lamps. A bench and a
 * running-in board every beat and a half, on the same scroll, so the platform
 * has furniture to slide past when it starts to move at 6:1.
 *
 * They are NOT drawn in the silhouette black the note asked for, and the
 * reason is worth the two lines: the mid layer behind them is already that
 * exact colour -- five-bit (0,0,1) -- and a black bench in front of a black
 * building is invisible. It is invisible in life too, but in life the bench
 * is two metres from a sodium lamp and this one is as well, so it is drawn
 * in the tone the platform's own lip is drawn in, with a lit top edge. It
 * still reads as a silhouette, because everything around it is brighter or
 * black; it just is not the same black as the thing behind it. -- Overscan */
static void platform_furniture(float on)
{
    /* Three quarters of a beat, not a beat and a half: at the wider spacing
     * a platform had furniture on it a third of the time, which is not a
     * platform with furniture, it is a platform that occasionally has a
     * bench. */
    const int S = (int)(BEAT_PX * 0.75f);
    const int first = (int)floorf(on / (float)S) - 1;
    const uint16_t body = RGB(0x14, 0x16, 0x20), lit = RGB(0x2C, 0x2E, 0x38);
    for (int i = first; ; i++) {
        const float x = (float)(i * S) - on;
        if (x > WIDTH + 40.f) break;
        if (x < -40.f) continue;
        const uint32_t h = rhash((uint32_t)i * 2654435761u ^ 0x3B1Fu);
        if (h & 1u) {                                  /* a bench            */
            rect((int)x - 13, 176, (int)x + 13, 179, body);
            hspan(176, (int)x - 13, (int)x + 13, lit);
            rect((int)x - 13, 179, (int)x - 11, 191, body);
            rect((int)x + 11, 179, (int)x + 13, 191, body);
            rect((int)x - 13, 166, (int)x + 13, 168, body);
            hspan(166, (int)x - 13, (int)x + 13, lit);
        } else {                                       /* a running-in sign  */
            rect((int)x - 1, 156, (int)x + 1, 191, body);
            rect((int)x - 17, 140, (int)x + 17, 157, body);
            hspan(140, (int)x - 17, (int)x + 17, lit);
            rect((int)x - 15, 143, (int)x + 15, 154, RGB(0x06, 0x07, 0x0E));
            hspan(148, (int)x - 12, (int)x + 12, RGB(0x50, 0x54, 0x60));
        }
    }
}

/* --------------------------------------------------- world profiles */

static void style_for(unsigned world, band_t *far_s, band_t *mid_s, band_t *near_s)
{
    band_t f = { 0x51EEu, 90, 26, 0, 0, 0, C_SODIUM, C_SODIUM_H, 6, 10, 1, 0, 0, 0, 0 };
    band_t m = { 0xB0A7u, 46, 20, 0, 0, 0, C_SODIUM, C_SODIUM_H, 8, 12, 2, 0, 0, 0, 0 };
    band_t n = { 0x3C1Du, 30, 14, 0, 260, 30, C_SODIUM, C_SODIUM_H, 13, 14, 2, 0, 0, 0, 0 };
    switch (world) {
    case WORLD_PLATFORM:
        f.box = 1; f.cell = 70; f.amp = 22;
        m.box = 1; m.cell = 40; m.amp = 18; m.windows = 3;
        n.cell = 200; n.amp = 4; n.lamp_step = (int)(BEAT_PX / 3.f); n.lamp_h = 34;
        n.radius = 15; n.intensity = 16; n.core_r = 3;
        break;
    case WORLD_SUBURBS:
        f.box = 1; f.cell = 84; f.amp = 20;
        m.box = 1; m.cell = 44; m.amp = 24; m.windows = 4; m.grid = 1;
        n.cell = 40; n.amp = 10; n.lamp_step = (int)(BEAT_PX * 2.f); n.lamp_h = 40;
        n.radius = 14; n.intensity = 15; n.core_r = 2; n.poles = 1; n.farms = 1;
        break;
    case WORLD_LINE:
    case WORLD_OPEN:
        f.cell = 130; f.amp = 34;
        m.cell = 70; m.amp = 18;
        n.cell = 34; n.amp = 12; n.lamp_step = 0; n.lamp_h = 16;
        n.radius = 11; n.intensity = 13; n.core_r = 2; n.poles = 1; n.farms = 1;
        break;
    case WORLD_BRIDGE:
        f.cell = 120; f.amp = 12;
        m.cell = 60; m.amp = 6;
        n.cell = 400; n.amp = 2; n.lamp_step = 0;
        break;
    case WORLD_CITY:
        f.box = 1; f.cell = 46; f.amp = 46; f.windows = 5; f.grid = 1;
        m.box = 1; m.cell = 34; m.amp = 40; m.windows = 6; m.grid = 1;
        n.box = 1; n.cell = 60; n.amp = 22; n.windows = 3; n.grid = 1;
        n.lamp_step = (int)(BEAT_PX / 2.f); n.lamp_h = 30;
        n.radius = 14; n.intensity = 15; n.core_r = 3;
        break;
    case WORLD_YARD:
        f.box = 1; f.cell = 90; f.amp = 16;
        m.box = 1; m.cell = 56; m.amp = 14; m.windows = 2;
        n.cell = 120; n.amp = 5; n.lamp_step = (int)(BEAT_PX / 4.f); n.lamp_h = 42;
        n.radius = 15; n.intensity = 15; n.core_r = 3; n.poles = 1;
        break;
    case WORLD_STATION:
    case WORLD_TERMINUS:
        f.box = 1; f.cell = 70; f.amp = 20;
        m.box = 1; m.cell = 46; m.amp = 16; m.windows = 3; m.grid = 1;
        n.cell = 240; n.amp = 4; n.lamp_step = (int)(BEAT_PX / 2.f); n.lamp_h = 32;
        n.radius = 15; n.intensity = 16; n.core_r = 3;
        break;
    case WORLD_FIELDS:
        f.cell = 150; f.amp = 14;
        m.cell = 90; m.amp = 8;
        n.cell = 60; n.amp = 6; n.lamp_step = 0; n.lamp_h = 14;
        n.radius = 10; n.intensity = 11; n.core_r = 2; n.poles = 1; n.farms = 1;
        break;
    case WORLD_COAST:
        f.cell = 160; f.amp = 10;
        m.cell = 100; m.amp = 5;
        n.cell = 70; n.amp = 6; n.lamp_step = 0; n.lamp_h = 14;
        n.radius = 10; n.intensity = 11; n.core_r = 2; n.poles = 1;
        break;
    default: break;
    }
    *far_s = f; *mid_s = m; *near_s = n;
}

void far_layer(int baseline, float offset, int amp_div)
{
    band_t f, m, n; style_for(F.world, &f, &m, &n);
    if (amp_div > 1) f.amp = f.amp / amp_div + 1;
    band_fill(baseline, baseline + 3, offset, &f, C_LAND);
}

/* ------------------------------------------------------- note 6: bridge */

/* Thin posts, one a beat, with a top chord across the frame and a diagonal
 * brace between them. They are drawn above the waterline before the water
 * pass, so the water reflects them; the deck comes after. A slab a fifth of
 * the frame wide is a wall, not a bridge. */
static void bridge_posts(float on, int above_water)
{
    const int S = (int)BEAT_PX;
    const int first = (int)floorf(on / (float)S) - 1;
    const uint16_t dark = RGB(0x02, 0x03, 0x06);
    const int bottom = above_water ? HORIZON : HEIGHT - 1;
    float prev = 0.f; int have = 0;
    for (int i = first; ; i++) {
        const float x = (float)(i * S) - on;
        if (x > WIDTH + (float)S) break;
        if (x < -(float)S - 10.f) { have = 0; continue; }
        rect((int)x - 3, 0, (int)x + 3, bottom, dark);
        if (have && above_water) {
            /* the brace, corner to corner of the bay */
            line_a((int)prev, 18, (int)x, 116, dark, 22);
            line_a((int)prev, 116, (int)x, 18, dark, 18);
        }
        prev = x; have = 1;
    }
    if (above_water) {
        rect(0, 0, WIDTH - 1, 15, dark);                  /* the top chord   */
        hspan(16, 0, WIDTH - 1, RGB(0x08, 0x0A, 0x12));
    }
}

/* ------------------------------------------------------------ the shots */

/* Note 8. The sleep window is bar 60's composition, dimmed. Everything is
 * drawn exactly as the station shot at 60 draws it and then the whole page
 * is scaled toward black over the two bars, while one sodium lamp is drawn
 * on top afterwards with a widening halo -- so the lamp survives the dimming
 * and nothing else does. It is a lighting change inside a shot, not a fade
 * between shots, which is why cut_check still sees no discontinuity here.
 * The carriage reflection is the roof's own tubes, mirrored faintly low in
 * the glass; that is what a night train window actually shows, and the three
 * discs of round one were a guess. */
static void station_body(int stopped);

static void sleep_window(void)
{
    station_body(1);
    const float u = (float)F.since / (float)(2u * BAR_SAMPLES);
    const float k = u < 0.f ? 0.f : u > 1.f ? 1.f : u;
    page_dim(32 - (int)(27.f * k));
    /* the roof's tubes, reflected in the lower half of the glass */
    {
        const float off = layer_off(PAR_NEAR);
        const int S = RHYTHM;
        const int first = (int)floorf(off / (float)S);
        const int a = (int)(7.f * (1.f - k * 0.5f));
        for (int i = first; ; i++) {
            const float x = (float)(i * S) - off;
            if (x > WIDTH + 30.f) break;
            if (x < -30.f) continue;
            halo(x, 214.f, 16.f, C_FLUO_H, a);
            rect_a((int)x - 10, 213, (int)x + 10, 215, C_FLUO_H, a + 3);
        }
    }
    vspan((int)MATCH_X, (int)MATCH_Y, BASE_NEAR - 4, RGB(0x08, 0x0A, 0x10));
    light_t L = { MATCH_X, MATCH_Y, 0.f, 0.f, 17.f + 21.f * k, C_SODIUM, C_SODIUM_H,
                  (int16_t)(19 - (int)(4.f * k)), 3, 0 };
    light_draw(&L);
}

/* The station at 60 and at 64: roof, tubes, platform, buildings, the board. */
static void station_body(int stopped)
{
    band_t f, m, n;
    style_for(F.world, &f, &m, &n);
    const float of = layer_off(PAR_FAR), om = layer_off(PAR_MID), on = layer_off(PAR_NEAR);
    const float vf = layer_vel(PAR_FAR), vm = layer_vel(PAR_MID), vn = layer_vel(PAR_NEAR);
    sky_expand(0, HORIZON, 0);
    rect(0, HORIZON, WIDTH - 1, HEIGHT - 1, F.dawn > 0.35f ? RGB(0x18, 0x18, 0x20) : C_LAND);
    band_fill(BASE_FAR, BASE_MID, of, &f, C_LAND);
    band_lamps(BASE_FAR, of, vf, &f);
    band_fill(BASE_MID, BASE_NEAR, om, &m, C_LAND);
    if (m.windows) band_windows(BASE_MID, om, &m, C_FLUO_H);
    band_lamps(BASE_MID, om, vm, &m);
    band_lamps(BASE_NEAR, on, vn, &n);
    /* the ballast in the four-foot, then the platform on top of it: a
     * platform has a surface and it is not ballast */
    ballast(on);
    platform_edge(on);
    platform_furniture(on);
    station_roof();
    board_platform((WIDTH - 11 * 8) / 2, 46);
    (void)stopped;
}

void HOT(world_side)(void)
{
    if (F.world == WORLD_TUNNEL) { tunnel_wall(); return; }
    if (F.world == WORLD_STATION && (F.flags & CUT_STOPPED)) {
        sleep_window();
        if (F.flags & CUT_RAIN) rain_draw();
        return;
    }
    if ((F.world == WORLD_STATION || F.world == WORLD_TERMINUS) && (F.flags & CUT_ROOF)) {
        station_body(0);
        if (F.flags & CUT_RAIN) rain_draw();
        return;
    }

    band_t f, m, n;
    style_for(F.world, &f, &m, &n);
    const float of = layer_off(PAR_FAR), om = layer_off(PAR_MID), on = layer_off(PAR_NEAR);
    const float vf = layer_vel(PAR_FAR), vm = layer_vel(PAR_MID), vn = layer_vel(PAR_NEAR);

    const int bridge = (F.world == WORLD_BRIDGE);
    const int painted_sea = sky_is_dawn();
    const int mirror_sea = !painted_sea && (bridge || (F.world == WORLD_FIELDS && F.variant == 1)
                                            || F.world == WORLD_COAST);

    sky_expand(0, HORIZON, 0);
    if (painted_sea && (F.world == WORLD_COAST || F.world == WORLD_TERMINUS))
        sky_expand(HORIZON, HEIGHT, HORIZON);
    else
        rect(0, HORIZON, WIDTH - 1, HEIGHT - 1, F.dawn > 0.35f ? RGB(0x18, 0x18, 0x20) : C_LAND);

    if (bridge || mirror_sea) {
        f.amp = bridge ? 10 : 12;
        band_fill(BASE_FAR, BASE_FAR + 3, of, &f, C_LAND);
        if (bridge) {
            halo(70.f, (float)HORIZON, 46.f, RGB(0x60, 0x40, 0x18), 5);
            bridge_posts(on, 1);                       /* before the water    */
        }
        water(HORIZON + 1, RGB(0x04, 0x06, 0x0C));
        if (!painted_sea)
            for (int k = 0; k < 8; k++)
                halo(253.f, (float)HORIZON + 6.f + (float)k * 11.f, 5.f + (float)k * 1.6f, C_MOON, 8 - k);
        if (bridge) {
            rect(0, 224, WIDTH - 1, HEIGHT - 1, RGB(0x03, 0x04, 0x07));   /* the deck */
            hspan(223, 0, WIDTH - 1, RGB(0x0C, 0x0E, 0x16));
        }
    } else {
        band_fill(BASE_FAR, painted_sea ? BASE_FAR + 3 : BASE_MID, of, &f, C_LAND);
        if (f.windows) band_windows(BASE_FAR, of, &f, C_FLUO_H);
        band_lamps(BASE_FAR, of, vf, &f);
    }

    if (!mirror_sea && !painted_sea) {
        band_fill(BASE_MID, BASE_NEAR, om, &m, C_LAND);
        if (m.windows) band_windows(BASE_MID, om, &m, C_FLUO_H);
        band_lamps(BASE_MID, om, vm, &m);
    }

    if (F.world == WORLD_PLATFORM) {
        ballast(on);
        platform_edge(on);
        platform_furniture(on);
    } else if (!bridge && !painted_sea) {
        band_fill(BASE_NEAR, BALLAST_TOP, on, &n, C_LAND);
        if (n.windows) band_windows(BASE_NEAR, on, &n, C_FLUO_H);
    }
    if (n.poles) poles(on);
    if (n.farms) farms(on, of, vn, vf);
    band_lamps(BASE_NEAR, on, vn, &n);

    if (F.world == WORLD_FIELDS) {
        for (int y = BASE_MID - 6; y < BASE_MID + 10; y++)
            hspan_a(y, 0, WIDTH - 1, F.dawn > 0.4f ? RGB(0x60, 0x70, 0x90) : RGB(0x20, 0x28, 0x38),
                    9 - abs(y - (BASE_MID + 2)) / 2);
    }

    if (!bridge && !mirror_sea && F.world != WORLD_PLATFORM) ballast(on);
    if (F.flags & CUT_ROOF) station_roof();
    if (F.flags & CUT_CROSSING) crossing();
    if (F.flags & CUT_PASSING) passing_train_draw();
    if (F.flags & CUT_RAIN) rain_draw();
}
