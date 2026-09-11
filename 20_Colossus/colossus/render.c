/* Phase / LATENT. VESPER's clipped span approach, full-height COLOSSUS page.
 * All mutable storage is scratch reset by r_begin; sample order is irrelevant. */
#include "render.h"
#include "body.h"
#include "song.h"
#include "assets/engine_assets.h"
#include "assets/painted_assets.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------- profiling --
 * Core 0's SysTick, free-running off the processor clock. Core 1 has its own
 * (video.c) and the two do not interfere. On the host these compile to
 * nothing: prof_end() adds zero and the compiler drops the arithmetic. */
#if defined(PICO_BUILD)
#  include "hardware/structs/systick.h"
#  define R_TICK_MASK 0x00FFFFFFu
static inline uint32_t r_tick(void){return systick_hw->cvr;}
static void r_prof_init(void)
{
    systick_hw->rvr=R_TICK_MASK;systick_hw->cvr=0;systick_hw->csr=0x5u;
}
#else
#  define R_TICK_MASK 0u
static inline uint32_t r_tick(void){return 0;}
static void r_prof_init(void){}
#endif
static uint32_t r_prof_cy[RP_COUNT];
static inline uint32_t prof_begin(void){return r_tick();}
static inline void prof_end(int slot,uint32_t t0){r_prof_cy[slot]+=(t0-r_tick())&R_TICK_MASK;}
void r_prof_read(uint32_t *out){for(int i=0;i<RP_COUNT;i++)out[i]=r_prof_cy[i];}
const char *r_prof_name(int slot)
{
    static const char *const names[RP_COUNT]={
        "clear","sky","floor","scene","flat","gouraud","chrome","texture",
        "furnace","embers","bloom","veil","titles","fade"};
    return (slot>=0&&slot<RP_COUNT)?names[slot]:"?";
}

static uint8_t r_depth[CV_H][CV_W],r_glow[60][80],r_blur[60][80];
/* Per-frame background tables: the resolved sky palette, the same darkened
 * for chapter 4's aperture, the fog row shared by every floor row, and the
 * fixed x -> sky-texel map. 1,664 bytes to delete eighteen million cycles. */
static uint16_t r_sky_pal[256],r_sky_dim[256],r_fog_row[CV_W];
static uint8_t  r_sky_x[CV_W];
static uint16_t r_light[256];   /* 100 + i*155/255, for the texture span */
static uint16_t r_shades[4][256];
static uint16_t *r_page;
static RCamera r_camera;
static unsigned r_env,r_dawn;
static int clip_x0,clip_y0,clip_x1,clip_y1;
#define r_chrome_palette r_shades[2]
static demo_stats_t r_stats;
static int glow_x0,glow_y0,glow_x1,glow_y1;
static int clamp(int x,int a,int b){return x<a?a:x>b?b:x;}
static float lerp(float a,float b,float t){return a+(b-a)*t;}
void demo_init(void)
{
    r_prof_init();
    for(int x=0;x<CV_W;x++)r_sky_x[x]=(uint8_t)(x*256/CV_W);
    for(int i=0;i<256;i++)r_light[i]=(uint16_t)(100+i*155/255);
    for(int i=0;i<256;i++){
        r_shades[0][i]=cv_rgb(16+i/2,24+i/3,32+i/5);
        r_shades[1][i]=cv_rgb(30+i*154/255,31+i*121/255,32+i*72/255);
        r_shades[2][i]=cv_rgb(40+i*176/255,56+i*168/255,64+i*160/255);
        r_shades[3][i]=cv_rgb(44+i/3,39+i/4,34+i/6);
    }
}
void r_begin(uint16_t *page,unsigned chapter,RCamera camera)
{
    r_page=page;r_camera=camera;r_stats=(demo_stats_t){0,0,0,(uint8_t)chapter};
    const uint32_t t=prof_begin();
    memset(r_depth,0,sizeof r_depth);memset(r_glow,0,sizeof r_glow);
    memset(r_blur,0,sizeof r_blur);
    prof_end(RP_CLEAR,t);
    r_env=0;r_dawn=0;r_portal_reset();
    glow_x0=80;glow_y0=60;glow_x1=glow_y1=-1;
}
/* Blend two packed DAC colours. Both inputs are already five bits a channel
 * and in range by construction, so this needs neither the round trip up to
 * eight bits and back that the old one did (*8/255 per channel, then cv_rgb's
 * >>3) nor cv_rgb's six clamps. t is 0..256 so the normalisation is a shift.
 * The old form cost three integer divides per call and this one costs none;
 * it is called about a hundred thousand times a frame. */
