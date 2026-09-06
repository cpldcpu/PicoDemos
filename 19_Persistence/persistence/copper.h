#ifndef PV_COPPER_H
#define PV_COPPER_H

#include <stdint.h>
#include "dither.h"

enum { COPPER_BARS = 0, COPPER_DUSK = 1 };

/* Fill row[0..479] with one EIGHT-BIT colour per scanline for frame f. The
 * caller dithers it down to the DAC's five bits at draw time (dither.h), which
 * is the whole reason this is not packed here. */
void copper_rows(rgb8_t *row, uint32_t f, int style);

#endif
