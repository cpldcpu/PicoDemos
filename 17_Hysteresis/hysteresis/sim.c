/* HYSTERESIS — the driver. See sim.h for why the clock is the frame index.
 *
 * BUILD ORDER STEP 1. This arc is provisional: enough forcing to prove the
 * feedback loop is alive, stable, and path-dependent. The real forcing
 * schedule is step 8, and it will be the sequencer rather than this table.
 */

#include "sim.h"
#include "field.h"
#include "score.h"
#include "stencil.h"
#include "palette.h"
#include "rd.h"
#include "vga.h"
#include "scene.h"

#include <stdio.h>
#include <stddef.h>

/* See the note above arc_rd_amp: reaction-diffusion is built, measured, and
 * currently off. ONE switch for both builds -- host and device must compile the
 * same code or the bit-identical guarantee is void. */
#define HYST_RD_ENABLE 0

#define FPS            60
#define SEC(s)         ((uint32_t)((s) * FPS))
#define TOTAL_FRAMES   SEC(210)          /* 3:30 */

static uint32_t g_frame;

/* Probe mode: pin the parameters so the dynamics can be swept without the arc
 * moving underneath the measurement. Tuning a feedback system by eye is how
 * you end up with a demo that dies at 2:40 on hardware only. */
static int            g_fixed_on;
static field_params_t g_fixed;

/* Debug: show the reaction-diffusion layer on its own, so its regime can be
 * judged without the feedback loop chewing on it. */
static int g_rd_only;
void sim_set_rd_only(int on) { g_rd_only = on; }

/* Gray-Scott regime selection is entirely a matter of F and k, and the usual
 * published pairs assume a laplacian normalised as (weighted mean - centre).
 * This one is (sum of 4 - 4*centre), which is four times larger, so the
 * diffusion coefficients do not transfer directly and the whole pair has to be
 * swept rather than copied. */
/* Swept, not copied from a paper: k=246 (0.060) is the labyrinth regime here.
 * k=230 floods to near-uniform cover (mean 84, sdev 23 -- no pattern), k>=254
 * dies out entirely. The window is narrow and these are its measured edges. */
static rd_params_t g_rdp = { .du = 56, .dv = 28, .feed = 225, .kill = 246 };
void sim_set_rd_params(const rd_params_t *p) { g_rdp = *p; }

/* Force a constant RD amplitude, for sweeping. INT16_MIN means "use the arc". */
static int g_rd_amp_forced = 1;
static int16_t g_rd_amp_val;
void sim_set_rd_amp(int16_t a) { g_rd_amp_val = a; g_rd_amp_forced = 0; }

/* Pin the kernel blend (0 = soft blur, 255 = full activator-inhibitor). -1
 * means follow the arc. Sweeping only. */
static int g_kern_forced = -1;
void sim_set_kern(int w) { g_kern_forced = w; }

void sim_set_fixed(const field_params_t *p)
{
    g_fixed = *p;
    g_fixed_on = 1;
}

static void arc_params(uint32_t f, field_params_t *p);

/* See sim.h. The arc at 90 s: past the percolation transition, fully developed,
 * and squarely inside the regime the parameter map says supports both structure
 * and memory. Any field added to field_params_t is populated here for free,
 * which is the whole point. */
void sim_default_params(field_params_t *p)
{
    arc_params(SEC(90), p);
}

/* ------------------------------------------------------------------ arc --- */

/* Smoothstep between two frame marks, returns 0..65536. Used for every
 * parameter ramp so nothing in the forcing schedule steps discontinuously —
 * the system would show the discontinuity for a long time afterwards. */
static int32_t ramp(uint32_t f, uint32_t a, uint32_t b)
{
    if (f <= a) return 0;
    if (f >= b) return 65536;
    int64_t t = ((int64_t)(f - a) << 16) / (b - a);
    return (int32_t)((t * t * (3 * 65536 - 2 * t)) >> 32);
}

static inline int32_t mix(int32_t a, int32_t b, int32_t w)
{
    return a + (int32_t)(((int64_t)(b - a) * w) >> 16);
}

/* ------------------------------------------------------------- the endcard --
 *
 * QUIESCENCE. From 204 s the field has collapsed to black, and from there the
 * operator is reduced to a hold: the transport is switched off entirely
 * (field.h, `still`), the convolution is mixed out, and the react curve becomes
 * a hard threshold. field_step then does exactly "read the same cell, threshold
 * it, store" -- one tap per cell, which is cheaper than a normal frame rather
 * than more expensive.
 *
 * This is how the title can be on screen without breaking the demo's one rule.
 * The stencils are INJECTED as forcing (arc_inject), the same mechanism as a
 * musical impact -- nothing is ever blitted over the picture, so no pixel
 * becomes a function of t and the palette stays the only declared exemption.
 * The credits are visible because the field was pushed into that shape and the
 * medium has nothing left that would erase them.
 *
 * A held image IS a fixed point of the operator, not an exception to it. Which
 * makes the ending the demo's own name taken literally, and I did not plan this:
 * a hysteretic system holds its state after the driving force is removed. The
 * credits persist because the field remembers them.
 *
 * The last two seconds drop the gain just below unity, so the fade to black is
 * the same multiplicative decay the whole demo has been running on.
 */
