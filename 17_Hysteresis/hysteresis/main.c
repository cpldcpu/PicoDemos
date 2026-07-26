/* HYSTERESIS — RP2350 entry point.
 *
 * Build order step 3: get it on the hardware and MEASURE, before tuning
 * anything else. On demo 16 every performance estimate made from the desktop
 * host turned out wrong, in both directions, because the host has no XIP cache
 * to miss. Nothing in this file's telemetry is inferred; it is all counted on
 * the target.
 *
 * The number that matters is CYCLES PER CELL. The plan budgeted 65 on one core
 * (76,800 cells x 60 Hz = 4.608 M cell-updates/s at 300 MHz). That figure has
 * never been checked. It is printed once a second below, and it either
 * confirms the design or kills it.
 */

#include "pico/stdlib.h"
#include "pico/scanvideo.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/gpio.h"
#include <stdio.h>

#include "sim.h"
#include "field.h"
#include "vga.h"
#include "audio.h"

#define LED_PIN 25

/* 300 MHz @ 1.20 V. Inherited from SUSTAIN, where it was established by
 * measurement rather than caution:
 *
 *   350 MHz  not PLL-reachable. set_sys_clock_khz needs vco = mhz*pd1*pd2
 *            exactly; with required=false it SILENTLY falls back to the
 *            150 MHz default and the demo runs at half speed looking fine.
 *   320/352  boot and enumerate USB, then the main loop hangs.
 *
 * required=true on purpose: a clock that cannot be set should fail loudly
 * rather than quietly halve the frame rate. */
#ifndef HYST_MHZ
#define HYST_MHZ 300
#endif

int main(void)
{
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(HYST_MHZ * 1000, true);

    stdio_init_all();
    sleep_ms(500);              /* let USB CDC enumerate */

    printf("\n=== HYSTERESIS (LATENT) : bring-up ===\n");
    printf("sys_clk = %u Hz\n", (unsigned)clock_get_hz(clk_sys));

    field_init();
    audio_init();
    vga_init();
    vga_set_mode(MODE_320);
    sim_reset(0);
    audio_start();

    printf("entering main loop\n");

    uint32_t frame = 0, last_frame = 0;
    uint64_t last_report = time_us_64();
    uint64_t step_us_acc = 0;
    uint32_t worst_step_us = 0;

    while (sim_frame() < sim_total_frames()) {
        /* compute -> wait for vblank -> flip. The flip has to land inside the
         * blanking interval or the beam shows half of two different frames;
         * the feedback loop means a torn frame would then be fed back in and
         * become permanent, which is a worse failure than a normal tear. */
        const uint64_t t0 = time_us_64();
        sim_step();
        const uint32_t step_us = (uint32_t)(time_us_64() - t0);
        step_us_acc += step_us;
        if (step_us > worst_step_us) worst_step_us = step_us;

        scanvideo_wait_for_vblank();
        sim_present();

        gpio_put(LED_PIN, (int)(frame & 1));
        frame++;

        /* Same cadence and same frame indices as the host's --hash, so the
         * two logs can be diffed line for line. */
        if ((sim_frame() % 300) == 0)
            printf("HASH f=%-6lu %08lx\n", (unsigned long)sim_frame(),
                   (unsigned long)field_hash(vga_320_front_buffer()));

        const uint64_t now = time_us_64();
        if (now - last_report >= 1000000ull) {
            const uint32_t nf = frame - last_frame;
            const double fps = (double)nf * 1e6 / (double)(now - last_report);
            const double avg_us = (double)step_us_acc / (nf ? nf : 1);

            /* cycles per cell — the one number the whole design rests on */
            const double mhz = clock_get_hz(clk_sys) / 1e6;
            const double cyc  = avg_us * mhz / (double)(FIELD_W * FIELD_H);

            printf("[f=%6lu] %5.1f fps | step avg %6.0f us worst %6lu us | "
                   "%5.1f cycles/cell | energy %lu\n",
                   (unsigned long)frame, fps, avg_us,
                   (unsigned long)worst_step_us, cyc,
                   (unsigned long)sim_energy());

            last_report = now;
            last_frame = frame;
            step_us_acc = 0;
            worst_step_us = 0;
        }
    }

    printf("demo finished after %lu frames\n", (unsigned long)frame);
    while (1) {
        gpio_put(LED_PIN, 1); sleep_ms(500);
        gpio_put(LED_PIN, 0); sleep_ms(500);
    }
}
