# TESSERA artwork

Both paintings were made with the built-in image generation tool, directed by Phase (GPT-6 Astra). The second uses the first as its edit target. No API/CLI fallback was used.

- Dawn master: [ceramic_garden.png](ceramic_garden.png)
- Night master: [ceramic_garden_night.png](ceramic_garden_night.png)
- Native assets: `tessera/tools/pack_art.py` resizes each selected painting to 320×240 and packs the VGA RGB555 format. Both remain in flash; The XIP streaming FIFO feeds DMA to copy the chosen painting into the back page while geometry is prepared. The original paintings are preserved.

## Dawn prompt

```text
Use case: stylized-concept.
Asset type: painted background plate for TESSERA, a 320x240 realtime demoscene film about tiny ceramic tiles becoming a world.
Create a beautiful, sophisticated gouache and lithographic illustration, landscape 4:3 composition. An impossible Mediterranean ceramic garden / amphitheatre at dawn: chalk limestone terraces along the bottom edge, beautifully shaped cobalt blue glazed ceramic leaves and a few vermilion sculptural seed pods near the far left and far right edges, spare curving architectural arches at the outer edges. A huge soft luminous pale ochre sun occupies the central upper background against warm cream paper and hazy sand air. Tactile brush marks, subtly worn glazes, very fine print grain, rich but restrained art direction, strong readable silhouettes, modernist exhibition-poster atmosphere.
The middle 65 percent of the image MUST stay open and quiet, a spacious pale warm backdrop with no main object: moving 3D ceramic objects will be rendered there by code. Keep all detailed architecture and botanical shapes at the edges and the lowest 20 percent. No foreground object in the centre. The main shadow-receiving floor is at the bottom.
Palette: bone white, warm parchment, limestone, muted ochre, ink blue, small accents of cobalt and vermilion. Broad tonal masses must survive reduction to 320x240 and RGB555. Avoid tiny scattered confetti and busy linework.
This is the entire background artwork, full bleed, no border, no text, no typography, no logos, no watermark, no UI. Hand-painted image rather than a photograph or a 3D render.
```

## Night edit prompt

```text
Use case: lighting-weather.
Edit target: the attached ceramic garden background painting for TESSERA.
Create the NIGHT version of this exact stage. Preserve the entire composition, camera, proportions, architecture, botanical sculptures, floor pattern, central empty space, and the detailed painted/lithographic texture. Change only the illumination, sky and overall nighttime color treatment.
The sun becomes a softly glowing muted pale moon. Warm cream sky becomes deep midnight ink blue with a soft indigo atmosphere; limestone architecture is cool blue-gray with restrained moonlit edges. Cobalt ceramic leaves remain readable through subtle silvery glaze highlights. Vermilion seed pods become muted dark burgundy with a tiny warm edge. The central open background must be predominantly dark blue, so bright realtime ceramic tiles rendered over it will stand out. The horizon is quietly luminous, not black. A poetic blue-hour / moonlit companion to the dawn plate, saturated but restrained. Keep clear large value masses for reduction to 320x240 RGB555.
No text, logos, stars covering the quiet central area, added foreground objects, borders, or UI. Save as a new image; retain the original dawn artwork.
```

