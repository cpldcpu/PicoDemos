/* timeline.c — QUICKSILVER storyboard + per-boundary transition styles.
 *
 * Boundaries snapped to "Quicksilver Mercy" sections (analyze_music.py, 123 BPM;
 * edges ~5.8 / 45 / 91.5 / 114.7 s; ends 267 s). The chrome scene appears twice
 * but each shows a DISJOINT set of objects (0..2 then 3..5) so no 3D object ever
 * repeats. Each boundary uses its own transition style (see effects/transition.h).
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
    {      0,   6000, &fx_title    },   /* intro: wordmark                    */
    {   6000,  28000, &fx_rotozoom },   /* verse: rubber rotozoomer           */
    {  28000,  47000, &fx_mode7    },   /* mercury plain to the horizon       */
    {  47000,  92000, &fx_chrome   },   /* chrome objects 0,1,2 (each once)   */
    {  92000, 116000, &fx_liquid   },   /* breakdown: liquid metal            */
    { 116000, 175000, &fx_chrome   },   /* DROP: chrome objects 3,4,5 (once)  */
    { 175000, 267080, &fx_credits  },   /* outro: grand finale + credits      */
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
