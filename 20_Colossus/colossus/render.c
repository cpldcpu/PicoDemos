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
static unsigned r_env,r_dawn;
static int clip_x0,clip_y0,clip_x1,clip_y1;
#define r_chrome_palette r_shades[2]
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
    r_env=0;r_dawn=0;r_portal_reset();
    glow_x0=80;glow_y0=60;glow_x1=glow_y1=-1;
}
static uint16_t mix_color(uint16_t a,uint16_t b,unsigned t)
{
    unsigned u=255-t;
    return cv_rgb(((a&31)*u+(b&31)*t)*8/255,(((a>>6)&31)*u+((b>>6)&31)*t)*8/255,(((a>>11)&31)*u+((b>>11)&31)*t)*8/255);
}
void r_environment(unsigned mode){r_env=mode;}
static float contact(float wx,float wz,float x,float z,float rx,float rz)
{
    float dx=(wx-x)/rx,dz=(wz-z)/rz;
    return fmaxf(0,1-dx*dx-dz*dz);
}
void r_background(uint32_t sample)
{
    float dawn=fminf(1,fmaxf(0,((float)sample/CV_BAR-128)/7));
    r_dawn=(unsigned)(255*dawn);
    /* Both maps use the dusk normal-index topology; dawn is a palette morph. */
    for(int i=0;i<256;i++)r_chrome_palette[i]=mix_color(dusk_matcap_palette[i],dawn_matcap_palette[i],r_dawn);
    unsigned chapter=r_stats.chapter;
    for(int y=0;y<CV_H;y++){
        int ty=clamp(y*64/((chapter==1||chapter==8||chapter==9)?120:240),0,63);
        for(int x=0;x<CV_W;x++){
            int idx=dusk_sky_pixels[ty*256+x*256/320];
            uint16_t c=mix_color(dusk_sky_palette[idx],dawn_sky_palette[idx],r_dawn);
            /* Inside chambers retain a dark environmental field, no false floor. */
            if(chapter==3||chapter==5 || (chapter==6 && sample<98*CV_BAR))
                c=mix_color(c,cv_rgb(16,24,32),210);
            if(chapter==0)c=mix_color(c,0,255-(unsigned)(75*fminf(1,(float)sample/(CV_BAR*7))));
            if(chapter==4){
                float z=-1.05f-r_camera.cz,rad=z>.05f?.49f*r_camera.focal/z:1000;
                if((x-160.f)*(x-160.f)+(y-120.f)*(y-120.f)<rad*rad)c=mix_color(c,cv_rgb(8,16,24),235);
            }
            r_page[y*CV_W+x]=c;
        }
    }
    if(chapter!=1 && chapter!=8 && chapter!=9)return;
    /* Level world plane: per-row reciprocal projection; x stepping is exact.
       Shallow fog converges to the same painted sky row above the horizon. */
    int horizon=120;
    float c=cosf(r_camera.yaw),sn=sinf(r_camera.yaw);
    for(int y=horizon+1;y<CV_H;y++){
        float z=r_camera.cy*r_camera.focal/(y-120.f),v=z*4;
        unsigned haze=(unsigned)(255*fmaxf(0,1-(y-horizon)/36.f));
        for(int x=0;x<CV_W;x+=8){
            float xx=(x-160)*z/r_camera.focal,dx=z/r_camera.focal;
            for(int j=0;j<8;j++,xx+=dx){
                float wx=r_camera.cx+xx*c-z*sn,wz=r_camera.cz+xx*sn+z*c;
                int tex=stone_pixels[((int)v&63)*64+((int)(wx*4)&63)];
                uint16_t col=stone_palette[tex];
                float shadow=.32f*contact(wx,wz,0,0,5,2.5f);
                shadow=fmaxf(shadow,.86f*contact(wx,wz,-1.6f,-.85f,1.75f,1.8f));
                shadow=fmaxf(shadow,.86f*contact(wx,wz,1.6f,.3f,1.65f,1.55f));
                col=mix_color(col,cv_rgb(8,16,24),(unsigned)(255*shadow));
                int idx=dusk_sky_pixels[63*256+(x+j)*256/320];
                uint16_t fog=mix_color(dusk_sky_palette[idx],dawn_sky_palette[idx],r_dawn);
                r_page[y*320+x+j]=mix_color(col,fog,haze);
            }
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
void r_portal_reset(void){clip_x0=clip_y0=0;clip_x1=320;clip_y1=240;}
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
#define SPAN_LOOP(COLOR) \
    for(int i=0;i<count;i++,off++,s.q+=s.dq,s.l+=s.dl,s.u+=s.du,s.v+=s.dv,s.e+=s.de){ \
        int q=clamp(s.q>>16,1,255); \
        if(q> ((uint8_t*)r_depth)[off]){ \
            ((uint8_t*)r_depth)[off]=(uint8_t)q;r_page[off]=(COLOR); \
            if(s.e>0){int e=clamp(s.e>>16,0,255);if(r_env!=3)r_page[off]=cv_rgb(130+e/2,50+e/3,24+e/8);glow_seed(off,e);} \
        } \
    }
static void CV_HOT(span_flat)(int off,int count,Span s){uint16_t color=r_shades[0][clamp(s.l>>16,0,255)];SPAN_LOOP(color)}
static void CV_HOT(span_gouraud)(int off,int count,Span s){SPAN_LOOP(r_shades[1][clamp(s.l>>16,0,255)])}
static void CV_HOT(span_chrome)(int off,int count,Span s){SPAN_LOOP(r_env==2?warm_environment_palette[warm_environment_pixels[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]]:r_chrome_palette[dusk_matcap_pixels[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]])}
static void CV_HOT(span_furnace)(int off,int count,Span s){SPAN_LOOP(furnace_palette[furnace_pixels[((s.v>>16)&31)*64+((s.u>>16)&63)]])}
static void CV_HOT(span_texture)(int off,int count,Span s){SPAN_LOOP(r_shades[1][(bronze_wear_pixels[((s.v>>16)&63)*64+((s.u>>16)&63)]*(100+clamp(s.l>>16,0,255)*155/255))>>8])}
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
        switch(mat){case R_FLAT:span_flat(y*320+xs,xe-xs,s);break;case R_GOURAUD:span_gouraud(y*320+xs,xe-xs,s);break;case R_CHROME:span_chrome(y*320+xs,xe-xs,s);break;case R_TEXTURE:span_texture(y*320+xs,xe-xs,s);break;case R_FURNACE:span_furnace(y*320+xs,xe-xs,s);break;}
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
}
/* A common foreground rib pair at each substitution, under local lit dust.
   Stateless screen-space occluder; only one chapter is ever rendered. */
void r_transition(uint32_t sample)
{
    static const unsigned boundaries[]={8,24,40,56,72,88,112,128,144};
    for(unsigned i=0;i<sizeof boundaries/sizeof boundaries[0];i++){
        float dt=((float)sample-boundaries[i]*CV_BAR)/CV_RATE;
        if(fabsf(dt)>1.5f)continue;
        float t=1-fabsf(dt)/1.5f;t=t*t*(3-2*t);
        if(i==8)t*=.35f; /* The coda holds the exact reveal camera/world. */
        float z=r_camera.near_z*1.1f;
        for(int rib=0;rib<2;rib++){
            float x= rib?232:64,w=18*t;
            RVertex a={(x-160)*z/r_camera.focal,120*z/r_camera.focal,z,130,0,0,0};
            RVertex b=a,c=a,d=a;b.x+=(w*z/r_camera.focal);c.x=b.x;c.y=-120*z/r_camera.focal;d.y=c.y;
            a.u=d.u=0;b.u=c.u=63;a.v=b.v=0;c.v=d.v=63;
            r_triangle(a,b,c,R_TEXTURE);r_triangle(a,c,d,R_TEXTURE);
        }
        for(int y=0;y<240;y++)for(int x=0;x<320;x++){
            float band=fmaxf(0,1-fabsf(x-150.f-(y-120)*.35f)/195.f);
            unsigned noise=hash((unsigned)(x/3)+(unsigned)(y/3)*107)&31;
            unsigned opacity=(unsigned)(t*band*(205+noise));
            r_page[y*320+x]=mix_color(r_page[y*320+x],cv_rgb(120+noise,112+noise/2,96),opacity);
        }
        break;
    }
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
    for(int y=0;y<64;y++)for(int x=0;x<320;x++){
        int i=y*320+x;unsigned p=wordmark_pixels[i/2],index=(i&1)?p&15:p>>4;
        if(!index || (unsigned)(top+y)>=240)continue;
        uint16_t c=wordmark_palette[index];
        r_page[(top+y)*320+x]=cv_rgb((c&31)*8*level/255,((c>>6)&31)*8*level/255,((c>>11)&31)*8*level/255);
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
    if(chapter==2)scene_hand(sample);else scene_body(sample,chapter);
    unsigned energy=(unsigned)song_energy(cv_bar_of(sample));
    unsigned count=64+energy/5;
    if(chapter==5)count=112+energy/16;
    if(chapter==9)count=64;
    r_embers(sample,count);r_bloom();r_transition(sample);scene_titles(sample,chapter);
    if(sample>CV_TOTAL_SAMPLES-CV_BAR){unsigned remain=CV_TOTAL_SAMPLES-1-sample;for(int i=0;i<320*240;i++){uint16_t p=page[i];page[i]=cv_rgb((p&31)*8*remain/CV_BAR,((p>>6)&31)*8*remain/CV_BAR,((p>>11)&31)*8*remain/CV_BAR);}}
    if(sample<CV_BAR){unsigned level=sample*255/CV_BAR;for(int i=0;i<320*240;i++){uint16_t p=page[i];page[i]=cv_rgb((p&31)*8*level/255,((p>>6)&31)*8*level/255,((p>>11)&31)*8*level/255);}}
    r_finish();
}
