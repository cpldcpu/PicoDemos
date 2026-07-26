/* HYSTERESIS — the field.
 *
 * One byte per cell. That byte is simultaneously the simulation state and the
 * displayed pixel: MODE_320 is 8bpp palette-indexed, so there is no conversion
 * step between "what the system is" and "what you see". The palette does all
 * colour, which is why the whole cycle budget belongs to the simulation.
 *
 * The field is not stored here. It IS the VGA double buffer — vga_320_present()
 * swaps back and front, which is exactly the ping-pong a feedback system needs,
 * at zero extra memory and zero copies. field_step() reads the front buffer
 * (frame n-1, read-only while it is scanned out) and writes the back buffer.
 *
 * INTEGER ONLY, deliberately. Not for speed — the RP2350 has an FPU and
 * SUSTAIN used it happily. For determinism: referee test 1 diffs a host render
 * against a device render sample-for-sample and pixel-for-pixel, and float
 * under -ffast-math is not guaranteed to agree across two compilers targeting
 * two architectures. An integer pipeline agrees by construction.
 *
 * Note what is NOT in this interface: a time argument. Sub-rule 1 of the rule
 * ("the only inputs to frame n are frame n-1 and a forcing vector") is enforced
 * structurally rather than tested. field_step cannot depend on t because it is
 * not given t. A test you cannot fail beats a test that passes.
 */

#ifndef HYST_FIELD_H
#define HYST_FIELD_H

#include <stdint.h>

#define FIELD_W 320
#define FIELD_H 240

/* Advection is computed per BLOCK, not per pixel — one source coordinate for
 * each BLOCK x BLOCK tile, after the Amiga blitter-feedback technique. Two
 * reasons, and the second matters more:
 *
 *   1. Cost. The inner loop becomes a fixed-offset run instead of a per-pixel
 *      affine transform.
 *   2. The block quantisation error IS the fractal structure. Adjacent blocks
 *      read overlapping (zoom in) or gapped (zoom out) source regions, and the
 *      seams accumulate over frames into self-similar detail. A more accurate
 *      transform produces a LESS interesting picture, which is a rare and
 *      welcome inversion of the usual trade.
 */
#ifndef FIELD_BLOCK
#define FIELD_BLOCK 16
#endif

/* The forcing vector: everything the arc is allowed to say to the field.
 * Deliberately small. If the arc could reach past this struct it would be
 * animating the picture instead of forcing the system. */