#ifndef HYST_QGAIN
#define HYST_QGAIN 260          /* swept below; see the note on gain */
#endif

static void arc_quiescent(uint32_t f, field_params_t *p)
{
    if (f < SEC(204)) return;

    /* Two halves to the ending.
     *
     * 204.0 - 204.5 s   STILL. The transport is switched off entirely and the
     *                   react curve is a hard threshold, which sweeps out
     *                   everything the collapse left behind (the field arrives
     *                   carrying dither values of 1 to 4 everywhere) and leaves
     *                   a genuinely black screen for the card to land on.
     *
     * 204.5 - 210 s     DUST. The transport comes back with a slow drift and no
     *                   diffusion, and the react curve goes to the exact
     *                   identity with the gain a little under one. The endcard
     *                   is re-injected every frame at full amplitude, so its
     *                   glyphs are pinned hard while anything that spills off
     *                   their edges is carried away and fades -- Azure's dust.
     *                   At 208 s the re-injection stops and the letters
     *                   themselves become dust and blow away.
     *
     * Azure's other suggestion was to blit the font into the framebuffer after
     * the update. It already IS after the update -- arc_inject runs when
     * field_step has finished -- and an additive saturating injection at
     * amplitude 255 is bit-for-bit a blit for every pixel it touches. So the
     * crisp font comes for free without a single pixel becoming a function of t,
     * which is the one thing this demo has claimed throughout. */
    const int dust = (f >= SEC(204) + 30);

    /* TRANSLATION, not zoom, and this is the whole reason the endcard stopped
     * falling apart into squares.
     *
     * Advection here is per 16x16 block: one source coordinate is computed at
     * the block corner and all 256 cells share it (field.h). Under a zoom,
     * adjacent blocks therefore step through the source at 16/zoom while
     * stepping through the destination at 16, leaving about 0.15 px of
     * discontinuity at every seam -- and because the output is fed straight back
     * in, that error compounds every frame. After a second the seams are pixels
     * wide. For the body of the demo that IS the effect; it is the Amiga
     * blitter-feedback structure the whole thing is built on. Applied to
     * hard-edged text it just looks broken, which is exactly what it looked
     * like.
     *
     * A uniform drift has no such term: every block gets the identical offset,
     * so the field translates rigidly and there are no seams to accumulate. The
     * direction turns slowly, so the dust streams off the letters and the stream
     * wanders over the six seconds. */
    p->still = !dust;
    p->zoom = 65536;
    p->angle = 0;
    if (dust) {
        /* About 1.2 px per frame, on a direction that turns once every ten
         * seconds. In Q16 pixels, so 65536 is one cell. */
        const int32_t a = (int32_t)((f * 110) & 65535);
        p->drift_x = (field_isin(a) * 78000) >> 15;
        p->drift_y = (field_isin(a + 16384) * 78000) >> 15;
    } else {
        p->drift_x = p->drift_y = 0;
    }
    for (int i = 0; i < 3; i++) p->vortex[i].strength = 0;
    p->shear_x = p->shear_y = 0;
    p->band_amp = 0;

    p->blur = 0;                        /* kernel mixed out to identity */

    /* A HARD THRESHOLD, not the identity, and this took three attempts.
     *
     * The identity LUT (lo == hi == 0) holds an image but cannot keep it clean.
     * The transport is not reachably exact -- icos peaks at 32767, so cos(0)
     * gives c = 65534 and every block samples about 1/256 of a pixel off -- and
     * that slow blur drained the endcard by 17% in three and a half seconds.
     * Compensating with a gain above unity fixed the letters and then amplified
     * the ORDERED DITHER: the field arrives at 204 s carrying values of 1 to 4
     * everywhere, which is field.c's anti-death dither working exactly as
     * designed, and 260/256 per frame turns 4 into 139 over the endcard's life.
     * Visible dirt, worst in the corners.
     *
     * lo = 200, hi = 201 removes both problems at once with a wide margin: an
     * endcard cell at 255 stays saturated, a nibbled edge anywhere above 200 is
     * put straight back, and everything the collapse left behind is below 200 and
     * goes to zero on the first frame. The image holds itself sharp and the
     * screen around it is genuinely black rather than nearly black.
     *
     * (Asking for lo = 0, hi = 255 to get "a linear ramp" is not the identity
     * either -- that is the smoothstep path, and it has its own fixed point; the
     * endcard settled at 152 and lowering the gain to fade it made the mean go
     * UP, because the fixed point moves too.) */
    if (dust) {
        /* The exact identity branch of build_react, so spilled energy keeps its
         * value and only the gain fades it. A threshold here would snap every
         * loose pixel to 0 or 255 and there would be no dust at all. */
        p->react_lo = 0;
        p->react_hi = 0;
    } else {
        p->react_lo = 200;
        p->react_hi = 201;
    }
    p->react_fold = 0;
    p->react_out = 255;
    p->persist = 0;                     /* rate = 256: take the new value whole */

    /* HOLD ABOVE UNITY, and the reason is worth writing down: the identity is
     * not reachable through this transport. inv * cos(0) uses icos, whose peak
     * is 32767 and not 32768, so c comes out 65534 rather than 65536 and every
     * block samples about 1/256 of a pixel off true. That is a small blur
     * applied every frame, and it drained a nominally exact hold by 17% over
     * three and a half seconds. No integer zoom fixes it -- c steps from 65535
     * to 65537 and skips the value wanted -- and widening the sine table would
     * move every hash in the demo to buy six seconds of still image.
     *
     * So instead of resisting the leak, saturate against it. Above unity a lit
     * cell that the blur nibbled is pushed back up and clamped by the identity
     * LUT at 255, while a black cell has nothing to scale, so the image holds
     * itself sharp. Which is the same self-correcting behaviour the demo is
     * named after, arrived at because the arithmetic left no alternative. */
    /* TWO decay rates, and they have to be different.
     *
     * While the card is being re-injected every frame, the decay must be fast
     * or the advected copies pile up: at 246 (a seventeen-frame half-life) the
     * background filled with dense grey smear instead of dust. 200 is under
     * three frames, which keeps the halo tight around the glyphs.
     *
     * After the release at 208 s nothing is being added any more, so the same
     * fast decay simply deletes the letters -- they were gone 0.2 s after being
     * let go, which is not blowing away, it is switching off. 248 was still too
     * quick, and not because of the arithmetic: PAL_ASH is a dark ramp, so a
     * cell at 57% of scale is already almost black on screen. The value has to
     * stay high and let the PALETTE take it down. 253 barely decays at all --
     * the letters come apart and drift, and the readout fades them out. */
    p->gain = dust ? (f >= SEC(209) + 18 ? 253 : 200) : 256;
}

