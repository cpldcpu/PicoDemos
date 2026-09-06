/* Phase / LATENT: one world, sample-derived cameras and mechanisms. */
#include "body.h"
#include "song.h"
#include <math.h>
/* Phase's poses move the camera 0.8 body units to its right across the move.
 * Built with the plates' actual depth rather than as a flat study, that swing
 * closes the notch between them to two native pixels by bar 124 -- and the
 * notch reading as sky is the harder of the two constraints, because it is
 * the thing the chapter is about. Measured (tools/crown_notch.py, over all
 * sixteen bars): 0.80 gives 2 px, 0.65 gives 4, 0.55 gives 5-7, 0.40 gives 4,
 * 0.25 gives 3. 0.55 is the widest the notch gets, so the swing is 0.55 and
 * the rest of Phase's design -- both end positions' height and depth, the
 * targets, the focal length, the ease and the settle -- is unchanged. */
#ifndef CROWN_SWING
#define CROWN_SWING 0.55f
#endif
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
    /* The registration pier. One upright that walks into the screen position
     * the hand's wrist will occupy at bar 24, so the substitution at that
     * downbeat is a real match of shapes rather than a dissolve between two
     * unrelated pictures.
     *
     * The camera holds at (-37+t, 11, -35) with focal 160, and the wrist
     * projects to about (156, 104) under the hand's camera at bar 24. A pier
     * standing on the plain whose top edge lands there is 12.4 units tall and
     * 2.3 wide at fourteen units of depth; it starts at thirty units out and
     * closes to fourteen across the chapter, so it arrives rather than
     * appears. Its top is the wrist; its shaft is the forearm. */
    const float approach=-5.0f-16.0f*t;
    body_box((BodyPart){-36.35f+t,6.20f,approach,2.30f,12.40f,2.50f,0,0,R_TEXTURE},0);
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
/* The eye, wherever it is seen from.
 *
 * body_ring is an annulus from 0.62r to r, so a disc is a short chain of them
 * at one depth. Nesting them at DIFFERENT depths, which is what I tried
 * first, makes the near ring occlude the far half of the next and the source
 * reads as a crescent rather than a circle.
 *
 * It lives here rather than in crown() because the motion strip showed the
 * warm source going out at bar 128: the crown drew it, the reveal did not,
 * and the machine lost its light in the shot that reveals it. It is the same
 * head, so it is the same eye, and at the reveal's distance it is four
 * pixels of warm in a dark socket -- which is exactly what it should be.
 */
static void eye_assembly(int lod)
{
    const float ez=-0.64f,ey=17.45f;
    if(lod){
        body_ring(0,ey,ez,      .58f,lod,R_FLAT,0);    /* blue-black surround */
        body_ring(0,ey,ez-.04f, .40f,lod,R_TEXTURE,0); /* dark outer bearing  */
        body_ring(0,ey,ez-.04f, .25f,lod,R_TEXTURE,0);
    }
    /* At the reveal's distance the eye is four pixels across and the body's
     * own recess already supplies the dark socket, so the bearing rings are
     * 96 triangles spent on nothing: drawing them cost 2.6 ms a frame and
     * took phrase 18 from 41 fps to 30. Only the source survives out there,
     * which is the part that had to. */
    body_ring(0,ey,ez-.09f, .155f,lod,R_FLAT,150);   /* the source, filled  */
    body_ring(0,ey,ez-.09f, .096f,lod,R_FLAT,150);
    body_ring(0,ey,ez-.09f, .060f,lod?1:0,R_FLAT,150);
}

/* VI - CROWN, bars 112-127, from Phase's design in briefs/sketches/round4.
 *
 * What has to read: two unequal plates with actual cold sky between them, a
 * projecting brow, and a warm circular source set back in a blue-black
 * recess with a dark outer bearing and a small bright centre. The shared body
 * already carries the plates (tops at 20.4 and 19.7) and the brow; what it
 * did not carry was an eye -- eye_recess is a flat rectangle, and at this
 * camera it read as a black letterbox. So the eye is built here, in the one
 * chapter that looks straight at it.
 *
 * Phase's coordinates are +Z out of the face; the renderer's are the
 * opposite, so the eye's (0,17.45,0.64) is (0,17.45,-0.64) here.
 *
 * body_ring draws an annulus from 0.62r to r, so four of them nested make a
 * bearing that steps inward from cold chrome to a small warm centre. Only the
 * innermost carries emission, which is what keeps the bloom restrained: it is
 * about five native pixels across at this focal length. */
