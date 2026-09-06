/* Shared modular body. Coordinates are the silhouette's, including its gaps. */
#include "body.h"
#include <math.h>
#include <stddef.h>
#include "body_layout.h"
static RVertex body_cache[24];
static float body_joints[5][3];
_Static_assert(sizeof(RVertex)==28,"ledger vertex size");
void body_pose(uint32_t sample)
{
    float bars=(float)sample/CV_BAR;
    float lift=fminf(1,fmaxf(0,(bars-128)/2));
    for(int i=0;i<5;i++) for(int j=0;j<3;j++) body_joints[i][j]=0;
    body_joints[1][1]=lift*BODY_SHOULDER_DROP;
    static const signed char parent[5]={-1,0,0,1,2};
    for(int i=1;i<5;i++)body_joints[i][1]+=body_joints[parent[i]][1];
    /* Parent shoulder displacement is composed into its child wrist.
       Finger tension is periodic, reconstructible, and never closes the gaps. */
    body_joints[3][2]=.045f*sinf(bars*6.2831853f/8);
    body_joints[4][2]=-.03f*sinf(bars*6.2831853f/8);
}
static RVertex point(BodyPart p,float x,float y,float z,float nx,float ny,float nz,float u,float v)
{
    float turn=p.turn*.0174532925f,tc=cosf(turn),ts=sinf(turn);
    float px=x-p.x,pz=z-p.z;
    x=p.x+px*tc+pz*ts;z=p.z-px*ts+pz*tc;
    float nnx=nx*tc+nz*ts;nz=-nx*ts+nz*tc;nx=nnx;
    int g=p.group; float angle=body_joints[g][2],c=cosf(angle),s=sinf(angle);
    float pivotx=g==3?-4.5f:4.5f,pivoty=g==3?BODY_NEAR_WRIST_Y:BODY_FAR_WRIST_Y;
    if(g>=3){float dx=x-pivotx,dy=y-pivoty; x=pivotx+dx*c-dy*s;y=pivoty+dx*s+dy*c;float t=nx*c-ny*s;ny=nx*s+ny*c;nx=t;}
    y+=body_joints[g][1];
    if(y>15)z-=(y-15)*.12f; /* stooped hood, same front silhouette */
    return r_transform(x,y,z,nx,ny,nz,u,v);
}
void body_draw(int group,int lod)
{
    static const unsigned char faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{2,6,7,3},{0,1,5,4}};
    for(size_t k=0;k<sizeof(body_parts)/sizeof(body_parts[0]);k++){
        BodyPart p=body_parts[k];
        if(group>=0 && p.group!=group && !(group==3&&p.group==1))continue;
        for(int i=0;i<8;i++){
            float nx=(i&1)?1:-1,ny=(i&2)?1:-1,nz=(i&4)?1:-1;
            body_cache[i]=point(p,p.x+nx*p.w*.5f,p.y+ny*p.h*.5f,p.z+nz*p.d*.5f,nx*.57735f,ny*.57735f,nz*.57735f,(i&1)?63:0,(i&2)?63:0);
        }
        for(int f=0;f<6;f++){
            RVertex a=body_cache[faces[f][0]],b=body_cache[faces[f][1]],c=body_cache[faces[f][2]],d=body_cache[faces[f][3]];
            /* Separate face UVs avoid collapsed coordinates on side faces. */
            if(p.mat!=R_CHROME){a.u=d.u=0;b.u=c.u=63;a.v=b.v=0;c.v=d.v=63;}
            RMaterial m=p.mat==R_FLAT?R_FLAT:p.mat==R_CHROME?R_CHROME:((k%5==0)?R_TEXTURE:R_GOURAUD);
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
    if(group<0)body_ring(0,17.4f,-1.05f,.78f,lod,R_CHROME,0);
}
void body_ring(float x,float y,float z,float radius,int lod,RMaterial mat,float emission)
{
    const int seg=lod==0?8:lod==1?12:24;
    for(int i=0;i<seg;i++){
        for(int j=0;j<4;j++){
            float a=(i+(j&1))*6.2831853f/seg,rr=radius*((j&2)?1:.62f);
            float nx=cosf(a),ny=sinf(a);
            body_cache[j]=r_transform(x+nx*rr,y+ny*rr,z,nx*.65f,ny*.65f,-.76f,0,0);
            body_cache[j].e=emission;
        }
        r_triangle(body_cache[0],body_cache[1],body_cache[3],mat);
        r_triangle(body_cache[0],body_cache[3],body_cache[2],mat);
    }
}
