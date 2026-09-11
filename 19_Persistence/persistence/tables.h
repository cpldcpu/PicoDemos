/* Shared lookup tables, built once at start-up on both targets. */

#ifndef PV_TABLES_H
#define PV_TABLES_H

#include <stdint.h>

/* Signed 8-bit sine, 1024 entries per cycle, stored twice over so a kernel
 * can index (base + x) for x < 1024 without a mask. Values in [-127, 127]. */
extern int8_t   pv_sin8_tab[2048];
/* The same, unsigned: 128 + sin, in [1, 255]. For kernels that sum bytes. */
extern uint8_t  pv_usin8_tab[2048];
/* Q15 sine, 1024 per cycle, also doubled. */
extern int16_t  pv_sin16_tab[2048];

void pv_tables_init(void);

static inline int pv_sin8(uint32_t i)  { return pv_sin8_tab[i & 1023]; }
static inline int pv_sin16(uint32_t i) { return pv_sin16_tab[i & 1023]; }
static inline int pv_cos16(uint32_t i) { return pv_sin16_tab[(i + 256) & 1023]; }

#endif
