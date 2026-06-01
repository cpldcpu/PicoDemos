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
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(300000, true);

    stdio_init_all();
    sleep_ms(500);   /* give USB CDC a moment to enumerate */

    printf("\n=== SLOP: bring-up ===\n");
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
    uint32_t frame = 0;
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
    }

    printf("demo finished after %lu frames\n", (unsigned long)frame);
    while (1) {
        gpio_put(LED_PIN, 1); sleep_ms(500);
        gpio_put(LED_PIN, 0); sleep_ms(500);
    }
}
