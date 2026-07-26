/* HYSTERESIS — the driver. See sim.h for why the clock is the frame index.
 *
 * BUILD ORDER STEP 1. This arc is provisional: enough forcing to prove the
 * feedback loop is alive, stable, and path-dependent. The real forcing
 * schedule is step 8, and it will be the sequencer rather than this table.
 */

#include "sim.h"
#include "field.h"
#include "palette.h"
#include "vga.h"
#include "scene.h"

#include <stdio.h>

#define FPS            60
#define SEC(s)         ((uint32_t)((s) * FPS))
#define TOTAL_FRAMES   SEC(210)          /* 3:30 */

static uint32_t g_frame;
static uint32_t g_energy;

/* Probe mode: pin the parameters so the dynamics can be swept without the arc
 * moving underneath the measurement. Tuning a feedback system by eye is how
 * you end up with a demo that dies at 2:40 on hardware only. */
static int            g_fixed_on;
static field_params_t g_fixed;

void sim_set_fixed(const field_params_t *p)
{
    g_fixed = *p;
    g_fixed_on = 1;
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
    p->zoom = mix(z0, z1, ramp(f, SEC(20), SEC(130)));
    p->zoom = mix(p->zoom, 65536 + 240, ramp(f, SEC(150), SEC(205)));

    /* Rotation: a slow constant drift, reversing nowhere. A tilt turns the
     * radial flow into a spiral, which is what stops a pure zoom from looking
     * like a corridor. */
    p->angle = (int32_t)((f * 7) & 65535);

    p->cx = (FIELD_W / 2) << 16;
    p->cy = (FIELD_H / 2) << 16;

    /* A slow wander of the transform centre. Without it the composition is
     * pinned to the middle of the screen for three and a half minutes. */
    p->drift_x = 0;
    p->drift_y = 0;

    /* Blur vs sharpen is the balance that decides whether structure survives.
     * Blur alone flattens the field; the LUT alone drives it to two values. */
    p->blur = 170;

    /* Gain rides just under unity for most of the run and crosses it at the
     * peak. Above unity the field blooms toward saturation, which is the
     * intended read at the climax and death everywhere else. */
    /* Held inside the swept envelope. The first arc ramped this to 254 at the
     * start and 248 at the end -- both outside anything that had been measured,
     * and the middle of the demo duly fell apart. */
    p->gain = 258;

    /* The excitable band. Narrow and low: cells that receive a little energy
     * from a neighbour get pulled UP into the band rather than decaying, which
     * is what lets a single lit cell colonise an empty field. */
    p->react_lo = 12;
    p->react_hi = 180;

    /* SELF-LIMITING IS THE MEMORY KNOB. tools/memory_map.py swept structure
     * against path-dependence and the split was total: every fold=160 regime
     * FORGETS a one-cell perturbation, every fold>=200 regime REMEMBERS it.
     * Above ~230 it remembers but degenerates into noise. So the demo lives in
     * a narrow band, and this parameter is not a look, it is the rule. */
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
    p->persist = (uint8_t)mix(202, 150, ramp(f, SEC(30), SEC(135)));
    p->persist = (uint8_t)mix(p->persist, 212, ramp(f, SEC(150), SEC(205)));
}

/* Forcing events: the only way anything enters the picture.
 *
 * Spaced in SECONDS, not beats. The visual response to an injection propagates
 * over the following 2-5 s and that propagation is the effect; re-forcing
 * before the last response is visible smears everything into noise. */
static void arc_inject(uint32_t f, uint8_t *dst)
{
    /* The opening: one lit cell, and nothing else for eight seconds. */
    if (f == 0) return;

    /* Provisional impulse schedule — replaced by the sequencer at step 8. */
    static const struct { uint32_t at; int x, y, r, amp; } hits[] = {
        { SEC(  8), 160, 120, 3, 255 },
        { SEC( 16), 104,  88, 4, 220 },
        { SEC( 25), 214, 150, 5, 230 },
        { SEC( 34),  88, 164, 5, 210 },
        { SEC( 43), 232,  76, 6, 240 },
        { SEC( 52), 140, 190, 6, 225 },
        { SEC( 60),  60, 100, 7, 245 },
        { SEC( 68), 258, 132, 7, 235 },
        { SEC( 76), 160,  60, 8, 250 },
        { SEC( 84), 108, 132, 8, 240 },
        { SEC( 92), 210, 100, 9, 255 },
        { SEC(100),  74, 176, 9, 245 },
        { SEC(107), 246, 186,10, 255 },
        { SEC(114), 130,  96,10, 250 },
        { SEC(120), 176, 158,11, 255 },
        { SEC(126),  96,  60,11, 255 },
        { SEC(132), 224, 128,12, 255 },
        { SEC(137), 160, 120,14, 255 },
        { SEC(142),  70, 140,12, 255 },
        { SEC(146), 250,  96,12, 255 },
    };
    for (unsigned i = 0; i < sizeof hits / sizeof hits[0]; i++)
        if (f == hits[i].at)
            field_inject_blob(dst, hits[i].x, hits[i].y, hits[i].r,
                              (uint8_t)hits[i].amp);
}

/* Palette — the one declared exemption, allowed to be f(t). */
static void arc_palette(uint32_t f)
{
    int a = PAL_COLD, b = PAL_COLD, w = 0;
    if (f < SEC(45))       { a = PAL_COLD;  b = PAL_EMBER; w = ramp(f, SEC(12), SEC(45)) >> 8; }
    else if (f < SEC(130)) { a = PAL_EMBER; b = PAL_BLOOM; w = ramp(f, SEC(80), SEC(130)) >> 8; }
    else                   { a = PAL_BLOOM; b = PAL_ASH;   w = ramp(f, SEC(148), SEC(200)) >> 8; }
    if (w > 255) w = 255;
    palette_apply(a, b, (uint8_t)w);
}

/* ----------------------------------------------------------------- core --- */

void sim_reset(int variant)
{
    field_init();
    g_frame  = 0;
    g_energy = 0;

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

void sim_tick(void)
{
    field_params_t p;
    if (g_fixed_on) { p = g_fixed; p.angle = (int32_t)((g_frame * 7) & 65535); }
    else            arc_params(g_frame, &p);

    uint8_t       *dst = vga_320_back_buffer();
    const uint8_t *src = vga_320_front_buffer();

    field_step(dst, src, &p);
    arc_inject(g_frame, dst);
    arc_palette(g_frame);

    vga_320_present();

    g_energy = field_energy(vga_320_front_buffer());
    g_frame++;
}

uint32_t sim_frame(void)        { return g_frame; }
uint32_t sim_total_frames(void) { return TOTAL_FRAMES; }
uint32_t sim_energy(void)       { return g_energy; }

/* --- scene.h compatibility, so main.c (device) needs no special casing ---- */

int scene_runner_tick(uint32_t t_ms_global)
{
    (void)t_ms_global;          /* deliberately unused: see sim.h */
    if (g_frame >= TOTAL_FRAMES) return 0;
    sim_tick();
    return 1;
}

uint32_t scene_next_boundary_ms(uint32_t t) { return t; }   /* no seek by design */
uint32_t scene_prev_boundary_ms(uint32_t t) { return t; }
uint32_t scene_cur_start_ms(void) { return 0; }
uint32_t scene_cur_end_ms(void)   { return TOTAL_FRAMES * 1000u / FPS; }
