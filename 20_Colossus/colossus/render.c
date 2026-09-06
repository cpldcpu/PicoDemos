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
static uint8_t r_depth[CV_H][CV_W],r_glow[60][80],r_blur[60][80];
static uint16_t r_shades[4][256];
static uint16_t *r_page;
static RCamera r_camera;
static demo_stats_t r_stats;
static int glow_x0,glow_y0,glow_x1,glow_y1;
static int clamp(int x,int a,int b){return x<a?a:x>b?b:x;}
static float lerp(float a,float b,float t){return a+(b-a)*t;}
void demo_init(void)
{
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
    memset(r_depth,0,sizeof r_depth);memset(r_glow,0,sizeof r_glow);
    memset(r_blur,0,sizeof r_blur);
    glow_x0=80;glow_y0=60;glow_x1=glow_y1=-1;
}
void r_background(uint32_t sample)
{
    float dawn=fminf(1,fmaxf(0,((float)sample/CV_BAR-128)/16));
    for(int y=0;y<CV_H;y++){
        int band=clamp(80-abs(y-158),0,80);
        int ty=clamp(y*64/184,0,63);
        for(int x=0;x<CV_W;x++){
            uint16_t c=dusk_sky_palette[dusk_sky_pixels[ty*256+x*256/320]];
            r_page[y*CV_W+x]=cv_rgb((c&31)*8+(int)(dawn*band),((c>>6)&31)*8+(int)(dawn*band*.65f),((c>>11)&31)*8);
        }
    }
    /* Dedicated floor: rational projection is corrected every 8 pixels.
       Constant row depth makes interpolation exact for this level plane. */
    for(int y=184;y<CV_H;y++){
        float z=240.f/(y-180),v=z*12;
        for(int x=0;x<CV_W;x+=8){
            float u=(x-160)*z*.12f,du=z*.12f;
            for(int j=0;j<8;j++,u+=du){int tex=diagnostic_tile[((int)v&63)*64+((int)u&63)];r_page[y*CV_W+x+j]=r_shades[3][tex/3];}
        }
    }
}
RVertex r_transform(float x,float y,float z,float nx,float ny,float nz,float u,float v)
{
    float c=cosf(r_camera.yaw),s=sinf(r_camera.yaw);
    x-=r_camera.cx;y-=r_camera.cy;z-=r_camera.cz;
    float xx=x*c+z*s,zz=-x*s+z*c;
    /* Lighting remains in world space. Matcap normals follow the camera. */
    float light=fmaxf(0,-nx*.4f+ny*.75f-nz*.45f);
    (void)u;(void)v;
    return (RVertex){xx,y,zz,35+200*light,31.5f+31*(nx*c+nz*s),31.5f-31*ny,0};
}
typedef struct {float x,y,q,l,u,v,e;} Screen;
static Screen project(RVertex a)
{
    float iz=1/a.z,n=r_camera.near_z,f=r_camera.far_z;
    return (Screen){160+a.x*r_camera.focal*iz,120-a.y*r_camera.focal*iz,1+254*(iz-1/f)/(1/n-1/f),a.l,a.u,a.v,a.e};
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
#define SPAN_LOOP(COLOR) \
    for(int i=0;i<count;i++,off++,s.q+=s.dq,s.l+=s.dl,s.u+=s.du,s.v+=s.dv,s.e+=s.de){ \
        int q=clamp(s.q>>16,1,255); \
        if(q> ((uint8_t*)r_depth)[off]){ \
            ((uint8_t*)r_depth)[off]=(uint8_t)q;r_page[off]=(COLOR); \
            if(s.e>0)glow_seed(off,s.e>>16); \
        } \
    }
static void CV_HOT(span_flat)(int off,int count,Span s){uint16_t color=r_shades[0][clamp(s.l>>16,0,255)];SPAN_LOOP(color)}
static void CV_HOT(span_gouraud)(int off,int count,Span s){SPAN_LOOP(r_shades[1][clamp(s.l>>16,0,255)])}
static void CV_HOT(span_chrome)(int off,int count,Span s){SPAN_LOOP(dusk_matcap_palette[dusk_matcap_pixels[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]])}
static void CV_HOT(span_texture)(int off,int count,Span s){SPAN_LOOP(r_shades[3][(diagnostic_tile[((s.v>>16)&63)*64+((s.u>>16)&63)]*clamp(s.l>>16,0,255))>>8])}
#undef SPAN_LOOP
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
    int ys=clamp((int)ceilf(a.y-.5f),0,240),ye=clamp((int)ceilf(c.y-.5f),0,240);
    float long_s=(c.x-a.x)/(c.y-a.y+1e-12f),upper=(b.x-a.x)/(b.y-a.y+1e-12f),lower=(c.x-b.x)/(c.y-b.y+1e-12f);
    for(int y=ys;y<ye;y++){
        float fy=y+.5f,x0=a.x+(fy-a.y)*long_s,x1=fy<b.y?a.x+(fy-a.y)*upper:b.x+(fy-b.y)*lower;
        if(x0>x1){float t=x0;x0=x1;x1=t;}
        int xs=clamp((int)ceilf(x0-.5f),0,320),xe=clamp((int)ceilf(x1-.5f),0,320);
        if(xs>=xe)continue;
        int v[5];for(int i=0;i<5;i++)v[i]=(int)((av[i]+dx[i]*(xs+.5f-a.x)+dy[i]*(fy-a.y))*65536);
        Span s={v[0],v[1],v[2],v[3],v[4],(int)(dx[0]*65536),(int)(dx[1]*65536),(int)(dx[2]*65536),(int)(dx[3]*65536),(int)(dx[4]*65536)};
        r_stats.fill+=(uint32_t)(xe-xs);
        switch(mat){case R_FLAT:span_flat(y*320+xs,xe-xs,s);break;case R_GOURAUD:span_gouraud(y*320+xs,xe-xs,s);break;case R_CHROME:span_chrome(y*320+xs,xe-xs,s);break;case R_TEXTURE:span_texture(y*320+xs,xe-xs,s);break;}
    }
}
static RVertex interpolate(RVertex a,RVertex b,float t)
{
    return (RVertex){lerp(a.x,b.x,t),lerp(a.y,b.y,t),lerp(a.z,b.z,t),lerp(a.l,b.l,t),lerp(a.u,b.u,t),lerp(a.v,b.v,t),lerp(a.e,b.e,t)};
}
void r_triangle(RVertex a,RVertex b,RVertex c,RMaterial mat)
{
    RVertex p[8]={a,b,c},out[8];int n=3;r_stats.triangles++;
    /* Clip near AND far; interpolate all attributes at both planes. */
    for(int plane=0;plane<2;plane++){
        float edge=plane?r_camera.far_z:r_camera.near_z;int m=0;
        for(int i=0;i<n;i++){
            RVertex v=p[i],w=p[(i+1)%n];int iv=plane?v.z<=edge:v.z>=edge,iw=plane?w.z<=edge:w.z>=edge;
            if(iv)out[m++]=v;
            if(iv!=iw)out[m++]=interpolate(v,w,(edge-v.z)/(w.z-v.z));
        }
        memcpy(p,out,(size_t)m*sizeof *p);n=m;if(n<3)return;
    }
    for(int i=1;i<n-1;i++)raster(project(p[0]),project(p[i]),project(p[i+1]),mat);
}
void CV_HOT(r_bloom)(void)
{
    if(glow_x1<glow_x0)return;
    int x0=clamp(glow_x0-2,0,79),x1=clamp(glow_x1+2,0,79),y0=clamp(glow_y0-2,0,59),y1=clamp(glow_y1+2,0,59);
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)r_blur[y][x]=(uint8_t)((r_glow[y][clamp(x-1,0,79)]+2*r_glow[y][x]+r_glow[y][clamp(x+1,0,79)])/4);
    for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)r_glow[y][x]=(uint8_t)((r_blur[clamp(y-1,0,59)][x]+2*r_blur[y][x]+r_blur[clamp(y+1,0,59)][x])/4);
    /* Composite only occupied 4x4 rectangles; dust may spread the blur bounds
       across the page, but empty cells never cause a full-resolution walk. */
    for(int gy=y0;gy<=y1;gy++)for(int gx=x0;gx<=x1;gx++){
        int v=r_glow[gy][gx];if(!v)continue;
        for(int y=gy*4;y<gy*4+4;y++)for(int x=gx*4;x<gx*4+4;x++){
            uint16_t p=r_page[y*320+x];r_page[y*320+x]=cv_rgb((p&31)*8+v/2,((p>>6)&31)*8+v/4,((p>>11)&31)*8+v/12);
        }
    }
}
static uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
void r_embers(uint32_t sample,unsigned count)
{
    if(count>256)count=256;
    r_stats.particles=count;
    for(unsigned i=0;i<count;i++){
        uint32_t life=CV_RATE*(3+i%5),t=sample+(hash(i)&65535),epoch=t/life,age=t%life,h=hash(i+epoch*1021);
        float f=(float)age/life;
        int x=(int)(h%320)+(int)(sinf(f*5+i)*9),y=239-(int)(f*255),q=16+(int)((h>>16)%220);
        int radius=(i%11==0)?1:0;
        for(int yy=y-radius;yy<=y+radius;yy++)for(int xx=x-radius;xx<=x+radius;xx++){
            if((unsigned)xx>=320||(unsigned)yy>=240||q<=r_depth[yy][xx])continue;
            int e=(int)(160*sinf(f*3.14159265f));if(xx!=x||yy!=y)e/=3;
            r_page[yy*320+xx]=cv_rgb(e+40,e/2+24,e/5+16);glow_seed(yy*320+xx,e/2);
        }
    }
}
void r_inscription(const char *text,int x,int y,uint16_t color)
{
    for(;*text;text++,x+=8){unsigned ch=(unsigned char)*text;
        if(ch<32||ch>95)continue;
        int n=(int)ch-32,ox=(n%16)*8,oy=(n/16)*12;
        for(int yy=0;yy<12;yy++)for(int xx=0;xx<8;xx++){
            int bit=(oy+yy)*128+ox+xx;
            if((unsigned)(x+xx)<320&&(unsigned)(y+yy)<240&&(inscription_atlas[bit>>3]&(128>>(bit&7))))r_page[(y+yy)*320+x+xx]=color;
        }
    }
}
void r_wordmark(int top,unsigned level)
{
    if(level>255)level=255;
    for(int y=0;y<64;y++)for(int x=0;x<320;x++){
        int i=y*320+x;unsigned p=wordmark_pixels[i/2],index=(i&1)?p&15:p>>4;
        if(!index || (unsigned)(top+y)>=240)continue;
        uint16_t c=wordmark_palette[index];
        r_page[(top+y)*320+x]=cv_rgb((c&31)*8*level/255,((c>>6)&31)*8*level/255,((c>>11)&31)*8*level/255);
    }
}
void r_finish(void){}
void demo_stats(demo_stats_t *out){if(out)*out=r_stats;}
void demo_render(uint16_t *page,uint32_t sample)
{
    if(sample>=CV_TOTAL_SAMPLES)sample=CV_TOTAL_SAMPLES-1;
    int chapter=song_section(cv_bar_of(sample));
    if(chapter==2){
        float t=((float)sample/CV_BAR-24)/16;
        RCamera camera={3,16,-4.2f,4.9f+1.8f*t,-8,-.10f+.07f*sinf(t*3.14159265f),190};
        r_begin(page,chapter,camera);r_background(sample);scene_hand(sample);
    }else{
        static const float depth_range[10][2]={{8,40},{12,40},{3,16},{8,34},{2,12},{6,34},{6,34},{2,16},{8,40},{8,40}};
        RCamera camera={depth_range[chapter][0],depth_range[chapter][1],0,10,-24,.08f,190};
        if(chapter==4){camera=(RCamera){2,12,0,17.4f,-6,0,190};}
        if(chapter==7){camera=(RCamera){2,16,0,18.5f,-7,.1f,190};}
        r_begin(page,chapter,camera);r_background(sample);scene_body(sample,chapter);
    }
    r_embers(sample,64+(unsigned)song_energy(cv_bar_of(sample))/4);r_bloom();
    if(chapter==2 && sample/CV_BAR<28)r_inscription("I . HAND",18,213,cv_rgb(184,168,144));
    if(sample>CV_TOTAL_SAMPLES-CV_BAR){unsigned remain=CV_TOTAL_SAMPLES-1-sample;for(int i=0;i<320*240;i++){uint16_t p=page[i];page[i]=cv_rgb((p&31)*8*remain/CV_BAR,((p>>6)&31)*8*remain/CV_BAR,((p>>11)&31)*8*remain/CV_BAR);}}
    if(sample<CV_BAR){unsigned level=sample*255/CV_BAR;for(int i=0;i<320*240;i++){uint16_t p=page[i];page[i]=cv_rgb((p&31)*8*level/255,((p>>6)&31)*8*level/255,((p>>11)&31)*8*level/255);}}
    r_finish();
}
