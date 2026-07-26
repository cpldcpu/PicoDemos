/* Audio playback API.
 *
 * audio_qoa.c decodes a QOA stream baked into flash and pumps samples to
 * a PWM-DMA ring (Pimoroni VGA Demo Base routing: GP27/GP28 mirrored
 * mono on both 3.5 mm jack channels).
 *
 * Sample-locked timing: audio_now_ms() returns ms since audio_start().
 * Because the DMA pacing and time_us_64() both derive from sys_clk,
 * the audio clock and the wall clock don't drift — so we use wall clock
 * seeded at audio_start() as the demo clock. Latency from "samples
 * queued into ring" to "samples flowing through PWM" is one DMA buffer
 * (~46 ms at 22050 Hz mono), constant.
 *
 * Call audio_pump() from the main loop at least every ~40 ms or the DMA
 * ring underruns.
 */

#ifndef THEDEMO_AUDIO_H
#define THEDEMO_AUDIO_H

#include <stdint.h>

void     audio_init(void);   /* PWM + DMA + parse QOA header, leaves DMA running on silence */
void     audio_start(void);  /* rewind QOA stream + zero the demo clock */
void     audio_pump(void);   /* refill DMA halves as needed; call frequently from main loop */
uint32_t audio_now_ms(void); /* ms since audio_start() */

/* Jump the audio playhead to target_ms AND reset the demo clock so
 * audio_now_ms() returns target_ms. Host implements as a direct sample
 * jump; Pico version is currently a no-op (would need a QOA frame-skip
 * walk to be sample-accurate). */
void     audio_seek_ms(uint32_t target_ms);

/* Shallowest the DMA ring has been, in unplayed samples out of 1023. Telemetry
 * for the one audio failure the sample hash cannot see -- see audio_synth.c. */
int      audio_min_fill(void);

#endif
