/* The synth beyond pelagic.h's four public calls: what the tools and a
 * device-side referee need.
 *
 * The contract (pelagic.h): stereo interleaved int16 at 24 kHz, pull model.
 * synth_render() may be called with any block size and the output must not
 * depend on it. Integer arithmetic only, so the host WAV and the device DMA
 * ring are bit-identical and can be diffed by hash.
 */

#ifndef PELAGIC_SYNTH_H
#define PELAGIC_SYNTH_H

#include <stdint.h>
#include "pelagic.h"

int32_t synth_peak(void);                        /* largest |sample| so far  */

/* FNV-1a over every emitted int16, latched every second (host/device diff). */
int     synth_hash_latch(uint32_t *pos, uint32_t *hash);

/* Mixing: mute all but the named voices. Tools only. */
#define SOLO_KICK    1u
#define SOLO_BRUSH   2u
#define SOLO_SHAKER  4u         /* and the whoosh                 */
#define SOLO_BASS    8u
#define SOLO_PLUCK   16u
#define SOLO_LEAD    32u
#define SOLO_PAD     64u
#define SOLO_FX      128u       /* the delay and the reverb       */
#define SOLO_LEAD2   256u
#define SOLO_DRONE   512u
#define SOLO_GLASS   1024u
#define SOLO_CHOIR   2048u
#define SOLO_SHIMMER 4096u
#define SOLO_BOOM    8192u
#define SOLO_ALL     16383u
void    synth_solo(unsigned mask);

#endif
