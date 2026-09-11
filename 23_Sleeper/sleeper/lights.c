/* lights.c -- the production's signature, and every pixel primitive under it.
 *
 * PLANNING §3: everything bright is a light, and a light is three things --
 * a small hard core, a soft halo, and, when the train is moving, a streak:
 * the halo dragged along its own motion for the one sixtieth of a second the
 * field exposes. Streak length is speed x field time, so it is a fact about
 * the frame and not a filter, and it costs lines and halos rather than a
 * full-frame blur.
 *
 * halo() is HELION's, with its magic-divide note, plus a 4x4 ordered dither
 * of the falloff: the DAC has five bits a channel, and a halo is a ramp
 * across forty pixels, which without dithering lands as five visible rings.
 * The dither costs one table read and one add per pixel and it is the
 * difference between a light and a target.  -- Overscan
 */
#include "render.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef PICO_BUILD
#include "hardware/structs/systick.h"
#endif

/* Core 0's own SysTick, so light_draw() can be priced without the pricing
 * costing more than the thing priced: one register read each side. Core 1
 * has its own and video.c uses that one for audio_pump(). On the host both
 * are zero and demo_stats()'s lights_us reads 0, which is why the HOST
 * per-shot table in the reply is whole-frame times and the split is a DEVICE
 * measurement. */
uint32_t g_lights_cy;
#ifdef PICO_BUILD
uint32_t prof_cyc(void) { return systick_hw->cvr; }
uint32_t prof_since(uint32_t t0) { return (t0 - systick_hw->cvr) & 0x00FFFFFFu; }
#else
uint32_t prof_cyc(void) { return 0; }
uint32_t prof_since(uint32_t t0) { (void)t0; return 0; }
#endif

frame_t F;
uint16_t *g_fb;
demo_stats_t g_stats;

/* The classic 4x4 Bayer matrix, 0..15. Ordered, not random, so the pattern
 * is stable frame to frame and does not crawl -- PLANNING §3's rule. */
const uint8_t bayer4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

/* HELION's packed-field blend, unchanged: red (bits 0-4) and blue (11-15) are
 * far enough apart that one 32-bit multiply scales both without their partial
 * products meeting -- 31*32 needs ten bits and blue starts at eleven -- and
 * green (6-10) takes the second. Four multiplies, no clamps. */
#define RB_MASK 0xf81fu
#define G_MASK  0x07c0u
uint16_t HOT(mixc)(uint16_t a, uint16_t b, int f)
{
    unsigned g = 32u - (unsigned)f, h = (unsigned)f;
    unsigned rb = ((a & RB_MASK) * g + (b & RB_MASK) * h) >> 5;
    unsigned gr = ((a & G_MASK) * g + (b & G_MASK) * h) >> 5;
    return (uint16_t)((rb & RB_MASK) | (gr & G_MASK));
}

uint32_t rhash(uint32_t x) { x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; return x ^ (x >> 16); }
float rnd01(uint32_t h) { return (float)(h & 0xffffffu) * (1.f / 16777216.f); }

int profile1(int wx, int cell, int amp, uint32_t seed)
{
    /* Value noise with a smoothstep between hashed cell heights. One octave
     * is enough for a silhouette that is going to be black anyway; two would
     * cost twice as much and read the same at 320x240. */
    int c = wx >= 0 ? wx / cell : -((-wx + cell - 1) / cell);
    int f = wx - c * cell;
    float t = (float)f / (float)cell;
    t = t * t * (3.f - 2.f * t);
    float a = rnd01(rhash((uint32_t)c * 2654435761u ^ seed));
    float b = rnd01(rhash((uint32_t)(c + 1) * 2654435761u ^ seed));
    return (int)((a + (b - a) * t) * (float)amp);
}

void HOT(px)(int x, int y, uint16_t c, int a)
{
    if ((unsigned)x < WIDTH && (unsigned)y < HEIGHT) { unsigned p = (unsigned)y * WIDTH + (unsigned)x; g_fb[p] = a >= 32 ? c : mixc(g_fb[p], c, a); }
}

void HOT(hspan)(int y, int x0, int x1, uint16_t c)
{
    if ((unsigned)y >= HEIGHT) return;
    if (x0 < 0) x0 = 0; if (x1 > WIDTH - 1) x1 = WIDTH - 1;
    if (x0 > x1) return;
    uint16_t *row = g_fb + (unsigned)y * WIDTH;
    for (int x = x0; x <= x1; x++) row[x] = c;
    g_stats.spans++;
}

