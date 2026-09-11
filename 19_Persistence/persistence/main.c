/* PERSISTENCE -- RP2350 entry point. Core 0: time.
 *
 * The loop is the whole synchronisation story:
 *
 *   core 1 is drawing frame f from g_beam[f & 1]
 *   -> prepare frame f+1 into g_beam[(f+1) & 1]   (never touches f's tables)
 *   -> top up the audio ring
 *   -> publish f+1; core 1 latches it at its next scanline 0 and acks
 *   -> f = f+1
 *
 * Which frame is "next" is read off the DMA's sample counter, not counted: if
 * core 0 ever takes longer than a frame, the picture skips a frame and stays
 * on the music instead of drifting one frame behind it forever. This demo can
 * afford that because every frame is a function of f.
 *
 * Telemetry once a second over USB CDC. The number that matters is the worst
 * line in cycles against the 8,400 the governor allows; 'late' and 'lod' must
 * read 0 for the whole run or the build does not ship.
 */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/gpio.h"
#include <stdio.h>

#include "beam.h"
#include "tables.h"
#include "synth.h"
#include "audio.h"
#include "vga.h"

#define LED_PIN 25

int main(void)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(300000, true);

    stdio_init_all();
    sleep_ms(400);
    printf("\n=== PERSISTENCE (LATENT) === sys_clk %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    pv_tables_init();
    synth_init();
    beam_init();
    audio_init();

    /* Frame 0 before core 1 exists, then let the DMA go the moment core 1 has
     * latched it, so sample 0 is consumed at the top of frame 0. */
    beam_frame(0);
    vga_init();
    vga_publish(0);
    vga_wait_latched(0, 100000);
    audio_start();

    uint32_t f = 0;
    uint64_t last_report = time_us_64();
    const uint64_t t_start = last_report;
    uint32_t worst_prep = 0, worst_line = 0, skipped = 0;
    int last_cue = beam_cue_index(0);

    while (f + 1 < PV_TOTAL_FRAMES) {
        const uint64_t t0 = time_us_64();
        uint32_t next = audio_consumed() / PV_SPF + 1;
        if (next <= f) next = f + 1;
        if (next > f + 1) skipped += next - f - 1;

        /* scene change: the governor's verdict is per scene */
        int cue = beam_cue_index(next);
        if (cue != last_cue) { g_lod = 0; last_cue = cue; }

        beam_frame(next);
        audio_pump();
        const uint32_t prep = (uint32_t)(time_us_64() - t0);
        if (prep > worst_prep) worst_prep = prep;

        vga_publish(next);
        vga_wait_latched(next, 40000);
        f = next;

        uint32_t wl = vga_worst_cycles();
        if (wl > worst_line) worst_line = wl;
        gpio_put(LED_PIN, (int)(f & 32));

        const uint64_t now = time_us_64();
        if (now - last_report >= 1000000ull) {
            uint32_t apos, ahash;
            const double fps = (double)vga_frames_done() * 1e6 / (double)(now - t_start);
            const double sps = (double)audio_consumed() * 1e6 / (double)(now - t_start);
            printf("[f=%5u %-9s] line %5u/%5u cy (mean/worst) | slips %u | prep %5u us | lod %u ev %u | skip %u | ring %u | %.2f fps %.0f sps",
                   (unsigned)f, beam_scene_name(f), (unsigned)vga_mean_cycles(), (unsigned)worst_line,
                   (unsigned)vga_slips(), (unsigned)worst_prep,
                   (unsigned)g_lod, (unsigned)vga_lod_events(),
                   (unsigned)skipped, audio_min_fill(), fps, sps);
            if (synth_hash_latch(&apos, &ahash)) printf(" | AHASH s=%u %08x", (unsigned)apos, (unsigned)ahash);
            printf("\n");
            last_report = now; worst_prep = 0; worst_line = 0;
        }
    }

    printf("done: %u frames, late %u, lod events %u, skipped %u\n",
           (unsigned)f, (unsigned)vga_late_lines(), (unsigned)vga_lod_events(), (unsigned)skipped);
    for (;;) { gpio_put(LED_PIN, 1); sleep_ms(500); gpio_put(LED_PIN, 0); sleep_ms(500); }
}