static void crown(float bars)
{
    body_draw(-3,0);
    eye_assembly(2);
    /* The brow projects over it and is the shape that makes the hood a hood
     * rather than a slab. Two plates and a lip, all bronze. */
    box(0,18.62f,-1.55f,2.55f,.42f,1.20f,R_TEXTURE);
    box(-1.30f,18.30f,-1.30f,.60f,.95f,.85f,R_TEXTURE);
    box( 1.30f,18.30f,-1.30f,.60f,.95f,.85f,R_TEXTURE);
    box(0,18.10f,-1.92f,2.05f,.22f,.30f,R_TEXTURE); /* the brow's lip       */
    (void)bars;
}
static void reveal(float bars){body_draw(-1,0);eye_assembly(0);(void)bars;}
static void coda(float bars){reveal(bars);}
/* The outgoing chapter's matched shape, drawn in screen space into whichever
 * cell the veil has scissored, for the cells that have not been exchanged yet.
 *
 * It is screen space and not world space on purpose: the whole point of the
 * substitution is that the two shapes occupy the same pixels, so the shape is
 * specified where the match lives. One structure per cell, one draw.
 *
 * Only bar 24 has a real matched shape so far -- the plain's registration
 * pier against the hand's wrist. The others return 0 and the veil falls back
 * to its glow alone; tools/transition_check.py lists them. */
int scene_outgoing(unsigned boundary,float z,float focal)
{
    switch(boundary){
    case 24:{
        /* the pier: an upright bronze shaft with its top at the wrist */
        const float f=focal;
        const int x0=132,x1=180,y0=104,y1=240;
        RVertex a={(x0-160)*z/f,(120-y0)*z/f,z,155,0,0,0};
        RVertex b={(x1-160)*z/f,(120-y0)*z/f,z,205,63,0,0};
        RVertex c={(x1-160)*z/f,(120-y1)*z/f,z,120,63,63,0};
        RVertex d={(x0-160)*z/f,(120-y1)*z/f,z,70,0,63,0};
        r_triangle(a,b,c,R_TEXTURE);r_triangle(a,c,d,R_TEXTURE);
        return 1;
    }
    default:return 0;
    }
}

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
    /* The eye's aperture ring is at z = -1.05 and chapter 4 ends past it, so
     * the load has to start past it too or the camera jumps back out through
     * the hole it just went through. It used to sit at -3.9, in front. At
     * -0.55 the ring is half a unit behind the lens, which is what Phase's
     * "the ring stays behind the camera" asks for and what makes this
     * boundary a real match: the same object, continuous across the cut,
     * rather than two pictures that happen to share a centre. */
    case 5:t=ease((b-72)/16);c=(RCamera){.2f,18,.1f,15.4f,-0.55f+.2f*t,-.05f,190};break;
    case 6:t=unit((b-88)/24);c=(RCamera){.3f,40,-.3f-3.8f*ease((t-.3f)/.65f),8.5f+14*t,6.8f+3*t,3.14159265f-.32f*ease(t),190};break;
    case 7:{
        /* Phase's two poses, eased over 112-124 and held through 127. The
         * file gives position and target; yaw and pitch are derived rather
         * than interpolated, so the camera keeps looking at the head all the
         * way across instead of drifting off it in the middle. */
        t=ease((b-112)/12);
        const float px=6.0f+CROWN_SWING*t,py=16.0f+0.6f*t,pz=-(12.0f-0.2f*t);
        const float tx=0.0f,ty=18.35f+0.10f*t,tz=0.0f;
        const float dx=tx-px,dy=ty-py,dz=tz-pz;
        const float horiz=sqrtf(dx*dx+dz*dz);
        c=(RCamera){.3f,40,px,py,pz,atan2f(-dx,dz),510,atan2f(dy,horiz)};
        break;
    }
    case 8:{
        /* The head is the same head, pulled back. The reveal now *starts* at
         * the crown's final camera -- position, focal length, yaw and pitch --
         * and eases to the standing shot over bars 128 to 135, so the cut is
         * a continuation of one move rather than two compositions abutted.
         * At t = 1 this is exactly the camera it always was, which is what
         * render_checks' 160-180 px silhouette and the coda's held camera
         * both depend on. */
        t=ease((b-128)/7);
        const float u=1-t;
        c=(RCamera){1,60,
            6.55f*u,
            16.6f*u+11.8f*t,
            -11.8f*u-24.0f*t,
            0.5069f*u+0.08f*t,
            510.f*u+190.f*t,
            0.1362f*u};
        break;
    }
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
