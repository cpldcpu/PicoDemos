/* Desktop entry point for HYSTERESIS.
 *
 * Flags exist mainly so the referee and the agent loop can drive it without a
 * human at a keyboard:
 *
 *   --frames N        run exactly N steps then exit (default: whole demo)
 *   --variant 0|1     initial condition; 1 lights ONE extra cell (referee 2)
 *   --headless        no window, run as fast as possible
 *   --fielddump       raw 320x240 state bytes per frame to stdout
 *   --rawpipe         raw 640x480 BGRA per frame to stdout (video capture)
 *   --shot N          screenshot at frame N (repeatable)
 *   --energy          print frame,energy to stderr each second
 *   --mute            no audio, and let vsync pace the frames again
 *
 * There is deliberately no --start-ms. You cannot seek a system with memory;
 * you can only get there. --headless --frames N is how you "seek".
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../sim.h"
#include "../field.h"
#include "../vga.h"

void vga_raw_begin(void);
void vga_raw_emit(void);
void vga_field_emit(void);
void vga_screenshot(void);
extern int g_offline, g_rawpipe, g_fielddump, g_headless;

/* host/audio_sdl.c */
void hostaudio_init(void);
void hostaudio_start(void);
void hostaudio_wait_frame(uint32_t frame);
void hostaudio_reset(void);
void hostaudio_shutdown(void);
int  hostaudio_active(void);

#define MAX_SHOTS 32

