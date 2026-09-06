/* Placement of the hot path.
 *
 * On RP2350 both CODE and DATA default to flash, reached through the XIP
 * cache. The ray walk touches ~40,000 samples per frame, each one reading
 * height and material textures — a working set far larger than the XIP cache,
 * so almost every sample pays a flash fetch. That is why the first firmware
 * measured ~1,500 cycles per sample when the arithmetic accounts for a small
 * fraction of that: it is not computing, it is waiting for flash.
 *
 * SUSTAIN_HOT puts a function in SRAM. Applied to the renderer, the world
 * samplers and the field evaluators, so the inner loop neither executes from
 * flash nor (once the textures are copied) reads from it.
 *
 * This is the lesson the repo already recorded from earlier beam-racing work:
 * on this chip, texture and code must be in SRAM or it underruns.
 */
#ifndef SUSTAIN_HOT_H
#define SUSTAIN_HOT_H

#ifdef PICO_BUILD
#include "pico.h"          /* not pico/platform.h — the SDK rejects that directly */
#define SUSTAIN_HOT(fn) __not_in_flash_func(fn)
#else
#define SUSTAIN_HOT(fn) fn
#endif

#endif
