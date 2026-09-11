/* TESSERA: Phase adapts Overscan's SLEEPER / HELION platform.
 * Original double-page ownership, scanline transport and stereo DMA preserved. */
/* TESSERA platform. Adapted by Phase from Overscan's SLEEPER transport. */
#ifndef TESSERA_DEVICE_H
#define TESSERA_DEVICE_H
#include "tessera.h"
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
uint32_t video_missed(void);
uint32_t video_missed_boot(void);
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
