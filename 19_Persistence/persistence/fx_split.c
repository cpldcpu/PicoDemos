/* The raster split: five programs on one screen.
 *
 * This is the scene the architecture exists for. On a machine with a
 * framebuffer, putting five different effects in five moving horizontal bands
 * is a compositing problem: you render each one somewhere and then combine
 * them, and it costs five renders plus a blend. Here it costs nothing at all,
 * because a band is just a row range and a row is just a function call. The
 * whole effect is one table lookup per scanline:
 *
 *     s_kernel[ owner[y] ]->line(f, px, y);
 *
 * Every scanline is running a different program, which is exactly what a
 * copper list did and exactly what a framebuffer took away.
 *
 * The bands are re-dealt on the beat and their boundaries breathe, so the
 * screen tears itself apart in time with the music. The kernels are the four
 * cheapest ones in the demo plus the affine floor, which keeps the worst line
 * at the cost of the most expensive band rather than the sum of all of them.
 */

#include "beam.h"
#include "scenes.h"
#include "affine.h"
#include "dither.h"
#include "tables.h"
#include "song.h"

#include <math.h>
#include <string.h>

#define NBAND 8

/* The affine floor as a band: it needs its own tiny director, because the
 * plane scene's one also drives 3D objects that have no place here. */
static void split_floor_frame(uint32_t f, uint32_t local)
{
    const float t = (float)local / 60.0f;
    affine_cam_t cam;
    cam.horizon  = 150;
    cam.height   = 72.0f;
    cam.angle    = t * 0.55f;
    cam.x        = 128.0f + 220.0f * t;
    cam.y        = 128.0f;
    cam.fog_near = 420.0f;
    cam.fog_far  = 1700.0f;
    affine_rows(&cam, f);
    rgb8_t sky[PV_H];
    affine_sky_dusk(sky, cam.horizon, 0);
    affine_sky(sky, f);
}
static void PV_HOT(split_floor_line)(uint32_t f, uint16_t *px, int y) { affine_line_p(f, px, y); }
static const scene_t s_floor = { "floor", NULL, split_floor_frame, split_floor_line, NULL, NULL };

static const scene_t *const s_kernel[5] = {
    &fx_copper, &fx_plasma, &fx_kefrens, &fx_twister, &s_floor,
};
#define NKERNEL 5

typedef struct {
    uint8_t owner[PV_H];
} split_p_t;

static split_p_t P[2];

static void split_enter(void)
{
    /* Every band's kernel has to be resident at once, so their arena regions
     * must not overlap -- plasma owns SMALL_PLASMA_XT, kefrens SMALL_KEFRENS
     * and the floor owns TEXTURE. Nothing here touches SPANS or MESH, which is
     * why the 3D plane is deliberately not one of the five. */
    for (int i = 0; i < NKERNEL; i++)
        if (s_kernel[i]->enter) s_kernel[i]->enter();
    affine_texture_generate(AFFINE_TEX_FLOOR);
}

static void split_frame(uint32_t f, uint32_t local)
{
    split_p_t *p = &P[f & 1];

    for (int i = 0; i < NKERNEL; i++) s_kernel[i]->frame(f, local);

    /* Band edges: seven moving boundaries, each on its own sine, kept sorted
     * and at least six rows apart so no band vanishes. */
    int e[NBAND + 1];
    e[0] = 0; e[NBAND] = PV_H;
    for (int i = 1; i < NBAND; i++) {
        const int centre = i * PV_H / NBAND;
        const int sway = (pv_sin16((uint32_t)(f * (7 + i * 3) + i * 170)) * 40) >> 15;
        e[i] = centre + sway;
    }
    for (int i = 1; i < NBAND; i++) if (e[i] < e[i - 1] + 6) e[i] = e[i - 1] + 6;

    /* Deal a PERMUTATION of the kernels, not an independent draw per band.
     * Drawing each band independently from five kernels puts the same one in
     * neighbouring bands about half the time, and the screen then reads as two
     * effects with a seam rather than as five programs at once -- which is the
     * whole point of the scene. A shuffled deck guarantees all five are on
     * screen, and the three spare bands re-deal from the top of it.
     *
     * The shuffle is a Fisher-Yates driven by a hash of the beat number, so it
     * is still a pure function of f and still changes exactly on the beat. */
    const uint32_t beat = f / PV_FPB;
    uint8_t deck[NKERNEL];
    for (int i = 0; i < NKERNEL; i++) deck[i] = (uint8_t)i;
    uint32_t h = beat * 2654435761u + 12345u;
    for (int i = NKERNEL - 1; i > 0; i--) {
        h ^= h << 13; h ^= h >> 17; h ^= h << 5;
        const int j = (int)(h % (uint32_t)(i + 1));
        const uint8_t t = deck[i]; deck[i] = deck[j]; deck[j] = t;
    }
    for (int i = 0; i < NBAND; i++) {
        const uint8_t k = deck[i % NKERNEL];
        for (int y = e[i]; y < e[i + 1] && y < PV_H; y++) p->owner[y] = k;
    }
}

static void split_setup1(void) { qs_texmap_setup_interp0(); }
static void split_line0(uint32_t f) { kefrens_line0(f); }

static void PV_HOT(split_line)(uint32_t f, uint16_t *px, int y)
{
    s_kernel[P[f & 1].owner[y]]->line(f, px, y);
}

const scene_t fx_split = { "split", split_enter, split_frame, split_line, split_setup1, split_line0 };
