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

    /* Value pipeline. */
    uint16_t gain;       /* 8.8 -- 256 is unity. >256 blooms, <256 decays. */
    uint8_t  blur;       /* 0..255 mix toward the 5-tap neighbourhood mean */

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
} field_params_t;

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

/* Radial impulse -- the ordinary forcing on a musical hit. */
void field_inject_blob(uint8_t *f, int x, int y, int radius, uint8_t amp);

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
