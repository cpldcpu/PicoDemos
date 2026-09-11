/* light_sheet.c -- the still the round-one brief asks for before anything is
 * built on lights.c: one sodium lamp standing, one moving at cruise, one
 * fluorescent window, one green signal, on black.
 *
 * It calls the production's own light_draw() with the production's own
 * colours and the production's own cruise velocity -- 7.0 px a field, which
 * is NEAR_PX_PER_UNIT * FIELD_SAMPLES at speed 1.0 -- so what it shows is
 * what the film will show and not an illustration of it.
 *
 *   light_sheet out.ppm
 */
#include "render.h"
#include <stdio.h>
#include <string.h>

static uint16_t page[WIDTH * HEIGHT];

static void label(int x, int y, const char *s);

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "lights.ppm";
    lights_init();
    g_fb = page;
    memset(&g_stats, 0, sizeof g_stats);
    memset(&F, 0, sizeof F);
    memset(page, 0, sizeof page);

    const float cruise = NEAR_PX_PER_UNIT * FIELD_SAMPLES;   /* 7.0 px a field */

    /* 1. a sodium lamp standing: the platform lamp of bars 4-11 */
    { light_t L = { 58.f, 70.f, 0.f, 0.f, 15.f, C_SODIUM, C_SODIUM_H, 16, 3, 0 }; light_draw(&L); }
    /* 2. the same lamp moving at cruise */
    { light_t L = { 190.f, 70.f, -cruise, 0.f, 15.f, C_SODIUM, C_SODIUM_H, 16, 3, 0 }; light_draw(&L); }
    /* 3. a fluorescent window: a passing train's, with its own velocity */
    { light_t L = { 58.f, 150.f, 0.f, 0.f, 12.f, C_FLUO, C_FLUO_H, 16, 3, 0 }; light_draw(&L); }
    { light_t L = { 190.f, 150.f, -12.7f, 0.f, 12.f, C_FLUO, C_FLUO_H, 16, 3, 0 }; light_draw(&L); }
    /* 4. a green signal, standing and at cruise */
    { light_t L = { 58.f, 210.f, 0.f, 0.f, 10.f, C_GREEN, C_GREEN, 20, 2, 0 }; light_draw(&L); }
    { light_t L = { 190.f, 210.f, -cruise, 0.f, 10.f, C_GREEN, C_GREEN, 20, 2, 0 }; light_draw(&L); }
    /* the reference strip: what one light of each kind costs the frame at
     * three radii, so the falloff can be judged enlarged */
    for (int k = 0; k < 3; k++) {
        light_t L = { 268.f, 60.f + (float)k * 62.f, 0.f, 0.f, 8.f + (float)k * 9.f,
                      k == 0 ? C_SODIUM : k == 1 ? C_FLUO : C_MOON,
                      k == 0 ? C_SODIUM_H : k == 1 ? C_FLUO_H : C_MOON, 14, 2, 0 };
        light_draw(&L);
    }
    label(4, 4, "");

    FILE *f = fopen(out, "wb");
    if (!f) { perror(out); return 1; }
    fprintf(f, "P6\n%d %d\n255\n", WIDTH, HEIGHT);
    for (int i = 0; i < WIDTH * HEIGHT; i++) {
        unsigned char rgbv[3] = { (unsigned char)red(page[i]), (unsigned char)green(page[i]), (unsigned char)blue(page[i]) };
        fwrite(rgbv, 1, 3, f);
    }
    fclose(f);
    fprintf(stderr, "light_sheet: %s, %u lights, %u spans\n", out, (unsigned)g_stats.lights, (unsigned)g_stats.spans);
    return 0;
}

static void label(int x, int y, const char *s) { (void)x; (void)y; (void)s; }
