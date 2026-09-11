/* world_dream.c -- phrases 9 and 10, and the only place the demo shows off.
 *
 * Phase's correction is the whole design of this file: the dream is not
 * three effects, it is *one remembered object changing*. At 66 the lamp
 * standing in the stopped window at 64 -- the same colour, at the same
 * screen position, MATCH_X/MATCH_Y -- becomes the one light beyond the rails
 * of light. At 68 the crossbars that were the ties bend into the wheel and
 * that lamp is its hub. At 70 the carriage windows fold around the same hub
 * as the kaleidoscope. At 79 the shapes compress, four cuts a bar, and at
 * 80:1 the last light is back in the real window.
 *
 * So every shot here draws the same lamp in the same place, and the black is
 * PLANNING §3's #101830 rather than the night's, because in the dream
 * everything glows. -- Overscan
 */
#include "render.h"
#include <math.h>

/* The dream's own clock. Variant 0 is the slow sleep, variant 1 accelerates
 * with the timetable's speed ramp through the riser, variant 2 is the
 * compression at 79. All three are functions of the sample and the distance,
 * never of a previous frame. */
static float dream_phase(float rate)
{
    return (float)F.sample * (1.f / (float)SAMPLE_RATE) * rate + F.dist * 0.0000009f * (float)F.variant;
}

static void hub_lamp(float radius, int intensity)
{
    light_t L = { MATCH_X, MATCH_Y, 0.f, 0.f, radius, C_SODIUM, C_SODIUM_H, (int16_t)intensity, 3, 0 };
    light_draw(&L);
}

/* Rails of light: pairs radiating from the off-centre vanishing point, with
 * the crossbars that will become the wheel's spokes. Everything is a light. */
static void HOT(rails_of_light)(void)
{
    const float rate = F.variant == 2 ? 1.9f : F.variant == 1 ? 0.55f : 0.20f;
    const float turn = dream_phase(rate * 0.5f);
    const float run = dream_phase(rate);
    /* Nine pairs, not eighteen: at eighteen the ties tessellate into a
     * spider's web and the shot becomes a visualiser, which is exactly the
     * failure Phase named. Nine leaves black between the tracks, and black
     * between the tracks is what makes them tracks. */
    const int pairs = F.variant == 2 ? 12 : 9;
    const float gauge = 0.115f;                /* how fast a pair opens out  */
    for (int j = 0; j < pairs; j++) {
        const float a = (float)j * (6.28318530718f / (float)pairs) + turn;
        const float dx = fcos(a), dy = fsin(a) * 0.78f;
        const float pxd = -dy, pyd = dx * 0.78f;
        float prev_ax = 0.f, prev_ay = 0.f, prev_bx = 0.f, prev_by = 0.f;
        int have = 0;
        /* ties bunch toward the vanishing point: a geometric run, which is
         * what perspective along a straight track actually looks like */
        for (int k = 1; k <= 13; k++) {
            const float ph = (float)k + run;
            const float t = 6.f * (float)pow(1.34f, (double)ph - floor((double)run));
            if (t > 420.f) break;
            const float cx = MATCH_X + dx * t, cy = MATCH_Y + dy * t;
            const float o = t * gauge;
            const float ax = cx + pxd * o, ay = cy + pyd * o;
            const float bx = cx - pxd * o, by = cy - pyd * o;
            /* the crossbar: at 68 these bend into the wheel */
            /* the crossbar -- the tie that becomes the wheel's spoke at 68 */
            if (o > 1.5f) line_a((int)ax, (int)ay, (int)bx, (int)by, C_SODIUM_H, o > 6.f ? 22 : 13);
            if (have) {
                line_a((int)prev_ax, (int)prev_ay, (int)ax, (int)ay, C_SODIUM, 18);
                line_a((int)prev_bx, (int)prev_by, (int)bx, (int)by, C_SODIUM, 18);
            }
            if (k % 3 == 0 && o > 4.f) {
                light_t L = { cx, cy, 0.f, 0.f, 4.f + o * 0.25f, C_SODIUM, C_SODIUM_H, 10, 1, 0 };
                light_draw(&L);
            }
            prev_ax = ax; prev_ay = ay; prev_bx = bx; prev_by = by; have = 1;
        }
    }
    hub_lamp(F.variant == 2 ? 40.f : 26.f, 20);
}