static void arc_params(uint32_t f, field_params_t *p)
{
    /* Magnification. Held ABOVE unity for the whole demo, which is a design
     * constraint rather than a taste: a contractive map converges to an
     * attractor independent of its initial condition, i.e. it forgets, and the
     * demo's whole claim is memory (PLANNING.md section 8). Zoom out is the
     * prettier look and it is off the table. */
    /* Both well under +1400. Measured ceiling, not caution: at +1500 and above
     * the field collapses to a flat colour within seconds, because magnifying
     * shrinks the sampled domain until the whole screen is a blow-up of the
     * centre and the react curve has nothing left to work on. */
    const int32_t z0 = 65536 + 380;                 /* ~1.006 — barely moving */
    const int32_t z1 = 65536 + 1150;                /* ~1.018 — the peak */
    p->zoom = mix(z0, z1, ramp(f, SEC(12), SEC(126)));
    p->zoom = mix(p->zoom, 65536 + 240, ramp(f, SEC(150), SEC(205)));

    /* Rotation: a slow constant drift, reversing nowhere. A tilt turns the
     * radial flow into a spiral, which is what stops a pure zoom from looking
     * like a corridor. */
    p->angle = (int32_t)((f * 7) & 65535);

    /* Explicit: sim_step declares field_params_t on the stack, so anything
     * arc_params does not assign is whatever was there before. */
    p->still = 0;

    p->cx = (FIELD_W / 2) << 16;
    p->cy = (FIELD_H / 2) << 16;

    /* A slow wander of the transform centre. Without it the composition is
     * pinned to the middle of the screen for three and a half minutes. */
    p->drift_x = 0;
    p->drift_y = 0;

    /* --- the flow field, i.e. the answer to "it is very polar" -----------
     *
     * Three vortices on orbits whose rates share no common factor, so the
     * configuration never repeats inside the demo's length. Signs alternate:
     * counter-rotating neighbours produce a shear line between them, which
     * is the gesture a single centre can never make. */
    /* Flow in EARLY. This is what breaks the phase-lock: while the field is
     * static, every region oscillates together and the mean pulses between 1
     * and 18 with nothing filling; once the vortices are strong enough to carry
     * material away from where it grew, neighbouring regions fall out of step
     * and the frame fills progressively. The fill used to land at 34 s, which
     * is exactly where this ramp reached about a third of full strength -- so
     * the transport, not the impacts or the react curve, was the gate. */
    const int32_t vs = ramp(f, SEC(3), SEC(26));
    static const struct { int16_t r, rate, ph, str; } orb[3] = {
        {  74,  13,     0,  210 },
        {  96,  -8, 21000, -170 },
        {  52,  21, 43000,  150 },
    };
    for (int i = 0; i < 3; i++) {
        const int32_t a = (int32_t)(f * orb[i].rate + orb[i].ph) & 65535;
        p->vortex[i].x = (int16_t)(FIELD_W / 2 + ((orb[i].r * field_isin(a)) >> 15));
        p->vortex[i].y = (int16_t)(FIELD_H / 2 + ((orb[i].r * field_isin(a + 16384)) >> 16));
        p->vortex[i].strength = (int16_t)((orb[i].str * vs) >> 16);
    }

    /* Shear breathes in and out on a slow cycle of its own, so the frame is
     * sometimes spinning and sometimes sliding. */
    p->shear_x = (int32_t)((field_isin((int32_t)(f * 5) & 65535) * 90) >> 15);
    p->shear_y = (int32_t)((field_isin((int32_t)(f * 3 + 30000) & 65535) * 60) >> 15);

    /* The transverse band comes up hardest in the middle third — pure linear
     * motion against all that rotation. */
    p->band_amp   = (int16_t)(mix(0, 5, ramp(f, SEC(55), SEC(105)))
                            - mix(0, 5, ramp(f, SEC(140), SEC(185))));
    p->band_freq  = 640;
    p->band_phase = (uint16_t)((f * 300) & 65535);

    /* Blur vs sharpen is the balance that decides whether structure survives.
     * Blur alone flattens the field; the LUT alone drives it to two values. */
    /* The kernel. Azure's suggestion, and the highest-leverage knob added since
     * persistence: it sets the spatial scale the field organises at, which the
     * RD experiment showed dominates everything else.
     *
     * The arc walks from a plain isotropic blur (soft, diffuse -- right for the
     * quiet opening) toward a centre-positive / ring-negative kernel, which is a
     * discrete activator-inhibitor and makes the field grow its own Turing-like
     * structure instead of importing it. Interpolated per frame so the change is
     * continuous; a kernel that jumped would show for a long time afterwards. */
    {
        /* centre, edge_h, edge_v, corner — each sums to 256 with its
         * multiplicities (centre + 2*eh + 2*ev + 4*corner). */
        static const int16_t soft[4] = {  48,  32,  32,  20 };
        static const int16_t edge[4] = { 472, -20, -20, -34 };

        /* CAPPED at ~148/255, measured. A 3x3 ring-negative kernel amplifies
         * hardest at Nyquist, so at full strength it grows the checkerboard
         * mode and the second half of the demo degenerates into fine noise.
         * The sweep put spatial structure at its peak between 110 and 160
         * (sdev 55.4) while frame-to-frame churn and the flicker ratio both
         * worsen sharply above 210 -- the degeneration shows up in the temporal
         * numbers before it is visible. Peaking at a LOWER spatial frequency
         * needs a 5x5 kernel, which is the next thing to try, not this. */
        /* Starts at ~70/255, not 0. THE DEAD ZONE FIX: with a pure blur
         * kernel the early field diffuses faster than the react band can hold
         * it, so the seed grew to mean 23.5 by 6 s, collapsed to 1.6 by 16 s,
         * and only recovered at 33 s when the sharpening ramp finally arrived
         * -- twenty seconds of near-black followed by a step change. Carrying
         * some ring-negative weight from the first frame lets the opening blob
         * persist and spread steadily instead of dying and restarting. */
        int32_t w = mix(18000, 38000, ramp(f, SEC(6), SEC(105)));
        w = mix(w, 23000, ramp(f, SEC(158), SEC(200)));   /* softer outro */
        if (g_kern_forced >= 0) w = g_kern_forced * 257;
        p->k_centre = (int16_t)mix(soft[0], edge[0], w);
        p->k_edge_h = (int16_t)mix(soft[1], edge[1], w);
        p->k_edge_v = (int16_t)mix(soft[2], edge[2], w);
        p->k_corner = (int16_t)mix(soft[3], edge[3], w);
    }

    /* Diffusion sets how fast the excited region SPREADS, so it is the pacing
     * control for the opening: low early means the first blob grows into the
     * frame over twenty seconds instead of filling it in four. */
    p->blur = (uint8_t)mix(146, 170, ramp(f, SEC(4), SEC(40)));

    /* Gain rides just under unity for most of the run and crosses it at the
     * peak. Above unity the field blooms toward saturation, which is the
     * intended read at the climax and death everywhere else. */
    /* Held inside the swept envelope. The first arc ramped this to 254 at the
     * start and 248 at the end -- both outside anything that had been measured,
     * and the middle of the demo duly fell apart. */
    /* Gain crosses below unity for the decay -- outside the swept envelope on
     * purpose, because the envelope was mapped for a SUSTAINING system and the
     * ending needs the opposite. */
    /* THE ENDING, second attempt: pull the gain down instead. Gain scales every
     * cell multiplicatively, so relative structure survives while the whole
     * field dims, and cells drop below react_lo patchily rather than all at
     * once -- the pattern erodes from its dimmest parts inward instead of
     * dissolving into an average. */
    p->gain = 258;

    /* The excitable band. Narrow and low: cells that receive a little energy
     * from a neighbour get pulled UP into the band rather than decaying, which
     * is what lets a single lit cell colonise an empty field. */
    /* THE ENDING. Raising the excitation threshold is what actually makes the
     * system run down: below react_lo a cell decays to nothing, so lifting it
     * withdraws the field's ability to sustain itself anywhere. Nothing in the
     * arc did that before, which is why the mean sat at ~137 from 36 s all the
     * way to the last frame and the demo simply stopped instead of ending.
     *
     * This is the honest ending for a system with memory: forcing stops, the
     * medium stops being excitable, and the field decays to equilibrium. */
    /* Permissive early so a small blob is SUPERCRITICAL from the first impact
     * and grows visibly, rather than sitting sub-critical until it happens to
     * cross a nucleation threshold and then flooding the screen in four
     * seconds. Growth rate is controlled by diffusion instead (blur, below). */
    p->react_lo = (uint8_t)mix(7, 12, ramp(f, SEC(6), SEC(38)));

    /* THE ENDING, and the only one that erodes rather than dissolves: starve
     * the excitable band by lifting its floor past what the field can reach.
     * Cells below react_lo decay to nothing, so the dimmest regions die first
     * and the brightest hold on longest -- the pattern is eaten from its edges
     * inward instead of averaging into grey or flooding white. Runs to 236,
     * well above anything the field sustains, so the end state is black. */
    p->react_lo = (uint8_t)mix(p->react_lo, 62, ramp(f, SEC(150), SEC(198)));
    p->react_lo = (uint8_t)mix(p->react_lo, 148, ramp(f, SEC(198), SEC(210)));

    p->react_hi = 180;

    /* The ending. Withdrawing the curve's output is the only thing that
     * actually removes energy from the system -- see field.h. */
    /* Reaches zero at the last frame, not twenty seconds early. The collapse
     * from ~85 to nothing takes about five seconds once the source falls below
     * what the field needs to sustain itself, which is the right shape for a
     * system losing its ability to hold together -- but it has to land ON the
     * end of the demo. */
/* Held at full. Scaling the curve's output turned out to be the WRONG kill
     * switch: it takes the contrast down before the brightness, so the field
     * converged to a flat uniform grey at 195 s -- mean 111 with no structure
     * at all, which the energy figure happily reported as healthy. A flat grey
     * screen is a worse ending than a black one. */
    p->react_out = 255;

    /* SELF-LIMITING IS THE MEMORY KNOB. tools/memory_map.py swept structure
     * against path-dependence and the split was total: every fold=160 regime
     * FORGETS a one-cell perturbation, every fold>=200 regime REMEMBERS it.
     * Above ~230 it remembers but degenerates into noise. So the demo lives in
     * a narrow band, and this parameter is not a look, it is the rule. */
    /* Ramped in, not constant. The dead zone was never a slow start: the early
     * mean OSCILLATED -- 18.1 at 6 s, 1.7 at 11 s, 7.5 at 21 s, 10.1 at 31 s --
     * because fold at full strength makes a growing bright region fold back to
     * dark from the inside. The whole field was acting as one relaxation
     * oscillator, and only became steady around 33 s when it decorrelated
     * spatially. Easing fold in lets the first region grow without eating
     * itself, and full self-limiting arrives once there is enough independent
     * structure for it to shape rather than destroy. Kept at or above the
     * measured memory floor (160) throughout -- see memory_map.py. */
    /* THE FOLD MUST GO FIRST to end the demo, and this took three attempts to
     * see. While the curve is non-monotone every obvious kill knob works
     * backwards: reducing gain pushes cells toward the hump's PEAK and the mean
     * rose from 142 to 177; raising react_lo narrows the band and does the same;
     * scaling the output flattens contrast into uniform grey before it dims
     * anything. Nothing monotone exists while the hump does.
     *
     * Easing fold to near zero leaves a plain smoothstep -- monotone, no
     * regeneration surprises -- and only then do gain and threshold behave the
     * way intuition says. The demo loses its self-limiting inhibitor in the last
     * forty seconds, which is exactly right: that is what "the system stops
     * being able to sustain itself" means. */
    /* Held at 200 to the end. Easing the fold out was the third failed ending:
     * without the self-limiting hump the curve is a monotone threshold, and a
     * monotone threshold plus diffusion is the SWITCH from the very first sweep
     * -- it flooded to a uniform 226 with sdev 0.
     *
     * Between them the three attempts establish that this system has no gentle
     * death. Its only stable states are structured-and-oscillating, flooded
     * uniform, and black; there is no path where structure gradually fades. */
    p->react_fold = 200;

    /* PERSISTENCE IS THE PACING CONTROL, and the discovery of this session.
     *
     * Azure saw the demo flickering; measurement showed every cell moving
     * 50-90 grey levels EVERY frame. Damping toward the previous value at the
     * same position fixed it (frame-to-frame churn down 7x, flicker ratio
     * 0.72 -> 1.89) and -- unexpectedly -- made the memory far STRONGER, with
     * the one-cell perturbation spreading to 95% of the field instead of 22%.
     * A slower mode is a second state variable, and two slow variables are
     * more chaotic than one fast one.
     *
     * So it is free, and it is also the best directorial knob in the demo:
     * high persistence reads as languid, low as agitated. High through the
     * quiet opening, dipping at the peak, high again as it runs down. */
    /* Higher through the opening than it used to be. The early field is one
     * relaxation oscillator and its TROUGHS were the visible problem -- the mean
     * dipped to 0.8 at 13 s, which reads as an empty screen between pulses.
     * Persistence damps the fastest mode, so raising it shallows the trough
     * without touching the transition, which turned out to be a percolation
     * bifurcation and immovable by any of these knobs. */
    /* CAPPED AT 205. Persistence has an upper limit as well as a lower one:
     * pushed to 232 to shallow the opening's oscillation troughs, it damped the
     * field out of its chaotic regime altogether and the referee's
     * path-dependence test went from 99.9% spread to 0.0% -- the demo stopped
     * having memory. The earlier sweep only explored upward to 192 and found
     * memory improving, so this ceiling was invisible until the referee ran on
     * the real arc. Worth remembering that the sweep mapped one direction. */
    p->persist = (uint8_t)mix(205, 150, ramp(f, SEC(14), SEC(128)));
    p->persist = (uint8_t)mix(p->persist, 212, ramp(f, SEC(150), SEC(205)));

    /* LAST, and it must be last: it overrides rather than blends, so anything
     * assigned after it would quietly win. Placed mid-function on the first
     * attempt, where the persistence and react lines below it undid most of what
     * it set, and the endcard held at 151 instead of 255. */
    arc_quiescent(f, p);
}

