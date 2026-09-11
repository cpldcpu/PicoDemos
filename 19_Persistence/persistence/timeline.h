/* The arc, as a table of cues.
 *
 * A cue says which scene owns the screen from `start` and how the rows are
 * handed over from the previous scene:
 *
 *   TR_CUT    at `start`, every row.
 *   TR_WIPE   over n frames from `start`, the new scene takes rows from the
 *             top down (beam_frame moves the split). Both scenes' frame()
 *             run; both must be resident in the arena at once.
 *   TR_BLIND  the previous scene blacks out in a venetian pattern over the
 *             n/2 frames BEFORE `start`; the new scene reveals the same way
 *             over the n/2 frames after. Its enter() runs at `start`.
 */

#ifndef PV_TIMELINE_H
#define PV_TIMELINE_H

#include "beam.h"

enum { TR_CUT = 0, TR_WIPE = 1, TR_BLIND = 2 };

typedef struct {
    const scene_t *scene;
    uint32_t start;         /* absolute frame */
    uint8_t  trans;
    uint16_t n;             /* transition length in frames */
} cue_t;

#define TL_MAX_CUES 32

const cue_t *tl_cues(void);
int          tl_cue_count(void);

#endif