typedef struct {
    /* Affine source transform, 16.16 fixed point.
     * zoom > 65536 magnifies (perturbations GROW -- the regime with memory).
     * zoom < 65536 minifies (nested recursion, but it CONTRACTS and forgets).
     * See PLANNING.md section 8: a purely contractive system has no memory and
     * the whole premise fails, so zoom must not live below 1.0 for long. */
    int32_t  zoom;
    int32_t  angle;      /* 0..65535 = full turn */
    int32_t  cx, cy;     /* transform centre, 16.16, in field coords */
    int32_t  drift_x;    /* constant translation per step, 16.16 */
    int32_t  drift_y;

    /* ---- the rest of the flow field ------------------------------------
     *
     * Azure: "it is very polar". Correct, and structural rather than a
     * tuning miss — a similarity transform (scale + rotate about one fixed
     * centre) can only ever produce spirals and radial bursts. Every frame
     * looked like the same motion because it WAS the same motion.
     *
     * The fix is nearly free. The source coordinate is computed once per
     * 16x16 block, so about 300 times a frame rather than 76,800; a flow
     * field costing forty operations per block is 12k operations a frame,
     * which is nothing. The inner loop is untouched and still a rigid copy.
     * All of this is per-block arithmetic that never reaches the hot path. */

    /* Affine shear. Breaks the rotational symmetry the similarity transform
     * enforces: diagonal sliding and stretching instead of pure spin. */
    int32_t  shear_x;    /* 16.16, applied as sx += shear_x * dy */
    int32_t  shear_y;

    /* Point vortices. THE main answer to "more diversity in motion": three
     * independent swirl centres, each with tangential velocity falling off
     * as 1/r, summed. Two counter-rotating vortices produce a shear line
     * between them; three produce something that never repeats. The arc
     * moves them, so the composition stops being pinned to the middle. */
    struct {
        int16_t x, y;        /* centre, in pixels */
        int16_t strength;    /* v * r0; signed, so vortices can counter-rotate */
    } vortex[3];

    /* Banded shear — a transverse wave along y. Purely linear motion, the
     * one thing no arrangement of vortices can give you, and the cheapest
     * possible way to get a non-circular gesture on screen. */
    int16_t  band_amp;   /* pixels */
    uint16_t band_freq;  /* turns per screen height */
    uint16_t band_phase;

    /* Value pipeline. */
    uint16_t gain;       /* 8.8 -- 256 is unity. >256 blooms, <256 decays. */
    /* The 3x3 convolution, as FOUR symmetric coefficients rather than nine
     * free ones (Q8, 256 = unity):
     *
     *     corner  edge_v  corner
     *     edge_h  centre  edge_h
     *     corner  edge_v  corner
     *
     * A general 9-tap kernel was measured on the device at 88.4 cycles/cell
     * against a 65 budget -- 42.6 fps, which breaks the demo's frame-time
     * contract. Every kernel the arc wants is symmetric anyway, and separating
     * edge_h from edge_v keeps directional blur available, so four multiplies
     * buy everything nine did.
     *
     * Sums near 256 keep unity gain, and kernels that do NOT sum to 256 are the
     * interesting ones: centre-positive with a negative ring is a discrete
     * activator-inhibitor, which is the one-field route to Turing structure that
     * importing Gray-Scott could not deliver. */
    int16_t  k_centre, k_edge_h, k_edge_v, k_corner;

    /* How much of the convolved value to take, against the raw one. Keeps the
     * old blur knob's meaning: 0 = untouched, 255 = fully convolved. */
    uint8_t  blur;

    /* The react curve, as a threshold pair rather than a fixed shape.
     *
     * This is an EXCITABLE medium, not a sharpener. The first version used an
     * S-curve about mid grey, which pushes every dim value toward zero; since
     * the field starts almost entirely dark, that killed the seed in five
     * frames. The system needs a stable fixed point ABOVE zero for anything to
     * propagate into.
     *
     * Values below react_lo decay to nothing; values above react_hi saturate;
     * between them the curve is steep, so a cell that receives a little energy
     * from its neighbours is pulled up rather than down. Blur spreads,
     * react re-sharpens, and the balance between the two is the whole
     * behaviour of the demo. */
    uint8_t  react_lo;
    uint8_t  react_hi;

    /* Self-limiting, 0..255. This is the second thing the sweep taught me.
     *
     * A monotone threshold plus diffusion is a SWITCH, not a medium: every
     * parameter set either floods to 255 or dies to 0, because anything above
     * the threshold climbs and drags its neighbours up with it. Nothing stops
     * growth. Reaction-diffusion solves this with a second chemical acting as
     * an inhibitor, which costs a second field and a second pass.
     *
     * The one-field version is to bend the top of the curve back down, so a
     * cell that gets too bright falls instead of saturating. It is its own
     * inhibitor. 0 = monotone (switch), 255 = full band-pass hump. */
    uint8_t  react_fold;

    /* Output scale on the react curve, 0..255. THE KILL SWITCH, and the thing
     * the arc had no way to express before.
     *
     * The react curve is the field's energy SOURCE -- it maps mid values up
     * toward the rails, which is what lets structure persist against diffusion.
     * So nothing else can end the demo. Gain below unity loses to it (249/256
     * per frame is nothing against a curve that regenerates), and raising
     * react_lo actually makes the picture BRIGHTER, because a narrower
     * excitable band puts the surviving cells nearer the hump's peak -- 
     * measured, mean rose to 151 when the intent was to extinguish.
     *
     * Scaling the curve's output is the direct, continuous way to withdraw the
     * source. At 0 the field has nothing regenerating it and decays to black. */
    uint8_t  react_out;

    /* Persistence, 0..255 — how much of the PREVIOUS value at this same screen
     * position is retained. A leaky integrator, i.e. a time constant.
     *
     * Azure caught the demo flickering while watching it run, which no still
     * can show. Measuring frame-to-frame delta confirmed it: every cell was
     * moving 50-90 grey levels EVERY FRAME. The react curve is a stiff map and
     * the system was taking a full step of it per frame, with nothing to slow
     * the fastest mode. A non-monotone curve then turns that into outright
     * period-2 alternation.
     *
     * Damping the fastest mode leaves the slow dynamics — and the chaos that
     * carries the memory — intact. It is also, pleasingly, exactly phosphor
     * persistence: the field remembering its own previous value, in a demo
     * named after systems that depend on their history.
     *
     * Note this reads the previous frame UNADVECTED, so it also lays trails
     * along the flow, which is the other half of the video-feedback look. */
    /* Hold the field STILL: the source cell is the destination cell, with no
     * affine transform and no bilinear fetch. The rest of the value pipeline
     * (gain, dither, react, persistence) is unchanged.
     *
     * This is not a shortcut, it is the only way to hold an image exactly. The
     * affine path cannot express the identity: inv * cos(0) uses a Q15 sine
     * whose peak is 32767, so c comes out 65534 instead of 65536 and every block
     * samples about 1/256 of a pixel off true. No integer zoom recovers it --
     * c steps from 65535 to 65537 and skips the value wanted. Left running, that
     * residual drifts the endcard sideways for six seconds and the block-origin
     * clamp smears its right-hand column into the screen edge, which is visible
     * and looks like a bug because it is one.
     *
     * It is also markedly CHEAPER than the normal path: one tap per cell instead
     * of four, no per-block setup. Azure suggested overlaying a band of pixels
     * directly for the same saving; this gets it without any pixel becoming a
     * function of t, because the operator is still being iterated -- it has just
     * been handed parameters for which the previous frame is a fixed point. */
    uint8_t  still;

    uint8_t  persist;

    /* Spatial bias on the react threshold, from the reaction-diffusion layer.
     * Half resolution (FIELD_W/2 x FIELD_H/2), nearest-sampled -- the bias
     * changes the dynamics smoothly enough that interpolating it would be
     * wasted work in the hot loop. NULL disables it at no cost. */
    const uint8_t *bias;
    uint8_t  bias_amt;
} field_params_t;

