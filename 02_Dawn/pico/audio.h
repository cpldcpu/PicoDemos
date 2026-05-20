/* Paula-style audio playback — Pimoroni VGA Base PWM audio (GP27/28).
 *
 * Original: 4 channels, fixed 8-byte triangle-ish waveform, slightly
 * detuned periods (9724/9740/9756/9772) at PAL_CLOCK = 3,579,545 Hz.
 *
 * Phase 1 (current): stub — no audio. Saves us from wiring PWM-DMA
 * before visuals are confirmed. Audio is monophonic in spirit (one short
 * detuned chord pad) so adding it later is small.
 */

#ifndef AUDIO_H
#define AUDIO_H

void audio_init(void);
void audio_update(int frame_count);  /* called once per engine tick */

#endif
