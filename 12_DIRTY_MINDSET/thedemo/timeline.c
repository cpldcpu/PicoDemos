/* The DIRTY_MINDSET demo timeline. Each entry binds a time range to one effect. */

#include "scene.h"

extern const effect_t fx_cpc_boot;
extern const effect_t fx_plasma_chip;
extern const effect_t fx_text_matrix;
extern const effect_t fx_dirty_logo;
extern const effect_t fx_reaction_mind;
extern const effect_t fx_fractal_zoom;
extern const effect_t fx_greetings;
extern const effect_t fx_outro;

const timeline_entry_t timeline[] = {
    {      0,  17043, &fx_cpc_boot       },   /* 0:00.00 - 0:17.04  Scene 1: CPC Boot        */
    {  17043,  38289, &fx_plasma_chip    },   /* 0:17.04 - 0:38.29  Scene 2: Plasma Chip     */
    {  38289,  49156, &fx_text_matrix    },   /* 0:38.29 - 0:49.16  Scene 3: Text Matrix     */
    {  49156,  72306, &fx_dirty_logo     },   /* 0:49.16 - 1:12.31  Scene 4: Dirty Logo      */
    {  72306,  93576, &fx_reaction_mind  },   /* 1:12.31 - 1:33.58  Scene 5: Reaction Mind   */
    {  93576, 115310, &fx_fractal_zoom   },   /* 1:33.58 - 1:55.31  Scene 6: Fractal Zoom    */
    { 115310, 143197, &fx_greetings      },   /* 1:55.31 - 2:23.20  Scene 7: Greetings       */
    { 143197, 181520, &fx_outro          },   /* 2:23.20 - 3:01.52  Scene 8: Outro           */
};
const int timeline_count = sizeof(timeline) / sizeof(timeline[0]);
