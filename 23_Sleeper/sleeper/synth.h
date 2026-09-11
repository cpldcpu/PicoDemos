/* The synth beyond sleeper.h's public calls: what the tools and a
 * device-side referee need. SLEEPER / LATENT / 2026. Phosphor.
 *
 * The contract (sleeper.h): stereo interleaved int16 at 24 kHz, pull model.
 * synth_render() may be called with any block size and the output must not
 * depend on it. Integer arithmetic only, so the host WAV and the device DMA
 * ring are bit-identical and can be diffed by hash.
 */
#ifndef SLEEPER_SYNTH_H
#define SLEEPER_SYNTH_H
#include <stdint.h>
#include "sleeper.h"

int32_t synth_peak(void);                        /* largest |sample| so far  */

/* Mixing: mute all but the named voices. Tools only. */
#define SOLO_DRUMS   1u         /* kick, snare, hats, ride, crash        */
#define SOLO_CLACK   2u         /* the rail joints and the points        */
#define SOLO_BASS    4u         /* the Reese                             */
#define SOLO_PIANO   8u         /* the electric piano                    */
#define SOLO_PAD     16u
#define SOLO_LEAD    32u
#define SOLO_CHOIR   64u
#define SOLO_FX      128u       /* the delay and the plate               */
#define SOLO_HORN    256u
#define SOLO_BELLS   512u       /* the bells, the chime, the board flaps */
#define SOLO_BRAKE   1024u      /* the brake and the riser               */
#define SOLO_ALL     2047u
void    synth_solo(unsigned mask);
#endif
