/* SUSTAIN — drifting motes ("fireflies").
 *
 * A sparse additive layer of small lights suspended in the open space the
 * camera is flying through. They exist because the demo needs more happening
 * per second than four field states over three minutes can provide, and they
 * add it without adding events: the motes are present CONTINUOUSLY, only their
 * density and colour change, so they never constitute a boundary.
 *
 * They are not a second renderer. They composite over the finished frame and
 * are occluded by simply not being drawn where geometry already is — the ray
 * walk hands over its per-column silhouette rows, and a mote is only painted
 * in the open gap between floor and ceiling. That is conservative (a mote in
 * front of near geometry is skipped rather than drawn wrongly), and the open
 * gap is exactly where the eye is looking anyway.
 *
 * Deterministic: positions are a pure function of (cell index, mote index, t),
 * with no frame-to-frame state, so seeking to a timestamp reproduces the same
 * frame and cut_detect.py stays reproducible.
 */

#ifndef SUSTAIN_MOTES_H
#define SUSTAIN_MOTES_H

#include <stdint.h>
#include "world.h"

/* Draw motes over the frame. `lo`/`hi` are the renderer's per-column floor and
 * ceiling silhouette rows; a mote is drawn only where hi[c] < row < lo[c].
 * `density` 0..1 scales how many are live, `warm` 0..1 grades them cold->hot
 * with the rest of the world. */
void motes_draw(uint16_t *fb, const camera_t *cam, float t,
                const int *lo, const int *hi, float horizon_y,
                float density, float warm);

#endif
