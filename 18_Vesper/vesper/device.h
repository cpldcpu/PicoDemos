#ifndef VESPER_DEVICE_H
#define VESPER_DEVICE_H
#include "vesper.h"
void video_init(void);
uint16_t *video_back(void);
void video_present(void);
void audio_init(void);
void audio_start(void);
void audio_pump(void);
uint32_t audio_position(void);
unsigned audio_min_fill(void);
#endif
