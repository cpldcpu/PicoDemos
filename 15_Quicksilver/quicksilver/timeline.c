/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Scored to "Taiko Dorian Bells", TRIMMED to 3:15 (tools/trim_music.sh cuts the
 * saggy sustained centre; see music.qoa). Earlier cuts dragged — 7 effects over
 * 4:09 meant ~35 s scenes. This is a TIGHT re-cut: 12 scenes, ~16 s each, laid
 * on the trimmed track's segment edges / strong onsets (analyze_music.py).
 *
 * CHROME lands on all THREE drops, each with a DISJOINT object pair (A=0,1 /
 * B=2,3 / C=4,5 — see chrome.c), so the centrepiece keeps coming back with
 * fresh objects. The texture effects (rotozoom / mode7 / liquid) revisit at
 * well-spaced intervals to keep the pace up between drops.
 *   33.7  DROP 1            -> chrome A (objects 0,1)
 *   100.1 BIGGEST drop      -> chrome B (objects 2,3)
 *   131.2 second drop       -> chrome C (objects 4,5)
 *   170.3 outro             -> credits, ends with the music
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
    {      0,  15000, &fx_title    },   /*  1 intro: brand -> wordmark (tight)     */
    {  15000,  33690, &fx_rotozoom },   /*  2 build: beam-raced rotozoomer -> drop1*/
    {  33690,  50670, &fx_chrome   },   /*  3 DROP 1: chrome A (objects 0,1)       */
    {  50670,  63480, &fx_mode7    },   /*  4 mercury plain                        */
    {  63480,  76140, &fx_liquid   },   /*  5 liquid metal                         */
    {  76140,  85910, &fx_rotozoom },   /*  6 rotozoomer (revisit, short punch)    */
    {  85910, 100100, &fx_mode7    },   /*  7 mercury plain (revisit) -> big drop  */
    { 100100, 116120, &fx_chrome   },   /*  8 BIGGEST DROP: chrome B (objects 2,3) */
    { 116120, 131220, &fx_liquid   },   /*  9 liquid metal (revisit)               */
    { 131220, 151230, &fx_chrome   },   /* 10 second drop: chrome C (objects 4,5)  */
    { 151230, 170320, &fx_mode7    },   /* 11 mercury plain (cruise to the outro)  */
    { 170320, 195500, &fx_credits  },   /* 12 outro: credits + end card; ends w/music*/
};

/* transition used when LEAVING each entry (varied; punchy SLAM on each drop) */
const uint8_t timeline_trans[] = {
    QS_TR_IRIS,     /*  1 title  -> rotozoom : iris opens                */
    QS_TR_BLINDS,   /*  2 roto   -> chrome   : blinds SLAM on DROP 1     */
    QS_TR_WIPE,     /*  3 chrome -> mode7    : sweep onto the plain      */
    QS_TR_DISSOLVE, /*  4 mode7  -> liquid   : speckle into the plasma   */
    QS_TR_MELT,     /*  5 liquid -> rotozoom : melt into the zoomer      */
    QS_TR_WIPE,     /*  6 roto   -> mode7    : sweep onto the plain      */
    QS_TR_BLINDS,   /*  7 mode7  -> chrome   : blinds SLAM on the big drop*/
    QS_TR_DISSOLVE, /*  8 chrome -> liquid   : speckle into the plasma   */
    QS_TR_BLINDS,   /*  9 liquid -> chrome   : blinds SLAM on 2nd drop   */
    QS_TR_WIPE,     /* 10 chrome -> mode7    : sweep onto the plain      */
    QS_TR_MELT,     /* 11 mode7  -> credits  : melt into the finale      */
    QS_TR_MELT,     /* 12 credits-> end      : (suppressed)              */
};

const int timeline_count = (int)(sizeof(timeline) / sizeof(timeline[0]));
