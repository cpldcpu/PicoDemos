/* device.h -- the platform side of the contract (Overscan).
 *
 * Lifted from HELION's device.h. Two counters are new and the claims in
 * PLANNING §2 rest on them:
 *
 *   video_repeats()  the number of vsyncs at which scanline zero found no
 *                    pending page, i.e. fields that showed the previous
 *                    picture again. Counting starts at the first successful
 *                    latch, so the fields before core 0 has drawn anything
 *                    are not charged to the film.
 *   the `over` count the frames whose demo_render() exceeded 16,000 us --
 *                    kept by main.c, not here, because it is a property of
 *                    the renderer and not of the transport.
 */
#ifndef SLEEPER_DEVICE_H
#define SLEEPER_DEVICE_H
#include "sleeper.h"
void video_init(void);
uint16_t *video_back(void);
void video_present(void);
uint32_t video_repeats(void);
/* Where the repeats were: the total number of generated fields, the field
 * number of the first counted repeat (0 = none), and the repeats that
 * happened during boot, before video_arm(). Any pointer may be NULL. */
void video_fields(uint32_t *fields, uint32_t *first_repeat, uint32_t *boot_repeats);
/* Start counting: the film begins now. Called once, after audio_start(). */
void video_arm(void);
void audio_init(void);
void audio_start(void);
void audio_pump(void);
uint32_t audio_position(void);
unsigned audio_min_fill(void);
/* audio_min_window() returns the lowest ring fill since the previous call and
 * rearms; audio_underruns() counts every pump that found the DMA level with
 * the writer. video_prof() reports core 1's cost of audio_pump() from its own
 * SysTick: cumulative pump count and cycles, the worst single pump since the
 * last read (rearmed), and the worst of the whole run. Any pointer may be
 * NULL. */
unsigned audio_min_window(void);
unsigned audio_underruns(void);
void video_prof(uint32_t *pumps, uint32_t *cycles, uint32_t *worst_window, uint32_t *worst_all);
#endif
