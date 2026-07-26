# 16_Sustain — *SUSTAIN*: a demo with no cuts

**A LATENT production. Code & direction: Overscan (Claude Opus 5). Critic: Azure.**

---

## Context

Demos 10–15 in this repo established a mature dual-target engine: effects
compile byte-identically for RP2350 firmware and an SDL host preview, sharing
`scene.c` / `vga.c` / `audio_qoa.c` and differing only in swapped
`main`/`vga`/`audio` files. 15_Quicksilver added a bit-exact software emulator
of the SIO interpolator so host and silicon are pixel-identical.

The engineering is good. The demos are not yet *good demos*. This plan is about
closing that gap, and it starts with an honest diagnosis.

### The diagnosis

The scene's standard verdict on productions like ours is well-worn vocabulary,
visible in any pouët comment thread: *"isn't it just random scenes?"*,
*"bad direction"*, *"it's going nowhere — I can't tell if a scene is starting
or ending or building to something"*, and the one that stings most:

> *"the direction seemed completely random, but this is such an awesome effect pack!"*

Read QUICKSILVER's own arc table
([15_Quicksilver/README.md:54](../15_Quicksilver/README.md#L54)) against that.
Ten scenes, explicitly *"ordered by rising impact"*, joined by — per
[scene.c:53](../15_Quicksilver/quicksilver/scene.c#L53) — a **uniform** glint
crossfade *"kept in the runner so no scene needs to wire it."* The transition is
architecturally an afterthought, decoupled by design from the things it joins.
That is an effect pack with a wipe, and it would draw exactly the comment above.

The bar on this exact silicon is already set by a human production:
[*hard fault* by Otomata Labs](https://www.pouet.net/prod.php?which=102951)
(RP2350, dual M33 @ 126 MHz, DVI, tracked music) took **1st at DiHALT Winter
2025**. Read its comments: viewers praised *"perfect 90ies PC style"*, the
music, and a *"cute gouraud duck."* **Nobody praised a register trick.** On this
hardware, coherence and charm win. More interpolator does not get us there.

### The winning angle

**SUSTAIN never cuts.** Three minutes, one unbroken camera move. No fades, no
crossfades, no dissolves, no scene boundaries, and the demo never returns to
black until the very last frame.

The rule is the entire design: *an effect may not end — it may only **become**
the next one.* The viewer is never handed a boundary at which to disengage,
which is the literal mechanism behind "it's going nowhere."

This is not a gimmick, because it is unimplementable on the current
architecture and forces a better one (§3). And it is a claim the scene has
existing vocabulary to praise.

---

## 1. Identity

**SUSTAIN** — in synthesis, the envelope stage that holds while the key is
down; in cinema, the shot that is never cut away from. The demo is one held
note.

Visually: **strata**. A single continuous descent through a world that keeps
turning out to be made of something else. The palette moves cold → hot → cold
across the run so that colour alone tells you where you are in the arc, without
a single cut to mark it.

---

## 2. The rule, stated so it can be enforced

Three prohibitions, in priority order:

1. **No frame-to-frame discontinuity.** Between any two consecutive frames, the
   image must be a continuous deformation of the previous one. Enforced
   mechanically by `tools/cut_detect.py` (§8) — this is SUSTAIN's equivalent of
   QUICKSILVER's `interp_selftest.c`: a machine check on the demo's central
   claim, not a matter of taste.
2. **No black.** The screen never fully clears until the final collapse.
3. **The camera never teleports.** Position and orientation are C¹-continuous
   for the whole run — one spline, evaluated start to finish.

**The one sanctioned exception: the motivated cut.** Where a morph is genuinely
impossible (§6), the change may be hidden behind a *diegetic* event — passing
through an opaque surface, a light blowing out, a whip-pan whose motion blur
covers the swap. The camera keeps moving through it and the rule-1 audit still
passes. Budget: **at most three** in the whole demo, each individually
justified in `IMPLEMENTATION.md`. A crossfade is never acceptable.

---

## 3. Architecture: one renderer, one camera, one shot

The current engine cannot express this. [scene.h:43](../15_Quicksilver/quicksilver/scene.h#L43)
defines `effect_t {init, frame, done}` and
[scene.c:37-48](../15_Quicksilver/quicksilver/scene.c#L37-L48) hard-swaps at
each boundary: `done()` → `vga_set_mode()` → `init()`. Nothing survives the
transition; nothing *can* morph across it. So the timeline changes shape:

| | QUICKSILVER | SUSTAIN |
|---|---|---|
| Unit | `effect_t` (own renderer, own buffers) | `field_t` (a *description* of the world) |
| Timeline | `{start_ms, end_ms, effect*}` | camera spline + **blend schedule** |
| Boundary | `done()` / `init()` swap + glint | weight `w(t)` crosses between two fields |
| Live at once | exactly 1 | 1, or 2 during a morph |

### 3.1 The shared representation

Every "effect" is expressed as a **field** sampled along view rays from one
shared camera — not as a program that owns the screen:

```c
typedef struct field {
    const char *name;
    /* Distance/height of the world at p, in shared world space. */
    float (*h)(float x, float z, float t);
    /* Surface shading given hit point + normal. Returns RGB565. */
    uint16_t (*shade)(hit_t *hit, float t);
    void  (*prepare)(int slot, float t);   /* fill scratch slot with LUTs */
} field_t;
```

One renderer walks the rays; the fields only answer questions. A transition is
then **lerping the answers**, not compositing two finished frames — which is
what makes the morph cheap and, more importantly, what makes it look like one
world changing rather than two pictures dissolving.

### 3.2 Two morph classes, and why the budget survives

- **Intra-family (the default, ~free).** Two fields of the same family differ
  only in parameters or domain warp, so a morph lerps the *parameters* and
  evaluates the field **once**. Sea → canyon → tunnel are all one heightfield
  renderer under different domain warps. **6 of the 8 boundaries are this.**
- **Cross-family (expensive, 2 evaluations).** Genuinely different
  representations — heightfield vs. particle swarm vs. polygon mesh. Costs ~2×
  for the morph's duration. **Only 2 exist**, and both are deliberately
  scheduled into low-detail moments (heavy fog, near-darkness, high motion blur)
  where the resolution/rate can drop without being read as a drop.

This is the load-bearing design decision. If every boundary were cross-family,
SUSTAIN would not fit in the frame budget and the plan would be dishonest.

### 3.3 Memory: two scratch slots

[scene_scratch.h:5](../15_Quicksilver/quicksilver/scene_scratch.h#L5) —
*"Only one scene is active at a time, so heavy per-scene buffers all alias the
same physical bytes via a union"* — is precisely the assumption a morph breaks.
During a morph two fields are live, so the union becomes **ping-pong**:

```c
union field_scratch_u { ... };
extern union field_scratch_u g_slot[2];   /* outgoing renders from A, incoming prepares into B */
```

The incoming field's `prepare()` runs into the idle slot over the ~2 s
*before* it is needed, spread across frames — so a morph never pays a
precompute spike. This is the piece that makes rule 1 achievable at all: a
stall is a discontinuity.

Cost is one extra slot. Budget in §7.

---

## 4. The arc

≈3:00, one camera spline, eight morphs. Structure is **departure →
transformation → return**: the final vista is the opening vista, approached
from the other side, in the other palette. That return is the directorial
device that answers *"is it going anywhere?"* — it went somewhere, and came
back changed. Nothing else in this repo has a shape.

| Time | Where the camera is | Field | Morph into next | Class |
|------|---------------------|-------|-----------------|-------|
| 0:00 | Skimming low over a still, mirror-flat sea under a cold dawn sky | heightfield (calm) | swell steepens, wave crests rise into walls | intra |
| 0:22 | Between the crests — they are now canyon walls; camera drops | heightfield (domain-warped) | walls lean in and close overhead | intra |
| 0:46 | Enclosed: the canyon has become a **tunnel**, camera accelerating | heightfield (cylindrical domain) | wall texture detaches from the wall | **cross ①** |
| 1:10 | Flying through a **swarm** — the tunnel is now particles in the same tube shape | particles | swarm contracts on a common centre | intra |
| 1:34 | The swarm has condensed into a single **chrome solid**; camera orbits it | particles → mesh | surface tessellation flattens outward | **cross ②** |
| 1:58 | The object's surface *is* the ground — camera pulls back, we're on a plain | heightfield (warm/night) | plain tilts; horizon rolls to vertical | intra |
| 2:22 | The horizon has become the sea again, inverted, hot palette | heightfield (calm, hot) | palette cools; camera rises | intra |
| 2:40 | **The return.** Opening vista, opposite heading, cold again — with credits | heightfield + type | — | — |
| 2:56 | Camera rises past the sky; frame collapses to a line, then black | — | first and only black | — |

Both cross-family morphs sit where the plan says they must: ① in the darkest,
foggiest moment of the tunnel; ② under a specular bloom that whites out most of
the frame.

---

## 5. What we reuse, and the one hardware hero

**Reuse verbatim** from `15_Quicksilver/quicksilver/`: `vga.c/.h`, `rgb565.h`
(always `rgb565_pack()`), `audio_qoa.c`, `audio.h`, `qoa.h`, `font8x8.h`,
`main.c`, `interp_compat.h` + `interp_emu.{c,h}` (+ its selftest),
`pico_*_import.cmake`, `host/{main_host.c,vga_sdl.c,audio_sdl.c}`.

**Rewritten:** `scene.c/.h` → `world.c/.h` (camera spline + blend schedule),
`scene_scratch.h` → `field_scratch.h` (ping-pong), `timeline.c` → `arc.c`.

**The hero is not a peripheral this time.** QUICKSILVER's hero was the
interpolator; SUSTAIN's is the *architecture* — that the demo is one continuous
function of time. But there is real silicon left on the table and one piece of
it directly pays for the morphs:

**Cortex-M33 DSP/SIMD.** No demo in this repo uses it — a grep for `SMUAD`,
`SMLAD`, `PKHBT`, `UQADD16`, `SSAT16` across every `.c`/`.h` in the tree
returns **zero hits**. These do packed 16-bit arithmetic: *two RGB565 channels
per instruction*. The one place it matters most is exactly the cross-family
morph, where we must blend two full fields — `UQADD16`/`PKHBT` halve that
inner loop. That is a hero with a *reason*, not a checkbox.

The interpolator still does address-generation and BLEND for the heightfield
sampling, unchanged from QUICKSILVER. It is no longer the point.

---

## 6. Where this fails, honestly

Listed because a plan that hides these is worthless.

1. **Particles ↔ mesh (morph ②) does not lerp.** A swarm and a triangle mesh
   have no shared parameterisation. *Mitigation:* the mesh's vertices **are**
   the particles' targets — the swarm converges onto vertex positions, then the
   renderer switches to drawing the edges between particles that have arrived.
   No frame changes representation; the *density* of drawn edges ramps. If that
   still reads badly, this is where motivated cut #1 is spent (the specular
   bloom is already scheduled there).
2. **Cross-family frame cost.** 2× for ~2 s twice. *Mitigation:* drop the
   morph interval to half-resolution behind fog/bloom, and the DSP-SIMD blend.
   *Fallback:* extend the fog and shorten the morph to 1.2 s.
3. **Zero slack for a stall.** A dropped frame is a visible discontinuity and
   fails the rule-1 audit. *Mitigation:* `prepare()` amortised across ~120
   frames into the idle slot; hot loops `__not_in_flash_func`; no allocation
   after boot.
4. **SRAM.** Two scratch slots + framebuffer arena is the tightest budget yet
   (§7). *Mitigation ladder:* shrink slot size → single slot with
   compute-on-the-fly for the cheap fields → drop to single-buffered
   `MODE_RACE` for the tunnel segment, which frees the entire 307 KB arena.
5. **"One shot" can read as monotonous.** Real risk; continuity is not the same
   as interest. *Mitigation:* the camera's *speed* is the pacing instrument —
   it must accelerate and stall hard against the music where cuts would
   otherwise be. If the host preview looks placid, the arc is wrong, not the
   rule.
6. **Suno cannot be told "no structural boundaries."** A track with hard
   section breaks will fight a demo with none. *Mitigation:* brief explicitly
   for continuous transformation and long crossfaded sections; pick the take
   whose energy curve matches the camera-speed curve, and author the arc to the
   track rather than the reverse.

---

## 7. Budget

Targets, to be checked at every build (QUICKSILVER shipped at **BSS ≈ 479 KB /
520 KB**, arena 307 KB, ~30 KB heap for the scanline pool):

| Item | Budget |
|---|---|
| Framebuffer arena (`MODE_HIRES` double-buffered, 320×240 RGB565) | 307 KB |
| `g_slot[2]` field scratch | 2 × 64 KB = 128 KB |
| Engine + audio + fonts + misc | ~35 KB |
| **Total BSS** | **~470 KB / 520 KB** |
| Scanline pool heap | ≥ 30 KB |
| Flash (textures, meshes, `music.qoa`) | < 4 MB |

64 KB per slot is the number to defend; if a field needs more, it is the wrong
field. Textures live in flash via incbin (proven by 13's 131 KB `disk_tex.bin`).

---

## 8. Verification

- **`tools/cut_detect.py` — the rule-1 audit.** Run the host in `--rawpipe`,
  compute mean absolute frame delta for all ~10 800 frames, and flag any frame
  whose delta exceeds `k ×` the rolling median. Every flagged frame must be
  either the final collapse or one of the ≤3 justified motivated cuts. **A
  build that fails this audit is not SUSTAIN and does not ship.** Wire it into
  the build like `interp_selftest.c`.
- **Camera continuity assert.** Host build asserts C¹ continuity of the spline
  each frame — position and orientation deltas bounded — catching arc-authoring
  errors before they reach the audit.
- **Frame-time trace.** Log per-frame ms on device; the morph windows are the
  spikes to watch. Zero frames over 16.6 ms, especially in the two cross-family
  windows.
- **Per-segment eyeball:** `./sustain.exe --screenshot-at <ms>` at each morph's
  start / midpoint / end. The midpoint is the shot that matters — if the
  midpoint of a morph looks like a crossfade, it *is* one.
- **Device timing:** `arm-none-eabi-objdump -d` on the ray-walk and blend loops;
  confirm the SIMD blend actually emitted packed instructions rather than
  scalar fallback.
- **Budget check** after each segment lands (§7).

---

## 9. Build order

1. **`world.c` + camera spline + blend schedule**, with two trivial
   placeholder fields (flat plane, sine plane) — prove a parameter morph is
   seamless with nothing else in the way.
2. **`cut_detect.py`** immediately after. Build the referee before the game.
   Everything downstream is measured by it.
3. **Heightfield renderer** (interpolator address-gen + BLEND) and the three
   intra-family domain warps: sea → canyon → tunnel. This is 6 of 8 boundaries
   and the bulk of the demo.
4. **Particle field** + cross-morph ① (fog window). Audit.
5. **Mesh field** + cross-morph ② (converge-onto-vertices, bloom window). Audit.
   This is the highest-risk item — if it fails, spend the motivated cut and
   move on rather than sinking the schedule into it.
6. **The return** — reuse field 1 with the hot→cold palette and reversed
   heading; credits typography over it; final collapse.
7. **DSP-SIMD pass** on the blend and ray-walk loops, only where §8's frame
   trace shows it is needed.
8. **Music and art in parallel from day one** (`assets/PROMPTS.md`), so the arc
   can be authored against the real track's energy curve rather than retrofitted.

---

## 10. The claim

> *The first demo on this hardware where the transitions are the demo.*

If SUSTAIN ships and the rule holds, that sentence is defensible on pouët, and
it is not a sentence about a microcontroller. If the rule does not hold, we
shipped an effect pack with a fancier wipe, and we should say so.

---

## Postscript — what the plan got right and wrong

Written after the demo shipped, against the plan above rather than over it.

### Held up

- **The rule.** "An effect may not end, it may only become the next one" drove
  every architectural decision and never needed softening. The finished demo
  has no cuts.
- **The referee.** `cut_detect.py` was the single best decision. It caught a
  whole-frame pixel jump from an `int` horizon, 294 black frames from a
  mis-mapped sky, camera-through-terrain in three separate places, and a
  mirror-symmetric world. None of those would have been found by eye in time.
- **Enclosure as a parameter.** §3.1's bet that one renderer could do sea,
  canyon and tunnel paid off completely — and then paid again when cross-section
  shape and channel meander became parameters too.
- **Intra-family morphs are nearly free** (§3.2). Six of eight, as predicted.

### Got wrong

- **§4's swarm cannot exist.** A heightfield has exactly one surface per
  (x, z), so nothing can float. It became a cave of stalactites and
  stalagmites, and was renamed to what it is.
- **§8's embossed wordmark was unreadable.** A low camera over a heightfield
  sees an embossed word almost edge-on. It is composited instead — still fading
  up over live water, still never cut to.
- **§7's memory budget was for the wrong thing.** 128 KB was reserved for a
  `prepare()` hook no field ever used, while the real pressure turned out to be
  getting textures *out of flash* and into SRAM.
- **The arc was too short.** Planned at 3:00 with 8 morphs; the delivered track
  was 4:49 and needed 14 to avoid feeling empty. The critic's "not enough
  happens" note was right twice.

### Not anticipated at all

**Performance.** The plan has a memory budget and a frame-time note, and no
strategy for either. The demo first ran at 2.9 fps and needed a full
optimisation pass — on-device profiling, textures and hot code into SRAM,
halved column marching, shading restructured — to reach 10-15 fps.

The lesson worth carrying forward is narrower than "plan for performance": it
is that **every estimate made from the desktop host was wrong**, including the
confident ones, and that the fix was a microsecond timer on the target and a
`picotool` flash loop. Measure on the machine that has to run it.
