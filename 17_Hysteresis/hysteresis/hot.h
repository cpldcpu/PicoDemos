/* SRAM placement for the hot path.
 *
 * On RP2350 the flash is memory-mapped through the XIP cache. A function that
 * streams over a 76,800-byte array will evict its own code, and SUSTAIN's
 * profiling showed this is not a small effect. Anything that touches the field
 * every frame goes in SRAM.
 *
 * Note "pico.h", not "pico/platform.h" -- the latter does not pull in the
 * section attributes on its own, which cost an afternoon on demo 16.
 */

#ifndef HYST_HOT_H
#define HYST_HOT_H

#if defined(HOST_BUILD)
#  define HYST_HOT(fn) fn
#else
#  include "pico.h"
#  define HYST_HOT(fn) __not_in_flash_func(fn)
#endif

#endif