void HOT(hspan_a)(int y, int x0, int x1, uint16_t c, int a)
{
    if ((unsigned)y >= HEIGHT || a <= 0) return;
    if (a >= 32) { hspan(y, x0, x1, c); return; }
    if (x0 < 0) x0 = 0; if (x1 > WIDTH - 1) x1 = WIDTH - 1;
    if (x0 > x1) return;
    uint16_t *row = g_fb + (unsigned)y * WIDTH;
    for (int x = x0; x <= x1; x++) row[x] = mixc(row[x], c, a);
    g_stats.spans++;
}

void HOT(vspan)(int x, int y0, int y1, uint16_t c)
{
    if ((unsigned)x >= WIDTH) return;
    if (y0 < 0) y0 = 0; if (y1 > HEIGHT - 1) y1 = HEIGHT - 1;
    if (y0 > y1) return;
    uint16_t *p = g_fb + (unsigned)y0 * WIDTH + (unsigned)x;
    for (int y = y0; y <= y1; y++, p += WIDTH) *p = c;
    g_stats.spans++;
}

void HOT(rect)(int x0, int y0, int x1, int y1, uint16_t c)
{
    if (y0 < 0) y0 = 0; if (y1 > HEIGHT - 1) y1 = HEIGHT - 1;
    for (int y = y0; y <= y1; y++) hspan(y, x0, x1, c);
}

void HOT(line_a)(int x, int y, int xx, int yy, uint16_t c, int a)
{
    int dx = abs(xx - x), sx = x < xx ? 1 : -1, dy = -abs(yy - y), sy = y < yy ? 1 : -1, e = dx + dy;
    int limit = 1000;
    while (limit--) {
        px(x, y, c, a);
        if (x == xx && y == yy) break;
        int e2 = e * 2;
        if (e2 >= dy) { e += dy; x += sx; }
        if (e2 <= dx) { e += dx; y += sy; }
    }
}

/* A dithered vertical ramp. The DAC has 32 levels a channel and a sky across
 * 140 rows is a staircase without this; with the 4x4 pattern the same ramp
 * carries about 128 distinguishable steps. COLOSSUS's rule. */
void vgradient(int y0, int y1, int x0, int x1, uint16_t top, uint16_t bottom)
{
    if (y1 <= y0) return;
    const int tr = red(top), tg = green(top), tb = blue(top);
    const int br = red(bottom), bg = green(bottom), bb = blue(bottom);
    if (x0 < 0) x0 = 0; if (x1 > WIDTH - 1) x1 = WIDTH - 1;
    for (int y = y0; y < y1; y++) {
        if ((unsigned)y >= HEIGHT) continue;
        int num = y - y0, den = y1 - y0;
        int r = tr + (br - tr) * num / den, g = tg + (bg - tg) * num / den, b = tb + (bb - tb) * num / den;
        const uint8_t *bay = bayer4 + ((y & 3) << 2);
        uint16_t *row = g_fb + (unsigned)y * WIDTH;
        for (int x = x0; x <= x1; x++) {
            int d = bay[x & 3] >> 1;           /* 0..7: the three dropped bits */
            row[x] = rgb(r + d, g + d, b + d);
        }
        g_stats.spans++;
    }
}

/* Same disc, same alphas, same pixels as HELION's halo, with the falloff
 * dithered. Each row computes its own span and writes the page directly; the
 * division by the loop-invariant r*r becomes a multiply-and-shift that is
 * exactly equal over this domain: with M = floor(2^32/rr)+1 and
 * e = M*rr-2^32 <= rr, floor(n*M/2^32) == floor(n/rr) for every n with
 * n*rr < 2^32. Carrying the alpha at eight times its final precision (so the
 * dither has three bits to work in) makes n as large as 256*rr, so the guard
 * that was rr <= 8192 in HELION is rr <= 4095 here; above that the divide
 * still runs and nothing is wrong, only slower, and radius 64 halos are
 * three shots in the film. -- Overscan */
void HOT(halo)(float cx, float cy, float radius, uint16_t c, int a)
{
    int r = (int)radius;
    if (r < 1 || a <= 0) return;
    const int rr = r * r, ox = (int)cx, oy = (int)cy;
    if (ox + r < 0 || ox - r > WIDTH - 1 || oy + r < 0 || oy - r > HEIGHT - 1) return;
    const uint32_t magic = (uint32_t)(0x100000000ull / (uint32_t)rr) + 1u;
    const int exact = rr <= 4095;
    const uint32_t a8 = (uint32_t)(a * 8);
    for (int y = -r; y <= r; y++) {
        int py = oy + y;
        if ((unsigned)py >= HEIGHT) continue;
        int rem = rr - y * y;
        if (rem <= 0) continue;
        int half = (int)sqrtf((float)rem);
        while (half * half >= rem) half--;
        while ((half + 1) * (half + 1) < rem) half++;
        int x0 = -half, x1 = half;
        if (ox + x0 < 0) x0 = -ox;
        if (ox + x1 > WIDTH - 1) x1 = WIDTH - 1 - ox;
        uint16_t *row = g_fb + (unsigned)py * WIDTH + ox;
        const uint8_t *bay = bayer4 + ((py & 3) << 2);
        for (int x = x0; x <= x1; x++) {
            uint32_t n = a8 * (uint32_t)(rem - x * x);
            unsigned q = exact ? (unsigned)(((uint64_t)n * magic) >> 32) : (unsigned)(n / (uint32_t)rr);
            int alpha = (int)((q + (bay[(ox + x) & 3] >> 1)) >> 3);
            if (alpha <= 0) continue;
            row[x] = alpha >= 32 ? c : mixc(row[x], c, alpha);
        }
    }
}