static inline uint16_t mix5(uint16_t a,uint16_t b,unsigned t)
{
    const unsigned u=256-t;
    const unsigned r =( (a     &31u)*u+ (b     &31u)*t)>>8;
    const unsigned g =(((a>> 6)&31u)*u+((b>> 6)&31u)*t)>>8;
    const unsigned bl=(((a>>11)&31u)*u+((b>>11)&31u)*t)>>8;
    return (uint16_t)(r|(g<<6)|(bl<<11));
}
/* Ordered dither (PLANNING section 4). One 4x4 Bayer table serves every ramp
 * that gets quantised to five bits: the veil's alpha here, and the sky, floor
 * and shading ramps. The value is added to a fixed-point ramp before the
 * shift, so a gradient that would step every eight pixels instead scatters
 * its step over a 4x4 cell. */
static const uint8_t bayer4[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
#define BAYER4(x,y) bayer4[((((y)&3)<<2)|((x)&3))]

/* The 0..255 spelling every caller already uses; 255 must reach a full 256. */
static inline uint16_t mix_color(uint16_t a,uint16_t b,unsigned t)
{
    return mix5(a,b,t+(t>>7));
}
void r_environment(unsigned mode){r_env=mode;}
/* Reciprocals, not divisions: gcc will not turn /1.75f into *(1/1.75f)
 * without -ffast-math, and this was six single-precision divides per floor
 * pixel -- about eighty cycles of the roughly three hundred each floor pixel
 * used to cost. The caller passes 1/rx and 1/rz. */
static inline float contact(float wx,float wz,float x,float z,float irx,float irz)
{
    const float dx=(wx-x)*irx,dz=(wz-z)*irz;
    const float d=1-dx*dx-dz*dz;
    return d>0?d:0;
}
/* The three ground contacts as one smooth field. Sampled every eight pixels
 * and interpolated; it is quadratic and the error over eight pixels is below
 * one DAC step. */
static float shadow_at(float wx,float wz)
{
    float s=.32f*contact(wx,wz,0,0,1/5.f,1/2.5f);
    const float a=.86f*contact(wx,wz,-1.6f,-.85f,1/1.75f,1/1.8f);
    const float b=.86f*contact(wx,wz,1.6f,.3f,1/1.65f,1/1.55f);
    if(a>s)s=a;
    if(b>s)s=b;
    return s;
}
void CV_HOT(r_background)(uint32_t sample)
{
    float dawn=fminf(1,fmaxf(0,((float)sample/CV_BAR-128)/7));
    r_dawn=(unsigned)(255*dawn);
    /* Both maps use the dusk normal-index topology; dawn is a palette morph. */
    for(int i=0;i<256;i++)r_chrome_palette[i]=mix_color(dusk_matcap_palette[i],dawn_matcap_palette[i],r_dawn);
    unsigned chapter=r_stats.chapter;
    const uint32_t t_sky=prof_begin();

    /* The sky was 76,800 pixels of palette morph, chamber darkening and
     * overture fade, all recomputed per pixel although every one of them
     * depends only on the palette index. They are resolved once into a
     * 256-entry table and the pixel loop becomes two loads and a store.
     *
     * Rows repeat as well: ty takes 64 values over 240 rows, so three rows in
     * four are a memcpy of the row above. Chapter 4's aperture is the one
     * thing that genuinely varies per pixel, so it is a second pass over the
     * circle's bounding box rather than a test in the main loop. */
    const int chamber=(chapter==3||chapter==5||(chapter==6&&sample<98*CV_BAR));
    const unsigned rise=chapter==0?255-(unsigned)(75*fminf(1,(float)sample/(CV_BAR*7))):0;
    for(int i=0;i<256;i++){
        uint16_t c=mix_color(dusk_sky_palette[i],dawn_sky_palette[i],r_dawn);
        if(chamber)c=mix_color(c,cv_rgb(16,24,32),210);
        if(chapter==0)c=mix_color(c,0,rise);
        r_sky_pal[i]=c;
        r_sky_dim[i]=mix_color(c,cv_rgb(8,16,24),235);
    }
    /* The sky is sampled, not computed, so it is NOT dithered. Bayer belongs
     * on a ramp evaluated at more precision than five bits and then
     * quantised -- the veil's alpha, the Gouraud and texture light, the bloom
     * halo -- and this is none of those: it is a painting whose palette is
     * already five bits, error-diffused by Phase's converter. I tried
     * dithering the row choice between source rows ty and ty+1 and measured
     * the result: the banded column at the reveal went from 114 runs to 118
     * and its longest flat run stayed at 28 rows, because the band is six
     * consecutive rows of index 200 in dusk_sky.pixels.bin, not a sampling
     * seam. What the dither did do was add visible noise to the sky and cost
     * the memcpy below. Both were the wrong trade; the band is a note for
     * Phase, in the reply.
     *
     * Rows repeat: ty takes 64 values over 240 rows, so three rows in four
     * are a memcpy of the row above. */
    const int div=(chapter==1||chapter==8||chapter==9)?120:240;
    int prev_ty=-1;
    for(int y=0;y<CV_H;y++){
        uint16_t *row=r_page+y*CV_W;
        const int ty=clamp(y*64/div,0,63);
        if(ty==prev_ty){memcpy(row,row-CV_W,CV_W*sizeof *row);continue;}
        prev_ty=ty;
        const uint8_t *src=dusk_sky_pixels+ty*256;
        for(int x=0;x<CV_W;x++)row[x]=r_sky_pal[src[r_sky_x[x]]];
    }
    if(chapter==4){
        const float z=-1.05f-r_camera.cz;
        float rad=z>.05f?.49f*r_camera.focal/z:1000;
        if(rad>640)rad=640;
        const int ir=(int)rad,r2=ir*ir;
        const int x0=clamp(160-ir,0,CV_W),x1=clamp(160+ir+1,0,CV_W);
        const int y0=clamp(120-ir,0,CV_H),y1=clamp(120+ir+1,0,CV_H);
        for(int y=y0;y<y1;y++){
            const int dy=y-120,dy2=dy*dy;
            const uint8_t *src=dusk_sky_pixels+clamp(y*64/div,0,63)*256;
            uint16_t *row=r_page+y*CV_W;
            for(int x=x0;x<x1;x++){
                const int dx=x-160;
                if(dx*dx+dy2<r2)row[x]=r_sky_dim[src[r_sky_x[x]]];
            }
        }
    }
    prof_end(RP_SKY,t_sky);
    if(chapter!=1 && chapter!=8 && chapter!=9)return;

    /* Level world plane. Everything that was per pixel is now per row, per
     * frame, or per eight pixels:
     *   - the fog is the painted sky's bottom row and does not vary with y,
     *     so it is built once a frame instead of 38,080 times;
     *   - the world position steps linearly in x, so the texture coordinate
     *     is a 16.16 accumulator instead of two multiplies and two float
     *     truncations per pixel;
     *   - the three contact shadows are a smooth quadratic field, sampled at
     *     eight-pixel boundaries and interpolated, and skipped outright where
     *     both ends are zero -- which is most of the plain;
     *   - the haze is zero below row 156, so two thirds of the floor does not
     *     blend at all.
     * The wrap is now an arithmetic shift rather than a truncation toward
     * zero, so texels left of the origin land one texel over from where they
     * used to. On a 64-texel tile of stone that is invisible, and flooring is
     * the correct wrap.  */
    const uint32_t t_floor=prof_begin();
    const int horizon=120;
    const float c=cosf(r_camera.yaw),sn=sinf(r_camera.yaw);
    for(int x=0;x<CV_W;x++){
        const int idx=dusk_sky_pixels[63*256+r_sky_x[x]];
        r_fog_row[x]=mix_color(dusk_sky_palette[idx],dawn_sky_palette[idx],r_dawn);
    }
    const uint16_t deep=cv_rgb(8,16,24);
    for(int y=horizon+1;y<CV_H;y++){
        const float z=r_camera.cy*r_camera.focal/(y-120.f),v=z*4;
        const int hz=(int)(255*fmaxf(0,1-(y-horizon)/36.f));
        const unsigned haze=(unsigned)hz+(unsigned)(hz>>7);
        const uint8_t *tex_row=stone_pixels+((int)v&63)*64;
        const float dxs=z/r_camera.focal;
        const float dwx=dxs*c,dwz=dxs*sn;
        float wx=r_camera.cx-160*dwx-z*sn,wz=r_camera.cz-160*dwz+z*c;
        int32_t u=(int32_t)(wx*4*65536.f);
        const int32_t du=(int32_t)(dwx*4*65536.f);
        uint16_t *row=r_page+y*CV_W;
        int sh0=(int)(255*shadow_at(wx,wz));
        for(int x=0;x<CV_W;x+=8){
            wx+=dwx*8;wz+=dwz*8;
            const int sh1=(int)(255*shadow_at(wx,wz));
            if(!sh0 && !sh1){
                if(haze){
                    for(int j=0;j<8;j++,u+=du)
                        row[x+j]=mix5(stone_palette[tex_row[(u>>16)&63]],r_fog_row[x+j],haze);
                }else{
                    for(int j=0;j<8;j++,u+=du)
                        row[x+j]=stone_palette[tex_row[(u>>16)&63]];
                }
            }else{
                const int ds=sh1-sh0;
                for(int j=0;j<8;j++,u+=du){
                    const int sv=sh0+ds*j/8;
                    uint16_t col=mix_color(stone_palette[tex_row[(u>>16)&63]],deep,(unsigned)sv);
                    row[x+j]=haze?mix5(col,r_fog_row[x+j],haze):col;
                }
            }
            sh0=sh1;
        }
    }
    prof_end(RP_FLOOR,t_floor);
}
RVertex r_transform(float x,float y,float z,float nx,float ny,float nz,float u,float v)
{
    const float c=cosf(r_camera.yaw),s=sinf(r_camera.yaw);
    x-=r_camera.cx;y-=r_camera.cy;z-=r_camera.cz;
    float xx=x*c+z*s,zz=-x*s+z*c,yy=y;
    float nxx=nx*c+nz*s,nzz=-nx*s+nz*c,nyy=ny;
    /* Pitch about the camera's own X axis, after yaw. Zero for every chapter
     * but the crown, which has to look up at a head four body units above the
     * lens and cannot do it by moving the horizon. */
    if(r_camera.pitch!=0){
        const float cp=cosf(r_camera.pitch),sp=sinf(r_camera.pitch);
        const float y2=yy*cp-zz*sp;zz=yy*sp+zz*cp;yy=y2;
        const float n2=nyy*cp-nzz*sp;nzz=nyy*sp+nzz*cp;nyy=n2;
    }
    /* Lighting remains in world space. Matcap normals follow the camera. */
    const float light=fmaxf(0,-nx*.4f+ny*.75f-nz*.45f);
    (void)u;(void)v;
    return (RVertex){xx,yy,zz,35+200*light,31.5f+31*nxx,31.5f-31*nyy,0};
}
typedef struct {float x,y,q,l,u,v,e;} Screen;
static Screen project(RVertex a)
{
    float iz=1/a.z,n=r_camera.near_z,f=r_camera.far_z;
    return (Screen){160+a.x*r_camera.focal*iz,120-a.y*r_camera.focal*iz,1+254*(iz-1/f)/(1/n-1/f),a.l,a.u,a.v,a.e};
}
void r_portal_reset(void){clip_x0=clip_y0=0;clip_x1=320;clip_y1=240;}
void r_scissor(int x0,int y0,int x1,int y1)
{
    clip_x0=clamp(x0,0,CV_W);clip_x1=clamp(x1,0,CV_W);
    clip_y0=clamp(y0,0,CV_H);clip_y1=clamp(y1,0,CV_H);
}
void r_portal(float x,float y,float z,float radius)
{
    RVertex v=r_transform(x,y,z,0,0,-1,0,0);
    if(v.z<r_camera.near_z)return;
    Screen p=project(v);float r=radius*r_camera.focal/v.z;
    clip_x0=clamp((int)floorf(p.x-r),clip_x0,clip_x1);
    clip_x1=clamp((int)ceilf(p.x+r),clip_x0,clip_x1);
    clip_y0=clamp((int)floorf(p.y-r),clip_y0,clip_y1);
    clip_y1=clamp((int)ceilf(p.y+r),clip_y0,clip_y1);
}
/* Fixed-point increments; each material has its own tight inner loop. */
typedef struct {int q,l,u,v,e,dq,dl,du,dv,de;} Span;
static void glow_seed(int off,int strength)
{
    if(strength<=0)return;
    int x=(off%320)/4,y=(off/320)/4;
    if(strength>r_glow[y][x])r_glow[y][x]=(uint8_t)clamp(strength,0,255);
    if(x<glow_x0)glow_x0=x;
    if(x>glow_x1)glow_x1=x;
    if(y<glow_y0)glow_y0=y;
    if(y>glow_y1)glow_y1=y;
}
/* Two loops per material, not one.
 *
 * Almost every span in the demo is non-emissive, and the emission test used
 * to run for every pixel of every span. It is a property of the span, not of
 * the pixel -- e and de are both zero or they are not -- so it is hoisted and
 * the common path loses a load, a test and a branch per pixel. The chrome
 * environment is a per-frame mode, so its two source tables are chosen before
 * the loop rather than inside it. */
#define SPAN_BODY(COLOR,EMIT) \
    for(int i=0;i<count;i++,off++,s.q+=s.dq,s.l+=s.dl,s.u+=s.du,s.v+=s.dv,s.e+=s.de){ \
        int q=clamp(s.q>>16,1,255); \
        if(q> ((uint8_t*)r_depth)[off]){ \
            ((uint8_t*)r_depth)[off]=(uint8_t)q;r_page[off]=(COLOR); \
            EMIT \
        } \
    }
#define SPAN_EMIT \
    if(s.e>0){int e=clamp(s.e>>16,0,255);if(r_env!=3)r_page[off]=cv_rgb(130+e/2,50+e/3,24+e/8);glow_seed(off,e);}
#define SPAN_LOOP(COLOR) \
    if(s.e|s.de){SPAN_BODY(COLOR,SPAN_EMIT)}else{SPAN_BODY(COLOR,)}
static void CV_HOT(span_flat)(int off,int count,Span s,const uint8_t *br,int x0){(void)br;(void)x0;uint16_t color=r_shades[0][clamp(s.l>>16,0,255)];SPAN_LOOP(color)}
static void CV_HOT(span_gouraud)(int off,int count,Span s,const uint8_t *br,int x0){SPAN_LOOP(r_shades[1][clamp((s.l+(br[(x0+i)&3]<<12))>>16,0,255)])}
static void CV_HOT(span_chrome)(int off,int count,Span s,const uint8_t *br,int x0)
{
    (void)br;(void)x0;
    const uint16_t *pal=r_env==2?warm_environment_palette:r_chrome_palette;
    const unsigned char *tex=r_env==2?warm_environment_pixels:dusk_matcap_pixels;
    SPAN_LOOP(pal[tex[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]])
}
static void CV_HOT(span_furnace)(int off,int count,Span s,const uint8_t *br,int x0){(void)br;(void)x0;SPAN_LOOP(furnace_palette[furnace_pixels[((s.v>>16)&31)*64+((s.u>>16)&63)]])}
/* r_light replaces an integer divide by 255 in the innermost loop of the
 * material that covers most of the body. */
static void CV_HOT(span_texture)(int off,int count,Span s,const uint8_t *br,int x0){SPAN_LOOP(r_shades[1][(bronze_wear_pixels[((s.v>>16)&63)*64+((s.u>>16)&63)]*r_light[clamp((s.l+(br[(x0+i)&3]<<12))>>16,0,255)])>>8])}
#undef SPAN_LOOP
#undef SPAN_BODY
#undef SPAN_EMIT
static void CV_HOT(raster)(Screen a,Screen b,Screen c,RMaterial mat)
{
    if(a.y>b.y){Screen t=a;a=b;b=t;}if(b.y>c.y){Screen t=b;b=c;c=t;}if(a.y>b.y){Screen t=a;a=b;b=t;}
    float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
    if(fabsf(area)<.01f || c.y<0 || a.y>=240)return;
    float inv=1/area,dx[5],dy[5];
    float av[5]={a.q,a.l,a.u,a.v,a.e},bv[5]={b.q,b.l,b.u,b.v,b.e},cv[5]={c.q,c.l,c.u,c.v,c.e};
    for(int i=0;i<5;i++){
        dx[i]=((bv[i]-av[i])*(c.y-a.y)-(cv[i]-av[i])*(b.y-a.y))*inv;
        dy[i]=((b.x-a.x)*(cv[i]-av[i])-(c.x-a.x)*(bv[i]-av[i]))*inv;
    }
    int ys=clamp((int)ceilf(a.y-.5f),clip_y0,clip_y1),ye=clamp((int)ceilf(c.y-.5f),clip_y0,clip_y1);
    float long_s=(c.x-a.x)/(c.y-a.y+1e-12f),upper=(b.x-a.x)/(b.y-a.y+1e-12f),lower=(c.x-b.x)/(c.y-b.y+1e-12f);
    for(int y=ys;y<ye;y++){
        float fy=y+.5f,x0=a.x+(fy-a.y)*long_s,x1=fy<b.y?a.x+(fy-a.y)*upper:b.x+(fy-b.y)*lower;
        if(x0>x1){float t=x0;x0=x1;x1=t;}
        int xs=clamp((int)ceilf(x0-.5f),clip_x0,clip_x1),xe=clamp((int)ceilf(x1-.5f),clip_x0,clip_x1);
        if(xs>=xe)continue;
        int v[5];for(int i=0;i<5;i++)v[i]=(int)((av[i]+dx[i]*(xs+.5f-a.x)+dy[i]*(fy-a.y))*65536);
        Span s={v[0],v[1],v[2],v[3],v[4],(int)(dx[0]*65536),(int)(dx[1]*65536),(int)(dx[2]*65536),(int)(dx[3]*65536),(int)(dx[4]*65536)};
        r_stats.fill+=(uint32_t)(xe-xs);
        const uint8_t *br=bayer4+((y&3)<<2);
        switch(mat){case R_FLAT:span_flat(y*320+xs,xe-xs,s,br,xs);break;case R_GOURAUD:span_gouraud(y*320+xs,xe-xs,s,br,xs);break;case R_CHROME:span_chrome(y*320+xs,xe-xs,s,br,xs);break;case R_TEXTURE:span_texture(y*320+xs,xe-xs,s,br,xs);break;case R_FURNACE:span_furnace(y*320+xs,xe-xs,s,br,xs);break;}
    }
}
static RVertex interpolate(RVertex a,RVertex b,float t)
{
    return (RVertex){lerp(a.x,b.x,t),lerp(a.y,b.y,t),lerp(a.z,b.z,t),lerp(a.l,b.l,t),lerp(a.u,b.u,t),lerp(a.v,b.v,t),lerp(a.e,b.e,t)};
}
void CV_HOT(r_triangle)(RVertex a,RVertex b,RVertex c,RMaterial mat)
{
    const uint32_t t_tri=prof_begin();
    RVertex p[8]={a,b,c},out[8];int n=3;r_stats.triangles++;
    /* Clip near AND far; interpolate all attributes at both planes. */
    for(int plane=0;plane<2;plane++){
        float edge=plane?r_camera.far_z:r_camera.near_z;int m=0;
        for(int i=0;i<n;i++){
            RVertex v=p[i],w=p[(i+1)%n];int iv=plane?v.z<=edge:v.z>=edge,iw=plane?w.z<=edge:w.z>=edge;
            if(iv)out[m++]=v;
            if(iv!=iw)out[m++]=interpolate(v,w,(edge-v.z)/(w.z-v.z));
        }
        memcpy(p,out,(size_t)m*sizeof *p);n=m;
        if(n<3){prof_end(RP_TRI_FLAT+(int)mat,t_tri);return;}
    }
    for(int i=1;i<n-1;i++)raster(project(p[0]),project(p[i]),project(p[i+1]),mat);
    prof_end(RP_TRI_FLAT+(int)mat,t_tri);
}
void CV_HOT(r_bloom)(void)
{
    if(glow_x1<glow_x0)return;
    const uint32_t t_bl=prof_begin();
    int x0=clamp(glow_x0-2,0,79),x1=clamp(glow_x1+2,0,79),y0=clamp(glow_y0-2,0,59),y1=clamp(glow_y1+2,0,59);
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)r_blur[y][x]=(uint8_t)((r_glow[y][clamp(x-1,0,79)]+2*r_glow[y][x]+r_glow[y][clamp(x+1,0,79)])/4);
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)r_glow[y][x]=(uint8_t)((r_blur[clamp(y-1,0,59)][x]+2*r_blur[y][x]+r_blur[clamp(y+1,0,59)][x])/4);
    /* Composite only occupied 4x4 rectangles; dust may spread the blur bounds
       across the page, but empty cells never cause a full-resolution walk. */
    for(int gy=y0;gy<=y1;gy++)for(int gx=x0;gx<=x1;gx++){
        int v=r_glow[gy][gx];if(!v)continue;
        for(int y=gy*4;y<gy*4+4;y++)for(int x=gx*4;x<gx*4+4;x++){
            /* The glow is added in eight-bit space and then quantised to five,
             * so a soft halo steps. Bayer scatters the step. */
            const int d=BAYER4(x,y)/2;
            uint16_t p=r_page[y*320+x];
            r_page[y*320+x]=cv_rgb((p&31)*8+(v*4+d)/8,((p>>6)&31)*8+(v*2+d)/8,((p>>11)&31)*8+(v*2+d)/24);
        }
    }
    prof_end(RP_BLOOM,t_bl);
}
/* Scale every channel of the whole page by level/255.
 * Was three multiplies, three divides and a cv_rgb per pixel, 76,800 times,
 * for 1.85 M cycles in the overture. A five-bit channel has 32 values, so
 * the scaling is a 32-entry table built once and three lookups per pixel. */
