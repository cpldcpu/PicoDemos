/* SINGULARITY — the demo arc.
 *
 * Beat-synced to assets/Graviton Choir.mp3 (5:30 = 330 s, 78.3 BPM, 4/4;
 * bar = 3.07 s). Boundaries below are snapped to the librosa beat/onset/
 * segment analysis of that exact track (tools/analyze_music.py):
 *
 *   anchor hits:  1:14 (74.0 s, strongest early onset) = star ignition
 *                 3:50 (230 s)                          = the plunge
 *                 5:25 (325.8 s, final cadence)         = endcard settled
 *   segments:     0:50, 1:51, long body to 4:28, loud finale 4:28-5:08
 *
 * The journey: dust gathers and ignites a star, the star dies into a
 * relativistic fall through its accretion disk, past the Einstein ring,
 * across the event horizon, and out the other side reborn.
 */

#include "scene.h"

extern const effect_t fx_title_real;
extern const effect_t fx_nebula_real;
extern const effect_t fx_star_real;
extern const effect_t fx_starfield_real;
extern const effect_t fx_disk_real;
extern const effect_t fx_lensing_real;
extern const effect_t fx_spacetime_real;
extern const effect_t fx_rebirth_real;

const timeline_entry_t timeline[] = {
    {      0,  18000, &fx_title_real     },  /* 0:00 – 0:18  title           (intro) */
    {  18000,  50000, &fx_nebula_real    },  /* 0:18 – 0:50  curl-noise nebula(intro→seg) */
    {  50000,  74000, &fx_star_real      },  /* 0:50 – 1:14  star ignition    (flare on 1:14 hit) */
    {  74000, 111000, &fx_starfield_real },  /* 1:14 – 1:51  relativistic warp(energetic section) */
    { 111000, 163000, &fx_disk_real      },  /* 1:51 – 2:43  accretion disk   (orbital body) */
    { 163000, 230000, &fx_lensing_real   },  /* 2:43 – 3:50  GRAVITATIONAL LENSING (climax) */
    { 230000, 277000, &fx_spacetime_real },  /* 3:50 – 4:37  spacetime collapse / horizon */
    { 277000, 330000, &fx_rebirth_real   },  /* 4:37 – 5:30  rebirth + endcard(finale) */
};
const int timeline_count = sizeof(timeline) / sizeof(timeline[0]);