void HOT(disc)(float cx, float cy, float r, uint16_t c)
{
    int ri = (int)r; if (ri < 0) return;
    int ox = (int)cx, oy = (int)cy;
    if (ri == 0) { px(ox, oy, c, 32); return; }
    const int rr = ri * ri + ri;
    for (int y = -ri; y <= ri; y++) {
        int rem = rr - y * y; if (rem < 0) continue;
        int half = (int)sqrtf((float)rem);
        hspan(oy + y, ox - half, ox + half, c);
    }
}

/* ------------------------------------------------------------ sine table */
/* 1,024 entries. Nothing here calls it per pixel -- the water's ripple is
 * once a row, the wheel's spokes are 24 a frame -- but sinf() on the M33 is
 * a software call of a hundred-odd cycles and there is no reason to pay it
 * inside a loop that runs 90 times. */
static float g_sine[1024];
static int g_sine_built;
void lights_init(void)
{
    if (g_sine_built) return;
    for (int i = 0; i < 1024; i++) g_sine[i] = sinf((float)i * (6.28318530718f / 1024.f));
    g_sine_built = 1;
}
float fsin(float a) { return g_sine[(int)(a * (1024.f / 6.28318530718f)) & 1023]; }
float fcos(float a) { return g_sine[((int)(a * (1024.f / 6.28318530718f)) + 256) & 1023]; }

/* A light, drawn once. PLANNING §3 after Phase's critique:
 *
 *   "a streak spreads a light's energy along its length rather than adding
 *    to it: a passing train's windows stay windows, they do not turn the
 *    frame white."
 *
 * So the whole of light_draw() is written to conserve, near enough, the
 * integral of the alpha it lays down. With the streak resolved into n+1
 * overlapping halos, each carries intensity * 2/(n+1) -- the 2 because
 * consecutive halos are spaced half a radius apart and so overlap about
 * twice. The core does the same, with no floor under it: a lamp smeared over
 * forty pixels really is dimmer per pixel than the same lamp standing still,
 * and the moment a floor is put under that, twenty windows of a passing
 * train at 11 px a field whiten the frame. The one thing that does not
 * scale is the standing halo, because a light with no motion has no streak
 * to spread into.
 *
 * The moon and the sun are drawn with halo() directly and never through
 * this, so they never streak, which is Phase's other rule: they are not in
 * the near world and they do not move with it. -- Overscan */
void HOT(light_draw)(const light_t *L)
{
    const uint32_t t_light = prof_cyc();
    const float len = sqrtf(L->vx * L->vx + L->vy * L->vy);
    const int moving = len > 1.f;
    if (moving && L->intensity > 0) {
        /* Spacing is capped at 3.5 px, not left at half a radius: a 28 px
         * streak resolved into three overlapping halos reads as three
         * blobs, and into eight it reads as a streak. */
        float stepd = L->radius * 0.5f;
        if (stepd < 1.f) stepd = 1.f;
        if (stepd > 3.5f) stepd = 3.5f;
        int n = (int)(len / stepd); if (n < 1) n = 1; if (n > 20) n = 20;
        /* The over-range factor of four is the honest part of the model. A
         * sodium lamp is far brighter than the top of a five-bit channel, so
         * spreading its energy over eight positions does not make it eight
         * times dimmer than the display can show -- it stays clipped for the
         * first few, and only then falls off. Without it the physics is
         * right and the picture is a faint smudge; with it the passing
         * train's twenty windows still do not whiten the frame, which is
         * the case Phase named. */
        const int ov = L->over ? L->over : 4;
        int a = L->intensity * ov / (n + 1); if (a > L->intensity) a = L->intensity;
        if (a >= 1) {
            const float sx = L->vx / (float)n, sy = L->vy / (float)n;
            float x = L->x - L->vx * 0.5f, y = L->y - L->vy * 0.5f;
            for (int i = 0; i <= n; i++, x += sx, y += sy) halo(x, y, L->radius * 0.8f, L->glow, a);
        }
        int b = L->intensity * ov / (n + 3); if (b > L->intensity) b = L->intensity;
        halo(L->x, L->y, L->radius, L->glow, b);
    } else {
        halo(L->x, L->y, L->radius, L->glow, L->intensity);
    }
    if (L->core_r > 0) {
        if (moving) {
            int n = (int)(len * 0.75f); if (n > 40) n = 40; if (n < 1) n = 1;
            const float sx = L->vx / (float)(n + 1), sy = L->vy / (float)(n + 1);
            float x = L->x - L->vx * 0.5f, y = L->y - L->vy * 0.5f;
            int a = 25 * (L->over ? L->over : 4) / (n + 2); if (a > 32) a = 32;
            if (a >= 1) for (int i = 0; i <= n; i++, x += sx, y += sy) halo(x, y, (float)L->core_r + 0.9f, L->core, a);
        } else {
            disc(L->x, L->y, (float)L->core_r, L->core);
        }
    }
    g_stats.lights++;
    g_lights_cy += prof_since(t_light);
}

