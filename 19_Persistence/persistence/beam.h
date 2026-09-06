/* The line contract, and the runner that dispatches it.
 *
 * A scene is three functions. `enter` builds whatever the scene needs in the
 * arena (core 0, once, at a cue boundary). `frame` prepares every per-row
 * table for frame f (core 0, once per frame, double-buffered by f & 1).
 * `line` turns row y of frame f into 640 pixels (core 1 on the device, the
 * present loop on the host). A scene may not keep a frame anywhere: the
 * biggest thing it is allowed to hand across is a span list.
 *
 * beam_frame(f) is the arc: it decides which scene(s) own frame f and how the
 * rows are shared between them during a transition. beam_line(f, px, y) is
 * the per-row switch that makes those transitions free.
 */

#ifndef PV_BEAM_H
#define PV_BEAM_H

#include "persistence.h"

typedef struct scene {
    const char *name;
    void (*enter)(void);
    void (*frame)(uint32_t f, uint32_t local);
    void (*line)(uint32_t f, uint16_t *px, int y);
    /* Optional: runs ON THE DRAWING CORE before the scene's first line. The
     * SIO interpolator is per core, so a kernel that uses it configures it
     * here, not in enter(). NULL if not needed. */
    void (*setup1)(void);
    /* Optional: runs ON THE DRAWING CORE at scanline 0 of every frame, for
     * kernels whose per-row state has to start each frame from a known value.
     * The kefrens line buffer is the case this exists for: it deliberately
     * carries down the frame, so it has to be cleared exactly once, at the
     * top, on the core that draws it -- and not by a `y == 0` test inside the
     * line function, because the raster split gives a kernel a band that does
     * not contain row 0. */
    void (*line0)(uint32_t f);
} scene_t;

/* What core 1 reads for frame f: g_beam[f & 1]. Written by beam_frame(f) on
 * core 0 while core 1 is drawing frame f-1 from the other parity. */
typedef struct {
    const scene_t *a;       /* owns every row not owned by b                  */
    const scene_t *b;       /* owns rows y < split (beam wipe), or NULL        */
    int16_t  split;
    uint8_t  blind;         /* 0..8: rows with (y & 7) < blind are black       */
    uint8_t  fade;          /* 0..255 global dim, applied per row (cheap only) */
    uint32_t f;
} beam_state_t;

extern beam_state_t g_beam[2];

/* Half-horizontal-resolution fallback, set by the device governor if a line
 * ever misses its deadline. Kernels that cost more than ~3,000 cycles per line
 * honour it by rendering 320 pixels and writing pairs. */
extern volatile uint8_t g_lod;

void beam_init(void);                      /* once; enters nothing            */
void beam_reset(void);                     /* forget which scenes are entered */
void beam_frame(uint32_t f);               /* core 0                          */
void beam_line(uint32_t f, uint16_t *px, int y);   /* core 1                  */
void beam_line_setup(uint32_t f);                  /* core 1, at scanline 0   */


const char *beam_scene_name(uint32_t f);
int         beam_cue_index(uint32_t f);
uint32_t    beam_cue_start(int ci);
int         beam_cue_count(void);

/* Shared helpers for kernels. */
static inline void pv_fill(uint16_t *px, int x0, int x1, uint16_t c)
{
    /* Word fill; the odd edges are handled so callers do not think about
     * alignment. x1 is exclusive. */
    if (x0 >= x1) return;
    if (x0 & 1) { px[x0] = c; x0++; }
    uint32_t cc = (uint32_t)c | ((uint32_t)c << 16);
    uint32_t *w = (uint32_t *)(px + x0);
    int n = (x1 - x0) >> 1;
    for (int i = 0; i < n; i++) w[i] = cc;
    if ((x1 - x0) & 1) px[x1 - 1] = c;
}

/* Per-row black-out for the blind transition: the whole line in one fill. */
static inline void pv_black(uint16_t *px) { pv_fill(px, 0, PV_W, 0); }

#endif
