#ifndef PV_AUDIO_H
#define PV_AUDIO_H

#include <stdint.h>
#include "persistence.h"

void     audio_init(void);        /* PWM + DMA, ring prefilled with the first 85 ms */
void     audio_start(void);       /* DMA go: sample 0 leaves now                     */
void     audio_pump(void);        /* core 0, once per frame: top the ring up         */
uint32_t audio_consumed(void);    /* samples the DMA has played: the master clock    */
unsigned audio_min_fill(void);    /* shallowest the ring has been                    */

#endif
