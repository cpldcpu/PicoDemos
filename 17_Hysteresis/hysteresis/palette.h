/* HYSTERESIS — the palette.
 *
 * THE ONE DECLARED EXEMPTION to the rule (PLANNING.md section 2): the palette
 * MAY be a function of t. Colour is not state, it is how the state is read
 * out. 256 entries recolour the entire world for 256 writes, and pretending
 * otherwise would cost the demo its only free direction lever for no gain in
 * honesty.
 *
 * Declared in the plan before it became convenient, which is the difference
 * between a design decision and an excuse.
 */

#ifndef HYST_PALETTE_H
#define HYST_PALETTE_H

#include <stdint.h>

/* A ramp is four control points in RGB, spread over the 0..255 value axis.
 * The field's value maps to colour through this, so the ramp's shape is doing
 * the job a shader would do elsewhere -- where the contrast sits, where it
 * goes hot, whether the low end is black or a cold blue. */
typedef struct {
    uint8_t rgb[4][3];
} pal_ramp_t;

/* Cross-fade between two named ramps and upload. w is 0..255. */
void palette_apply(int ramp_a, int ramp_b, uint8_t w);

/* Named ramps, in arc order. */
enum {
    PAL_COLD = 0,   /* near-black to steel blue -- the opening, one lit cell */
    PAL_EMBER,      /* deep red through orange -- growth */
    PAL_BLOOM,      /* hot, high contrast, white shoulder -- the peak */
    PAL_ASH,        /* desaturating, compressing -- the decay */
    PAL_BLACK,      /* all zero -- the endcard fades out through the readout */
    PAL_COUNT
};

#endif
