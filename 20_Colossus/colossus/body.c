/* Shared modular body. Coordinates are the silhouette's, including its gaps. */
#include "body.h"
#include <math.h>
#include <stddef.h>
#include "song.h"
static float body_curl;
#include "body_layout.h"
static RVertex body_cache[24];
static float body_joints[5][3];
_Static_assert(sizeof(RVertex)==28,"ledger vertex size");
void body_pose(uint32_t sample)
{
    float bars=(float)sample/CV_BAR;
    float tension=fminf(1,fmaxf(0,(bars-24)/16));
    body_curl=.12f+.48f*tension+.08f*song_energy(cv_bar_of(sample))/255.f;
    float lift=fminf(1,fmaxf(0,(bars-128)/2));
    for(int i=0;i<5;i++) for(int j=0;j<3;j++) body_joints[i][j]=0;
    lift=lift*lift*(3-2*lift);
    body_joints[1][1]=lift*BODY_SHOULDER_DROP;
    static const signed char parent[5]={-1,0,0,1,2};
    for(int i=1;i<5;i++)body_joints[i][1]+=body_joints[parent[i]][1];
    /* Parent shoulder displacement is composed into its child wrist.
       Wrist sway and finger tension reconstruct from the sample and retain gaps. */
    float settling=fminf(1,fmaxf(0,(160-bars)/16));
    body_joints[3][2]=settling*.045f*sinf(bars*6.2831853f/8);
    body_joints[4][2]=-settling*.03f*sinf(bars*6.2831853f/8);
}
static RVertex point(BodyPart p,float x,float y,float z,float nx,float ny,float nz,float u,float v)
{
    float turn=p.turn*.0174532925f,tc=cosf(turn),ts=sinf(turn);
    float px=x-p.x,pz=z-p.z;
    x=p.x+px*tc+pz*ts;z=p.z-px*ts+pz*tc;
    float nnx=nx*tc+nz*ts;nz=-nx*ts+nz*tc;nx=nnx;
    int g=p.group;
    if(g>=3 && (p.y<(g==3?5.3f:7.9f) || fabsf(p.x)<2.3f)){
        float base=(g==3?5.275f:7.875f),joint=base-1.325f;
        /* Thumb opposes on a distinct axis; the broad finger ribs bend in YZ. */
        if(fabsf(p.x)<2.3f){
            float joint=g==3?5.45f:8.05f;
            if(p.y<joint){
                float a=(g==3?-1:1)*body_curl*.4f,c=cosf(a),s=sinf(a),dx=x-p.x,dy=y-joint;
                x=p.x+dx*c-dy*s;y=joint+dx*s+dy*c;float n=nx*c-ny*s;ny=nx*s+ny*c;nx=n;
            }
            float ax=g==3?-2.25f:2.25f,ay=g==3?6.1f:8.7f;
            float a=(g==3?1:-1)*(.42f+body_curl*.55f),c=cosf(a),s=sinf(a);
            float dx=x-ax,dy=y-ay;x=ax+dx*c-dy*s;y=ay+dx*s+dy*c;
            float t=nx*c-ny*s;ny=nx*s+ny*c;nx=t;
        }else{
            if(fabsf(p.x)>5.1f){base+=.35f;joint+=.35f;}
            if(p.y<joint){float a=body_curl,c=cosf(a),s=sinf(a),dy=y-joint;
                y=joint+dy*c-z*s;z=dy*s+z*c;float t=ny*c-nz*s;nz=ny*s+nz*c;ny=t;}
            float a=body_curl*.35f,c=cosf(a),s=sinf(a),dy=y-base;
            y=base+dy*c-z*s;z=dy*s+z*c;float t=ny*c-nz*s;nz=ny*s+nz*c;ny=t;
        }
    }
    float angle=body_joints[g][2],c=cosf(angle),s=sinf(angle);
    float pivotx=g==3?-4.5f:4.5f,pivoty=g==3?BODY_NEAR_WRIST_Y:BODY_FAR_WRIST_Y;
    if(g>=3){float dx=x-pivotx,dy=y-pivoty; x=pivotx+dx*c-dy*s;y=pivoty+dx*s+dy*c;float t=nx*c-ny*s;ny=nx*s+ny*c;nx=t;}
    y+=body_joints[g][1];
    if(y>15)z-=(y-15)*.12f; /* stooped hood, same front silhouette */
    return r_transform(x,y,z,nx,ny,nz,u,v);
}
void body_box(BodyPart p,int lod)
{
    static const unsigned char faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{2,6,7,3},{0,1,5,4}};
        for(int i=0;i<8;i++){
            float nx=(i&1)?1:-1,ny=(i&2)?1:-1,nz=(i&4)?1:-1;
            body_cache[i]=point(p,p.x+nx*p.w*.5f,p.y+ny*p.h*.5f,p.z+nz*p.d*.5f,nx*.57735f,ny*.57735f,nz*.57735f,(i&1)?63:0,(i&2)?63:0);
        }
        for(int f=0;f<6;f++){
            if(lod==3 && f!=0)continue; /* Frontal eye approach: interior faces are occluded by the hood. */
            RVertex a=body_cache[faces[f][0]],b=body_cache[faces[f][1]],c=body_cache[faces[f][2]],d=body_cache[faces[f][3]];
            /* Separate face UVs avoid collapsed coordinates on side faces. */
            if(p.mat!=R_CHROME){a.u=d.u=0;b.u=c.u=63;a.v=b.v=0;c.v=d.v=63;}
            RMaterial m=p.mat==R_FLAT?R_FLAT:p.mat==R_CHROME?R_CHROME:R_TEXTURE;
            if(m==R_FLAT)a.l=b.l=c.l=d.l=0;
            if(lod==2){
                /* Close template: a 3x3 face grid in the same bounded cache.
                   Vertex attributes subdivide together; no heap mesh. */
                for(int yy=0;yy<3;yy++)for(int xx=0;xx<3;xx++){
                    float u=xx*.5f,v=yy*.5f;
                    #define BILERP(field) ((1-v)*((1-u)*a.field+u*b.field)+v*((1-u)*d.field+u*c.field))
                    body_cache[8+yy*3+xx]=(RVertex){BILERP(x),BILERP(y),BILERP(z),BILERP(l),BILERP(u),BILERP(v),0};
                    #undef BILERP
                }
                for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++){
                    int i=8+yy*3+xx;
                    r_triangle(body_cache[i],body_cache[i+1],body_cache[i+4],m);
                    r_triangle(body_cache[i],body_cache[i+4],body_cache[i+3],m);
                }
            }else{
                /* Whole: discard faces pointing away from the camera. Body:
                   keep both sides for close traversals through the anatomy. */
                float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z;
                float vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
                float facing=(uy*vz-uz*vy)*a.x+(uz*vx-ux*vz)*a.y+(ux*vy-uy*vx)*a.z;
                if(lod==0 && facing>=0)continue;
                r_triangle(a,b,c,m);r_triangle(a,c,d,m);
            }
        }
}
void body_draw(int group,int lod)
{
    for(size_t k=0;k<sizeof(body_parts)/sizeof(body_parts[0]);k++){
        BodyPart p=body_parts[k];
        if(group==-2 && (p.group!=0 || p.mat==R_FLAT))continue;
        if(group==-3 && (p.group!=0 || p.y<15))continue;
        if(group==-4 && (p.group!=0 || p.y<7 || p.y>14.5f))continue;
        if(group>=0 && p.group!=group && !(group==3&&p.group==1))continue;
        if(p.group>=3 && ((p.h>2 && p.h<3) || (fabsf(p.x)<2.3f && p.h>1.8f && p.h<2))){
            BodyPart half=p;half.h=p.h*.5f;half.y=p.y+p.h*.25f;body_box(half,lod);
            half.y=p.y-p.h*.25f;body_box(half,lod);
            if(fabsf(p.x)<2.3f){BodyPart joint=p;joint.h=.18f;joint.w=.75f;joint.d=1.05f;joint.mat=R_CHROME;body_box(joint,0);}
            if(p.mat==R_CHROME){BodyPart joint=p;joint.h=.24f;joint.w=.32f;joint.d=.64f;body_box(joint,0);}
        }else body_box(p,lod);
    }
    if(group==-1 || group==3){
        body_box((BodyPart){-4.35f,6.48f,0,3.3f,.45f,2.35f,0,3,R_TEXTURE},0);
    }
    if(group==-1){
        body_box((BodyPart){4.35f,9.08f,0,3.3f,.45f,2.35f,0,4,R_TEXTURE},0);
    }
    if(group==-1)body_ring(0,17.4f,-1.05f,.78f,lod,R_CHROME,0);
}
void body_link(float x,float y,float z,float ex,float ey,float ez,float width,RMaterial mat)
{
    /* A rectangular linkage in the XY plane, with a separate depth bearing. */
    float dx=ex-x,dy=ey-y,len=sqrtf(dx*dx+dy*dy)+.00001f;
    float nx=-dy/len,ny=dx/len;
    RVertex a=r_transform(x+nx*width,y+ny*width,z,nx,ny,-.7f,0,0);
    RVertex b=r_transform(ex+nx*width,ey+ny*width,ez,nx,ny,-.7f,63,0);
    RVertex c=r_transform(ex-nx*width,ey-ny*width,ez,-nx,-ny,-.7f,63,63);
    RVertex d=r_transform(x-nx*width,y-ny*width,z,-nx,-ny,-.7f,0,63);
    if(mat!=R_CHROME){a.u=d.u=0;b.u=c.u=63;a.v=b.v=0;c.v=d.v=63;}
    r_triangle(a,b,c,mat);r_triangle(a,c,d,mat);
}
void body_ring(float x,float y,float z,float radius,int lod,RMaterial mat,float emission)
{
    const int seg=lod==0?8:lod==1?12:24;
    for(int i=0;i<seg;i++){
        for(int j=0;j<4;j++){
            float a=(i+(j&1))*6.2831853f/seg,rr=radius*((j&2)?1:.62f);
            float nx=cosf(a),ny=sinf(a);
            body_cache[j]=r_transform(x+nx*rr,y+ny*rr,z,nx*.65f,ny*.65f,-.76f,0,0);
            if(mat==R_TEXTURE){body_cache[j].u=31.5f+nx*rr/radius*31.5f;body_cache[j].v=31.5f+ny*rr/radius*31.5f;}
            body_cache[j].e=emission;
        }
        r_triangle(body_cache[0],body_cache[1],body_cache[3],mat);
        r_triangle(body_cache[0],body_cache[3],body_cache[2],mat);
    }
}
