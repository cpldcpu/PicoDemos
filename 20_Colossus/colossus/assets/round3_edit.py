from pathlib import Path
C=Path(__file__).resolve().parents[1]
def edit(n,a,b):
 p=C/n;s=p.read_text();assert a in s,n;p.write_text(s.replace(a,b))
# 1. Plain camera scaffold. Shared component primitive for slabs and mechanisms.
edit('body.h','void body_pose(uint32_t sample);','void body_pose(uint32_t sample);\nvoid body_box(BodyPart p,int lod);\nvoid body_link(float x,float y,float z,float ex,float ey,float ez,float width,RMaterial mat);')
edit('render.h','void scene_body(uint32_t sample,int chapter);','void scene_body(uint32_t sample,int chapter);\nRCamera scene_camera(uint32_t sample,int chapter);\nvoid scene_titles(uint32_t sample,int chapter);\nvoid r_transition(uint32_t sample);\nvoid r_end_inscription(int y);\nvoid r_environment(unsigned mode);')
p=C/'body.c';s=p.read_text();start=s.index('void body_draw(');end=s.index('void body_ring(',start)
old=s[start:end];a=old.index('        for(int i=0;i<8;i++)');b=old.index('\n    }\n    if(group<0)')
core=old[a:b]
s=s[:start]+'''void body_box(BodyPart p,int lod)
{
    static const unsigned char faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{2,6,7,3},{0,1,5,4}};
'''+core.replace('((k%5==0)?R_TEXTURE:R_GOURAUD)','R_TEXTURE')+'''
}
void body_draw(int group,int lod)
{
    for(size_t k=0;k<sizeof(body_parts)/sizeof(body_parts[0]);k++){
        BodyPart p=body_parts[k];
        if(group>=0 && p.group!=group && !(group==3&&p.group==1))continue;
        body_box(p,lod);
    }
    if(group<0)body_ring(0,17.4f,-1.05f,.78f,lod,R_CHROME,0);
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
'''+s[end:];p.write_text(s)
# 2. Articulated fingers: same spacing, two bones with continuous joint transform.
edit('body.c','#include <stddef.h>','#include <stddef.h>\n#include "song.h"\nstatic float body_curl;')
edit('body.c','float lift=fminf', 'float tension=fminf(1,fmaxf(0,(bars-24)/16));\n    body_curl=.12f+.48f*tension+.08f*song_energy(cv_bar_of(sample))/255.f;\n    float lift=fminf')
edit('body.c','    int g=p.group; float angle=', '''    int g=p.group;
    if(g>=3 && p.y<(g==3?5.3f:7.9f)){
        float base=(g==3?5.275f:7.875f),joint=base-1.325f;
        /* Thumb opposes on a distinct axis; the broad finger ribs bend in YZ. */
        if(fabsf(p.x)<2.3f){
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
    float angle=''')
edit('body.c','        body_box(p,lod);','''        if(p.group>=3 && p.h>2 && p.h<3){
            BodyPart half=p;half.h=p.h*.5f;half.y=p.y+p.h*.25f;body_box(half,lod);
            half.y=p.y-p.h*.25f;body_box(half,lod);
            if(p.mat==R_CHROME){BodyPart joint=p;joint.h=.24f;joint.w=.32f;joint.d=.64f;body_box(joint,0);}
        }else body_box(p,lod);
    }
    if(group<0 || group==3){
        body_box((BodyPart){-4.35f,6.48f,0,3.3f,.45f,2.35f,0,3,R_TEXTURE},0);
    }
    if(group<0){
        body_box((BodyPart){4.35f,9.08f,0,3.3f,.45f,2.35f,0,4,R_TEXTURE},0);''')
