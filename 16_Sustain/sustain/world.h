/* SUSTAIN — the continuous world.
 *
 * THE RULE: the demo never cuts. There is one camera, one renderer, and one
 * world function of time. An effect cannot end; it can only BECOME the next
 * one. Everything here exists to make that mechanically true.
 *
 * FAMILIES AND FIELDS
 * -------------------
 * A FAMILY is a shared implementation — "a heightfield with a domain warp",
 * say. A FIELD is a family plus a parameter block. Sea, canyon and tunnel are
 * three parameter sets of ONE family, which is what makes the morphs between
 * them cheap: when both sides of a morph share a family, world.c lerps the
 * PARAMETERS and evaluates the family ONCE. Only a morph between different
 * families has to evaluate both sides and lerp the results.
 *
 * That distinction is load-bearing, not tidiness (PLANNING.md §3.2). Six of
 * the arc's eleven morphs are intra-family and therefore nearly free; the two
 * cross-family ones cost 2x and are deliberately scheduled into fog and bloom
 * where resolution can drop unnoticed. If every morph were cross-family the
 * frame budget would not close.
 *
 * Note what a parameter lerp buys beyond speed: it is also more *correct*.
 * Lerping two evaluated heightfields gives the average of two surfaces;
 * lerping the parameters and evaluating once gives a single surface halfway
 * between them. The first can produce a shape neither field would ever
 * generate. The second always produces a plausible member of the family.
 */

#ifndef SUSTAIN_WORLD_H
#define SUSTAIN_WORLD_H

#include <stdint.h>

#define FIELD_MAX_PARAMS 24

/* UNIVERSAL PARAMETER SLOTS.
 *
 * Every family reserves these two at the same indices, whatever else its
 * parameter block means. That lets world.c blend them with a plain scalar lerp
 * that works across families as well as within one — so the mote layer keeps
 * evolving continuously through a cross-family morph instead of being handed
 * off between two families' private conventions and jumping. */
#define P_UNI_MOTES (FIELD_MAX_PARAMS - 2)   /* mote density 0..1  */
#define P_UNI_WARM  (FIELD_MAX_PARAMS - 1)   /* mote colour cold->hot */

/* "Open sky" / "no ceiling". Any ceiling at or above this is not drawn. */
#define WORLD_NO_CEILING 1.0e6f

typedef struct field_family {
    const char *name;

    /* Floor height at (x,z). */
    float (*h)(const float *p, float x, float z, float t);

    /* Ceiling height at (x,z), or WORLD_NO_CEILING for open sky. This is what
     * lets sea, canyon and tunnel be one family: the canyon is a floor with
     * walls and open sky above; the tunnel is the same floor with a ceiling
     * brought down to meet it. Enclosure becomes a parameter rather than a
     * different renderer, so flying into a tunnel is an intra-family morph. */
    /* `floor_h` is the height h() just returned at this point. Passing it in
     * rather than recomputing saves one relief sample per ray step in every
     * enclosed section — roughly a third of the field cost through the whole
     * tunnel half of the demo. */
    float (*ceil)(const float *p, float x, float z, float t, float floor_h);

    /* Surface colour. `up` is 1 for floor, 0 for ceiling, so a family can
     * light the two differently. `dist` is the view distance to the sample,
     * which an enclosed field needs: once a tunnel closes there is no sky
     * lighting anything, so it carries its own camera-attached glow and
     * without a distance term it renders black. */
    void (*shade)(const float *p, float x, float z, float h, float t,
                  int up, float dist, float foot, int *r, int *g, int *b);

    /* Sky/fog colour. `u` is view azimuth 0..1 around the compass, `v` is
     * normalised screen height (0 = horizon, 1 = top). The sky is a panorama,
     * so it depends on where the camera is looking and not only on the row. */
    void (*sky)(const float *p, float u, float v, float t,
                int *r, int *g, int *b);

    /* Optional: fill scratch slot `slot`. Run on the IDLE slot ahead of time,
     * never during a morph — a precompute stall is a dropped frame, and a
     * dropped frame is a discontinuity. */
    void (*prepare)(int slot, const float *p);
} field_family_t;

typedef struct field {
    const char           *name;
    const field_family_t *family;
    float                 params[FIELD_MAX_PARAMS];
} field_t;

/* ------------------------------------------------------------------- arc -- */

typedef struct arc_node {
    uint32_t       t_ms;      /* when the morph INTO this field completes */
    const field_t *field;
    uint32_t       morph_ms;  /* length of that morph (0 for the first node) */
} arc_node_t;

extern const arc_node_t arc[];
extern const int        arc_count;

/* -------------------------------------------------------------- camera --- */

/* The camera is a cubic Hermite spline through these keys, C1-continuous by
 * construction — so rule 3 ("the camera never teleports") is a property of the
 * representation. An arc cannot author a jump cut even by mistake. */
typedef struct cam_key {
    uint32_t t_ms;
    float    x, y, z;
    float    yaw;          /* radians, 0 = +z */
    float    pitch;        /* radians, + = up */
} cam_key_t;

extern const cam_key_t cam_keys[];
extern const int       cam_key_count;

typedef struct camera {
    float x, y, z;
    float yaw, pitch;
} camera_t;

void world_camera_at(uint32_t t_ms, camera_t *out);

/* ------------------------------------------------------- world sampling --- */

/* The blended world. The renderer calls these and never sees an individual
 * field, how many are live, or that a morph is happening — a renderer that
 * cannot perceive a boundary cannot draw one. */
float world_height(float x, float z);

/* Terrain height averaged over a disc — what the camera rides. Spatial, not
 * temporal, so seeking to a timestamp still reproduces the same frame. */
float world_height_smooth(float x, float z);
float world_ceiling(float x, float z, float floor_h);
/* `foot` is how many world units one screen pixel covers on this surface,
 * relative to a face seen head-on. At grazing incidence it is large even when
 * the surface is close, which is exactly the case plain distance-based LOD
 * cannot see. */
void  world_shade(float x, float z, float h, int up, float dist, float foot,
                  int *r, int *g, int *b);
void  world_sky(float u, float v, int *r, int *g, int *b);

/* True while two DIFFERENT families are live — the expensive case. The
 * renderer may drop detail while this holds (PLANNING.md §6.2). */
int   world_is_cross_family(void);

/* Mote layer controls, blended across every morph (see P_UNI_* above). */
float       world_mote_density(void);
float       world_mote_warm(void);

float       world_blend_w(void);
const char *world_field_a_name(void);
const char *world_field_b_name(void);
uint32_t    world_duration_ms(void);

#endif
