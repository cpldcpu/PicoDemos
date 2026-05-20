/* Dawn — RP2040 port entry point.
 *
 * Boot order:
 *   1. Stock SDK clock init (default 125 MHz; scanvideo wants ≥150 for
 *      the 320×240@60 mode, but pico_scanvideo bumps sys_clk itself when
 *      it sets up the dot clock — we don't need to override).
 *   2. sequencer_init — generates math tables, voxel maps, polygon list,
 *      first torus frame; sets palette to scheme 1.
 *   3. audio_init — stub for now.
 *   4. vga_start — launches scanout on core 1.
 *   5. Frame loop on core 0: tick the sequencer at the 50 Hz cadence.
 */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"

#include "dawn.h"
#include "sequencer.h"
#include "audio.h"
#include "vga.h"
#include "chunky.h"
#include "button.h"

int main(void)
{
    /* Overclock to 250 MHz at 1.20 V — RP2040 default 125 MHz couldn't
     * sustain 60 Hz in the FINALE (text + torus + blur per frame).
     * Mirrors the pattern 04_SOTA_DEMO/sota/pico/main.c uses. Done
     * before stdio_init_all so USB CDC enumerates at the final clock. */
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(250000, true);

    stdio_init_all();

    sequencer_init();
    audio_init();
    button_init();
    vga_start();

    int frame_count = 0;
    while (true) {
        /* Button A on the Pimoroni VGA Base advances to the next scene.
         * Polled here so a press-and-hold only fires one skip per press
         * (button_a_just_pressed returns the rising-edge event). */
        if (button_a_just_pressed()) {
            sequencer_skip();
        }

        sequencer_step();
        audio_update(frame_count++);
        /* chunky_present() blocks on vblank then atomically publishes
         * the rendered back buffer to the scanline reader. This both
         * eliminates tearing and gives us 60 Hz cadence — the original's
         * 50 Hz timings are frame-counted, so running at VGA's 60 Hz just
         * makes the demo 20% faster. Acceptable. */
        chunky_present();
    }
}