/* The shared integer sine, exposed so the arc can move the flow field using
 * exactly the same table the field does. Two sine implementations would be two
 * things to keep bit-identical between host and device. a: 0..65535 = one
 * turn; returns Q15. */
int32_t field_isin(int32_t a);

/* Build the react LUTs once. */
void field_init(void);

/* One step. dst and src must not alias. This is the whole per-frame pipeline:
 * gather at the advected source, blur, gain, dither, react, store. One pass,
 * ~5 reads and 1 write per cell. */
void field_step(uint8_t *dst, const uint8_t *src, const field_params_t *p);

/* Seed: a single lit cell on an empty field. The demo opens on this, and the
 * one-pixel divergence experiment (referee test 2) is the same operation. */
void field_clear(uint8_t *f);
void field_poke(uint8_t *f, int x, int y, uint8_t v);

/* Inject a 1-bit stencil as energy at (x, y). Additive and saturating: the
 * stencil adds to whatever the field already holds rather than replacing it,
 * so an injection is a disturbance rather than a paste. */
void field_inject_stencil(uint8_t *f, const uint8_t *bits, int sw, int sh,
                          int x, int y, uint8_t amp);

/* The same, restricted to stencil rows [j0, j1).
 *
 * SPLITTING A TITLE CARD ACROSS FRAMES IS A BUDGET REQUIREMENT, not an effect.
 * Measured on the device: one full injection costs about 554 us and the frame
 * has roughly 410 us spare, so there is no version of this that fits in a single
 * frame -- the endcard reached 17,045 us against a 16,667 us budget even after
 * the inner loop was rewritten. And an overrun here is not a dropped frame: the
 * simulation advances once per frame from its own counter, so the picture falls
 * behind the music and never catches up.
 *
 * A card spread over four frames is 67 ms, which reads as instant. */
void field_inject_stencil_rows(uint8_t *f, const uint8_t *bits, int sw, int sh,
                               int x, int y, uint8_t amp, int j0, int j1);

/* Radial impulse -- the ordinary forcing on a musical hit. */
void field_inject_blob(uint8_t *f, int x, int y, int radius, uint8_t amp);

/* FNV-1a over the whole field.
 *
 * This is referee test 1 extended across the host/device boundary, which
 * PLANNING.md promised and which energy alone cannot deliver: two runs can
 * agree on a sum to four significant figures and still be different pictures.
 * A hash printed by the firmware and compared against the host at the SAME
 * frame index answers the question exactly. */
uint32_t field_hash(const uint8_t *f);

/* Total energy, for telemetry and for the equilibrium test at the end. */
uint32_t field_energy(const uint8_t *f);

/* The nonlinearity is a 256-entry table, so an arbitrary curve costs one byte
 * load. That is what makes the pipeline fit the budget, and it is also where
 * genuine path-dependence lives -- a contractive linear map would forget its
 * initial condition and the demo would have no memory at all.
 *
 * react_lo == react_hi == 0 selects the identity, which exists so the referee
 * can demonstrate the contractive case FAILING the divergence test. A referee
 * with no known-bad input is not a referee. */

#endif
