/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Scored to "Second Key Change" (assets/, 3:04, 140 BPM). BEAT-SYNCED: cuts land
 * on onsets/structural edges (analyze_music.py) tuned against a video playback.
 * ROTOZOOM appears ONCE (twice was too much); TUNNEL takes that slot. CHROME and
 * LIQUID and MODE7 each appear twice but in clearly DIFFERENT variants (so no
 * pass reads as a repeat). CHROME lands on the two big drops. Energy arc:
 *
 *   0:00  title     intro, ends on the 0:13 break
 *   0:13  rotozoom  the one zoomer — energetic rise
 *   0:27  tunnel    chrome conduit, pulsing rush (effect-change break)
 *   0:41  mode7     mercury-plain GROOVE cruise (cool silver)
 *   0:54  liquid    BUILD — fast plasma riser into drop 1 (0:54 music break)
 *   1:08  chrome A  DROP 1 (ICO neutral + KNOT violet + TORUS gold)
 *   1:49  liquid    breakdown, re-tensions into the climax
 *   2:02  chrome B  CLIMAX (SPIKE gold + KNOT2 violet)
 *   2:18  mode7     "victory lap" (warm copper, low & fast, banking) — short
 *   2:25  credits   outro on the melody — slower readable roll (~40s), ends w/music (3:04)
 *
 * If the track is swapped, keep the two CHROME / LIQUID starts on opposite sides
 * of 100 s (chrome.c, liquid.c, mode7.c all pick their variant off that 100 s
 * threshold).
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
    {  13000,  27000, &fx_rotozoom },   /* 2 rise   : the one zoomer (energetic)     */
    {  27000,  41190, &fx_tunnel   },   /* 3 conduit: chrome tunnel                  */
    {  41190,  54000, &fx_mode7    },   /* 4 groove : mercury plain (ends 0:54 break)*/
    {  54000,  68000, &fx_liquid   },   /* 5 build  : liquid riser into drop 1       */
    {  68000, 109230, &fx_chrome   },   /* 6 DROP 1 : chrome A (ICO+KNOT+TORUS, 3)   */
    { 109230, 122420, &fx_liquid   },   /* 7 break  : liquid breakdown (re-tensions) */
    { 122420, 138070, &fx_chrome   },   /* 8 CLIMAX : chrome B (SPIKE + KNOT2)       */
    { 138070, 145000, &fx_mode7    },   /* 9 lap    : mercury plain (reprise, warm, short)*/
    { 145000, 184840, &fx_credits  },   /* 10 outro : slow readable credits (~40s); ends w/music*/
};

/* transition used when LEAVING each entry (punchy SLAM on each drop) */
const uint8_t timeline_trans[] = {
    QS_TR_IRIS,     /* 1 title  -> rotozoom : iris opens                 */
    QS_TR_WIPE,     /* 2 roto   -> tunnel   : sweep into the conduit     */
    QS_TR_MELT,     /* 3 tunnel -> mode7    : melt onto the plain        */
    QS_TR_DISSOLVE, /* 4 mode7  -> liquid   : speckle into the riser     */
    QS_TR_BLINDS,   /* 5 liquid -> chrome   : blinds SLAM on DROP 1      */
    QS_TR_DISSOLVE, /* 6 chrome -> liquid   : speckle into the breakdown */
    QS_TR_BLINDS,   /* 7 liquid -> chrome   : blinds SLAM on the CLIMAX  */
    QS_TR_WIPE,     /* 8 chrome -> mode7    : sweep onto the plain       */
    QS_TR_MELT,     /* 9 mode7  -> credits  : melt into the finale       */
    QS_TR_MELT,     /* 10 credits-> end     : (suppressed)               */
};

const int timeline_count = (int)(sizeof(timeline) / sizeof(timeline[0]));
