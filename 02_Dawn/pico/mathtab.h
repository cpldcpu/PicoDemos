/* Math tables — RP2040 port of Dawn
 *
 * Original (dawn_final.s) generates THREE tables at startup:
 *   1. sine table (1024 entries, ~2 KB)
 *   2. division table (256×256 = 128 KB) — replaced with HW divider here
 *   3. normalization table (65536 entries = 128 KB) — replaced with sqrt
 *
 * RP2040 has a hardware divider and a fast multiplier; on a 125 MHz core
 * the original LUT optimizations don't pay back the SRAM cost.
 */

#ifndef MATHTAB_H
#define MATHTAB_H

#include "dawn.h"

extern int16_t sin_tab[SIN_TAB_LEN];

void mathtab_init(void);

static inline int sin_lookup(int angle)
{
    return sin_tab[angle & SIN_TAB_MASK];
}

static inline int cos_lookup(int angle)
{
    return sin_tab[(angle + (SIN_TAB_LEN / 4)) & SIN_TAB_MASK];
}

#endif
