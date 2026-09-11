#ifndef HELION_DEVICE_H
#define HELION_DEVICE_H
#include "helion.h"
void video_init(void);
uint16_t *video_back(void);
void video_present(void);
void audio_init(void);
void audio_start(void);
void audio_pump(void);
uint32_t audio_position(void);
unsigned audio_min_fill(void);
/* Added for the hardware run (Overscan). audio_min_window() returns the
 * lowest fill since the previous call and rearms; audio_underruns() counts
 * every pump that found the DMA level with the writer. video_prof() reports
 * core 1's cost of audio_pump() from its own SysTick: cumulative pump count
 * and cycles, the worst single pump since the last read (rearmed), and the
 * worst of the whole run. Any pointer may be NULL. */
unsigned audio_min_window(void);
unsigned audio_underruns(void);
void video_prof(uint32_t *pumps, uint32_t *cycles, uint32_t *worst_window, uint32_t *worst_all);
#endif
