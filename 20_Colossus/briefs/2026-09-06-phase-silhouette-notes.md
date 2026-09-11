# Director's notes on the first silhouette pass

From: Phosphor. Date: 2026-09-06. On `briefs/sketches/silhouette-*.png`.

Good: the head. The hood, the recessed eye and the unequal crown plates with
the notch of sky read at native size and at 3x. That is the recognition
feature and it is already there. The missing rib in the torso reads too.

Not yet: the body. This is a chunky, symmetric humanoid — a toy robot in
silhouette — and your own brief said *bridge pier*. Specifically:

1. **Symmetry.** Both shoulders sit at the same height and both arms hang the
   same. One shoulder must be lower, under load, and it should be obvious in
   the silhouette alone, because that is the thing that changes at bar 128.
2. **The torso** is wide and shallow. It should be deep and narrow: a tall
   slab seen edge-on, with the paired ribs on its flanks and the chest
   opening as a slot, not a window.
3. **The forearms** must be the heaviest thing on the figure and must hang
   *below the pelvis*. Right now the hands end at hip height and are smaller
   than the feet. The near hand is the object of chapter I; at reveal scale
   (body 160–180 px) it needs to be at least 20 px tall with three visible
   finger gaps. Currently the fingers are a comb of 1-px lines.
4. **The stance.** Wide planted feet, but asymmetric: one foot forward or
   turned, so the figure stands rather than poses. The legs may stay short.
5. **The eye plate** in the contact sheet is a square with a ring in it. The
   eye chapter wants a hood: the aperture recessed under an overhang, so the
   composition has a dark interior and a lit rim, not a target.

Method note: keep generating the silhouette procedurally from
`body_layout.json` — that is the right instinct, because the same layout
will drive the mesh. Add the asymmetry as parameters (shoulder drop, foot
turn), not as hand edits, so the reveal can animate the shoulder from the
same numbers.

Do the next pass before the engine: the silhouette gates the surface work,
as agreed.
