# AI prompts for 14_Origami ("ORIGAMI")

**Paper-craft and pastel aesthetic:** soft warm whites, light pastels, clean folds, hand-made textures.
**No text in any AI image** — all text is rendered in code on top with `font8x8.h`.

Generate at the listed source resolution (or higher). Save as PNG into `14_Origami/assets/`.

After generating, batch-normalize from WSL:

```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/PicoDemos/14_Origami/assets && for f in *.png; do convert \"\$f\" -strip -auto-orient \"\$f\"; done"
```

---

## 1. Washi Paper Texture Tile

### `washi_paper_tile_512.png` — subtle facet texture / overlay
**Source res:** 512×512 (seamless/tileable)
**Prompt:**
> *Seamless tileable close-up of textured washi paper, fine fibers, soft warm white, faint deckle grain, even lighting, no text.*

**Avoid:** vignettes, strong shadows, text.

---

## 2. Scene 1 Backdrop

### `paper_sky_640.png` — scene-1 backdrop
**Source res:** 1280×960 → 640×480
**Prompt:**
> *A soft pastel paper-craft sky, cut-paper clouds layered with gentle drop shadows, sky-blue gradient, flat hand-made collage look, no text.*

**Avoid:** text.
**Post:**
```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/PicoDemos/14_Origami/assets && \
  convert paper_sky_640.png -resize 640x480^ -gravity center -extent 640x480 paper_sky_640.png"
```

---

## 3. Rebirth Endcard

### `endcard_paper_640.png` — credits backdrop
**Source res:** 1280×960 → 640×480
**Prompt:**
> *A calm flat-lay of folded pastel paper shapes on a cream surface with soft shadows, empty space in the lower third for text, hand-made paper-craft, no text.*

**Avoid:** text, busy detail in the bottom third.
**Post:**
```bash
wsl -e bash -c "cd /mnt/d/Toyprojects/PicoDemos/14_Origami/assets && \
  convert endcard_paper_640.png -resize 640x480^ -gravity center -extent 640x480 endcard_paper_640.png"
```
