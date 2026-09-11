/* The synth beyond helion.h's four public calls: what the tools and a
 * device-side referee need.
 *
 * The contract (helion.h): stereo interleaved int16 at 24 kHz, pull model.
 * synth_render() may be called with any block size and the output must not
 * depend on it. Integer arithmetic only, so the host WAV and the device DMA
 * ring are bit-identical and can be diffed by hash.
 */

#ifndef HELION_SYNTH_H
#define HELION_SYNTH_H

#include <stdint.h>
#include "helion.h"

int32_t synth_peak(void);                        /* largest |sample| so far  */

/* FNV-1a over every emitted int16, latched every second (host/device diff).
 * Returns 1 once per new latch, then 0 until the next one. */
int     synth_hash_latch(uint32_t *position, uint32_t *hash);

/* Mixing: mute all but the named voices. Tools only. */
#define SOLO_DRUM    1u         /* the frame drum, struck and touched */
#define SOLO_RIM     2u
#define SOLO_SCRAPE  4u
#define SOLO_BASS    8u
#define SOLO_BRONZE  16u
#define SOLO_REED    32u
#define SOLO_BOW     64u
#define SOLO_FX      128u       /* the delay and the hall         */
#define SOLO_HARM    256u
#define SOLO_GONG    512u
#define SOLO_AIR     1024u
#define SOLO_ALL     2047u
void    synth_solo(unsigned mask);

#endif
