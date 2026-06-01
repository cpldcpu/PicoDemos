/* host_main640.c — SDL preview of the full-VGA (640x480) beam-raced rotozoom.
 * Renders each scanline via qs_race_scanline (interp POP emulator) into a
 * 640x480 buffer — exactly what RP2350 core 1 does into the scanvideo line
 * buffer, but here gathered for display. Proves the full-VGA math/visual.
 *
 *   ./quicksilver_vga640.exe                          # interactive
 *   ./quicksilver_vga640.exe --screenshot-at 4000 --exit-after 4100
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <direct.h>     /* _mkdir on Windows/MSYS */

#include "race.h"
#include "../rgb565.h"

#define W 640
#define H 480

static uint32_t pio565_to_argb(uint16_t c) {
    int r5 = rgb565_r5(c), g5 = rgb565_g5(c), b5 = rgb565_b5(c);
    int r = (r5 << 3) | (r5 >> 2), g = (g5 << 3) | (g5 >> 2), b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

int main(int argc, char **argv)
{
    int shot_at = -1, exit_after = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--screenshot-at") && i + 1 < argc) shot_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--exit-after") && i + 1 < argc) exit_after = atoi(argv[++i]);
    }

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("QUICKSILVER VGA640 (host)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    SDL_Texture *tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, W, H);

    static uint16_t line[W];
    static float flex[H];
    int quit = 0, shot = 0;
    uint32_t frame = 0;
    while (!quit) {
        SDL_Event e; while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT ||
            (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_ESCAPE || e.key.keysym.sym == SDLK_q))) quit = 1;

        uint32_t t_ms = (uint32_t)(frame * 1000.0f / 60.0f);
        float t = t_ms * 0.001f;
        float ang = t * 0.5f, zoom = 1.4f + 0.9f * sinf(t * 0.4f);
        for (int y = 0; y < H; y++) flex[y] = 1.0f + 0.15f * sinf(y * 0.018f + t * 2.0f);
        qs_race_params p = {
            .ca0 = cosf(ang) * zoom,
            .sa0 = sinf(ang) * zoom,
            .cu  = 128.0f + 50.0f * sinf(t * 0.21f),
            .cv  = 128.0f + 50.0f * cosf(t * 0.17f),
            .flex = flex,
        };

        void *pixels; int pitch;
        SDL_LockTexture(tex, NULL, &pixels, &pitch);
        uint32_t *out = (uint32_t *)pixels; int stride = pitch / 4;
        qs_race_setup();
        for (int y = 0; y < H; y++) {
            qs_race_scanline(line, y, W, H, &p);
            uint32_t *row = out + y * stride;
            for (int x = 0; x < W; x++) row[x] = pio565_to_argb(line[x]);
        }
        SDL_UnlockTexture(tex);
        SDL_RenderClear(ren); SDL_RenderCopy(ren, tex, NULL, NULL); SDL_RenderPresent(ren);

        if (shot_at >= 0 && !shot && (int)t_ms >= shot_at) {
            SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
            SDL_LockTexture(tex, NULL, &pixels, &pitch);
            memcpy(s->pixels, pixels, (size_t)H * pitch); SDL_UnlockTexture(tex);
            _mkdir("screenshots");
            SDL_SaveBMP(s, "screenshots/vga640.bmp"); SDL_FreeSurface(s);
            printf("wrote screenshots/vga640.bmp\n"); shot = 1;
            if (exit_after < 0) break;
        }
        if (exit_after >= 0 && (int)t_ms >= exit_after) break;
        frame++;
    }
    SDL_Quit();
    return 0;
}
