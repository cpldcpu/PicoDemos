/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Boundaries snapped to "Quicksilver Through Hands" sections (analyze_music.py,
 * 129 BPM; structural edges ~19 / 38 / 68.4 / 118.9 / 153 / 189.4 s; ends 217.2 s,
 * with the biggest hit at 208 landing on the final card). The chrome scene
 * appears twice but each shows a DISJOINT set of objects (0..2 then 3..5) so no
 * 3D object ever repeats. Each boundary uses its own transition style.
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
    {      0,  19000, &fx_title    },   /* intro: brand reveal -> wordmark    */
    {  19000,  38000, &fx_rotozoom },   /* build: rubber rotozoomer           */
    {  38000,  68400, &fx_mode7    },   /* verse: mercury plain to horizon    */
    {  68400, 118900, &fx_chrome   },   /* chrome objects 0,1,2 (each once)   */
    { 118900, 153000, &fx_liquid   },   /* breakdown: liquid metal            */
    { 153000, 189400, &fx_chrome   },   /* DROP: chrome objects 3,4,5 (once)  */
    { 189400, 217240, &fx_credits  },   /* outro: grand finale + credits      */
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
