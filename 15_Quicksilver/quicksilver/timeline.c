/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Boundaries snapped to "Quicksilver Reflections" sections (analyze_music.py,
 * 129 BPM, 174.9 s). Key moments the cuts ride: first onset @20.5, the drop
 * cluster @55-62, and the BIGGEST hit @123.69 — which we land as the
 * liquid->chrome DROP so the second chrome burst slams in on the peak. The
 * outro's last section edge is only ~4 s from the end, so credits open early
 * (@149.4) to give the scroller + end card a full finale. The chrome scene
 * appears twice with DISJOINT objects (0..2 then 3..5) so none repeats.
 */

#include "scene.h"
#include "effects/transition.h"

extern const effect_t fx_title;
extern const effect_t fx_rotozoom;
extern const effect_t fx_mode7;
extern const effect_t fx_chrome;
extern const effect_t fx_liquid;
extern const effect_t fx_credits;

const timeline_entry_t timeline[] = {
    {      0,  20500, &fx_title    },   /* intro: brand reveal -> wordmark (1st onset @20.5) */
    {  20500,  54870, &fx_rotozoom },   /* build: rubber rotozoomer (drop @55 ends it) */
    {  54870,  86360, &fx_mode7    },   /* drop section: mercury plain (seg @86.36) */
    {  86360, 108510, &fx_chrome   },   /* chrome objects 0,1,2 (seg @108.51) */
    { 108510, 123690, &fx_liquid   },   /* breakdown: liquid metal -> the BIG hit @123.69 */
    { 123690, 149420, &fx_chrome   },   /* DROP on the peak: chrome objects 3,4,5 (seg @149.42) */
    { 149420, 174920, &fx_credits  },   /* outro: credits + end card; ends with the music */
};

/* transition used when LEAVING each entry (themed to the pair it joins) */
const uint8_t timeline_trans[] = {
    QS_TR_IRIS,     /* title  -> rotozoom : iris open                 */
    QS_TR_WIPE,     /* roto   -> mode7    : clean sweep onto the plain*/
    QS_TR_MELT,     /* mode7  -> chrome   : melt into the chrome       */
    QS_TR_DISSOLVE, /* chrome -> liquid   : speckle into the plasma    */
    QS_TR_BLINDS,   /* liquid -> chrome   : blinds into the drop       */
    QS_TR_MELT,     /* chrome -> credits  : melt into the finale       */
    QS_TR_MELT,     /* credits-> end      : (suppressed)               */
};

const int timeline_count = (int)(sizeof(timeline) / sizeof(timeline[0]));
