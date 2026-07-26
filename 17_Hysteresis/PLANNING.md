# 17_Hysteresis — planning

**HYSTERESIS** *(n.)* — the dependence of a system's state on its history.

A **LATENT** production for the Raspberry Pi Pico 2 (RP2350).
Code & direction: **Overscan**. Critic: **Azure**.

---

## 1. The diagnosis

Every demo in this repository is a pure function of time.

SOTA, Dawn, TheDemo, FlashDemo, Singularity, Origami, Quicksilver, Sustain —
all of them compute frame *n* from the clock. Nothing accumulates. That is why
SUSTAIN can jump to any arc node the instant you press `SPACE`: there is no
state to arrive at, only a value of `t` to substitute.

It is also why, for all its continuity, SUSTAIN is a *recording*. Every frame
could have been rendered in any order, on any machine, in any year. The demo
running is not evidence that the demo ran.

## 2. The rule

**No pixel may be a function of `t`. Every frame is computed from the previous
frame.**

Three sub-rules:

1. **The only inputs to frame *n* are frame *n−1* and a small forcing vector**
   from the arc (a handful of scalars, and where to inject energy). No
   `f(x, y, t)` fills anywhere in the pipeline.
2. **Deterministic.** Same seed → bit-identical run, every time.
3. **Irreversible.** There is no seek. You cannot jump to 2:30; you can only
   *get* there.

### The one declared exemption

**The palette may be a function of `t`.** Colour is not state — it is how the
state is read out. Animating 256 palette entries per frame recolours the entire
world for 256 writes, and pretending otherwise would cost the demo its only
free direction lever for no gain in honesty. Declared here, in the plan, before
it becomes convenient.

*(SUSTAIN's lesson: exemptions decided in advance are design. Exemptions
discovered at 3 a.m. are excuses.)*

## 3. Why this belongs on a Pico, and why it locks to the frame

This is the part that makes the concept and the machine the same idea.

### Cost is constant and content-independent

There is no march length, no visibility, no overdraw, no depth complexity.
Every frame touches every cell exactly once. Frame time is a *compile-time*
property, not an emergent one:

```
320 × 240 = 76,800 cells × 60 Hz = 4.608 M cell-updates / second
@ 300 MHz  →  65.1 cycles per cell on one core
           → 130.2 cycles per cell across two
```

That is the entire performance question, stated on day one, in one line. It is
exactly what SUSTAIN could not do — its cost swung 3× between open sea and
tunnel because ray-march length is data-dependent, so no budget was knowable
until the whole world existed.

The failure mode is also the right one. If we are over budget, we are over by
the *same amount on every frame*, and we find out on the first flash — not at
2:14 in one bad section three weeks in.

### Frame timing is a correctness property, not a quality one

This is the real reason the concept and the 60 Hz lock belong together.

In a `f(t)` demo, a dropped frame costs smoothness. The picture is unchanged;
you just saw less of it.

In a feedback system, the update rate **is a term in the equation**. Drop a
frame and the system takes a different number of steps, so it evolves to a
different state. The picture is not merely rougher — it is *wrong*, and it
stays wrong, because the error is carried forward forever. There is no
resynchronisation point, because that is the whole premise.

So HYSTERESIS cannot treat 60 fps as a target it would like to hit. It has to
hold the frame or it is not the same demo. A demo whose visual identity depends
on hitting the deadline is a much better reason to hit the deadline than
"smooth is nice."

## 4. Architecture

### The state field *is* the framebuffer

`MODE_320` is 8 bpp, 320×240, palette-indexed, and already **double-buffered**
(`fb_a`/`fb_b`, swapped by `vga_320_present()`). Therefore:

- one byte per cell, and that byte is simultaneously the simulation state and
  the displayed pixel — **no conversion step at all**;
- `vga_320_back_buffer()` is the write target, `fb_front` is frame *n−1*,
  read-only while it is being scanned out — **the ping-pong we need already
  exists**, at zero extra memory and zero copies;
- the palette carries all colour, so 100 % of the cycle budget goes to the
  simulation.

The engine and the display are the same array. That is not an optimisation, it
is the reason this idea fits this machine.

### Operators

A frame is a short pipeline of operators over the field. Four families:

| operator | what it does | cost shape |
|---|---|---|
| `advect` | sample frame *n−1* through a transform (rotate/zoom/shear/flow) | one bilinear fetch per cell |
| `diffuse` | separable 3×3 blur | 2 passes, adds/shifts only |
| `react` | pointwise nonlinearity | **256-entry LUT, ~1 cycle** |
| `inject` | write sources — the arc's only way to touch the picture | sparse |

`advect` is a rotozoom of the previous frame, which is the video-feedback
effect (a camera pointed at its own monitor). QUICKSILVER already has a
rotozoom and an interpolator path; SUSTAIN inherited `interp_emu`. That code is
reusable and already bit-exact against hardware.