static void CV_HOT(page_scale)(uint16_t *page,unsigned level)
{
    if(level>=255)return;
    uint16_t lut[32];
    for(int i=0;i<32;i++)lut[i]=(uint16_t)(i*level/255);
    for(int i=0;i<CV_W*CV_H;i++){
        const uint16_t p=page[i];
        page[i]=(uint16_t)(lut[p&31u]|(lut[(p>>6)&31u]<<6)|(lut[(p>>11)&31u]<<11));
    }
}
static uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
void CV_HOT(r_embers)(uint32_t sample,unsigned count)
{
    const uint32_t t_em=prof_begin();
    if(count>256)count=256;
    r_stats.particles=count;
    for(unsigned i=0;i<count;i++){
        uint32_t life=CV_RATE*(5+i%6),t=sample+hash(i)%life,epoch=t/life,age=t%life,h=hash(i+epoch*1021);
        float f=(float)age/life;
        int x=(int)(h%320)+(int)(sinf(f*5+i)*12),y=235-(int)(f*(85+(h%55))),q=16+(int)((h>>16)%220);
        unsigned cls=i%3;int size=cls==2?3:cls==1?2:1;
        for(int yy=0;yy<size;yy++)for(int xx=0;xx<size;xx++){
            int px=x+xx,py=y+yy;
            if((unsigned)px>=320||(unsigned)py>=240||q<=r_depth[py][px])continue;
            int sx=(2*xx+1)*8/size,sy=(2*yy+1)*8/size;
            int idx=cls*256+sy*16+sx;unsigned packed=ember_stamps_pixels[idx/2];
            int stamp=(idx&1)?packed&15:packed>>4;
            int e=(int)((70+cls*70)*sinf(f*3.14159265f))*stamp/15;
            e=e*(90+song_energy(cv_bar_of(sample))*165/255)/255;
            if(e<12)continue;
            r_page[py*320+px]=cv_rgb(50+e,28+e*2/3,16+e/4);
        }
    }
    prof_end(RP_EMBERS,t_em);
}
/* A common foreground rib pair at each substitution, under local lit dust.
   Stateless screen-space occluder; only one chapter is ever rendered. */