/* Forcing events: the only way anything enters the picture.
 *
 * THE TABLE LIVES IN score.c, and is the same table the synth strikes its
 * resonators from -- PLANNING.md section 6 argued the sequencer and the forcing
 * schedule should be one object rather than two that have to be kept in
 * agreement, and this is where that is cashed in. There is no alignment step
 * between the music and the picture because there is nothing to align: the
 * event that injects energy here IS the event that is heard.
 *
 * Times are beats at 120 BPM, and score.h fixes a beat at exactly 30 frames, so
 * this is a multiply and not a conversion with a remainder.
 *
 * Radii start at 7, not 3. An excitable medium has a CRITICAL NUCLEUS: a blob
 * too small to sustain a growing front just decays, so the early impacts were
 * doing nothing and the opening was one organism pulsing alone near the centre
 * for thirty seconds before the screen filled all at once. Big enough seeds each
 * start their own growing region, so the frame fills progressively from several
 * places -- a better image, and an actual narrative of growth rather than a step
 * change. Spacing is dense through the opening for the same reason and widens
 * once the field can carry structure on its own. */
static void arc_inject(uint32_t f, uint8_t *dst)
{
    /* The opening: one lit cell, and nothing else for five seconds. */
    if (f == 0) return;

    for (unsigned i = 0; i < score_hit_count; i++) {
        const score_hit_t *h = &score_hits[i];
        /* r == 0 is an audio-only event. The one at 203 s is deliberately not
         * felt by the field: the ending is the field failing to hold its state,
         * not the field being pushed. */
        if (!h->r) continue;
        if (f == (uint32_t)h->beat * SCORE_FRAMES_PER_BEAT)
            field_inject_blob(dst, h->x, h->y, h->r, h->amp);
    }

    /* The two title cards. Not in score.c: they are not impulses and they have
     * no pitch, so giving score_hit_t a discriminator to carry them would make
     * the shared table worse at the one job it is good at.
     *
     * WORDMARK at 191 s, in the decay -- NOT in the opening, which is where it
     * was first put and where it does not work.
     *
     * At 6 s the field is empty and permissive, so 21-pixel strokes at 96 per
     * frame are wildly supercritical: the letters read, and then they seed their
     * own growth and the screen is full by 6.7 s instead of percolating at 24 s.
     * That destroys the entire sparse opening the music was written against, for
     * a logo. The opening is this demo's strongest thirty seconds -- one lit
     * cell, near silence, everything after it derived -- and interrupting it is
     * exactly what makes a demo feel like a slideshow.
     *
     * In the decay the same injection is subcritical: react_lo has risen to
     * about 58, so the letters cannot grow, and the dying field erases them over
     * a second and a half instead. Measured, the collapse is untouched -- 146 at
     * 199 s, 122 at 203 s with sdev rising, black at 204 s, the same numbers as
     * before the wordmark existed. And the system's name surfacing out of its
     * own decay, just before it fails, is a better place for it than the top.
     *
     * SUSTAINED, not struck. Injected once, it was legible for three frames --
     * 50 milliseconds, which reads as a glitch and not as a title, because the
     * transport smears it diagonally almost at once. Re-injected it stays pinned
     * at saturation, and tapering the last 24 frames makes the release a
     * handover to the medium rather than a cut.
     *
     * Worth being plain about: 14 KB of stencil is a far larger forcing input
     * than a blob's four numbers, and this is the one place the "small forcing
     * vector" of PLANNING.md section 2 is stretched. What it is not is a blit --
     * the bits are ADDED to the field and the field is what gets displayed, so
     * no pixel becomes a function of t and the rule still holds.
     *
     * ENDCARD at 204.5 s, into the quiescent field (arc_quiescent), where the
     * operator is the identity and the image simply stays. */
    {
        const uint32_t at = SEC(191), len = 84, rel = 24;
        if (f >= at && f < at + len) {
            const uint32_t left = at + len - f;
            const int amp = left > rel ? 96 : (int)(left * 96 / rel);
            /* Half the rows per frame, alternating, so each band is refreshed at
             * 30 Hz -- one whole-stencil injection does not fit in the budget
             * (field.h). Between refreshes a band blurs by well under a pixel,
             * so the letters stay pinned at saturation either way. */
            const int half = WORDMARK_H / 2;
            const int band = (int)((f - at) & 1);
            field_inject_stencil_rows(dst, g_wordmark_bits,
                                      WORDMARK_W, WORDMARK_H,
                                      WORDMARK_OX, WORDMARK_OY + 60,
                                      (uint8_t)amp,
                                      band * half, (band + 1) * half);
        }
    }

    /* Re-injected EVERY frame while it is up, at full amplitude.
     *
     * Additive and saturating, so this pins the glyphs at 255 exactly -- the
     * same pixels a blit would write -- while the field carries off and fades
     * whatever spills past their edges. Crisp letters, dust around them, and
     * nothing written as a function of t.
     *
     * The first four frames go in at a lower amplitude so the card accumulates
     * up rather than popping, and at 208 s the injection simply stops: the
     * letters stop being held, the drift takes them, and the credits erode the
     * way everything else in this demo does. */
    {
        /* Released at 209.3, not 208. Letting go early and watching the letters
         * blow away does not work: advection here is per 16x16 block by design,
         * so within a fifth of a second the glyphs are not particles, they are a
         * block-aligned grid, and it reads as corruption rather than dust. The
         * halo around the held card is the dust effect -- moving, fading pixels,
         * which is what was asked for -- and the release now happens late and
         * under an already-dimming palette, so the letters visibly loosen at the
         * very end without the grid ever taking over. */
        const uint32_t at = SEC(204) + 30, off = SEC(209) + 18;
        if (f >= at && f < off) {
            const int amp = (f - at) < 4 ? 72 : 255;
            field_inject_stencil(dst, g_endcard_bits, ENDCARD_W, ENDCARD_H,
                                 ENDCARD_OX, ENDCARD_OY, (uint8_t)amp);
        }
    }
}