### The Amiga blitter-feedback trick — the artefact *is* the effect

The Amiga demos that produced fractal-looking recursive zooms did **not**
transform per pixel. From the pouët thread on the effect:

> "i divide the screen into 16×16 blocks and for each block i set the blitter
> dest pointer.. then according to the rotation/zoom/whatever required, i
> calculate a source coordinate"

— with two buffers alternating as source and destination ("blit from buffer A
to B in one frame and then from B to A in the next"), and a seed plotted at
the centre each frame "which gets distorted more and more for each frame."

That is the architecture in §4 already, arrived at independently. But the block
decomposition is a substantial win we should copy outright: **compute the
source coordinate once per 16×16 block, not once per cell.** The inner loop
becomes a fixed-offset run — a shifted copy — instead of a per-pixel transform.
That is single-digit cycles per cell against roughly twenty for a general
rotozoom, and it is what buys headroom for the nonlinear stage inside the 65.

And the part that matters most:

> "The self-similar, fractal-like quality emerges from accumulated rounding
> errors in the coordinate transformations across multiple frames — not true
> fractals, but the cumulative distortion of displaced pixels."

The block quantisation error is not a defect to be minimised, it **is** the
fractal structure. A more accurate transform produces a *less* interesting
picture. This is a rare and welcome inversion: on this machine the cheap
approximation is also the better-looking one, so there is no
quality-versus-speed trade to negotiate here at all.

### Direction vocabulary

Video-feedback practice maps parameters to outcomes predictably enough to
author against, which is what makes a forcing schedule (§5) tractable:

| parameter | visual result |
|---|---|
| rotation angle | spiral direction and velocity; small tilt → vortices |
| zoom **in** (magnify) | exploding, outward-flowing patterns |
| zoom **out** (minify) | nested recursion, inward tunnels with visible layers |
| gain > 1 | bright areas amplify and bloom toward saturation |
| gain < 1 | slow, controlled evolution; dark areas drop out |
| sharpening vs blur | crystalline latticework vs nebular cloud masses |

The `react` LUT is what makes 65 cycles/cell achievable: arbitrary
nonlinearity, including the sharpening that stops diffusion from washing
everything flat, for a byte load.

Sections differ by **operator parameters**, not by different code — the
family/parameter split that worked in SUSTAIN. It is my method now; reusing it
is deliberate.

### Reaction–diffusion runs at half resolution

Gray–Scott needs two chemicals, and feedback needs two pages of each:
4 × 76,800 = **307 KB**. Too much beside scanvideo's buffers in 520 KB.

At 160×120 it is 4 × 19,200 = **76.8 KB**, which is comfortable, and RD
patterns are inherently low-frequency — upsampling them costs almost nothing
visually. So: **video feedback and advection at 320×240, reaction–diffusion at
160×120, composited up.** Decided now, with the arithmetic, rather than
discovered when the linker complains. *(SUSTAIN overflowed RAM by 55 KB because
a budget was written down without being computed.)*

### Dual core is safe here

Splitting the field in half by row is trivially parallel: a pure array→array
map with one shared halo row. Unlike SUSTAIN — where the work was entangled
with the scanvideo callback, which is why I left that lever unpulled — there is
nothing here for the two cores to race over. This is the honest path to the
130 cycles/cell figure.

## 5. Direction: you do not animate it, you garden it

The hard problem, stated plainly: **how do you sync a path-dependent system to
fixed music?**

You cannot keyframe it. So the arc is not a list of what the picture does — it
is a schedule of **forcings**: what energy goes in, where, and how the operator
parameters bend around the event.

This still lands on the beat, because injection is *immediate and local* — the
frame the hit happens on visibly changes. What is unpredictable is the
**response**, which propagates over the following seconds. That is the correct
division: the sync is authored, the consequence is emergent.

### The arc is inherent to the medium

- **Open on a single lit cell.** One byte set in an otherwise empty field. The
  demo begins from nothing, and it is provable that it did.
- **Grow.** Forcing builds structure; operators sharpen; the field fills.
- **Run down.** Stop forcing it and diffusion carries the field to a uniform
  value. The demo ends by reaching **equilibrium** — heat death, the honest
  ending for a system with memory.

One pixel → complexity → equilibrium. That arc is not imposed on the material;
it is what the material *does*. SUSTAIN's return had to be constructed. This
one is free.

## 6. The referee

Same discipline as SUSTAIN: the central claim is checked by a machine, and a
build that fails does not ship.

`tools/no_keyframes.py`:

1. **Determinism** — run the same seed twice, require bit-identical output.
   Catches any accidental dependence on wall-clock, uninitialised memory, or
   core scheduling.
2. **Path-dependence** — flip **one pixel** at frame 0 and run both to the end.
   The divergence must *grow*. If a demo has hidden `f(t)` structure driving
   it, the two runs re-converge and this test fails. This is the mechanical
   proof that the demo could not have been keyframed.
3. **Frame time** — on device, worst frame < 16.67 ms over the full run, or the
   build fails. Strong here precisely because cost is constant.

Sub-rule 1 is enforced *structurally* rather than tested: **the operator
signature takes no time argument.** A test you cannot fail is better than a
test that passes.

## 7. Risks, and what I would do about each

### The concept-level threat: contraction forgets

Researching the Amiga effect turned up something that could invalidate the
whole premise, so it goes first.

Iterated feedback of this kind is an **iterated function system**, and a
*contractive* IFS converges to an attractor **that does not depend on where it
started**. Two runs from different initial images end up at the same picture.
If HYSTERESIS is built from contraction maps, then it has no memory at all —
sub-rule 3 is false, referee test 2 fails, and the demo would genuinely have
looked identical if I had keyframed it. That is failure mode two in §10, and it
is a property of the mathematics rather than of the code, so no amount of
implementation care would rescue it.

The distinction is sharp and useful:

- **Zoom out (minify)** contracts. Detail flows inward and shrinks toward
  nothing; perturbations die. Beautiful, and it *forgets*.
- **Zoom in (magnify)** expands. A single altered cell grows into a visible
  region; perturbations amplify. This is the regime with memory.
- **Nonlinearity** (`react`, and reaction–diffusion proper) is where genuine
  path-dependence lives — Gray–Scott is famously sensitive to initial
  conditions.

So the demo must not be built only from zoom-out feedback, however good it
looks. It needs magnifying feedback and a real nonlinear stage, with the
system held near gain ≈ 1 — the edge between forgetting and blowing out.

The useful consequence: **referee test 2 is not a formality, it is the
experiment that decides whether this demo can exist.** It runs at step 2 of the
build order, before any content, and it is cheap. If a plain feedback loop
re-converges after a one-pixel perturbation, I want to know that in week one,
not after authoring an arc to it.

### The rest

| risk | mitigation |
|---|---|
| **It looks like a screensaver.** The single biggest threat — pretty process, no authorship. | The one-pixel→equilibrium arc; hard injection sync so hits are unmistakable; palette direction. If at the halfway mark it still reads as ambient, that is a kill signal, and I would rather hear it from you early. |
| **8-bit dead states.** Repeated integer blur rounds toward flat and the system quantises into a frozen fixed point — the simulation dies while looking fine. | Dithered rounding: carry the residual back in (Bayer 4×4, already in SUSTAIN). This is a known failure of 8-bit feedback and it needs to be in the design, not patched later. |
| **No seek makes iteration slow.** SUSTAIN's `SPACE`-to-node is gone by construction. | Host build runs headless at max speed to reach any *t*, plus dev-only checkpoint snapshots. Snapshots never exist in the demo build. |
| **No camera, no 3D** — audience expectation. | Accepted. This is deliberately not another flythrough, and hedging it into one would forfeit the whole point. |

## 8. Assets — later, and far fewer

SUSTAIN's real constraint turned out to be flash: 2.59 MB of QOA plus 678 KB of
art left under 1 MB for code, and I discovered that late.

HYSTERESIS needs almost no art. Injection sources are **1-bit stencils** —
logo, typography, shapes — which are tiny, and the palette does the colour that
textures used to do.

- **Music** — the one real dependency. Needs a track with clear, spaced
  impulses (the forcings) over sustained material, and a genuine decay at the
  end for the equilibrium close.
- **Stencils** — 1-bit, ≤ 320×240, a handful.
- **Target: under 200 KB of assets excluding music**, against SUSTAIN's 678 KB.

Assets are requested **after** the operator loop runs on hardware, not before.

## 9. Build order

Device bring-up is step 3, not step 9. Every performance estimate I made from
the desktop host during SUSTAIN was wrong, in both directions, because the host
has no XIP cache to miss.

1. Host harness: field + one `advect` operator + animated palette. Prove the
   feedback loop is alive and stable.
2. `no_keyframes.py` — determinism and divergence tests, before there is
   content to protect.
3. **On hardware.** Measure real cycles/cell with the telemetry line that
   already exists in `main.c`. Confirm or kill the 65-cycle budget *now*.
4. Full operator set + the `react` LUT + dithered rounding.
5. Dual-core row split; re-measure.
6. Arc: forcing schedule authored against the track.
7. The ending — forcing stops, field decays to equilibrium.

## 10. What would make this a failure

Stated up front so it can be called honestly later:

- It holds 60 fps but no one can tell what they are looking at.
- The path-dependence is technically true and visually invisible — i.e. it
  would have looked the same keyframed, and the rule bought nothing.
- It drifts into being an effect pack again, with operator changes standing in
  for scene cuts.

The first is a direction problem, the second means the concept was wrong, and
the third means I did not learn anything from demo 16.
