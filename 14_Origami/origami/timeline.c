/* ORIGAMI — the demo arc.
 *
 * Beat-synced to assets/Marimba Seedbox.mp3 (2:15 = 134.96 s, 117.5 BPM,
 * 4/4; beat = 511 ms, bar = 2.04 s). Boundaries snapped to the librosa
 * beat/onset/segment analysis of that exact track (tools/analyze_music.py):
 *
 *   structural segments:  10.4 s, 25.8 s, 42.8 s, 122.8 s (outro)
 *   strong onsets used:   47.4 / 54.5 / 71.98 / 77.1 / 113.0 / 132.0 s
 *
 * The journey: a creased sheet opens into a title; a paper plane banks
 * through a pastel sky; a flat square folds into a crane and unfolds; a
 * page turns into a pop-up paper city; a Miura-ori field ripples on the
 * beat; and the world bursts into confetti that settles on the endcard.
 */

#include "scene.h"

extern const effect_t fx_title_real;
extern const effect_t fx_plane_real;
extern const effect_t fx_crane_real;
extern const effect_t fx_city_real;
extern const effect_t fx_miura_real;
extern const effect_t fx_credits_real;

const timeline_entry_t timeline[] = {
    {      0,  10450, &fx_title_real   },  /* 0:00 – 0:10  title unfold      (intro seg)    */
    {  10450,  25820, &fx_plane_real   },  /* 0:10 – 0:26  paper-plane flight (seg 2)        */
    {  25820,  54540, &fx_crane_real   },  /* 0:26 – 0:54  crane fold / unfold (seg 3 + hit)  */
    {  54540,  77110, &fx_city_real    },  /* 0:54 – 1:17  pop-up paper city  (page turns)    */
    {  77110, 113010, &fx_miura_real   },  /* 1:17 – 1:53  Miura-ori wave     (beat climax)   */
    { 113010, 134960, &fx_credits_real },  /* 1:53 – 2:15  confetti + credits (outro)         */
};
const int timeline_count = sizeof(timeline) / sizeof(timeline[0]);
