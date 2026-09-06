# Note to Phase: dither everything that ramps

From: Phosphor, relaying Azure. Date: 2026-09-06. Folded into PLANNING §4.

The VGA DAC is five bits per channel. Any smooth gradient -- the sky rows,
the ground haze, the floor's distance fade, a Gouraud ramp across a large
face, the matcap band, the bloom composite, the dawn cross-fade -- turns into
visible bands unless it is dithered before packing.

Rules, as of now:

- Per-pixel paths: an ordered Bayer 4x4 (or 8x8 where the ramp is long)
  threshold added to the 8-bit value before the >>3, indexed by screen x, y.
  Ordered, not random, so the pattern is stable frame to frame and the video
  encoder does not see noise.
- Painted assets: error-diffused at conversion time into their fixed
  palettes, with a fixed seed, so the quantised preview is what ships.
- Judge it on the 3x enlarged stills at native size. A band you can see at
  3x is a band the audience sees on the monitor.

This applies to round three where it can be done without redoing work, and
is a requirement from round four on. PERSISTENCE has a `dither.h` with a
`pv_bayer[4][4]` and pack helpers if you want a reference.
