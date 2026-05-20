# AI Image Prompts for VOLTAGE (High-Voltage Cyber-Reactor Theme)

A unified **High-Voltage Cyber-Reactor** aesthetic: high-contrast electric sparks, glowing copper inductors, neon cyan and hot magenta currents, and deep obsidian/indigo grounds. Sleek, premium, metallic, and clean. No text in the AI images themselves — we render all text in code on top.

Generate at the listed source resolution (or higher). Save as PNG. Drop into `thedemo/assets/`. The packer script `tools/pack_assets.py` will automatically downscale, quantize, palettize, and emit C-incbinned blobs.

---

## Scene 1 — Title / Spark-Gap Grid (640×480)

### `title_bg_640.png` — Title screen backdrop
*   **Target resolution:** 640×480 (Source: 1920×1440 downscaled)
*   **Nano Banana Prompt:**
    > *Sleek futuristic cybernetic power grid, glowing cyan and electric violet circuits, high-voltage sparks running along metallic paths, deep dark vignette around the edges, dark premium background, 8k, photorealistic octane render, no text, no letters, no logos, no humans.*
*   **Avoid:** Any letters, shapes in the center (where the title sits).
*   **Post:** `convert title_bg_640.png -resize 640x480^ -gravity center -extent 640x480 title_bg_640.png`

---

## Scene 4 — Vector Strike Greetings Backdrop (640×480)

### `greetz_bg_640.png` — Mood backdrop for split-screen greetings
*   **Target resolution:** 640×480 (Source: 1920×1440 downscaled)
*   **Nano Banana Prompt:**
    > *Abstract premium cybernetic bokeh, soft out-of-focus electric neon cyan and hot magenta sparks, atmospheric high-voltage discharges, dark moody obsidian background, highly aesthetic, minimalist, no sharp lines, no text, 4k.*
*   **Avoid:** Sharp structures or recognizable objects. This is a background mood board.

---

## Scene 5 — Spark Generator 3D Tunnel (256×256)

### `tunnel_tex_512.png` — Seamless tileable cylindrical tunnel texture
*   **Target resolution:** 256×256 (Source: 512×512 downscaled)
*   **Nano Banana Prompt:**
    > *Seamless tileable texture of a high-tech cyber-reactor wall, circular copper coils, glowing blue electrical induction paths, electric arcs, metallic plating, seamless in both horizontal and vertical directions, 8k, flat texture, no shadows, no text.*
*   **Critical:** This image must be **completely tileable** as it wraps around the inside of our 3D cylinder.

---

## Scene 7 — Voltage Arc Credits Backdrop (640×480)

### `endcard_640.png` — Outro and credits background
*   **Target resolution:** 640×480 (Source: 1920×1440 downscaled)
*   **Nano Banana Prompt:**
    > *Industrial cyber-reactor core deck, massive metallic capacitors discharging giant glowing electrical rings, neon cyan and deep purple energy fields, bottom 30% gradually fades to pitch black, cinematic atmospheric haze, premium look, no text, 4k.*
*   **Avoid:** Any letters or bright features in the lower third (to ensure credit readability).