/* REACTION-DIFFUSION IS BUILT AND CURRENTLY OFF. A negative result, recorded
 * rather than quietly deleted.
 *
 * rd.c works: swept to k=246 it produces proper Gray-Scott labyrinths, and
 * tools/ can show them (--rdonly). What does not work is getting that structure
 * to survive contact with the feedback field. Three integrations were tried:
 *
 *   additive value      floods brightness, kills the remaining black, and the
 *                       organic shapes are ground into block texture within a
 *                       frame or two
 *   inhibitory value    raises sdev and brings some black back, but produces
 *                       fine dark SPECKLE, not channels
 *   react-threshold bias  changes the dynamics rather than the state, which
 *                       should have been the strong version; still speckle, and
 *                       above ~200 it suppresses the field outright
 *
 * The field imposes its own spatial scale regardless of what is fed in -- the
 * block advection and the react curve dominate the frequency content, and a
 * smooth 160x120 pattern has no way to assert itself against them. Which is a
 * slightly ironic confirmation of the demo's own thesis about strong attractors.
 *
 * Left in the tree with its tooling because the finding is worth keeping and
 * the next attempt should start from a different lever (advection direction, or
 * the flow field itself, rather than the value pipeline). Compiled out so the
 * device pays nothing for it. */

#if HYST_RD_ENABLE
static int16_t arc_rd_amp(uint32_t f)
{
    int32_t a = mix(0, 90, ramp(f, SEC(34), SEC(100)));
    a = mix(a, 0, ramp(f, SEC(162), SEC(200)));
    return (int16_t)a;
}
#endif