/* The wheel: the same crossbars, bent. 24 spokes and a rim about the lamp. */
static void HOT(wheel)(void)
{
    const float rate = F.variant == 2 ? 2.6f : F.variant == 1 ? 1.1f : 0.32f;
    const float turn = dream_phase(rate);
    const float R = F.variant == 2 ? 132.f : 104.f;
    const int spokes = 24;
    for (int j = 0; j < spokes; j++) {
        const float a = (float)j * (6.28318530718f / (float)spokes) + turn;
        const float ex = MATCH_X + fcos(a) * R, ey = MATCH_Y + fsin(a) * R * 0.84f;
        const float ix = MATCH_X + fcos(a) * 14.f, iy = MATCH_Y + fsin(a) * 14.f * 0.84f;
        line_a((int)ix, (int)iy, (int)ex, (int)ey, C_SODIUM_H, 15);
        light_t L = { ex, ey, 0.f, 0.f, 7.f, C_SODIUM, C_SODIUM_H, 12, 1, 0 };
        light_draw(&L);
    }
    /* the rim, as a ring of small halos so it is lights and not an outline */
    for (int j = 0; j < 96; j++) {
        const float a = (float)j * (6.28318530718f / 96.f) + turn;
        halo(MATCH_X + fcos(a) * R, MATCH_Y + fsin(a) * R * 0.84f, 5.f, C_SODIUM_H, 12);
    }
    hub_lamp(30.f, 22);
}

/* The kaleidoscope, note 11. Round one folded the rail texture through the
 * tunnel's polar grid, which satisfied Phase's "made only of the journey's
 * shapes" on a technicality: the shapes were a texture of ballast.
 *
 * The remembered object here is the *passing train's windows* -- the last
 * bright thing the passenger saw before the station -- and this is what they
 * become: six mirrored sectors of window lights, rectangular cores and
 * halos, exactly the lights world_side.c draws on the other train, streaming
 * outward from the same hub at beat spacing and turning slowly. Lights only:
 * no texture, no polar grid, no palette. The grid stays where it belongs, in
 * the tunnel.
 *
 * It is also cheaper than the fold it replaces, because the fold was a
 * full-page pass and this is seventy-two small halos. -- Overscan */
static void HOT(kaleido)(void)
{
    const float rate = F.variant == 2 ? 2.2f : F.variant == 1 ? 0.9f : 0.30f;
    const float turn = dream_phase(rate * 0.35f);
    const float run  = dream_phase(rate);
    const int sectors = 6;
    /* the windows stream outward: radii on a geometric run, so consecutive
     * windows are a beat apart in the dream's own perspective */
    const float phase = run - floorf(run);
    for (int s = 0; s < sectors; s++) {
        const float base = (float)s * (6.28318530718f / (float)sectors) + turn;
        for (int mirror = 0; mirror < 2; mirror++) {
            const float spread = mirror ? -0.20f : 0.20f;   /* the fold      */
            const float a = base + spread;
            const float dx = fcos(a), dy = fsin(a) * 0.82f;
            for (int j = 0; j < 6; j++) {
                const float t = 22.f * powf(1.42f, (float)j + phase);
                if (t > 300.f) break;
                const float x = MATCH_X + dx * t, y = MATCH_Y + dy * t;
                if (x < -30.f || x > WIDTH + 30.f || y < -30.f || y > HEIGHT + 30.f) continue;
                const int w = 5 + (int)(t * 0.035f), h = 3 + (int)(t * 0.018f);
                window_light(x, y, 0.f, 0.f, w, h, 6.f + t * 0.05f,
                             C_FLUO, C_FLUO_H, 15, 4);
            }
        }
    }
    hub_lamp(34.f, 20);
}

void HOT(world_dream)(unsigned shot)
{
    rect(0, 0, WIDTH - 1, HEIGHT - 1, C_DREAM);
    switch (shot) {
    case SHOT_RAILS:   rails_of_light(); break;
    case SHOT_WHEEL:   wheel(); break;
    case SHOT_KALEIDO: kaleido(); break;
    default: break;
    }
}
