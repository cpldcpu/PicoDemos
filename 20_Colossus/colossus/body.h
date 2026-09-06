#ifndef PHASE_BODY_H
#define PHASE_BODY_H
#include "render.h"
typedef struct { float x,y,z,w,h,d,turn; unsigned char group,mat; } BodyPart;
void body_pose(uint32_t sample);
void body_box(BodyPart p,int lod);
void body_link(float x,float y,float z,float ex,float ey,float ez,float width,RMaterial mat);
void body_draw(int group,int lod); /* -1 whole, 3 near hand plus its forearm */
void body_ring(float x,float y,float z,float radius,int lod,RMaterial mat,float emission);
#endif
