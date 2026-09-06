/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Scored to "Second Key Change" (assets/, 3:04, 140 BPM). BEAT-SYNCED: cuts land
 * on onsets/structural edges (analyze_music.py) tuned against a video playback.
 * ROTOZOOM appears ONCE; TUNNEL appears ONCE — saved as the CLIMAX, the most
 * awesome effect. CHROME, LIQUID and MODE7 each appear twice but in clearly
 * DIFFERENT variants (so no pass reads as a repeat). Effects are ordered by
 * rising "awesomeness", matched to the song's energy — the two highest-impact
 * effects land on the two musical peaks (the long DROP 1 and the CLIMAX):
 *
 *   0:00  title     intro, ends on the 0:13 break
 *   0:13  mode7     mercury-plain GROOVE cruise (cool silver) — calm opener
 *   0:27  liquid    plasma riser (cool silver, soft) — energetic rush
 *   0:41  rotozoom  the one zoomer — groove
 *   0:54  liquid    bump-mapped mercury (moving light) — the showpiece
 *   1:08  chrome A  DROP 1 (ICO neutral + KNOT violet + TORUS gold)
 *   1:49  mode7     warm "lap" cruise — breakdown that re-tensions
 *   2:02  tunnel    voxel conduit — the CLIMAX, apex effect
 *   2:18  chrome B  CLIMAX reprise (SPIKE gold + KNOT2 violet) — short punch
 *   2:25  credits   outro on the melody — slower readable roll (~40s), ends w/music (3:04)
 *
 * Variant selection (effects pick their look from their start time): CHROME and
 * MODE7 split on 100 s (one instance each side); LIQUID's two instances are both
 * before 100 s, so liquid.c splits them at 40 s (plasma < 40 s, bump >= 40 s).
 */

#include "scene.h"
#include "effects/transition.h"

extern const effect_t fx_title;
extern const effect_t fx_rotozoom;
extern const effect_t fx_tunnel;
extern const effect_t fx_mode7;
extern const effect_t fx_chrome;
extern const effect_t fx_liquid;
extern const effect_t fx_credits;

const timeline_entry_t timeline[] = {
    {      0,  13000, &fx_title    },   /* 1 intro  : brand -> wordmark; 0:13 break  */
    {  13000,  27000, &fx_mode7    },   /* 2 groove : mercury plain (cool) calm open */
    {  27000,  41190, &fx_liquid   },   /* 3 riser  : plasma (cool silver, soft)     */
    {  41190,  54000, &fx_rotozoom },   /* 4 groove : the one zoomer (energetic)     */
    {  54000,  68000, &fx_liquid   },   /* 5 showpc : bump-mapped mercury (moving lt)*/
    {  68000, 109230, &fx_chrome   },   /* 6 DROP 1 : chrome A (ICO+KNOT+TORUS, 3)   */
    { 109230, 122420, &fx_mode7    },   /* 7 break  : mercury plain (warm lap) tension*/
    { 122420, 138070, &fx_tunnel   },   /* 8 CLIMAX : voxel conduit — apex effect    */
    { 138070, 145000, &fx_chrome   },   /* 9 punch  : chrome B (SPIKE+KNOT2), short  */
    { 145000, 184840, &fx_credits  },   /* 10 outro : slow readable credits (~40s); ends w/music*/
};

/* transition used when LEAVING each entry (punchy SLAM on each drop) */
const uint8_t timeline_trans[] = {
    QS_TR_IRIS,     /* 1 title  -> mode7    : iris opens onto the plain  */
    QS_TR_DISSOLVE, /* 2 mode7  -> liquid   : speckle into the riser     */
    QS_TR_WIPE,     /* 3 liquid -> rotozoom : sweep into the zoomer      */
    QS_TR_MELT,     /* 4 roto   -> liquid   : melt into the mercury      */
    QS_TR_BLINDS,   /* 5 liquid -> chrome   : blinds SLAM on DROP 1      */
    QS_TR_DISSOLVE, /* 6 chrome -> mode7    : speckle into the breakdown */
    QS_TR_BLINDS,   /* 7 mode7  -> tunnel   : blinds SLAM on the CLIMAX  */
    QS_TR_WIPE,     /* 8 tunnel -> chrome   : sweep into the punch       */
    QS_TR_MELT,     /* 9 chrome -> credits  : melt into the finale       */
    QS_TR_MELT,     /* 10 credits-> end     : (suppressed)               */
};

const int timeline_count = (int)(sizeof(timeline) / sizeof(timeline[0]));
