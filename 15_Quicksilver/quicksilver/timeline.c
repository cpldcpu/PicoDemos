/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Scored to "Taiko Dorian Bells" (instrumental, 129 BPM, 249.4 s). No lyric
 * sheet this time, so boundaries are extracted with analyze_music.py and laid
 * on its structural segment edges / strongest onsets. The track has TWO energy
 * peaks bracketing a long sustained centre, so the two chrome bursts land on
 * the peaks and the cruise/breakdown fill the rest:
 *   33.7  DROP 1 (onset 13.5)        -> SLAM into the beam-raced rotozoomer
 *   63.5  section change             -> liquid metal builds toward the peak
 *   100.1 BIGGEST drop (onsets ~14.5)-> chrome objects 0..2 on the peak
 *   126.7 long sustained centre      -> mercury plain, a cruising journey
 *   180.6 SECOND drop (onsets ~10)   -> chrome objects 3..5, the reprise
 *   224.7 outro                      -> credits finale, ends with the music
 * The chrome scene shows DISJOINT objects each time (0..2 then 3..5).
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
    {      0,  33690, &fx_title    },   /* intro build -> DROP 1 @33.69            */
    {  33690,  63480, &fx_rotozoom },   /* DROP 1: beam-raced rubber rotozoomer    */
    {  63480, 100100, &fx_liquid   },   /* build/breakdown toward the biggest drop */
    { 100100, 126660, &fx_chrome   },   /* BIGGEST DROP: chrome objects 0,1,2      */
    { 126660, 180560, &fx_mode7    },   /* sustained centre: mercury plain cruise  */
    { 180560, 224680, &fx_chrome   },   /* SECOND DROP: chrome objects 3,4,5       */
    { 224680, 249400, &fx_credits  },   /* outro: credits + end card; ends w/ music*/
};

/* transition used when LEAVING each entry (themed to the pair it joins) */
const uint8_t timeline_trans[] = {
    QS_TR_IRIS,     /* title  -> rotozoom : iris opens on the drop      */
    QS_TR_DISSOLVE, /* roto   -> liquid   : speckle into the plasma     */
    QS_TR_BLINDS,   /* liquid -> chrome   : blinds SLAM on the big drop */
    QS_TR_WIPE,     /* chrome -> mode7    : clean sweep onto the plain  */
    QS_TR_MELT,     /* mode7  -> chrome   : plain melts into the chrome */
    QS_TR_MELT,     /* chrome -> credits  : melt into the finale        */
    QS_TR_MELT,     /* credits-> end      : (suppressed)                */
};

const int timeline_count = (int)(sizeof(timeline) / sizeof(timeline[0]));
