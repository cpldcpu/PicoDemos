/* The synth: one implementation, two hosts, pull model, block-size independent.
 *
 * Stereo interleaved int16 at PV_RATE. synth_render() may be called with any
 * block size and the output must not depend on it: everything control-rate
 * runs off the absolute sample counter. Integer arithmetic only, so the host
 * WAV and the device DMA ring are bit-identical and can be diffed by hash.
 */

#ifndef PV_SYNTH_H
#define PV_SYNTH_H

#include <stdint.h>
#include "persistence.h"

void     synth_init(void);                       /* tables; once             */
void     synth_reset(void);                      /* back to sample 0         */
void     synth_seek(uint32_t sample);            /* render-and-discard to it */
void     synth_render(int16_t *out, int frames); /* frames * 2 int16         */
uint32_t synth_pos(void);
int32_t  synth_peak(void);

/* FNV-1a over every emitted int16, latched every second (host/device diff). */
int      synth_hash_latch(uint32_t *pos, uint32_t *hash);

/* Mixing: mute all but the named voices. */
#define SOLO_KICK   1u
#define SOLO_SNARE  2u
#define SOLO_HAT    4u
#define SOLO_BASS   8u
#define SOLO_ARP    16u
#define SOLO_LEAD   32u
#define SOLO_PAD    64u
#define SOLO_FX     128u        /* riser and the delay */
#define SOLO_LEAD2  256u        /* the counter-melody  */
#define SOLO_ALL    511u
void     synth_solo(unsigned mask);

#endif
