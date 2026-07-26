/* HYSTERESIS — reaction-diffusion, as a SOURCE inside the feedback loop.
 *
 * Not a second effect to cut to. The demo has one picture and one system; this
 * layer joins the forcing vector. Its output is upsampled and added into the
 * field, so what the viewer sees is still the feedback loop -- now being fed
 * slow, large-scale organisation instead of only scheduled blobs.
 *
 * That is the gap it fills. The field's own dynamics produce texture very well
 * and composition not at all, because nothing in the loop operates at a scale
 * larger than the blur kernel. Gray-Scott does exactly that and nothing else.
 *
 * WHY IT FITS NOW. PLANNING.md section 4 budgeted this at 160x120 on the
 * grounds that four full-resolution pages would be 307 KB. That was arithmetic
 * on a guess. Measured on the device at step 3: 33% of the frame spare (11.0 ms
 * of 16.67), core 1 idle apart from the scanline copy, and ~200 KB of SRAM
 * free. Two chemicals at 160x120 in uint8 is 76.8 KB and about 15% of a core.
 *
 * Half resolution is still the right call, and now for a better reason than
 * memory: Gray-Scott patterns are inherently low-frequency, so the upsample
 * costs nothing visually, and the demo wants this layer to supply the LARGE
 * scale specifically.
 */

#ifndef HYST_RD_H
#define HYST_RD_H

#include <stdint.h>

#define RD_W 160
#define RD_H 120

typedef struct {
    uint8_t  du, dv;     /* diffusion rates, Q8 */
    /* F and k are the two numbers that decide which Gray-Scott regime you get
     * (spots, stripes, labyrinth, mitosis), and they are SMALL -- around 0.035
     * and 0.065. Q12 is the minimum that resolves them; and they must be 16-bit
     * because k*4096 is 266, which does not fit in a uint8. */
    uint16_t feed;       /* F, Q12 */
    uint16_t kill;       /* k, Q12 */
} rd_params_t;

/* Seed the chemistry: v full of the substrate, a few nuclei of u. */
void rd_reset(void);

/* One Gray-Scott step. */
void rd_step(const rd_params_t *p);

/* Half-resolution uint8 view of v, for field_step's threshold bias. This is
 * the integration that works: injecting RD into the VALUE fails in both signs
 * because the field's blur and react grind a gentle input away every frame,
 * whereas biasing the react threshold changes the DYNAMICS -- where RD is high
 * the field cannot sustain at all, so the channel persists. */
const uint8_t *rd_v8(void);

/* Copy the v chemical out as 0..255, RD_W x RD_H. */
void rd_read_v(uint8_t *out);

/* Apply the RD pattern to a FIELD_W x FIELD_H field, bilinearly upsampled.
 *
 * amp is SIGNED, and the sign is the whole design decision:
 *
 *   amp > 0   additive -- RD feeds the field. Tried first, and wrong: it adds
 *             brightness faster than it adds composition, the loop grinds the
 *             organic shapes into the same block texture within a second or
 *             two, and the little remaining black in the frame disappears.
 *   amp < 0   INHIBITORY -- RD suppresses the field, carving its labyrinth
 *             through it as dark channels. This is the one that works, and for
 *             a reason worth stating: the demo's real weakness at this point is
 *             that everything after 40 s is full-frame with no breathing room.
 *             An inhibitor removes energy and creates negative space, and
 *             negative space is what the composition was short of.
 *
 * Saturating either way. */
void rd_inject(uint8_t *dst, int16_t amp);

/* Disturb the chemistry where the music hits, so the slow layer responds to
 * the same events the fast one does. */
void rd_poke(int x, int y, int radius);

#endif
