/* SLOP — entry point.
 *
 * - Init Pico (overclock to 250 MHz, 1.20 V) — same recipe as 04/05.
 * - Init VGA (scanvideo on core 1, MODE_320).
 * - Init audio (phase-1 stub: just a ms clock).
 * - Main loop: pump the scene runner at frame cadence, page-flip,
 *   sleep to next vblank.
 */

#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/gpio.h"
#include <stdio.h>

#include "scene.h"
#include "vga.h"
#include "render.h"
#include "audio.h"

#define LED_PIN 25

int main(void)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    /* 300 MHz @ 1.20 V — verified stable on this specific Waveshare
     * RP2350-Plus board. Tried 400 MHz briefly (community-cited as a
     * safe everyday point for RP2350) but the demo didn't boot —
     * this particular chip / supply combo doesn't tolerate it. If
     * you want to retry at higher clocks: bump voltage to 1.25 V
     * first, then try 350 → 400. Audio PWM divisor and the DMA
     * pacing timer are computed from clock_get_hz(clk_sys) so they
     * follow whatever sys_clk ends up at. */
    /* Clock target: 300 MHz @ 1.20 V. MEASURED, not assumed — the telemetry
     * line prints the achieved clock, which is how the following was found:
     *
     *   350 MHz  the PLL cannot produce it. set_sys_clock_khz() needs
     *            vco = mhz * pd1 * pd2 exactly; 350 has no solution, so with
     *            required=false it SILENTLY fell back to the 150 MHz default
     *            and the demo ran at half speed while looking fine. The
     *            reachable steps above 300 are 320, 336, 352, 360, 368, 384,
     *            400.
     *   320 MHz  boots and enumerates USB, but the main loop hangs.
     *   352 MHz  same.
     *
     * So 300 is not a conservative guess, it is the ceiling for this board.
     * required=true on purpose: a clock that cannot be set should fail loudly
     * rather than quietly halve the frame rate. */
#ifndef SUSTAIN_MHZ
#define SUSTAIN_MHZ 300
#endif
#if SUSTAIN_MHZ > 300
    vreg_set_voltage(VREG_VOLTAGE_1_25);
#else
    vreg_set_voltage(VREG_VOLTAGE_1_20);
#endif
    sleep_ms(10);
    const bool clk_ok = set_sys_clock_khz(SUSTAIN_MHZ * 1000, true);

    stdio_init_all();
    sleep_ms(500);   /* give USB CDC a moment to enumerate */

    printf("\n=== SUSTAIN (LATENT) : bring-up ===\n");
    printf("sys_clk = %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    audio_init();
    vga_init();
    audio_start();

    printf("entering main loop\n");

    /* Main loop: pace each frame to the real VGA vblank (60 Hz) so the
     * demo's update rate matches the display refresh — matches the host
     * SDL build and removes the integer-truncation aliasing that the
     * old sleep_ms(20) 50 Hz pacer used to produce in scrollers and
     * other time-derived motion.
     *
     * We wait BEFORE present() so the back-buffer is fully populated
     * the moment scanout flips, avoiding any visible tear. If a frame
     * runs long (>16.67 ms) we'll miss this vblank and land on the
     * next one — that produces a brief 30 Hz hitch rather than tearing,
     * which is the right failure mode. */
    /* Audio is pumped by the repeating timer installed in audio_start()
     * — every 15 ms, ISR-priority, runs even when the main loop is
     * stalled in scene init or USB CDC printf. Don't call audio_pump()
     * from here as well: a timer ISR preempting a main-loop pump call
     * causes both threads to share refill_half / QOA decoder state,
     * which produces brief intermittent audible glitches whenever the
     * two happen to collide. The timer alone is sufficient and safe. */
    /* TELEMETRY over USB CDC.
     *
     * Reported once a second rather than per frame: at low frame rates a
     * per-frame printf would itself become a significant share of the budget,
     * and USB CDC blocks when no host is listening. Prints wall-clock fps, the
     * demo clock, and the audio clock separately — if the demo clock advances
     * while the audio clock does not, the fault is in audio; if both advance
     * and there is still no sound, the fault is downstream in PWM/DMA. */
    uint32_t frame = 0;
    uint32_t last_report_frame = 0;
    uint64_t last_report_us = time_us_64();
    uint32_t worst_frame_us = 0;
    uint64_t frame_start_us = time_us_64();

    while (1) {
        /* Buttons fire on the rising edge; consume + seek audio + visuals. */
        int sk = vga_consume_skip_request();
        if (sk != 0) {
            uint32_t now = audio_now_ms();
            uint32_t target = (sk == 1) ? scene_next_boundary_ms(now)
                            : (sk == -1) ? scene_prev_boundary_ms(now)
                            : 0;
            audio_seek_ms(target);
        }

        uint32_t t = audio_now_ms();
        int alive = scene_runner_tick(t);
        if (!alive) break;

        /* Wait for the next vertical blanking interval, then page-flip
         * during it so the new frame is fully ready when scanout starts
         * the next field. */
        scanvideo_wait_for_vblank();
        switch (vga_current_mode()) {
            case MODE_320:                 vga_320_present();   break;
            case MODE_160:                 vga_160_present();   break;
            case MODE_HIRES:               vga_hires_present(); break;
            case MODE_SPLIT_160_OVER_320:  vga_split_present(); break;
            case MODE_RACE:                /* core 1 beam-races; no present */ break;
            default: break;
        }

        gpio_put(LED_PIN, (int)(frame & 1));
        frame++;

        {
            const uint64_t now_us = time_us_64();
            const uint32_t this_frame_us = (uint32_t)(now_us - frame_start_us);
            if (this_frame_us > worst_frame_us) worst_frame_us = this_frame_us;
            frame_start_us = now_us;

            if (now_us - last_report_us >= 1000000ull) {
                const uint32_t nf = frame - last_report_frame;
                const float fps = (float)nf * 1000000.0f
                                / (float)(now_us - last_report_us);

                /* Where the frame actually goes, measured on the target.
                 * Estimating this from the host has been wrong repeatedly —
                 * the host has no XIP cache to miss, so the two machines have
                 * different bottlenecks entirely. */
                render_prof_t rp;
                render_prof_take(&rp);
                const uint32_t rf = rp.frames ? rp.frames : 1;

                printf("[t=%6lu ms] %5.1f fps  avg %5.1f ms  worst %5.1f ms  "
                       "audio=%lu ms | sky %lu  march %lu  post %lu us @%luMHz\n",
                       (unsigned long)t, (double)fps,
                       (double)((now_us - last_report_us) / 1000.0 / (nf ? nf : 1)),
                       (double)(worst_frame_us / 1000.0),
                       (unsigned long)audio_now_ms(),
                       (unsigned long)(rp.sky_us / rf),
                       (unsigned long)(rp.march_us / rf),
                       (unsigned long)(rp.post_us / rf),
                       (unsigned long)(clock_get_hz(clk_sys) / 1000000u));
                last_report_us = now_us;
                last_report_frame = frame;
                worst_frame_us = 0;
            }
        }
    }

    printf("demo finished after %lu frames\n", (unsigned long)frame);
    while (1) {
        gpio_put(LED_PIN, 1); sleep_ms(500);
        gpio_put(LED_PIN, 0); sleep_ms(500);
    }
}