void CV_HOT(r_transition)(uint32_t sample)
{
    const uint32_t t_v=prof_begin();
    static const unsigned boundaries[]={8,24,40,56,72,88,112,128,144};
    for(unsigned i=0;i<sizeof boundaries/sizeof boundaries[0];i++){
        const float dt=((float)sample-boundaries[i]*CV_BAR)/CV_RATE;
        if(fabsf(dt)>.5f)continue;

        /* Phase's envelope, from the six-moment strip: the glow is zero at
         * half a second either side, 0.85 of peak at a tenth, and the
         * substitution runs from the downbeat to +0.18 s -- inside the +0.3 s
         * the design allows. Before the downbeat nothing is exchanged, which
         * is why the outgoing structure is only ever drawn after it: until
         * then the chapter on screen IS the outgoing one. */
        const float u=1-fabsf(dt)/.5f;
        float glow=u*u*(3-2*u);
        const float sub=dt<=0?0:(dt>=.18f?1:dt/.18f);
        if(i==8)glow*=.35f; /* the coda holds the exact reveal camera */

        /* The exchange, in small stable screen-space cells inside the local
         * substitution silhouette. One structure per cell: a cell shows the
         * incoming chapter (which is simply what is already on the page) or
         * the outgoing shape, never a blend of the two, and which one is a
         * fixed per-cell threshold so the pattern does not crawl. */
        if(sub>0 && sub<1){
            const int sx=128,sy=56,sw=56,sh=126,cw=14,ch=21;
            for(int cy=0;cy<sh/ch;cy++)for(int cx=0;cx<sw/cw;cx++){
                const float thr=(float)(hash((unsigned)(cx+cy*7+(int)i*131))&1023)/1024.f;
                if(sub>thr)continue;               /* already exchanged */
                r_scissor(sx+cx*cw,sy+cy*ch,sx+cx*cw+cw,sy+cy*ch+ch);
                if(!scene_outgoing(boundaries[i],r_camera.near_z*1.15f,r_camera.focal))break;
            }
            r_portal_reset();
        }

        /* The veil itself: Phase's ellipse, centre (151,85), radii (69,110),
         * peak alpha 0.24 falling as (1-r^2)^2, dithered through Bayer so a
         * sixty-step ramp over a hundred pixels has no bands and no edge. */
        const int cx=151,cy=85,ra=69,rb=110;
        const int inv_ra2=(4096*4096)/(ra*ra),inv_rb2=(4096*4096)/(rb*rb);
        const int peak=(int)(.24f*255*16*glow);
        const uint16_t warm=cv_rgb(196,150,104);
        const int y0=cy-rb<0?0:cy-rb,y1=cy+rb>=CV_H?CV_H-1:cy+rb;
        const int x0=cx-ra<0?0:cx-ra,x1=cx+ra>=CV_W?CV_W-1:cx+ra;
        if(peak>0)for(int y=y0;y<=y1;y++){
            const int dy=y-cy,qy=(dy*dy*inv_rb2)>>12;
            if(qy>=4096)continue;
            uint16_t *row=r_page+y*CV_W;
            for(int x=x0;x<=x1;x++){
                const int dx=x-cx,q=qy+((dx*dx*inv_ra2)>>12);
                if(q>=4096)continue;
                const int f=4096-q,wq=(f*f)>>12;
                const unsigned av=(unsigned)((peak*wq)>>12);
                const unsigned a=(av+BAYER4(x,y))>>4;
                if(a)row[x]=mix5(row[x],warm,a+(a>>7));
            }
        }
        break;
    }
    prof_end(RP_VEIL,t_v);
}
void r_inscription(const char *text,int x,int y,uint16_t color)
{
    int advance=strlen(text)>38?6:8;
    for(;*text;text++,x+=advance){unsigned ch=(unsigned char)*text;
        if(ch=='~')ch='.';
        if(ch<32||ch>95)continue;
        int n=(int)ch-32,ox=(n%16)*8,oy=(n/16)*12;
        for(int yy=0;yy<12;yy++)for(int xx=0;xx<advance;xx++){
            int bit=(oy+yy)*128+ox+(advance==6?1:0)+xx;
            if((unsigned)(x+xx)<320&&(unsigned)(y+yy)<240&&(inscription_atlas[bit>>3]&(128>>(bit&7))))r_page[(y+yy)*320+x+xx]=color;
        }
    }
}
void r_wordmark(int top,unsigned level)
{
    if(level>255)level=255;
    /* Sixteen palette entries, not 20,480 pixels: the level scaling is the
     * same for every pixel that shares an index. */
    uint16_t pal[16];
    for(int i=0;i<16;i++){
        const uint16_t c=wordmark_palette[i];
        pal[i]=(uint16_t)(((c&31u)*level/255)|((((c>>6)&31u)*level/255)<<6)|((((c>>11)&31u)*level/255)<<11));
    }
    for(int y=0;y<64;y++){
        if((unsigned)(top+y)>=240)continue;
        uint16_t *row=r_page+(top+y)*320;
        const unsigned char *src=wordmark_pixels+y*160;
        for(int x=0;x<320;x+=2){
            const unsigned p=src[x>>1];
            const unsigned hi=p>>4,lo=p&15;
            if(hi)row[x]=pal[hi];
            if(lo)row[x+1]=pal[lo];
        }
    }
}
void r_end_inscription(int top)
{
    for(int y=0;y<32;y++)for(int x=0;x<320;x++){
        int i=y*320+x;unsigned v=end_inscription_pixels[i/2],idx=(i&1)?v&15:v>>4;
        if(idx && (unsigned)(top+y)<240)r_page[(top+y)*320+x]=end_inscription_palette[idx];
    }
}
void r_finish(void){}
void demo_stats(demo_stats_t *out){if(out)*out=r_stats;}
void demo_render(uint16_t *page,uint32_t sample)
{
    if(sample>=CV_TOTAL_SAMPLES)sample=CV_TOTAL_SAMPLES-1;
    int chapter=song_section(cv_bar_of(sample));
    r_begin(page,chapter,scene_camera(sample,chapter));r_background(sample);
    const uint32_t t_sc=prof_begin();
    if(chapter==2)scene_hand(sample);else scene_body(sample,chapter);
    prof_end(RP_SCENE,t_sc);
    unsigned energy=(unsigned)song_energy(cv_bar_of(sample));
    unsigned count=64+energy/5;
    if(chapter==5)count=112+energy/16;
    if(chapter==9)count=64;
    r_embers(sample,count);r_bloom();r_transition(sample);
    const uint32_t t_ti=prof_begin();
    scene_titles(sample,chapter);
    prof_end(RP_TITLES,t_ti);
    const uint32_t t_fa=prof_begin();
    if(sample>CV_TOTAL_SAMPLES-CV_BAR)page_scale(page,(CV_TOTAL_SAMPLES-1-sample)*255/CV_BAR);
    if(sample<CV_BAR)page_scale(page,sample*255/CV_BAR);
    prof_end(RP_FADE,t_fa);
    r_finish();
}
