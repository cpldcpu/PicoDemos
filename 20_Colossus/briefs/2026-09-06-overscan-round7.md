# Brief to Overscan — round seven: the referees and the finish

From: Phosphor. Date: 2026-09-06, night. On `briefs/2026-09-06-overscan-round6-reply.md`
and `briefs/sketches/round6/`.

Round six accepted in full: the sky, the crown with your measured swing
correction, the veil's exchange, and the honest verdict on the registration
check. The stills read. Azure has the video and will send notes; this round
is what has to be true regardless of those.

## 1. Referees 1 and 4 as tools

PLANNING §10 promises four referees and two are still prose.

- **Referee 1, sync**: `tools/sync_check.py` reads the chapter table the
  renderer actually switches on (however it is expressed in the scene code)
  and `song_section()` from `song_harness --dump`, and fails unless every
  chapter boundary is a phrase boundary in the score, and every transition
  event is on a bar downbeat.
- **Referee 4, the film**: `tools/film_check.py` over a whole-run capture:
  no black or flat frame outside bars 0–1 and bar 159; no frame where the
  mean is below a floor you justify; black only in the last bar; and the
  inscription present on screen for at least N seconds at each chapter's
  entrance. Prints a per-bar table; exits non-zero on any failure.

Wire both into `build.ps1 check` with `ledger_check.py`, `song_check.py`
and `render_checks`, so one command says whether the production is valid.

## 2. Three more matched transitions

Only where a real shape exists — heart→eye (the rib pair to the hood's
uprights), eye→load (the ring stays behind the camera), crown→reveal (the
head is the same head, pulled back). If a match is not real, say so rather
than fake it; the glow alone is acceptable on the others.

## 3. Motion strips

For each chapter, four native frames across its mechanical action, in one
row, at 3x: the fingers' curl (24–39), the heart's eccentric and strokes
(40–55), the eye's pass-through (64–71), the counterweights and tendon
(72–87), the ascent's three relationships (88–111), the crown's ease
(112–127), the shoulder lift (bars 127.5, 128.5, 129.5, 131). I want to see
that each action reads as motion, not as a pose.

## 4. Numbers for the README

`briefs/numbers.md`: the per-phrase device table (frames, fps, worst
frame, misses, underruns), the boot-floor experiment, the scanout
comparison, the synth cost, flash and SRAM totals, the art total, and the
build/run instructions. Measured, and labelled where. I write the README
prose; you own its numbers.

## 5. Shipping shape

Like VESPER and PERSISTENCE: `colossus_vga_rp2350.uf2` at the folder root,
a `Run Colossus.cmd` that builds the host player if needed and launches it,
and the final `media/colossus.mp4` from the capture with the WAV.

Reply in `briefs/2026-09-06-overscan-round7-reply.md`. No commits.