/* Palette — the one declared exemption, allowed to be f(t). */
static void arc_palette(uint32_t f)
{
    int a = PAL_COLD, b = PAL_COLD, w = 0;
    if (f < SEC(45))       { a = PAL_COLD;  b = PAL_EMBER; w = ramp(f, SEC(12), SEC(45)) >> 8; }
    else if (f < SEC(130)) { a = PAL_EMBER; b = PAL_BLOOM; w = ramp(f, SEC(80), SEC(130)) >> 8; }
    else                   { a = PAL_BLOOM; b = PAL_ASH;   w = ramp(f, SEC(148), SEC(200)) >> 8; }

    /* The endcard fades through the readout. Its cells never change: the
     * threshold in arc_quiescent holds them at 254 for the whole six seconds and
     * they are still there on the last frame. What goes to black is the palette,
     * which is the exemption this demo declared before it was convenient. */
    /* Starts at 209, not 208. The letters are released at 208 and need a second
     * of lit palette to visibly blow away in -- fading the readout at the same
     * moment simply hid the effect. */
    if (f >= SEC(208) + 45) {
        a = PAL_ASH; b = PAL_BLACK;
        w = ramp(f, SEC(208) + 45, SEC(210) - 6) >> 8;
    }

    if (w > 255) w = 255;
    palette_apply(a, b, (uint8_t)w);
}

