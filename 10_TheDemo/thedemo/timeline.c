/* The demo arc. Each entry binds a time range to one effect. */

#include "scene.h"

extern const effect_t fx_voxel_landscape_real;
extern const effect_t fx_title_real;
extern const effect_t fx_fluid_portraits_real;
extern const effect_t fx_spheres_real;
extern const effect_t fx_greetz_real;
extern const effect_t fx_reaction_diffusion_real;
extern const effect_t fx_tunnel_particles_real;
extern const effect_t fx_rotozoom_credits_real;

#define fx_voxel_landscape   fx_voxel_landscape_real
#define fx_title_copper      fx_title_real
#define fx_fluid_portraits   fx_fluid_portraits_real
#define fx_raytraced_spheres fx_spheres_real
#define fx_copperbar_scroll  fx_greetz_real
#define fx_reaction_diffuse  fx_reaction_diffusion_real
#define fx_tunnel_particles  fx_tunnel_particles_real
#define fx_rotozoom_credits  fx_rotozoom_credits_real

/* Track length: 4:06 = 246 s. Boundaries below are aligned to the
 * structural segments librosa identified (see tools/analyze_music.py),
 * not chosen by feel. Segment summary at the 103 BPM tempo:
 *
 *   seg 0  0:00–0:29  intro
 *   seg 1  0:29–0:36  transition / pre-drop  (folded into title)
 *   seg 2  0:36–1:13  first main section
 *   seg 3  1:13–1:31  break / build
 *   seg 4  1:31–2:07  second main section
 *   seg 5  2:07–2:44  third main section, ends on the big breakdown
 *   seg 6  2:44–3:39  fourth section (55 s, longest)
 *   seg 7  3:39–4:06  outro
 *
 * The voxel/fluid split inside seg 2+3 is at 1:01 — the strongest
 * onset inside seg 2 — so neither scene feels stranded. The credits
 * scene still runs 28 s past the music end so the scroller has time
 * to finish at a readable cadence (audio just goes silent then). */
const timeline_entry_t timeline[] = {
    {     0,  36000, &fx_title_copper      },   /* 0:00 - 0:36  640x480 title    (seg 0+1) */
    { 36000,  61000, &fx_voxel_landscape   },   /* 0:36 - 1:01  320x240 voxel    (first half of seg 2) */
    { 61000,  91000, &fx_fluid_portraits   },   /* 1:01 - 1:31  160x120 fluid    (rest of seg 2 + seg 3) */
    { 91000, 127000, &fx_copperbar_scroll  },   /* 1:31 - 2:07  split   greetz   (seg 4) */
    {127000, 164000, &fx_raytraced_spheres },   /* 2:07 - 2:44  160x120 spheres  (seg 5, ends on the breakdown) */
    {164000, 184000, &fx_reaction_diffuse  },   /* 2:44 - 3:04  160x120 RD       (opening of seg 6) */
    {184000, 219000, &fx_tunnel_particles  },   /* 3:04 - 3:39  320x240 tunnel   (rest of seg 6) */
    {219000, 274000, &fx_rotozoom_credits  },   /* 3:39 - 4:34  split   credits  (seg 7 + 28 s silent tail) */
};
const int timeline_count = sizeof(timeline) / sizeof(timeline[0]);
