/* The arc. Bars are 100 frames; see PLANNING.md section 5.
 *
 * A wipe keeps BOTH scenes alive for its duration, so two scenes joined by one
 * must not share arena space (arena.h). The tunnel and the plane do share it,
 * which is why they are joined by a blind -- a blind blacks the outgoing scene
 * out before the incoming one is entered, so the two are never resident at the
 * same time. The finale follows the split on a hard cut, on the downbeat.
 */

#include "timeline.h"
#include "scenes.h"

#define BAR(b) ((uint32_t)(b) * PV_FPBAR)

static const cue_t s_cues[] = {
    { &fx_title,   BAR(0),  TR_CUT,    0 },
    { &fx_plasma,  BAR(8),  TR_WIPE, 100 },
    { &fx_kefrens, BAR(16), TR_WIPE, 100 },
    { &fx_twister, BAR(24), TR_WIPE, 100 },
    { &fx_tunnel,  BAR(32), TR_BLIND, 80 },
    { &fx_plane,   BAR(40), TR_BLIND, 80 },
    { &fx_split,   BAR(56), TR_BLIND, 60 },
    { &fx_finale,  BAR(64), TR_CUT,    0 },
    { &fx_credits, BAR(80), TR_WIPE, 100 },
    { &fx_endcard, BAR(88), TR_BLIND, 60 },
};

#ifdef PV_SOLO
/* Development only: -DPV_SOLO=n runs cue n on its own from frame 0, so a
 * kernel deep in the arc can be measured on the device without waiting a
 * minute to reach it. Never defined in a release build. */
static cue_t s_solo[1];
const cue_t *tl_cues(void)
{
    s_solo[0] = s_cues[PV_SOLO];
    s_solo[0].start = 0; s_solo[0].trans = TR_CUT; s_solo[0].n = 0;
    return s_solo;
}
int tl_cue_count(void) { return 1; }
#else
const cue_t *tl_cues(void)     { return s_cues; }
int          tl_cue_count(void){ return (int)(sizeof s_cues / sizeof s_cues[0]); }
#endif
