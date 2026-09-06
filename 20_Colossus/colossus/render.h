/* Phase / LATENT: renderer internals; demo.h remains the platform contract. */
#ifndef PHASE_RENDER_H
#define PHASE_RENDER_H
#include "demo.h"
typedef struct { float x,y,z,l,u,v,e; } RVertex;
typedef enum { R_FLAT, R_GOURAUD, R_CHROME, R_TEXTURE } RMaterial;
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
/* Explicit diagnostic entry point; no hidden mode affecting demo_render. */
void render_material_test(uint16_t *page,uint32_t sample);
#endif
