/* The synth. One implementation, two hosts.
 *
 * The desktop build renders it to a WAV file (host/synthwav.c) so the music can
 * be listened to and thrown away without a Pico in the room; the device build
 * calls the identical code from core 1 straight into the PWM/DMA ring. Both
 * start from sample 0 and neither does any floating point, so the two renders
 * are bit-identical and referee test 1 can diff them.
 *
 * PULL MODEL. synth_render() is called with whatever block size the consumer
 * happens to want -- 4630500 samples at once on the host, a handful per
 * scanline on the device. The output must not depend on that chunking, so all
 * of the control-rate machinery is driven off an absolute sample counter and a
 * countdown that survives across calls. Anything that looked at the block
 * length would sound different on the two targets, which is precisely the class
 * of bug this arrangement exists to expose.
 */

#ifndef HYST_SYNTH_H
#define HYST_SYNTH_H

#include <stdint.h>

#define SYNTH_RATE     22050
#define SYNTH_CTL_DIV  64        /* control rate = 344.53 Hz */

void     synth_reset(void);
void     synth_render(int16_t *out, int n);   /* mono, appends n samples */
uint32_t synth_pos(void);                     /* samples since reset */
uint32_t synth_total_samples(void);           /* length of the piece */

/* Peak |sample| seen since reset. Headroom check for the mix, printed by the
 * WAV tool -- a synth that clips is a synth nobody can hear the bottom of. */
int32_t  synth_peak(void);

/* Mute all but the named voices, for mixing.
 *
 * This exists because the first level design was arithmetic rather than
 * measurement: the pad was scaled so that eight detuned saws could not overflow
 * if they ever lined up in phase, which they never do, so it played about
 * eighteen dB below where it was meant to. Guessing at a level is the same
 * mistake as guessing at filter headroom, and the fix is the same -- render one
 * voice at a time and read the number off. */
#define SOLO_BASS    1u
#define SOLO_PAD     2u
#define SOLO_NOISE   4u
#define SOLO_IMPACT  8u
#define SOLO_REVERB 16u
#define SOLO_ALL    31u
void     synth_solo(unsigned mask);

/* Running FNV-1a over every emitted sample, latched once per second.
 *
 * This is how the device render gets diffed against the host render
 * sample-for-sample (PLANNING.md section 7, referee test 1). Shipping 4.6
 * million samples up a USB CDC link to compare them is not practical; hashing
 * them is, and it is the same trick field_hash() already uses to prove the
 * picture matches. A mismatch localises to the second it first appears in.
 *
 * Returns 0 if no full second has elapsed yet. Safe to call from the other core:
 * the pair is published under a sequence counter, because a torn (pos, hash)
 * read would look exactly like a determinism bug and send me hunting one that
 * does not exist.
 */
int      synth_hash_latch(uint32_t *pos, uint32_t *hash);

#endif