void HOT(rect_a)(int x0, int y0, int x1, int y1, uint16_t c, int a)
{
    if (y0 < 0) y0 = 0; if (y1 > HEIGHT - 1) y1 = HEIGHT - 1;
    for (int y = y0; y <= y1; y++) hspan_a(y, x0, x1, c, a);
}

int daylight_scale(uint16_t core, int intensity)
{
    if (F.lightlv <= 200) return intensity;
    if (core == C_SODIUM) return 0;                 /* switched off        */
    if (core == C_FLUO) return intensity / 4;       /* on, and losing      */
    return intensity;                               /* signals are information */
}

/* HELION's word-at-a-time field scale, kept because it is the cheapest
 * full-page pass there is: red and blue are far enough apart that one 32-bit
 * multiply scales both, green takes the second, and blue is brought down to
 * bits 0-4 first because 31*31 shifted left 27 would not fit. Two pixels an
 * iteration, no clamps, no table. */
typedef uint32_t u32_alias __attribute__((may_alias));
void HOT(page_dim)(int f)
{
    if (f >= 32) return;
    if (f <= 0) { memset(g_fb, 0, (size_t)WIDTH * HEIGHT * 2); return; }
    const unsigned h = (unsigned)f;
    if (!((uintptr_t)g_fb & 3u)) {
        u32_alias *w = (u32_alias *)g_fb;
        for (int i = 0; i < WIDTH * HEIGHT / 2; i++) {
            const uint32_t q = w[i];
            const uint32_t r = (((q & 0x001f001fu) * h) >> 5) & 0x001f001fu;
            const uint32_t g = (((q & 0x07c007c0u) * h) >> 5) & 0x07c007c0u;
            const uint32_t b = ((((((q >> 11) & 0x001f001fu) * h) >> 5) & 0x001f001fu)) << 11;
            w[i] = r | g | b;
        }
    } else {
        for (int i = 0; i < WIDTH * HEIGHT; i++) {
            const unsigned q = g_fb[i];
            g_fb[i] = (uint16_t)(((((q & RB_MASK) * h) >> 5) & RB_MASK) | ((((q & G_MASK) * h) >> 5) & G_MASK));
        }
    }
}

/* A window: the halo and its streak from light_draw(), then a rectangular
 * core smeared along the same motion with the same energy rule. */
void HOT(window_light)(float x, float y, float vx, float vy, int w, int h,
                       float radius, uint16_t core, uint16_t glow, int intensity, int over)
{
    intensity = daylight_scale(core, intensity);
    if (intensity <= 0) return;
    const light_t L = { x, y, vx, vy, radius, core, glow, (int16_t)intensity, 0, (int16_t)over };
    light_draw(&L);
    const float len = sqrtf(vx * vx + vy * vy);
    const int hw = w / 2, hh = h / 2;
    if (len > 1.f) {
        int n = (int)(len * 0.75f); if (n > 48) n = 48; if (n < 1) n = 1;
        const float sx = vx / (float)(n + 1), sy = vy / (float)(n + 1);
        float px0 = x - vx * 0.5f, py0 = y - vy * 0.5f;
        int a = 25 * (over ? over : 4) / (n + 2); if (a > 32) a = 32;
        if (a < 1) return;
        for (int i = 0; i <= n; i++, px0 += sx, py0 += sy)
            rect_a((int)px0 - hw, (int)py0 - hh, (int)px0 + hw, (int)py0 + hh, core, a);
    } else {
        rect((int)x - hw, (int)y - hh, (int)x + hw, (int)y + hh, core);
    }
}
