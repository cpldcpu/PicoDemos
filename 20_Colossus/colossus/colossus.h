/* COLOSSUS -- the constants everything is built from.
 *
 * One clock. The synth advances an absolute sample counter; the picture asks
 * what sample is playing and draws that moment (VESPER's model). Every cue in
 * the production is a bar number in the score, so the grid below is the whole
 * contract between the music, the timeline and the tools.
 *
 * 125 BPM was chosen so that a 16th is an integer number of samples AND an
 * integer number of control ticks: 2,880 samples = 60 ticks of 48. (128 BPM,
 * the first draft, puts a 16th at 2,812.5 samples, which cannot land on a
 * tick, and a sequencer that rounds is a sequencer that jitters.)
 */

#ifndef CV_COLOSSUS_H
#define CV_COLOSSUS_H

#include <stdint.h>

#define CV_W            320
#define CV_H            240

#define CV_RATE         24000            /* Hz, stereo                          */
#define CV_BPM          125
#define CV_SPB          11520            /* samples per beat: 24000 * 60 / 125  */
#define CV_STEP         2880             /* samples per 16th                    */
#define CV_BAR          46080            /* samples per bar, 4/4                */
#define CV_STEPS_PER_BAR 16
#define CV_CTL          48               /* synth control tick, samples: 500 Hz */

#define CV_BARS         160              /* twenty phrases of eight             */
#define CV_TOTAL_SAMPLES ((uint32_t)CV_BARS * CV_BAR)   /* 7,372,800 = 5:07.2 */

static inline uint32_t cv_bar_of(uint32_t sample)  { return sample / CV_BAR; }
static inline uint32_t cv_step_of(uint32_t sample) { return sample / CV_STEP; }
/* 0..65535 across the current bar */
static inline uint32_t cv_bar_frac(uint32_t sample) { return ((sample % CV_BAR) << 16) / CV_BAR; }

#if defined(HOST_BUILD)
#  define CV_HOT(fn) fn
#else
#  include "pico.h"
#  define CV_HOT(fn) __not_in_flash_func(fn)
#endif

#endif
