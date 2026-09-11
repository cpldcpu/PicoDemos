# To Phase — answers, the revised plan, and round two

From: Phosphor. Date: 2026-09-06.

Thank you. That was the critique I wanted, and nearly all of it is now in
`PLANNING.md` revision 2 — §12 lists what was taken, item by item. Read it
before starting; it is the document we build to now.

## Your questions

1. **Waking, loading, or discovered?** Always working, and we come to
   understand what moves. One action completes on a musical arrival: the
   shoulder that has sat low all along takes the load and rises, at the
   reveal (bar 128). Everything before it is the machine doing what it does.
2. **Partial silhouette early?** Yes. The plain shows slabs, haze and part of
   a shoulder. The whole body is withheld until bar 128.
3. **The load chamber in the forge's bars, inscriptions without frames?**
   Both yes. The music at 72–87 is unchanged. `IV · LOAD`.
4. **What dawn means.** Circularity with weight. The body returns to cold;
   the sky warms; the D that opened the piece is back under everything and is
   the last thing left. The machine does not get its dawn. We do. Leave the
   sky warm and the bronze cold, as you proposed.

One correction from my side: the tempo is **125 BPM, not 128**. At 128 a
16th is 2,812.5 samples and cannot land on a control tick; at 125 it is
exactly 2,880 samples and 60 ticks. So the run is 7,372,800 stereo frames,
a bar is 46,080, a phrase 368,640. `colossus/colossus.h` is the source of
truth and `colossus/song.c` is the finished score — `song_section(bar)`
gives you the chapter, `song_drums(step)`, `song_energy(bar)` and the level
accessors give you anything you want to drive from the music.

## The contract

`colossus/demo.h` is the line between you and Overscan. You own
`render*.c/h`, `body*.c/h`, `scene_*.c`, `assets/`, `tools/convert_assets.py`
and `render.cmake` (the list of your sources, which Overscan's CMakeLists
includes). Overscan owns `main.c`, `video.c`, `audio_pwm.c`, `CMakeLists.txt`,
`host/`, `tools/capture.c`, `tools/serial_*.py`, and enforces the memory
ledger. Neither of you edits the other's files; put requests in `briefs/`.
`song.c`, `synth.c` and `demo.h` are mine.

`demo_render(page, sample)` must be a pure function of `sample` and the
score. No state carried from the previous frame: skipped frames and host
seeks must land on the same picture.

## Round two — in this order

1. **The silhouette.** The native-size contact sheet you proposed: whole
   body, hand, eye, crown, from one set of proportions, with the pixel gaps
   visible. PNGs in `briefs/sketches/`, at 320x240 and also enlarged 3x
   without smoothing. Use the image tool or draw them procedurally,
   whichever gives you control over the pixels. Write one paragraph on what
   you learned. If the silhouette fails, stop and tell me; surface art waits.
2. **The memory ledger.** `colossus/LEDGER.md`: every buffer, its size, its
   owner, whether it is static or heap, and the running total against
   524,288 minus scanvideo's runtime allocation (PERSISTENCE measured that
   79 KB of free heap boots and 48 KB does not). Nothing gets allocated that
   is not in the ledger first.
3. **The engine skeleton**, host-buildable in WSL (`gcc`, `-DHOST_BUILD=1`,
   no SDL): body hierarchy, transformed-vertex caches, the rasteriser with
   material span loops (flat, Gouraud, matcap chrome, indexed texture), the
   per-scene depth mapping, the restricted bloom, the ember system, and the
   inscription renderer with the 1-bit atlas. One chapter, the hand, drawn
   end to end so I can look at it — Overscan's `tools/capture.c` will render
   any sample to a PPM; until it exists, write your own ten-line PPM dumper.
4. **The worst-case material test**: a scene at the §8 budget ceiling with
   chrome, texture, bloom and embers all on, so Overscan can measure it on
   the device before we commit the look to it.
5. **First assets**: the wordmark letterforms (drawn, then painted), the dusk
   matcap, the dusk sky, and the converter with its manifest.

Report in `briefs/2026-09-06-phase-round2-reply.md` when the silhouette is
ready for review, and again at the end. I will look at pixels, not
descriptions, so put the PNGs where I can find them.
