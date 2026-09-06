/* Phase / LATENT: one world, sample-derived cameras and mechanisms. */
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
        float x=-37+t+(i-3)*3.9f;
        body_box((BodyPart){x,1.0f,-12+i%3,3.2f,.65f+(i%3)*.42f,2.5f,12.f*(i%3-1),0,R_TEXTURE},0);
    }
    (void)t;
}
static void heart(float bars)
{
    /* Paired ribs and the deliberately absent cross-rib are the body's own. */
    body_draw(-4,0);
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
    body_draw(-2,3);
    /* Rear chamber retains depth cues after crossing the front aperture. */
    body_ring(0,17.4f,-1.05f,.78f,2,R_CHROME,0);
    r_portal(0,17.4f,-1.05f,.78f*.62f);
    for(int i=0;i<4;i++){
        float z=1.5f+i*1.6f;
        body_ring(0,17.4f,z,.78f,1,R_TEXTURE,0);
        r_portal(0,17.4f,z,.78f*.62f);
    }
    r_portal_reset();
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
    r_environment(3);
    RVertex a=r_transform(-.8f,13.6f,2.7f,0,0,-1,0,0),b=a,c=a,d=a;
    b=r_transform(.8f,13.6f,2.7f,0,0,-1,0,0);
    c=r_transform(.8f,13.25f,2.7f,0,0,-1,0,0);d=r_transform(-.8f,13.25f,2.7f,0,0,-1,0,0);
    a.u=d.u=(bars-72)*3;b.u=c.u=a.u+63;a.v=b.v=0;c.v=d.v=31;
    a.e=b.e=c.e=d.e=110;r_triangle(a,b,c,R_FURNACE);r_triangle(a,c,d,R_FURNACE);
    r_environment(0);
}
static void spine(float bars)
{
    if(bars>=96)body_draw(-1,0);
    for(int i=0;i<9;i++){
        float y=7+i*1.35f;
        box(-.67f,y,2.6f,.45f,.7f,1.05f,R_TEXTURE);
        if(i!=5)box(.67f,y,2.6f,.45f,.7f,1.05f,R_TEXTURE);
        box(0,y,2.5f,.20f,.98f,.28f,R_CHROME);
    }
    (void)bars;
}
static void crown(float bars){body_draw(-3,0);(void)bars;}
static void reveal(float bars){body_draw(-1,0);(void)bars;}
static void coda(float bars){reveal(bars);}
RCamera scene_camera(uint32_t sample,int chapter)
{
    float b=(float)sample/CV_BAR,t;
    RCamera c={.4f,60,0,10,-24,.0f,190};
    switch(chapter){
    case 0:c=(RCamera){1,60,0,10,-24,0,190};break;
    case 1:t=ease((b-8)/16);c=(RCamera){2,60,-37+t,11,-36+t,0,160};break;
    case 2:t=ease((b-24)/16);c=(RCamera){1,24,-6.5f+.5f*t,5.1f+1.6f*t,-8,-.28f+.08f*t,190};break;
    case 3:t=ease((b-40)/16);c=(RCamera){.5f,26,0,10.6f,-6.4f+.3f*t,.04f,190};break;
    case 4:t=ease((b-64)/8);c=(RCamera){.08f,16,0,17.4f,-4.9f+6.4f*t,0,190};break;
    case 5:t=ease((b-72)/16);c=(RCamera){.2f,18,.1f,16.0f,-3.9f+.2f*t,-.05f,190};break;
    case 6:t=unit((b-88)/24);c=(RCamera){.3f,40,-.3f-3.8f*ease((t-.3f)/.65f),8.5f+14*t,6.8f+3*t,3.14159265f-.32f*ease(t),190};break;
    case 7:t=ease((b-112)/16);c=(RCamera){.3f,26,-3.5f,18.7f,-6.5f,-.52f+.10f*t,190};break;
    case 8:t=ease((b-128)/7);c=(RCamera){1,60,-1.7f*(1-t),14.8f-3.f*t,-12-12*t,.08f*t,190};break;
    case 9:c=(RCamera){1,60,0,11.8f,-24,.08f,190};break;
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
    if(chapter==8 && bars>=139)r_wordmark(-8,(unsigned)(255*ease((bars-139)/2)));
    if(chapter==9){
        static const char *const credit[]={
            "DIRECTION AND MUSIC ~ PHOSPHOR ~ CLAUDE FABLE 5.1",
            "BODY, LOOK AND ART ~ PHASE ~ GPT-6 ASTRA",
            "CODE AND PLATFORM ~ OVERSCAN ~ CLAUDE OPUS 5",
            "CRITIC AND PRODUCER ~ AZURE"};
        int line=(int)((bars-144)/3);
        if(line<4)r_inscription(credit[line],8,228,cv_rgb(184,168,144));
        else r_end_inscription(217);
    }
}