/* ----------------------------------------------------------------- core --- */

void sim_reset(int variant)
{
    field_init();
#if HYST_RD_ENABLE
    rd_reset();
#endif
    g_frame  = 0;

    /* Clear BOTH pages. The back buffer is next frame's source after the first
     * present, so leaving it uninitialised would make the run depend on
     * whatever was in memory — the exact class of bug referee test 1 exists to
     * catch, and it would have been embarrassing to ship it inside the tool
     * that checks for it. */
    field_clear(vga_320_back_buffer());
    vga_320_present();
    field_clear(vga_320_back_buffer());

    /* The seed: a single lit cell. The demo begins from nothing, and it is
     * provable that it did. */
    field_poke(vga_320_back_buffer(), FIELD_W / 2, FIELD_H / 2, 255);

    /* Referee test 2: the same world plus ONE extra cell. If a run from here
     * converges back to the unperturbed run, the system has no memory. */
    if (variant == 1)
        field_poke(vga_320_back_buffer(), FIELD_W / 2 + 37, FIELD_H / 2 - 23, 255);

    arc_palette(0);
    vga_320_present();
}

/* Compute the next frame into the back buffer, WITHOUT presenting.
 *
 * Split out because the device has to page-flip inside the vertical blanking
 * interval, so the order there must be compute -> wait for vblank -> present.
 * The host has no beam to race and just calls sim_tick(). */
