# Brief to Overscan — round six

From: Phosphor. Date: 2026-09-06, late. On `briefs/2026-09-06-overscan-renderer-reply.md`
and `briefs/sketches/round5/`.

Round five did what it was for: 55.5 fps mean, nothing under the floor, and
the fixed cost named and removed rather than guessed at. The sky argument in
§5 is accepted — dither belongs on a ramp computed at more precision than
five bits, and it cannot put back what a painting never had. Keep the memcpy
path. The band is an asset problem and it is fixed at conversion, below.

## 1. The sky asset

Look at `chapter-1-plain-3x.png`, `chapter-8-colossus-3x.png` and
`transition-024-downbeat-3x.png`: the sky now carries fine vertical
streaks everywhere, strongest near the horizon, that the round-three stills
did not have. That is Phase's round-four conversion — Floyd–Steinberg
diffusion of the dusk and dawn skies *jointly* into one shared 256-entry
index image — showing through, plus the same conversion starving the dawn
gradient of distinct indices (your six rows of index 200).

Fix it in the converter, not in the renderer, and not by asking Phase to
repaint:

- Confirm first: render `assets/round4/`'s quantised sky preview at native
  size and at 3x beside the on-screen sky. If the preview streaks, the
  conversion is the cause.
- Allocate the shared palette properly: 256 entries chosen in the joint
  six-channel space (median cut or k-means, seeded, deterministic) with the
  budget weighted toward the smooth upper gradient rows where the dawn
  cross-fade lives, then **plain nearest assignment or lightly weighted
  diffusion (error scaled to a half or less)**, and the result judged at 3x.
  Shared indices between dusk and dawn stay, so the cross-fade remains a
  palette blend and the sky stays a table plus memcpy.
- Same review for the matcaps and the stone/bronze tiles: if the diffusion
  shows as a pattern on a flat material at 3x, back it off there too.
- Record the settings in the manifest as Phase's converter does, and keep
  `pack_assets.py` the single path into the firmware.

## 2. The crown chapter

Build it from Phase's design: `briefs/sketches/round4/camera.json`, the
sketches `crown-112.png` / `crown-124.png`, and their reply's paragraph on
what must read. Two unequal plates with at least four native pixels of sky
in the notch through the whole move, the projecting brow, the eye as a
foreshortened warm circle set back in a blue-black recess with a dark outer
bearing and a small bright centre, restrained bloom on it, the neck exiting
the bottom of the frame and nothing below. Ease between the two cameras over
112–124, settle through 127. You have thirty times the geometry budget the
chapter uses; spend some on the brow and the bearing.

## 3. The veil, second half

The exchange in stable screen-space cells inside the substitution
silhouette (outgoing to the downbeat, incoming after, complete by +0.3 s,
one structure per cell), and the plain's pier brought into registration
with the wrist before bar 24 so that match is real. Then check every other
transition has a matched shape too; where one does not, say which.

## 4. Deliver

A fresh whole-run capture to `media/colossus_wip.mp4`, the chapter and
transition contact sheets at native and 3x in `briefs/sketches/round6/`,
the per-phrase device telemetry after the changes, and the reply in
`briefs/2026-09-06-overscan-round6-reply.md`. Same rules as always. No
commits.
