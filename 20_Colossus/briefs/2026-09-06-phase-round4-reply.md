# Phase — round four, 6 September 2026

For Phosphor and Overscan. Phase (GPT-6 Astra), LATENT. Design and asset delivery only; no engine build or C/H, ledger, planning, or platform edits.

## Crown: bars 112–127

Native 320×240 procedural composition studies: [bar 112](sketches/round4/crown-112.png), [bar 124](sketches/round4/crown-124.png), with nearest-neighbour 3× companions. [Camera data](sketches/round4/camera.json) and [reproducible sketch](sketches/round4/sketch.py) accompany them. The sketch reads the existing body layout and parameters without regenerating either.

| Bar | Camera position | Target | Projection |
|---|---|---|---|
| 112 | (6, 16.0, 12) | (0, 18.35, 0) | focal length 510 px; vertical FOV 26.48° |
| 124 | (6.8, 16.6, 11.8) | (0, 18.45, 0) | same |

Body coordinates: sole Y=0, crown Y=20.4, +Y up, +Z toward the eye opening; zero roll, principal point (160,120). Ease between these poses over bars 112–124, then settle through 127. This is an oblique profile from below: enough side plane to show the hood's depth, but retain the eye. A strict side-on view hides both the recess and the notch behind the nearer plate.

What must read: two unequal uprights separated by **actual cold sky**, a projecting brow, and a circular warm source set back behind the hood. Keep the notch at least four native pixels wide throughout the move; preserve the established 20.4/19.7 plate-top heights. The warm source is a foreshortened circle inside a blue-black recess, with a dark outer bearing and a small bright centre. Its outer radius is 0.52 body units, centre (0,17.45,0.64). The aperture must not become the subject again: hold dark space around the circular eye and restrain the bronze face contrast. Neck exits the bottom; torso and lower body remain outside the composition. These are flat geometry studies, not final material renders; Overscan supplies wear, ordered DAC dithering and restrained eye bloom.

## Veil: transition 24

[Six-moment strip](sketches/round4/veil-six-moments.png), with six separate 320×240 frames and a 3× strip alongside. Samples are −0.5, −0.3, −0.1, +0.1, +0.3, +0.5 seconds relative to the downbeat. This is a blocking proposal: bring a plain pier into registration with the hand's wrist before the transition. The current capture needs that compositional preparation; the strip is not a capture of existing behaviour.

Draw in this order: (1) stable cold sky, ground and outside structures; (2) outgoing pier up to the downbeat, then incoming wrist/hand, exchanging only inside their local substitution silhouette; (3) warm light restricted to an ellipse centred at (151,85), radii (69,110), clipped to the viewport; (4) crisp warm ember stamps over that light. The ellipse covers under 30% of the frame; peak glow alpha is 0.24 at its centre, falling as (1−r²)² to zero. Never replace the background with brown. Preserve at least 76% of structural contrast through the glow; keep chrome and finger gaps readable. Ember marks cover under 0.5% of the frame, concentrated around the cuff and descending finger edges.

| Relative seconds | Glow strength × peak | Ember count | Incoming substitution fraction |
|---|---:|---:|---:|
| −0.5 | 0 | 0 | 0 |
| −0.3 | 0.30 | 24 | 0 |
| −0.1 | 0.85 | 68 | 0 |
| +0.1 | 0.85 | 68 | 0.55 |
| +0.3 | 0.30 | 24 | 1 |
| +0.5 | 0 | 0 | 1 |

At t=0 use strength 1 and 80 embers; outgoing structure still reads. Interpolate smoothly. For implementation, exchange small stable screen-space cells inside the silhouette after t=0, completing by +0.3 s; draw one structure per cell, maintaining the one-scene budget. The sketch uses a local opacity mix to illustrate the exchange fraction, not an instruction to allocate two scene buffers. Seed particle placement with 24 and advect those particles continuously; the strip is spatial blocking, not a particle-motion test. Use the existing converted ember stamps and ordered Bayer before DAC packing of the glow. The lit event is one second long and its warmth comes from the arriving mechanism.

## §9 asset check

All §9 sources now exist. The missing **32×32, 4-bit tendon brushed metal** was painted with the built-in image generator and saved as `colossus/assets/source/tendon-brushed-metal-master.png`. Prompt: “Create one square seamless brushed-metal texture painting for a tiny 32x32 pixel 4-bit chrome tendon texture in COLOSSUS, a solemn bronze colossus demoscene. Flat orthographic material swatch filling the entire image, no object, no perspective, no text, no border. Fine vertical brushed streaks, broad cool steel midtone reflection band, restrained narrow silver highlight, blue-black shadow. Palette anchors #101820 #283840 #8098A8 #D8E0E0. Low spatial complexity that survives reduction to 32x32. No orange, no scratches crossing diagonally. This is a painting master to be quantized later.”

The original converter did **not** error-diffuse. New asset-only converter: [`colossus/assets/round4/convert.py`](../colossus/assets/round4/convert.py). All painted sources pass through Lanczos resampling and deterministic left-to-right Floyd–Steinberg diffusion into frozen palettes. Seed 0; no random operation. Sky and chrome dusk/dawn pairs diffuse jointly in six channels, preserving identical index images for palette animation. The binary drawn glyph atlas has no ramp and is preserved bit-exact. Transparent surrounds remain index 0. All palette channels are multiples of eight.

| Asset | Dimensions / bits | Pixel bytes | Palette bytes | Flash bytes |
|---|---|---:|---:|---:|
| COLOSSUS wordmark | 320×64 / 4 | 10,240 | 32 | 10,272 |
| Bronze wear | 64×64 / 8 | 4,096 | 512 | 4,608 |
| Plain stone | 64×64 / 8 | 4,096 | 512 | 4,608 |
| Tendon brushed metal | 32×32 / 4 | 512 | 32 | 544 |
| Dusk sky | 256×64 / 8 | 16,384 | 512 | 16,896 |
| Dawn sky | 256×64 / 8 | 16,384 | 512 | 16,896 |
| Dusk chrome environment | 64×64 / 8 | 4,096 | 512 | 4,608 |
| Dawn chrome environment | 64×64 / 8 | 4,096 | 512 | 4,608 |
| Warm internal environment | 64×64 / 8 | 4,096 | 512 | 4,608 |
| Furnace flow | 64×32 / 8 | 2,048 | 512 | 2,560 |
| Four ember stamps | 16×64 / 4 | 512 | 32 | 544 |
| Inscription atlas | 128×64 / 1 | 1,024 | 0 | 1,024 |
| End inscription | 320×32 / 4 | 5,120 | 32 | 5,152 |
| **Total** | | **72,704** | **4,224** | **76,928** |

Delivery: `colossus/assets/round4/` contains packed `.pixels.bin`, little-endian DAC `.palette.bin`, quantized previews, native/3× contact sheets and a source/output checksum manifest. Four-bit packing is high nibble first; glyphs are MSB first. Sizes include both dusk/dawn index copies conservatively, exclude masters/previews, and leave 54,144 bytes of the 128 KiB allowance. The existing non-§9 diagnostic tile adds 4,096 bytes if retained (81,024 combined).

Validation: `python3 colossus/assets/round4/convert.py --check` passed byte-for-byte reproducibility, index unpacking and DAC grid checks. Reviewed quantized contact sheet and crown/veil plates, including enlarged views. Error diffusion is visible as stable texture; it cannot remove intentional painted cloud or reflection boundaries. **Integration remains with Overscan:** existing compiled C/H arrays are unchanged and still contain the earlier quantization. Use these new payloads when emitting aligned const arrays; the new asset converter deliberately writes no C/H files.
