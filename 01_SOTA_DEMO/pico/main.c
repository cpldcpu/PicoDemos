/* SOTA pico entry point. Backend-agnostic: works for both
 * sota_pico (ST7789) and sota_composite (PAL/S-Video). The chosen
 * backend's backend_init() does any display-specific bring-up. */
#include "pico/stdlib.h"
#if SOTA_VGA_BUILD
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#endif

#include "backend.h"
#include "graphics.h"
#include "anim.h"
#include "scene.h"
#include "choreography.h"
#include "iff-font.h"
#include "heap.h"
#include "sound.h"
#include "tinf/src/tinf.h"

int main(void)
{
#if SOTA_VGA_BUILD
    /* Overclock BEFORE stdio_init_all so USB CDC enumerates at the final
     * clock; if we change sys_clk after USB is up, Windows drops the
     * connection and we lose serial debug output. */
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(250000, true);
#endif
    stdio_init_all();
    sleep_ms(200);

    heap_reset();
    tinf_init();

    if (!backend_init(0, 0, false, NULL)) for (;;) tight_loop_contents();
    if (!ifffont_init())                   for (;;) tight_loop_contents();
    /* sound_init's argument is nosound=true → disable audio. We want
     * audio enabled, so pass false. (sound_stub.c on the ST7789/composite
     * builds ignores the arg anyway.) */
    if (!sound_init(false))                for (;;) tight_loop_contents();
    if (!choreography_init())              for (;;) tight_loop_contents();

    graphics_init();
    anim_init();
    scene_init();

    backend_run(0, NULL);

    while (true) tight_loop_contents();
}
