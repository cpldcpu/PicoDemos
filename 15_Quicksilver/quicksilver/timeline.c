/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Boundaries ride the PRODUCER's own section markers (the .srt/.lrc in
 * assets/ for "Quicksilver Reflections", 129 BPM, 174.9 s), cross-checked
 * against analyze_music.py:
 *   10.3 intro (whispered "quicksilver" @10.6)  -> title holds here
 *   25.5 DROP 1 "full energy" / "liquid chrome" -> SLAM into the rotozoomer
 *   47.2 breakdown "running through my hands"   -> (within rotozoom)
 *   54.9 DROP 2 "bigger, double-time"           -> mercury plain enters
 *   62.7 "shine" (long main section)            -> chrome + liquid ride it
 *   123.7 biggest hit                           -> liquid->chrome DROP on the peak
 *   148.4 "shine" reprise (outro)               -> credits finale
 * The chrome scene appears twice with DISJOINT objects (0..2 then 3..5).
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
    {      0,  25530, &fx_title    },   /* intro: brand -> wordmark, holds to DROP 1 @25.53 */
    {  25530,  54940, &fx_rotozoom },   /* DROP 1 "liquid chrome" + breakdown; rotozoomer rides it */
    {  54940,  86360, &fx_mode7    },   /* DROP 2 double-time: mercury plain */
    {  86360, 108510, &fx_chrome   },   /* "shine": chrome objects 0,1,2 */
    { 108510, 123690, &fx_liquid   },   /* "shine": liquid metal -> the BIG hit @123.69 */
    { 123690, 148400, &fx_chrome   },   /* DROP on the peak: chrome objects 3,4,5 */
    { 148400, 174920, &fx_credits  },   /* outro "shine" reprise: credits + end card; ends w/ music */
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
