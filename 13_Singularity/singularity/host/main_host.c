/* Desktop entry point for SLOP. Mirrors main.c (the Pico boot)
 * minus the RP2350 init — SDL handles windowing, audio, timing.
 *
 * Keys:
 *   ESC / Q : quit
 *   S       : screenshot to screenshot_NNN.bmp
 *   SPACE   : pause/resume (frame counter freezes)
 *
 * CLI flags (mostly for agent-driven iteration without keypresses):
 *   --start-ms N       : skip the demo clock ahead by N ms before scene 0
 *   --screenshot-at N  : auto-screenshot at clock = N ms, then exit
 *   --exit-after N     : exit cleanly after N ms of demo time
 *
 * Note: --start-ms shifts only the visuals; the audio still plays from
 * t=0, so it'll be out of sync with the visuals when used. Fine for
 * snapshotting a scene.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../scene.h"
#include "../vga.h"
#include "../audio.h"

/* Implemented in vga_sdl.c — invoked by --screenshot-at to capture
 * exactly the frame that produced clock = N ms. */
void vga_screenshot(void);
extern int g_offline;

int main(int argc, char *argv[])
{
    int start_offset_ms = 0;
    int screenshot_at   = -1;
    int exit_after      = -1;
    int offline_mode    = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--start-ms") && i + 1 < argc)
            start_offset_ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--screenshot-at") && i + 1 < argc)
            screenshot_at = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--exit-after") && i + 1 < argc)
            exit_after = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--offline"))
            offline_mode = 1;
        else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return 1;
        }
    }

    if (offline_mode) {
        g_offline = 1;
    }

    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    printf("\n=== SLOP (host) ===\n");
    if (start_offset_ms)   printf("  --start-ms %d\n", start_offset_ms);
    if (screenshot_at>=0)  printf("  --screenshot-at %d\n", screenshot_at);
    if (exit_after>=0)     printf("  --exit-after %d\n", exit_after);
    if (offline_mode)      printf("  --offline\n");

    audio_init();
    vga_init();
    if (!offline_mode) {
        audio_start();
        if (start_offset_ms > 0) audio_seek_ms((uint32_t)start_offset_ms);
        printf("entering main loop — ESC/Q quit, S screenshot, SPACE skip to next scene\n");
    } else {
        printf("entering main loop (OFFLINE RENDER MODE) — generating 60fps frames...\n");
    }

    int shot_done = 0;
    uint32_t frame = 0;
    while (!vga_should_quit()) {
        if (!offline_mode) {
            audio_pump();   /* no-op under SDL but kept for symmetry */
        }

        /* SPACE / LEFT / R → scene navigation, with audio tracking. */
        int sk = vga_consume_skip_request();
        if (sk != 0 && !offline_mode) {
            uint32_t t_now = audio_now_ms();
            uint32_t target;
            if      (sk ==  1) target = scene_next_boundary_ms(t_now);
            else if (sk == -1) target = scene_prev_boundary_ms(t_now);
            else               target = 0;                          /* sk == 2: restart */
            audio_seek_ms(target);
            printf("skip(%d): %u → %u ms\n", sk, (unsigned)t_now, (unsigned)target);
        }

        uint32_t t;
        if (offline_mode) {
            t = (uint32_t)(frame * 1000.0f / 60.0f);
        } else {
            t = audio_now_ms();
        }

        int alive = scene_runner_tick(t);
        if (!alive) break;

        switch (vga_current_mode()) {
            case MODE_320:                 vga_320_present();   break;
            case MODE_160:                 vga_160_present();   break;
            case MODE_HIRES:               vga_hires_present(); break;
            case MODE_SPLIT_160_OVER_320:  vga_split_present(); break;
            default: break;
        }

        if (!offline_mode) {
            if (screenshot_at >= 0 && !shot_done && (int)t >= screenshot_at) {
                vga_screenshot();
                shot_done = 1;
                if (exit_after < 0) break;
            }
            if (exit_after >= 0 && (int)t >= exit_after) break;
        } else {
            vga_screenshot();
            if (t >= 330500) {     /* full 5:30 demo length */
                printf("Offline render finished: %u ms reached!\n", (unsigned)t);
                break;
            }
        }

        frame++;
    }

    printf("stopped after %u frames\n", (unsigned)frame);
    SDL_Quit();
    return 0;
}
