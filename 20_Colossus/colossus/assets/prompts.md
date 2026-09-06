# Phase asset provenance · built-in image_gen · 2026-09-06

All three masters were generated with the built-in image tool. No CLI/API
fallback. Masters are retained under `source/`; the converter emits the shipped
sizes and exact DAC previews. The wordmark edit target was inspected first.
Sketches and drawn letterforms are procedural, as permitted by the brief.

## Wordmark — edit of `source/wordmark-drawn.png`

Use case: sketch-to-render. Paint this exact drawn COLOSSUS wordmark for a 320x64 demoscene bitmap. Preserve the eight letter outlines, spacing, incision gaps and counters exactly: C O L O S S U S. The reference is the edit target, not merely inspiration. Broad worn bronze piers, dark bronze #302820, body #706048, very sparse grazing #B89868 edges. A single low cool sky opening lights upper-left edges. Recesses #101820. Quiet monumental hand-painted metal; broad planes, restrained wear that survives 4-bit quantization. Flat frontal lettering, no perspective, no extruded sides, no extra symbols, no glow or lens effects. Exact dark #101820 background. Preserve full word and margins. This will be reduced to 320x64 and masked back to the original drawn silhouettes; paint inside the letters.

Saved: `source/wordmark-painted-master.png`; shipped: `wordmark-quantized.png`.
The converter detects the painted bronze bounds, records the crop, resizes,
and reapplies the exact drawn mask. This corrects generated margins and keeps
the recognition gaps even where the painting softened them.

## Dusk matcap — new image

Use case: stylized-concept. Asset type: painted dusk chrome MATCAP texture for a low-resolution 3D demoscene. Square image, exactly one perfectly circular orthographically viewed polished metal sphere centered, touching all four sides; no perspective, no cast shadow, no border, no other objects, no text. Sphere is a normal lookup map to reduce to 64x64. Broad cool blue-gray sky reflection across upper half (#8098A8), ground reflected in lower half (#283840), deep occlusion #101820 near bottom. One narrow but soft pale #D8E0E0 light band diagonally grazing the upper-left quadrant, broad readable dark/light shapes, very restrained bronze reflection at the bottom edge. Painted environment reflection only, no small landscape details, no stars, no sparkles, no furnace, no orange sky. Outside circle fill #283840. Metallic appearance should survive 8-bit indexed color and five-bit DAC. Smooth quiet monumental lighting, no grain.

Saved: `source/dusk-matcap-master.png`; shipped: `dusk_matcap-quantized.png`.

## Dusk sky — new image

Use case: stylized-concept. Asset type: dusk sky panoramic painted texture, shipped at 256x64 in a 320x240 demoscene. Wide horizontal composition. ONLY empty sky, no ground, no mountains, no buildings, no sun disk, no stars, no text. Cold twilight: upper deep blue-black #101820 and muted blue #283840, a broad low opening of cool blue-gray #8098A8 near bottom quarter. Very soft long horizontal cloud veils with very low contrast, generous calm areas, no fine texture. Upper-left slightly brighter than upper-right to define one persistent world light. Horizon stays cold; this is dusk before the dawn. Hand-painted environment art, restrained and atmospheric, built from large tonal shapes that survive an 8-bit palette and five-bit DAC. Edge to edge, no border. Horizontal panorama aspect 4:1 if possible.

Saved: `source/dusk-sky-master.png`; shipped: `dusk_sky-quantized.png`.

## Rebuild

`python3 colossus/assets/silhouette.py` regenerates body layout/header and plates.
`python3 colossus/assets/draw_wordmark.py` regenerates the drawing.
`python3 colossus/assets/make_engine_assets.py` regenerates atlas and test tile.
`python3 colossus/tools/convert_assets.py` emits all five indexed/bit assets,
headers, palettes, quantized previews and the manifest; `--check` verifies
byte-for-byte reproduction. It uses fixed RGB palettes, deterministic first-index
ties, no stochastic quantizer, and records Pillow's version. Color conversion
and asset resampling are offline; the renderer reads aligned const flash data.
`python3 colossus/assets/review.py` compiles the WSL executable and regenerates
native/3x stills, a 7.68-second chrome GIF, and exact-color PNG keyframes.