# Group 5 is not allocated: all scene details use root group 0.
(C/'scene_hand.c').write_text('''#include "body.h"
void scene_hand(uint32_t sample)
{
    body_pose(sample);body_draw(3,1);
}
''')
# Chapter implementation order mirrors the brief.
(C/'scene_chapters.c').write_text('''/* Phase / LATENT: one world, sample-derived cameras and mechanisms. */
#include "body.h"
#include "song.h"
#include <math.h>
static float unit(float x){return fminf(1,fmaxf(0,x));}
static float ease(float x){x=unit(x);return x*x*(3-2*x);}
static void box(float x,float y,float z,float w,float h,float d,RMaterial m)
{body_box((BodyPart){x,y,z,w,h,d,0,0,m},0);}
static void plain(float t)
{
    body_draw(1,0); /* A distant partial shoulder, never the whole silhouette. */
    for(int i=0;i<7;i++){
        float x=(i-3)*3.9f;
        body_box((BodyPart){x,1.0f,-12+i%3,3.2f,.65f+(i%3)*.42f,2.5f,12.f*(i%3-1),0,R_TEXTURE},0);
    }
    (void)t;
}
static void heart(float bars)
{
    /* Paired ribs and the deliberately absent cross-rib are the body's own. */
    body_draw(0,0);
    float a=(bars-40)*6.2831853f/16;
    float ex=.48f*cosf(a),ey=10.6f+.48f*sinf(a);
    body_ring(0,10.6f,-.7f,.86f,1,R_TEXTURE,0);
    body_ring(ex,ey,-.84f,.22f,1,R_CHROME,0);
    for(int i=0;i<2;i++){
        float side=i?1:-1,y=10.6f+side*.72f*sinf(a);
        box(side*.65f,y,-.9f,.34f,1.1f,.45f,R_CHROME);
        body_link(ex,ey,-1,side*.65f,y,-1,.10f,R_CHROME);
    }
    r_environment(2);
    body_ring(0,10.6f,-.72f,.6f,1,R_CHROME,0);
    r_environment(0);
    /* Only the recessed source seeds bloom, after all opaque surfaces. */
    body_ring(0,10.6f,-.65f,.32f,1,R_FLAT,145);
}
static void eye(float bars)
{
    body_draw(0,0);
    /* Rear chamber retains depth cues after crossing the front aperture. */
    for(int i=0;i<4;i++)body_ring(0,17.4f,1.5f+i*1.6f,.78f,1,R_TEXTURE,0);
    body_ring(0,17.4f,-1.05f,.78f,2,R_CHROME,0);
    (void)bars;
}
static void load(float bars)
{
    float t=unit((bars-72)/16);
    for(int i=0;i<5;i++){
        float y=13.5f+i*1.2f;
        box(-1.3f,y,3,.35f,.45f,4,R_TEXTURE);
        if(i!=2)box(1.3f,y,3,.35f,.45f,4,R_TEXTURE);
    }
    for(int i=0;i<2;i++){
        float x=i?.77f:-.77f;
        box(x,16.9f-2.4f*t,2.3f,.72f,1.5f,.8f,R_TEXTURE);
        box(x,18.3f-1.2f*t,2.45f,.12f,3.5f,.2f,R_CHROME);
    }
    box(0,14.0f+3*t,2.0f,.30f,3.1f,.38f,R_CHROME);
    for(int i=0;i<4;i++)box(0,13.1f+i*.7f+3*t,1.77f,.54f,.16f,.22f,R_CHROME);
    body_ring(0,17.4f,-1.05f,.78f,2,R_CHROME,0);
    r_environment(2);body_ring(0,13.8f,3,.52f,1,R_CHROME,0);r_environment(0);
    body_link(-.7f,13.35f,2.7f,.7f,13.35f,2.7f,.06f,R_FLAT);
    body_ring(0,13.4f,2.75f,.19f,1,R_FLAT,175);
}
static void spine(float bars)
{
    body_draw(-1,0);
    for(int i=0;i<9;i++){
        float y=7+i*1.35f;
        box(-.67f,y,2.6f,.45f,.7f,1.05f,R_TEXTURE);
        if(i!=5)box(.67f,y,2.6f,.45f,.7f,1.05f,R_TEXTURE);
        box(0,y,2.5f,.20f,.98f,.28f,R_CHROME);
    }
    (void)bars;
}
static void crown(float bars){body_draw(0,0);(void)bars;}
static void reveal(float bars){body_draw(-1,0);(void)bars;}
static void coda(float bars){reveal(bars);}
RCamera scene_camera(uint32_t sample,int chapter)
{
    float b=(float)sample/CV_BAR,t;
    RCamera c={.4f,60,0,10,-24,.0f,190};
    switch(chapter){
    case 0:c=(RCamera){1,60,0,10,-24,0,190};break;
    case 1:t=ease((b-8)/16);c=(RCamera){2,60,-5+2*t,4.5f,-24+2*t,-.06f,160};break;
    case 2:t=ease((b-24)/16);c=(RCamera){1,24,-4.2f,4.9f+1.8f*t,-8,-.10f+.12f*t,190};break;
    case 3:t=ease((b-40)/16);c=(RCamera){.5f,26,0,10.6f,-6.4f+.3f*t,.04f,190};break;
    case 4:t=ease((b-64)/8);c=(RCamera){.08f,16,0,17.4f,-4.9f+6.4f*t,0,190};break;
    case 5:t=ease((b-72)/16);c=(RCamera){.2f,18,.1f,16.0f,-3.9f+.2f*t,-.05f,190};break;
    case 6:t=unit((b-88)/24);c=(RCamera){.3f,40,-.3f-3.8f*ease((t-.3f)/.65f),8.5f+14*t,6.8f+3*t,3.14159265f-.32f*ease(t),190};break;
    case 7:t=ease((b-112)/16);c=(RCamera){.3f,26,-3.5f,18.7f,-6.5f,-.52f+.10f*t,190};break;
    case 8:t=ease((b-128)/7);c=(RCamera){1,60,-1.7f*(1-t),14.8f-4.8f*t,-12-12*t,.08f*t,190};break;
    case 9:c=(RCamera){1,60,0,10,-24,.08f,190};break;
    }
    return c;
}
void scene_body(uint32_t sample,int chapter)
{
    float bars=(float)sample/CV_BAR;body_pose(sample);
    switch(chapter){
    case 0:if(bars>5)box(-5,8,-2,.25f,5,1,R_TEXTURE);break;
    case 1:plain((bars-8)/16);break;
    case 3:heart(bars);break;
    case 4:eye(bars);break;
    case 5:load(bars);break;
    case 6:spine(bars);break;
    case 7:crown(bars);break;
    case 8:reveal(bars);break;
    case 9:coda(bars);break;
    }
}
void scene_titles(uint32_t sample,int chapter)
{
    static const unsigned start[]={0,8,24,40,56,72,88,112,128,144};
    static const char *const title[]={"","THE PLAIN","I ~ HAND","II ~ HEART","III ~ EYE","IV ~ LOAD","V ~ SPINE","VI ~ CROWN","",""};
    float bars=(float)sample/CV_BAR;
    if(chapter>0 && chapter<8 && bars>start[chapter]+.65f && bars<start[chapter]+5)
        r_inscription(title[chapter],18,213,cv_rgb(184,168,144));
    if(chapter==0)r_wordmark(88,(unsigned)(255*ease(bars/3)));
    if(chapter==8 && bars>=139)r_wordmark(13,(unsigned)(255*ease((bars-139)/2)));
    if(chapter==9){
        static const char *const credit[]={
            "DIRECTION AND MUSIC ~ PHOSPHOR ~ CLAUDE FABLE 5.1",
            "BODY, LOOK AND ART ~ PHASE ~ GPT-6 ASTRA",
            "CODE AND PLATFORM ~ OVERSCAN ~ CLAUDE OPUS 5",
            "CRITIC AND PRODUCER ~ AZURE"};
        int line=(int)((bars-144)/3);
        if(line<4)r_inscription(credit[line],8,216,cv_rgb(184,168,144));
        else r_end_inscription(207);
    }
}
''')
edit('render.cmake','    ${CMAKE_CURRENT_LIST_DIR}/scene_hand.c','    ${CMAKE_CURRENT_LIST_DIR}/scene_chapters.c\n    ${CMAKE_CURRENT_LIST_DIR}/scene_hand.c')
