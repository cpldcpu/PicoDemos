/* Phase / LATENT: renderer internals; demo.h remains the platform contract. */
#ifndef PHASE_RENDER_H
#define PHASE_RENDER_H
#include "demo.h"
typedef struct { float x,y,z,l,u,v,e; } RVertex;
typedef enum { R_FLAT, R_GOURAUD, R_CHROME, R_TEXTURE, R_FURNACE } RMaterial;
typedef struct { float near_z,far_z,cx,cy,cz,yaw,focal; } RCamera;
void r_begin(uint16_t *page, unsigned chapter, RCamera camera);
void r_background(uint32_t sample);
RVertex r_transform(float x,float y,float z,float nx,float ny,float nz,float u,float v);
/* Pass order: opaque body, depth-tested emissive sources, embers, bloom.
 * Emissive sources must be submitted after opaque occluders. */
void r_triangle(RVertex a,RVertex b,RVertex c,RMaterial material);
void r_bloom(void);
void r_embers(uint32_t sample,unsigned count);
void r_inscription(const char *text,int x,int y,uint16_t color);
void r_finish(void);
void r_wordmark(int top,unsigned level);
void scene_hand(uint32_t sample);
void scene_body(uint32_t sample,int chapter);
RCamera scene_camera(uint32_t sample,int chapter);
void scene_titles(uint32_t sample,int chapter);
void r_transition(uint32_t sample);
void r_portal(float x,float y,float z,float radius);
void r_portal_reset(void);
void r_end_inscription(int y);
void r_environment(unsigned mode);
/* Explicit diagnostic entry point; no hidden mode affecting demo_render. */
void render_material_test(uint16_t *page,uint32_t sample);

/* ------------------------------------------------------------- profiling --
 *
 * Per-pass cycle counters, always compiled in, read off core 0's SysTick on
 * the device and zero on the host. The counters are monotonic and wrap; take
 * differences, as main.c does. Each individual pass must stay under the
 * 24-bit counter's 56 ms at 300 MHz -- the whole frame need not.
 *
 * RP_TRI_* are nested inside RP_SCENE, so RP_SCENE minus their sum is the
 * transform, pose and clip cost: the part that is charged per triangle
 * rather than per pixel, and the reason a 70-triangle phrase cost 66 ms.
 */
enum {
    RP_CLEAR, RP_SKY, RP_FLOOR, RP_SCENE,
    RP_TRI_FLAT, RP_TRI_GOURAUD, RP_TRI_CHROME, RP_TRI_TEXTURE, RP_TRI_FURNACE,
    RP_EMBERS, RP_BLOOM, RP_VEIL, RP_TITLES, RP_FADE,
    RP_COUNT
};
void        r_prof_read(uint32_t *cycles);   /* RP_COUNT entries              */
const char *r_prof_name(int slot);
#endif