int main(int argc, char *argv[])
{
    uint32_t frames = 0;
    int variant = 0, energy = 0, stats = 0, use_probe = 0, hashes = 0, rd_only = 0;
    int mute = 0;
    field_params_t probe = {0};
    rd_params_t rdp = {0}; int use_rdp = 0;
    int rd_amp = 0, use_rda = 0;
    int kern = -1;
    uint32_t shots[MAX_SHOTS]; int nshots = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--frames") && i + 1 < argc)       frames = (uint32_t)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--variant") && i + 1 < argc) variant = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--headless"))                g_headless = 1;
        else if (!strcmp(argv[i], "--fielddump"))               { g_fielddump = 1; g_headless = 1; }
        else if (!strcmp(argv[i], "--rawpipe"))                 { g_rawpipe = 1; g_headless = 1; }
        else if (!strcmp(argv[i], "--energy"))                  energy = 1;
        else if (!strcmp(argv[i], "--mute"))                    mute = 1;
        else if (!strcmp(argv[i], "--probe") && i + 1 < argc) {
            /* z,blur,gain,lo,hi[,fold[,persist]] -- pin the dynamics.
             *
             * OVERLAID ON sim_default_params, never built from zero. The old
             * version filled in only the fields named here, so react_out and the
             * convolution kernel arrived as zeros and the probe ran a field that
             * could not be alive; see sim.h. Omitted trailing fields now keep the
             * arc's own value instead of becoming 0. */
            sim_default_params(&probe);
            int z, b, g, lo, hi;
            int fold = probe.react_fold, persist = probe.persist;
            if (sscanf(argv[++i], "%d,%d,%d,%d,%d,%d,%d",
                       &z, &b, &g, &lo, &hi, &fold, &persist) < 5) {
                fprintf(stderr, "--probe wants z,blur,gain,lo,hi[,fold[,persist]]\n");
                return 1;
            }
            probe.zoom = 65536 + z; probe.blur = (uint8_t)b; probe.gain = (uint16_t)g;
            probe.react_lo = (uint8_t)lo; probe.react_hi = (uint8_t)hi;
            probe.react_fold = (uint8_t)fold;
            probe.persist = (uint8_t)persist;
            probe.cx = (FIELD_W / 2) << 16; probe.cy = (FIELD_H / 2) << 16;
            probe.drift_x = probe.drift_y = 0;
            use_probe = 1;
        }
        else if (!strcmp(argv[i], "--stats"))                   stats = 1;
        else if (!strcmp(argv[i], "--rdonly"))                  rd_only = 1;
        else if (!strcmp(argv[i], "--rdamp") && i + 1 < argc)   { rd_amp = atoi(argv[++i]); use_rda = 1; }
        else if (!strcmp(argv[i], "--kern") && i + 1 < argc)    kern = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--rdprobe") && i + 1 < argc) {
            int du, dv, F, K;
            if (sscanf(argv[++i], "%d,%d,%d,%d", &du, &dv, &F, &K) != 4) {
                fprintf(stderr, "--rdprobe wants du,dv,F,k\n"); return 1;
            }
            rdp.du = (uint8_t)du; rdp.dv = (uint8_t)dv;
            rdp.feed = (uint16_t)F; rdp.kill = (uint16_t)K;
            use_rdp = 1;
        }
        else if (!strcmp(argv[i], "--hash"))                    hashes = 1;
        else if (!strcmp(argv[i], "--shot") && i + 1 < argc) {
            if (nshots < MAX_SHOTS) shots[nshots++] = (uint32_t)atoi(argv[++i]);
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            return 1;
        }
    }

    /* Binary on stdout must be pixels only — redirect stray printf to stderr
     * before anything can print a banner onto the stream. */
    if (g_fielddump || g_rawpipe) vga_raw_begin();

    if (SDL_Init(0) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }

    /* Audio BEFORE vga_init, which asks whether it is running to decide about
     * vsync. Never in headless mode: the referee and the capture paths run as
     * fast as the machine allows, and a realtime clock would throttle them to
     * 210 seconds per run. */
    if (!g_headless && !mute) hostaudio_init();

    vga_init();
    if (use_probe) sim_set_fixed(&probe);
    if (rd_only)   sim_set_rd_only(1);
    if (use_rdp)   sim_set_rd_params(&rdp);
    if (use_rda)   sim_set_rd_amp((int16_t)rd_amp);
    if (kern >= 0) sim_set_kern(kern);
    sim_reset(variant);

    if (!frames) frames = sim_total_frames();
    fprintf(stderr, "=== HYSTERESIS (host) === %u frames, variant %d%s\n",
            frames, variant, g_headless ? ", headless" : "");

    /* Temporal metrics, for the flicker Azure spotted watching it live.
     *
     * A non-monotone react curve (bright folds down, dark rises) is a textbook
     * period-2 oscillator: a cell can alternate every frame forever. That is
     * invisible in a still and obvious in motion, which is exactly why it took
     * a human watching the exe to find it.
     *
     * d1 = mean |frame(n) - frame(n-1)|      how much changes per frame
     * d2 = mean |frame(n) - frame(n-2)|      how much changes per TWO frames
     *
     * For ordinary evolution d2 > d1: two frames of change exceed one. Under
     * period-2 flicker d2 collapses toward zero while d1 stays large, because
     * the field is simply alternating between two states. The ratio d2/d1 is
     * therefore the flicker meter: ~1.4 is healthy motion, below ~0.5 is a
     * strobing field. */
    static uint8_t prev1[320 * 240], prev2[320 * 240];
    double acc_d1 = 0, acc_d2 = 0; long acc_n = 0;

    int si = 0;
    hostaudio_start();
    while (sim_frame() < frames && !vga_should_quit()) {
        /* A restart request has to rebuild the world from the seed. There is
         * no other way back to the beginning -- and that goes for the music
         * too, which is a state machine with a 45-second decay in it. */
        if (vga_consume_skip_request() == 2) {
            sim_reset(variant); si = 0; hostaudio_reset();
        }

        sim_tick();
        hostaudio_wait_frame(sim_frame());

        if (g_fielddump) vga_field_emit();
        if (g_rawpipe)   vga_raw_emit();

        while (si < nshots && shots[si] == sim_frame()) { vga_screenshot(); si++; }

        if (stats) {
            const uint8_t *f = vga_320_front_buffer();
            if (sim_frame() > 2) {
                long d1 = 0, d2 = 0;
                for (int i = 0; i < 320 * 240; i += 7) {   /* stride-sampled */
                    int a = f[i] - prev1[i]; d1 += a < 0 ? -a : a;
                    int b = f[i] - prev2[i]; d2 += b < 0 ? -b : b;
                }
                acc_d1 += d1; acc_d2 += d2; acc_n++;
            }
            memcpy(prev2, prev1, sizeof prev2);
            memcpy(prev1, f, sizeof prev1);
        }

        if (hashes && (sim_frame() % 300) == 0)
            fprintf(stderr, "HASH f=%-6u %08lx\n", sim_frame(),
                    (unsigned long)field_hash(vga_320_front_buffer()));

        if (energy && (sim_frame() % 60) == 0)
            fprintf(stderr, "f=%6u  t=%3us  energy=%9u  mean=%5.1f\n",
                    sim_frame(), sim_frame() / 60, sim_energy(),
                    (double)sim_energy() / (320.0 * 240.0));
    }

    if (stats) {
        /* What "alive" means, measured rather than eyeballed:
         *   mean  — is there anything there at all
         *   sdev  — spatial structure. A uniform grey field scores a mean but
         *           no sdev, and that is the screensaver failure in numbers.
         *   live  — fraction of cells strictly between dead and saturated. A
         *           field pinned at 0 or 255 is perfectly stable and perfectly
         *           useless; without this the other two can both look fine
         *           while the picture is pure black and white.
         * Reported to stderr because stdout is the binary channel. */
        const uint8_t *f = vga_320_front_buffer();
        double m = 0, v = 0; int live = 0, mid = 0;
        for (int i = 0; i < 320 * 240; i++) m += f[i];
        m /= 320.0 * 240.0;
        for (int i = 0; i < 320 * 240; i++) {
            double d = f[i] - m; v += d * d;
            if (f[i] > 12 && f[i] < 243) live++;
            if (f[i] > 64 && f[i] < 192) mid++;
        }
        v = sqrt(v / (320.0 * 240.0));

        /* mid is the one that caught the real problem. The first living
         * regime scored a healthy mean and a big sdev while being effectively
         * ONE BIT: every cell sat at either ~0 or ~200, so the picture was two
         * colours and the palette had nothing to work with. A steep react curve
         * amplifies the ordered dither to full scale, which also explains the
         * speckle. sdev cannot see that; the midtone fraction can. */
        const double d1 = acc_n ? acc_d1 / acc_n : 0.0;
        const double d2 = acc_n ? acc_d2 / acc_n : 0.0;
        fprintf(stderr, "mean=%6.2f sdev=%6.2f live=%5.1f%% mid=%5.1f%% "
                        "d1=%8.0f flick=%5.2f\n",
                m, v, 100.0 * live / (320.0 * 240.0),
                100.0 * mid / (320.0 * 240.0),
                d1, d1 > 0 ? d2 / d1 : 0.0);
    }

    fprintf(stderr, "done: %u frames, final energy %u (mean %.1f)\n",
            sim_frame(), sim_energy(), (double)sim_energy() / (320.0 * 240.0));
    hostaudio_shutdown();
    SDL_Quit();
    return 0;
}
