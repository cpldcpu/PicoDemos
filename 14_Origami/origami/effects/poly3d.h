/* poly3d — a tiny flat-shaded, filled-polygon 3D engine for 14_Origami.
 *
 * The headline technique of ORIGAMI: real-time solid-polygon rasterisation
 * with painter's-algorithm depth sorting and per-face Lambert shading. No
 * prior demo in this repo did solid poly fill (the others did wireframe,
 * env-map, raymarch, per-pixel/LUT).
 *
 * Renders in MODE_HIRES (320x240 RGB565 truecolor) so polygon edges are
 * ANTIALIASED (analytic horizontal-coverage blending at span ends) and drop
 * shadows are true alpha-darkening of whatever lies beneath — smooth, with
 * no palette banding. The truecolor framebuffer is free here: vga.c always
 * allocates the arena at its largest (hires) size regardless of mode.
 *
 * Self-contained: depends only on vga.h + rgb565.h + <math.h>/<string.h>,
 * so it builds identically on the RP2350 and the SDL host. The per-frame
 * working set (~31 KB) lives in file-static BSS; scenes keep their own
 * animated geometry in g_scratch.
 */

#ifndef ORIGAMI_POLY3D_H
#define ORIGAMI_POLY3D_H

#include <stdint.h>

/* ---- capacities (file-static working set in poly3d.c) ----------------
 * Sized for the heaviest scenes: confetti (150 quads = 600 verts / 150
 * faces) and the Miura field (228 verts / 198 faces). */
#define P3_MAX_VERTS  640
#define P3_MAX_FACES  256
#define P3_MAX_FOLDS  24

#define P3_NO_VERT            0xFFFFu
#define P3_FACE_DOUBLE_SIDED  0x01u   /* paper has two lit sides; skip cull */

/* paper materials: an index into the engine's RGB base-colour table. Each
 * material's lit colour is set once per scene with p3_set_material(); the
 * engine darkens it per face by the Lambert term. 0..P3_MAT_MAX-1. */
#define P3_MAT_MAX  12
enum {
    P3_MAT_WHITE = 0,   /* warm paper white */
    P3_MAT_CREAM,
    P3_MAT_SKYBLUE,
    P3_MAT_CORAL,
    P3_MAT_SAGE,
    P3_MAT_MUSTARD,
    /* 6..11 free for per-scene accent papers */
};

/* ---- data ------------------------------------------------------------ */

typedef struct { float x, y, z; } p3_vec3;

/* A face: 3 or 4 vertex indices (i3 == P3_NO_VERT => triangle). Wound CCW
 * as seen from the front/lit side. `material` selects the RGB base. */
typedef struct {
    uint16_t i0, i1, i2, i3;
    uint8_t  material;
    uint8_t  flags;
} p3_face;

/* A crease/hinge: rotate the `moves` vertex set about the axis through the
 * (live, already-folded) world verts a,b by fold_angle[angle_src]. Creases
 * apply in array order, so a child crease whose `moves` is a subset of its
 * parent's folds within the parent. */
typedef struct {
    uint16_t        a, b;
    const uint16_t *moves;
    uint16_t        nmoves;
    uint8_t         angle_src;
    uint8_t         _pad;
} p3_crease;

typedef struct {
    const p3_vec3   *verts;     /* model space (flash, or scratch for animated models) */
    uint16_t         nverts;
    const p3_face   *faces;
    uint16_t         nfaces;
    const p3_crease *creases;   /* NULL => rigid */
    uint16_t         ncreases;
} p3_model;

typedef struct {
    /* camera */
    float cam_yaw, cam_pitch, cam_roll;
    float cam_x, cam_y, cam_z;
    float focal;                /* ~260..420 for a natural 320-wide FOV */
    float cx, cy;               /* screen centre (default 160,120)       */

    /* directional light in WORLD space (need not be unit; normalised internally) */
    p3_vec3 light;
    float   ambient;            /* 0..1 floor brightness */

    /* model placement (applied after folds): rotate then translate */
    float yaw, pitch, roll;
    float ox, oy, oz;

    float fold_angle[P3_MAX_FOLDS];   /* radians, indexed by crease.angle_src */

    uint8_t backface_cull;      /* 1 = cull back faces (double-sided exempt) */
} p3_render_params;

/* Sensible defaults: identity placement, centre 160,120, focal 300,
 * light from upper-front-left, ambient 0.30, cull on. */
void p3_params_default(p3_render_params *rp);

/* Render `m` into vga_hires_back_buffer() with `rp` (antialiased). Does NOT
 * clear the buffer (draw sky/ground first). Uses the file-static working set. */
void p3_render(const p3_model *m, const p3_render_params *rp);

/* Cast a soft drop-shadow of `m` onto the plane y = ground_y, using the SAME
 * camera/placement, by alpha-darkening whatever lies beneath (so it tints any
 * background and never produces a bright outline). Call BEFORE p3_render.
 * `darkness` 0..255 = how strongly the shadow darkens the ground.
 *
 * `mask` is a caller-provided 320*240 scratch byte buffer: all the model's
 * projected facets are first rasterised into it as a UNION (coverage), then
 * the ground is darkened ONCE per pixel — so overlapping facets don't
 * compound into dark blotches or show inner facet seams. Pass e.g.
 * (uint8_t*)g_scratch.bg_cache (the shadow-casting scenes don't otherwise
 * use it). */
void p3_render_shadow(const p3_model *m, const p3_render_params *rp,
                      float ground_y, int darkness, uint8_t *mask);

/* ---- material colours (call from a scene's init) --------------------- */

/* Set material `m`'s fully-lit RGB. The engine multiplies it by the per-face
 * Lambert brightness (ambient..1) at fill time. */
void p3_set_material(int m, int r, int g, int b);

/* ---- low-level antialiased fill (exposed for backdrops / clouds) ------ */

/* Fill a convex screen-space polygon (3 or 4 pts) with RGB, antialiased on
 * the left/right span edges, clipped to 320x240. */
void p3_fill_convex(const float *px, const float *py, int n, int r, int g, int b);

#endif
