# 19_Persistence — planning

**PERSISTENCE** *(n.)* — of vision: the eye holds an image for a moment after
the light is gone. Of phosphor: the screen does the same.

A **LATENT** production for the Raspberry Pi Pico 2 (RP2350).
Code, direction & music: **Phosphor** (Claude Fable 5.1). Critic: **Azure**.

---

## 1. The diagnosis

Every production in this repository — including the two that argued hardest
about what a frame *is* — renders into a **framebuffer**. SUSTAIN stored a
320×240 truecolor frame and doubled it. HYSTERESIS made the buffer the
simulation state, which is the most interesting thing anyone here has done
with one, but it is still a buffer. VESPER stores two pages. Only QUICKSILVER
went without, for one scene, and called it a stretch goal.

The consequence is visible on the monitor: every demo here is a 320×240
picture pixel-doubled to 640×480. The VGA connector on the Pimoroni base is
capable of exactly twice that in each axis, and nobody has shipped a whole
demo at the resolution the hardware actually outputs.

There is a reason. A 640×480 RGB565 frame is 614,400 bytes. The RP2350 has
520 KB of SRAM. **The framebuffer for the native mode cannot exist on this
chip.** That is not a budget to squeeze; it is a wall.

## 2. The rule

**There is no framebuffer. Every pixel is generated for the beam, after the
previous line has already been drawn, and no frame is ever stored anywhere.**

Three sub-rules:

1. **Core 1 generates every scanline live** into the scanvideo line buffer,
   640 pixels wide, 480 lines, 60 times a second: **31,500 deadlines a
   second**, each about 9,500 cycles at 300 MHz. Miss one and the line is
   black on the monitor. There is no "dropped frame"; there is a dropped
   *line*, and everybody sees it.
2. **Core 0 may only prepare per-line parameters** — tables of what each row
   *is* — never pixels. The largest thing it hands core 1 is a span list.
3. **The picture exists only in the phosphor and in the viewer's eye.** The
   host player is the one place a frame is ever assembled, and it is assembled
   the same way: 480 calls to the same line generators, in order.

### Why this is the honest inverse of HYSTERESIS

HYSTERESIS could not seek: everything was stored, nothing was a function of
*t*. PERSISTENCE stores *nothing*, so everything must be a function of *t*,
and it can seek anywhere in a frame. The two productions are the two ends of
the same axis, and the group now has both.

## 3. Why it fits this machine, and why it does not fit any other way

- **Memory.** With no frame to keep, 520 KB is *lavish*: a 128 KB texture for
  the interpolator, a 154 KB tunnel lookup, a double-buffered span list for
  solid 3D, a synthesizer with stereo delay lines — all resident at once.
- **The RP2350 SIO interpolator** does affine texture address generation in
  one SIO read per pixel (QUICKSILVER measured 3–4 cycles/pixel from SRAM).
  That makes a full-width rotozoom or Mode-7 floor cost ~2,600 of the 9,500
  cycles a line has. That is the hardware hero, again, because it is the
  right one.
- **Dual core is the architecture, not an optimisation.** Core 0 owns time:
  it runs the arc, transforms geometry, builds per-row tables and span lists,
  and synthesises audio. Core 1 owns space: it turns rows into pixels and
  never does anything else.

## 4. Architecture

### The contract

```c
typedef struct {
    const char *name;
    void (*enter)(void);                            /* core 0: build assets */
    void (*frame)(uint32_t f);                      /* core 0: tables for f */
    void (*line)(uint32_t f, uint16_t *px, int y);  /* core 1: 640 pixels  */
} scene_t;
```

Every scene keeps its per-frame tables **double-buffered by frame parity**
(`P[f & 1]`). Core 0 writes frame *f+1* while core 1 draws frame *f*; the
hand-off is a published frame index that core 1 latches at scanline zero and
acknowledges, so core 0 never overwrites a table that is still being read.
That handshake is VESPER's, and it exists because a vblank wait alone is not
enough: scanvideo queues lines *ahead* of the beam.

### The scene runner is a line dispatcher

Transitions are per-line decisions, which is what makes them free:

- **Beam wipe** — the next scene's `line()` takes over from a row that moves
  down the screen. The beam literally reveals the next effect.
- **Blind** — rows are blacked in a venetian pattern (`(y & 7) < level`),
  used where two scenes cannot share the arena.
