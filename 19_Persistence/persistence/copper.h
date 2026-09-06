#ifndef PV_COPPER_H
#define PV_COPPER_H

#include <stdint.h>

enum { COPPER_BARS = 0, COPPER_DUSK = 1 };

/* Fill row[0..479] with one RGB565 colour per row for frame f. */
void copper_rows(uint16_t *row, uint32_t f, int style);

#endif