void sim_step(void)
{
    field_params_t p;
    if (g_fixed_on) { p = g_fixed; p.angle = (int32_t)((g_frame * 7) & 65535); }
    else            arc_params(g_frame, &p);

    uint8_t       *dst = vga_320_back_buffer();
    const uint8_t *src = vga_320_front_buffer();

    /* The slow layer. Stepped every frame regardless of whether it is being
     * injected, so that when the arc brings it in it already has a developed
     * pattern rather than fading up from four nuclei. */
#if HYST_RD_ENABLE
    rd_step(&g_rdp);
#endif

#if HYST_RD_ENABLE
    if (g_rd_only) {
        field_clear(dst);
        rd_inject(dst, 255);
        arc_palette(g_frame);
        return;
    }
#endif

    /* Hand the RD pattern to the field as a threshold bias rather than adding
     * it to the picture. See rd.h: value-level injection fails in both signs. */
#if HYST_RD_ENABLE
    {
        const int16_t amt = g_rd_amp_forced ? arc_rd_amp(g_frame) : g_rd_amp_val;
        p.bias     = amt ? rd_v8() : NULL;
        p.bias_amt = (uint8_t)(amt < 0 ? 0 : amt > 255 ? 255 : amt);
    }
#else
    p.bias = NULL; p.bias_amt = 0;
#endif

    field_step(dst, src, &p);
    arc_inject(g_frame, dst);
    arc_palette(g_frame);
}

void sim_present(void)
{
    vga_320_present();
    g_frame++;
}

/* Energy is TELEMETRY, so it is computed on demand rather than every frame.
 *
 * It used to run inside sim_present: a full 76,800-byte sum, about half a
 * millisecond, on every frame, purely so a once-a-second printf could report
 * it. That is 3% of the frame budget spent on instrumentation, and because it
 * sat outside the timed region it did not show up in the cycles/cell figure at
 * all -- the step measured 15.97 ms while frames actually took 17.42. */
uint32_t sim_energy(void) { return field_energy(vga_320_front_buffer()); }

void sim_tick(void) { sim_step(); sim_present(); }

uint32_t sim_frame(void)        { return g_frame; }
uint32_t sim_total_frames(void) { return TOTAL_FRAMES; }


/* --- scene.h compatibility, so main.c (device) needs no special casing ---- */

int scene_runner_tick(uint32_t t_ms_global)
{
    (void)t_ms_global;          /* deliberately unused: see sim.h */
    if (g_frame >= TOTAL_FRAMES) return 0;
    sim_step();          /* main.c presents after waiting for vblank */
    return 1;
}

uint32_t scene_next_boundary_ms(uint32_t t) { return t; }   /* no seek by design */
uint32_t scene_prev_boundary_ms(uint32_t t) { return t; }
uint32_t scene_cur_start_ms(void) { return 0; }
uint32_t scene_cur_end_ms(void)   { return TOTAL_FRAMES * 1000u / FPS; }
