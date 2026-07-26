/* Audio placeholder.
 *
 * The demo synthesises its own music (PLANNING.md section 6) and that is build
 * order steps 6 and 7. Until then the device needs audio.h satisfied and a
 * clock that advances, and nothing else.
 *
 * Note what this deliberately does NOT do: drive the demo. The simulation is
 * stepped once per frame from an internal counter (sim.h), never from a
 * millisecond clock, so a stubbed audio clock cannot change what is on screen.
 * When the real synth lands it will be the sequencer, and the forcing schedule
 * and the music will be the same object.
 */

#include "audio.h"
#include "pico/stdlib.h"

static uint64_t g_t0;

void audio_init(void)  { g_t0 = time_us_64(); }
void audio_start(void) { g_t0 = time_us_64(); }
void audio_pump(void)  { }

uint32_t audio_now_ms(void)
{
    return (uint32_t)((time_us_64() - g_t0) / 1000ull);
}

void audio_seek_ms(uint32_t target_ms)
{
    /* There is no seek in this demo, by construction. Kept only because
     * audio.h declares it. */
    (void)target_ms;
}