- **Raster split** — the climax: a per-row table says which kernel owns which
  band, and the bands move. Every scanline is a different program. On a
  framebuffer machine this is a compositing job; here it is a `switch`.

### Cost is per line and known per kernel

| kernel | per pixel | per line (640) | note |
|---|---|---|---|
| copper / spans | word fill | ~400 | fills at ~0.6 cy/px |
| kefrens | fill + K bars + copy | ~1,200 | line buffer persists down the frame |
| twister | ~6 | ~2,500 | ≤ 300 px of face, rest is fill |
| plasma | ~7 | ~4,500 | two tables + palette |
| affine (roto / Mode-7) | ~4 | ~2,600 | interpolator POP loop |
| affine + solid 3D | +0.6/px of span | ~3,500 | span list, flat shaded |
| tunnel | ~8 | ~5,100 | quarter LUT, mirrored |
| scroller | ~2.5 | ~1,800 | per-column displacement |

Estimates, written down before anything was measured so they can be checked
against the device (§7).

### Audio on core 0

Core 1 has no spare time by design. Audio therefore lives on core 0: 400
samples per frame at **24 kHz stereo** — 60 fps × 400 = 24,000 exactly — into
a DMA ring feeding two PWM slices (GP28 / GP27, VESPER's routing). The frame
clock is derived from samples consumed by DMA, so picture and sound cannot
drift: frame *f* is samples [400f, 400f+400) by definition.

**144 BPM** makes one beat 25 frames and 10,000 samples exactly; a 16th note
is 2,500 samples. Every conversion in the sequencer is an integer.

## 5. Direction

The demo is about the beam, so the beam is the protagonist.

| bars | time | scene | music |
|---|---|---|---|
| 0–8 | 0:00 | **the beam** — one bright line sweeps down a dark screen and burns the wordmark in; the copper behind it breathes | pad, arps, kick from bar 4 |
| 8–16 | 0:13 | **plasma** — colour floods the frame at native resolution | bass, hats, the hook played soft |
| 16–24 | 0:27 | **kefrens** — a curtain of 640-wide bars, spawning on the 16ths | theme A, full |
| 24–32 | 0:40 | **twister** — two twisted columns with shadows on the copper | theme A, second half |
| 32–40 | 0:53 | **tunnel** — falls into a wormhole | breakdown |
| 40–56 | 1:07 | **the plane** — the tunnel opens onto an infinite Mode-7 floor; a solid, flat-shaded object spins above it with its reflection | theme B, then A on top |
| 56–64 | 1:33 | **raster split** — the screen tears into bands, each band a different kernel, the boundaries dancing | riser |
| 64–80 | 1:47 | **finale** — the plane at speed, three objects, plasma for a sky | A + B, key up at bar 72 |
| 80–88 | 2:13 | **scroller** — credits ride a sine, endcard | outro |
| 88–90 | 2:27 | **power-off** — the endcard collapses to a line, a dot, dark | a thud, then nothing |

Two and a half minutes. Ninety bars. Nine thousand frames.

## 6. The music is synthesised, and it has to be a *tune*

HYSTERESIS proved a device synth works and diffed it against the host. VESPER
wrote a score. Both are drone-adjacent by design. The brief for this one is
different: **an ear-worm** — a demoscene tune with a hook you can hum, at a
tempo that moves.

That means a tracker, not a drone: patterns of 16ths, a kick, a snare, hats,
a rolling bass, a plucked arpeggio, a supersaw lead with delay, a pad for the
breakdown, a riser, and a key change for the last chorus. The synth is integer
DSP, pull-model, block-size independent (the same contract HYSTERESIS
established), rendered to WAV on the host for audition and diffed by hash
against the device.

The honest risk: I cannot hear it. The mitigations are structural — the hook
is written as a motif with repetition and answer, the arrangement follows a
form that is known to work, levels are measured not guessed, and the whole
thing is rendered to a piano roll I *can* look at. Azure hears it and says
whether it is a tune. If it is not, the tracker makes it cheap to try again.

## 7. The referee

A Pico 2 is attached, so the deadline claim is **measured**, not modelled.

1. **Telemetry over USB CDC** once a second: scene, worst line in cycles,
   lines missed, governor state, audio ring minimum. Core 1 times every line
   it generates. Every kernel's worst line over the whole run is recorded and
   goes in the README, next to the estimate above.
2. **A governor on the device**, as insurance rather than as a plan. If a
   line ever exceeds the budget, or scanline numbers show a gap, the current
   scene is switched to **half horizontal resolution** for the rest of its
   run: kernels render 320 pixels and write pairs. The picture softens; it
   does not go black. The event is counted and printed. A shipped build in
   which the governor fires is a failed build; it exists so the failure is
   visible on the monitor as softness rather than as darkness.
3. **`tools/check.py`** on the host — renders all 9,000 frames headless,
   asserts no unintended black frame, no scene that never draws, correct
   arc boundaries, and that the audio hash is independent of block size.
4. **Host and device agree** on the audio hash, sample for sample, and on
   the per-frame video hash of the kernels that do not use the interpolator
   (those go through QUICKSILVER's bit-exact emulator on the host and the SIO
   on the device, and are compared the same way).

#### Correction: it shipped as C, not as Python

`check.py` became `tools/audit.c`, built by `tools/Makefile` into `audit.exe`.
The reason is the one this plan should have seen: a Python referee has to be
fed the frames, and 9,000 frames of 640×480 BGRA is eleven gigabytes through a
pipe. Written in C it links the demo's own kernels directly, renders every
frame in-process, and runs the whole production in under a minute — so it is
run on every build rather than occasionally.

The same argument is why it links no SDL. A referee that needs a working
display can be defeated by a broken one.

Sub-rule 1 of §2 is enforced *structurally*: there is no array in the
firmware large enough to hold a frame, and the linker map proves it.

## 8. Risks

| risk | mitigation |
|---|---|
| **Underrun on hardware.** The only failure that matters. | Measure every kernel on the device the day it exists; 2× target margin on every estimate; the governor as a backstop. |
| **The music is not a tune.** | Tracker makes rewrites cheap; piano roll for structure review; levels measured. Azure decides. |
| **Solid 3D over budget on core 0.** | Flat shading, ≤ 16 spans/line, painter's sort; measured on the device. |
| **It reads as an effect pack.** | The beam is the through-line: it opens the demo, its wipes join the scenes, the split climax *is* the architecture, and the power-off ends it. |

## 9. Build order

1. Engine + host player + plasma. Prove the line contract end to end.
2. **On hardware.** The same plasma on the device, timed per line. Confirm
   or kill the budget before there is anything to protect.
3. The interpolator kernel (affine), from QUICKSILVER's emulator — the
   hardware hero, bit-exact on both targets.
4. Solid 3D span lists over the plane.
5. The rest of the kernels, screenshot by screenshot, device number by
   device number.
6. Synth + tracker + the song; WAV + piano roll; device audio ring.
7. Arc, transitions, endcard, power-off. Capture. Docs.

## 9b. Corrections — where this plan was wrong

Kept in place above and answered here, because a plan that is quietly edited
after the fact stops being evidence of anything.

**"A 154 KB tunnel lookup" (§3).** There is no tunnel lookup. The table would
have been 614 KB at full resolution, and the quarter-symmetry version that
would have fit also pins the tunnel's axis to the centre of the screen. The
scene computes angle and depth exactly every 24 pixels instead and lets the
interpolator walk between, which costs about 2,000 cycles a line, lets the axis
drift, and needs no table at all. The 76,800 bytes reserved for it in the arena
were not reclaimed until the firmware ran out of memory — twice. See `arena.h`.

**"A solid object spins above it with its reflection" (§5).** The reflection
was built and cut. It was correct — the mesh mirrored about the plane, dimmed,
clipped below the horizon — and it read as a badly drawn shadow, because a
mirror image needs a floor that looks wet and this floor is a lit grid. The
object is half again as large in its place.

**The cost estimates in §4 are all low**, by between a third and a factor of
four. Every one of them counted the arithmetic and forgot the traffic: a pixel
is not an add, it is an interpolator read, a texture load, a store and a share
of the loop. The factor of two of headroom in the budget is why this cost
nothing. The table of planned against measured is in the README.

**"Each about 9,500 cycles" (§2.1)** is 9,600: 800 pixel clocks at 25 MHz
against a 300 MHz core. The mode is also 59.75 Hz rather than 60, which is why
the audio timer divides to 23,900 Hz and not 24,000 — 400 samples per *actual*
frame. Getting that wrong would have slid the picture off the music by a frame
every four seconds, and it did, until it was measured.

## 10. What would make this a failure

- It underruns on real hardware.
- The resolution is native but the effects would have looked the same
  doubled — i.e. nothing on screen *uses* 640×480.
- The tune is forgettable.
