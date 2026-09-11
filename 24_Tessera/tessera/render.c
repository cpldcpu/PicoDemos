/* TESSERA / Phase. A material, not a collection of unrelated effects.
 * 384 individually lit ceramic tesserae morph through five surfaces.
 * Core-0 SIO interpolates indexed glaze; DMA clears the page while the M33
 * FPU prepares geometry. Stable bucket painter, planar projected shadows.
 * All hot texels, palettes, trigonometry and geometry live in SRAM. */
#include "tessera.h"
#include "song.h"
#include "accelerator.h"
#include "font8x8.h"
#include "trig_table.h"
#include "assets.h"
#include <math.h>
#include <string.h>
#define PI 3.14159265359f
#define NX 24
#define NY 16
#define NT (NX*NY)
typedef struct {float x,y,z;} V3;
typedef struct {float x,y,u,v;} Point;
typedef struct {Point p[4],shadow[4];float depth;uint16_t color;uint8_t material,light;int16_t next;} Tile;
static Tile tiles[NT];
static int16_t buckets[256],tails[256];
static uint8_t glaze[64*64];
static uint16_t palette[4][16][16];
static uint32_t shadow_mask[HEIGHT][WIDTH/32];
static float sine[2048];
static uint16_t *fb;
static demo_stats_t stats;
static unsigned triangles;
static float seconds,bar,ca,sa,ce,se,dist,zoom;
static int phase_a,phase_b;
static float morph,assembly,endfold,pulse,burst;
#ifdef PICO_BUILD
#include "pico/time.h"
static uint32_t stamp(void){return time_us_32();}
#else
static uint32_t stamp(void){return 0;}
#endif
static inline float sn(float a){return sine[(int)(a*(2048.f/(2*PI)))&2047];}
static inline float cs(float a){return sn(a+PI/2);}
static inline float sat(float x){return x<0?0:x>1?1:x;}
static inline float smooth(float x){x=sat(x);return x*x*(3-2*x);}
static inline V3 add(V3 a,V3 b){return (V3){a.x+b.x,a.y+b.y,a.z+b.z};}
static inline V3 sub(V3 a,V3 b){return (V3){a.x-b.x,a.y-b.y,a.z-b.z};}
static inline V3 scale(V3 a,float s){return (V3){a.x*s,a.y*s,a.z*s};}
static inline V3 cross(V3 a,V3 b){return (V3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
static inline V3 blend(V3 a,V3 b,float m){return add(a,scale(sub(b,a),m));}
static uint32_t hash32(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
static inline uint16_t mix(uint16_t a,uint16_t b,unsigned f){unsigned rb=((a&0xf81fu)*(32-f)+(b&0xf81fu)*f)>>5;unsigned g=((a&0x7c0u)*(32-f)+(b&0x7c0u)*f)>>5;return (uint16_t)((rb&0xf81fu)|(g&0x7c0u));}
static V3 HOT(surface)(int form,float u,float v){
 float a=u*PI,r;
 switch(form){
 case 0:return (V3){u*4.2f,0.4f*sn(u*5+seconds)+0.32f*cs(v*6-seconds*.8f),v*3};
 case 1:r=2.65f*cs(v*PI*.47f);return (V3){r*sn(a),2.65f*sn(v*PI*.47f),r*cs(a)};
 case 2:{float twist=u*2.3f+seconds*.38f;return (V3){u*4.3f,1.3f*sn(u*3+seconds*.7f)+v*1.8f*cs(twist),v*1.8f*sn(twist)};}
 case 3:r=1.4f+.55f*cs(v*3+seconds*.7f);a+=v*2.6f+seconds*.25f;return (V3){r*sn(a),v*3.25f,r*cs(a)};
 default:r=.3f+(v+1)*1.7f;return (V3){r*sn(a),.7f*sn((v+1)*3-seconds)+.95f*cs(a*5+seconds*.35f)*(v+1)*.5f,r*cs(a)};
 }
}
static inline V3 current_surface(float u,float v){
 if(morph>=1)return surface(phase_b,u,v);
 if(morph<=0||phase_a==phase_b)return surface(phase_a,u,v);
 return blend(surface(phase_a,u,v),surface(phase_b,u,v),morph);
}
static V3 HOT(world_point)(float u,float v,unsigned id,float cu,float cv,V3 center){
 V3 p=current_surface(u,v);
 /* A breathing seam opens on the last phrase of a chapter. Tile centres
  * move apart while their size stays physical; the whole surface breaks
  * into pieces before it finds its next form. */
 p=add(p,scale(center,burst));
 uint32_t h=hash32(id+31);float delay=(h&255)/255.f;
 float fly=1-smooth(assembly-delay*.8f);
 if(fly>0){p.x+=fly*((int)((h>>8)&255)-128)*.055f;p.y+=fly*(3+delay*4);p.z+=fly*((int)((h>>16)&255)-128)*.035f;}
 if(endfold>0){
   V3 one={(u-cu)*4.5f,0,(v-cv)*4.5f};
   float individual=smooth(endfold*1.5f-delay*.45f);
   p=blend(p,one,individual);
 }
 return p;
}
static Point HOT(project)(V3 p,float *depth){float x=p.x*ca+p.z*sa,z=p.z*ca-p.x*sa,y=p.y*ce-z*se;z=p.y*se+z*ce+dist;if(z<1.5f)z=1.5f;*depth=z;return (Point){160+x*zoom/z,119-y*zoom/z,0,0};}
/* Scanline triangle: affine derivatives once, intersections once per row,
 * two interpolator accumulators and one palette lookup per covered pixel. */
static void HOT(triangle)(Point a,Point b,Point c,const uint16_t *pal,uint16_t flat){
 if(a.y>b.y){Point t=a;a=b;b=t;}if(b.y>c.y){Point t=b;b=c;c=t;}if(a.y>b.y){Point t=a;a=b;b=t;}
 (void)flat;
 float det=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);if(fabsf(det)<.1f)return;
 float du=0,dv=0,uy=0,vy=0;
 if(pal){float inv=1/det;du=((b.u-a.u)*(c.y-a.y)-(c.u-a.u)*(b.y-a.y))*inv;
   dv=((b.v-a.v)*(c.y-a.y)-(c.v-a.v)*(b.y-a.y))*inv;
   uy=((c.u-a.u)*(b.x-a.x)-(b.u-a.u)*(c.x-a.x))*inv;
   vy=((c.v-a.v)*(b.x-a.x)-(b.v-a.v)*(c.x-a.x))*inv;}
 int32_t iu=(int32_t)(du*65536),iv=(int32_t)(dv*65536);
 float long_slope=(c.x-a.x)/(c.y-a.y);
 /* Sorted edge walkers replace three edge tests and two divisions on
  * EVERY scanline. Shadow spans do no UV arithmetic at all. */
 for(int half=0;half<2;half++){
   Point q=half?b:a,r=half?c:b;if(r.y-q.y<.0001f)continue;
   int y0=clampi((int)ceilf(q.y-.5f),0,HEIGHT),y1=clampi((int)ceilf(r.y-.5f),0,HEIGHT);
   float slope=(r.x-q.x)/(r.y-q.y);
   float xa=a.x+(y0+.5f-a.y)*long_slope,xb=q.x+(y0+.5f-q.y)*slope;
   for(int y=y0;y<y1;y++,xa+=long_slope,xb+=slope){
     int x0=clampi((int)ceilf(fminf(xa,xb)-.5f),0,WIDTH),x1=clampi((int)ceilf(fmaxf(xa,xb)-.5f),0,WIDTH);
     if(x0>=x1)continue;uint16_t *dst=fb+y*WIDTH+x0;stats.spans++;
     if(pal){
       texture_span((int32_t)((a.u+du*(x0+.5f-a.x)+uy*(y+.5f-a.y))*65536),(int32_t)((a.v+dv*(x0+.5f-a.x)+vy*(y+.5f-a.y))*65536),iu,iv);
       for(int x=x0;x<x1;x++)*dst++=pal[glaze[texture_pop()]];
     }else{
       /* Union of all projected shadows. Blend the painted ground once,
        * instead of darkening overlaps repeatedly or erasing the artwork. */
       unsigned first=(unsigned)x0>>5,last=(unsigned)(x1-1)>>5;
       uint32_t left=0xffffffffu<<(x0&31),right=0xffffffffu>>(31-((x1-1)&31));
       if(first==last)shadow_mask[y][first]|=left&right;
       else{shadow_mask[y][first]|=left;shadow_mask[y][last]|=right;for(unsigned w=first+1;w<last;w++)shadow_mask[y][w]=0xffffffffu;}
     }
   }
 }
 triangles++;
}
static void rect(int x,int y,int w,int h,uint16_t color){for(int j=clampi(y,0,HEIGHT);j<clampi(y+h,0,HEIGHT);j++)for(int i=clampi(x,0,WIDTH);i<clampi(x+w,0,WIDTH);i++)fb[j*WIDTH+i]=color;}
static void text(int x,int y,const char *s,int scale,uint16_t color){for(;*s;s++,x+=8*scale){const uint8_t *g=font8x8_glyph(*s);for(int j=0;j<8;j++)for(int i=0;i<8;i++)if(g[j]&(128>>i))rect(x+i*scale,y+j*scale,scale,scale,color);}}
typedef struct {const char *label;int scale,gap_after;} CardLine;
typedef struct {int x,y,w,h;} TextBounds;
static TextBounds text_bounds(const char *s,int scale){
 int left=10000,top=8,right=-1,bottom=-1;
 for(int advance=0;*s;s++,advance+=8){
   const uint8_t *g=font8x8_glyph(*s);
   for(int y=0;y<8;y++)for(int x=0;x<8;x++)if(g[y]&(128>>x)){
     if(advance+x<left)left=advance+x;if(advance+x>right)right=advance+x;
     if(y<top)top=y;if(y>bottom)bottom=y;
   }
 }
 if(right<0)return (TextBounds){0,0,0,0};
 return (TextBounds){left*scale,top*scale,(right-left+1)*scale,(bottom-top+1)*scale};
}
/* Centre the visible glyphs, not the font's trailing blank cells. The same
 * measured bounds size the card and place every line inside its padding. */
static void text_card(int center,int top,const CardLine *lines,int count,uint16_t paper,uint16_t ink){
 TextBounds bounds[4];int width=0,height=0;const int pad_x=12,pad_y=8;
 for(int i=0;i<count;i++){
   bounds[i]=text_bounds(lines[i].label,lines[i].scale);
   if(bounds[i].w>width)width=bounds[i].w;
   height+=bounds[i].h+(i+1<count?lines[i].gap_after:0);
 }
 rect(center-width/2-pad_x,top,width+pad_x*2,height+pad_y*2,paper);
 int y=top+pad_y;
 for(int i=0;i<count;i++){
   text(center-bounds[i].w/2-bounds[i].x,y-bounds[i].y,lines[i].label,lines[i].scale,ink);
   y+=bounds[i].h+lines[i].gap_after;
 }
}
static void HOT(clear_page)(int night){
 background_begin(fb,night?stage_night:stage_day,WIDTH*HEIGHT);
}
void demo_init(void){
 for(int i=0;i<2048;i++)sine[i]=trig_q15[i]*(1.f/32768);
 accelerator_init();
 if(accelerator_selftest()){
#ifdef PICO_BUILD
 panic("TESSERA interpolator selftest");
#else
 __builtin_trap();
#endif
 }
 for(int y=0;y<64;y++)for(int x=0;x<64;x++){
   int rim=x<3||y<3?15:x>60||y>60?3:11;
   int speck=(hash32(y*64+x)&31)==0?-1:0;
   glaze[y*64+x]=(uint8_t)clampi(rim+speck+(x>6&&x<15&&y>7&&y<54?1:0),0,15);
 }
 const int base[4][3]={{224,221,200},{32,78,185},{220,64,32},{237,176,63}};
 for(int m=0;m<4;m++)for(int l=0;l<16;l++)for(int g=0;g<16;g++){
   int strength=80+l*9+(g-10)*8;int r=base[m][0]*strength/210,gr=base[m][1]*strength/210,b=base[m][2]*strength/210;
   if(g>=14){r=(int)palette_lerp(r,255,92);gr=(int)palette_lerp(gr,250,92);b=(int)palette_lerp(b,229,92);}
   palette[m][l][g]=rgb(r,gr,b);
 }
}
unsigned demo_triangles(void){return triangles;}
void demo_stats(demo_stats_t *out){*out=stats;}
void HOT(demo_render)(uint16_t *page,uint32_t sample){
 fb=page;triangles=0;memset(&stats,0,sizeof stats);
 if(sample>=DURATION_SAMPLES){memset(page,0,WIDTH*HEIGHT*2);return;}
 uint32_t start=stamp();seconds=(float)sample/SAMPLE_RATE;bar=(float)sample/BAR_SAMPLES;
 int section=(int)bar/16;stats.section=(uint8_t)section;
 float local=bar-section*16;
 /* Material transformations occupy two bars; the physical tessera stays. */
 int forms[6]={0,1,2,3,4,1};phase_b=forms[section];phase_a=section?forms[section-1]:0;morph=smooth(local/2);
 assembly=bar*.42f;endfold=smooth((bar-88)/5.5f);
 burst=section<5?smooth((local-12)/2)*(1-smooth((local-15)/1))*.35f:0;
 unsigned accent=sample-song_accent(sample);pulse=1-sat((float)accent/4200);
 float angle=seconds*.16f+.28f*sn(seconds*.09f);if(section==0)angle=.28f+seconds*.065f;
 float elevation=section==2?.46f:section==3?.2f:section==4?.8f:.38f;
 elevation+=.12f*sn(seconds*.12f);ca=cs(angle);sa=sn(angle);ce=cs(elevation);se=sn(elevation);
 dist=section==3?10.2f:10.7f;zoom=330+18*sn(seconds*.18f);
 /* Gather first, then dolly into the one remaining tile. Enlarging all 384
  * nearly coincident tiles made the last dissolve the worst overdraw case. */
 zoom+=smooth((bar-93.25f)/1.25f)*850;
 uint16_t paper=section==3?rgb(26,38,53):rgb(231,215,183),ink=section==3?rgb(227,221,201):rgb(33,46,60);
 clear_page(section==3);
 memset(buckets,0xff,sizeof buckets);memset(tails,0xff,sizeof tails);
 texture_config(6);
 for(int j=0;j<NY;j++)for(int i=0;i<NX;i++){
   unsigned id=j*NX+i;Tile *tile=&tiles[id];float cu=(i+.5f)*2/NX-1,cv=(j+.5f)*2/NY-1;
   float gap=.89f;V3 p[4];float depth=0;V3 center=current_surface(cu,cv);
   for(int k=0;k<4;k++){
     float u=cu+((k==0||k==3)?-gap:gap)/NX,v=cv+(k<2?-gap:gap)/NY;
     p[k]=world_point(u,v,id,cu,cv,center);float z;
     tile->p[k]=project(p[k],&z);depth+=z;
     tile->p[k].u=(k==0||k==3)?0:63.5f;tile->p[k].v=k<2?0:63.5f;
     V3 shadow=p[k];float height=fmaxf(0,shadow.y+3.35f);shadow.x+=height*.55f;shadow.z+=height*.3f;shadow.y=-3.35f;
     tile->shadow[k]=project(shadow,&z);
   }
   V3 n=cross(sub(p[1],p[0]),sub(p[3],p[0]));float len=sqrtf(n.x*n.x+n.y*n.y+n.z*n.z)+.00001f;
   float light=fabsf((n.x*-.35f+n.y*.82f+n.z*-.45f)/len);
   int mat=((i+j*2)%11<3)?1:((i*3+j)%17<3)?2:((i+j)%19==0)?3:0;
   unsigned lead=(sample/STEP_SAMPLES)%NX;float emphasis=(unsigned)i==lead?pulse*2:0;
   tile->material=(uint8_t)mat;tile->light=(uint8_t)clampi((int)(4+light*10+emphasis),0,15);tile->depth=depth*.25f;tile->next=-1;
   int bin=clampi((int)((tile->depth-3)*15),0,255);
   if(tails[bin]>=0)tiles[tails[bin]].next=(int16_t)id;else buckets[bin]=(int16_t)id;tails[bin]=(int16_t)id;
 }
 background_wait();stats.prepare=stamp()-start;uint32_t draw=stamp();
 memset(shadow_mask,0,sizeof shadow_mask);
 if(endfold<.95f)for(int i=0;i<NT;i++){Tile *p=&tiles[i];triangle(p->shadow[0],p->shadow[1],p->shadow[2],NULL,0);triangle(p->shadow[0],p->shadow[2],p->shadow[3],NULL,0);}
 for(int y=0;y<HEIGHT;y++)for(int w=0;w<WIDTH/32;w++){
   uint32_t mask=shadow_mask[y][w];while(mask){unsigned bit=(unsigned)__builtin_ctz(mask);unsigned x=w*32+bit;
     fb[y*WIDTH+x]=mix(fb[y*WIDTH+x],ink,8);mask&=mask-1;
   }
 }
 for(int b=255;b>=0;b--)for(int i=buckets[b];i>=0;i=tiles[i].next){
   Tile *p=&tiles[i];const uint16_t *pal=palette[p->material][p->light];
   if(endfold>.97f&&i!=NT/2)continue;
   triangle(p->p[0],p->p[1],p->p[2],pal,0);triangle(p->p[0],p->p[2],p->p[3],pal,0);stats.tiles++;
 }
 /* Quiet margins and editorial scale; no technical HUD in the film. */
 rect(0,0,WIDTH,23,paper);rect(0,216,WIDTH,24,paper);
 text(12,8,"LATENT",1,ink);text(244,8,"PHASE",1,ink);rect(12,21,296,1,ink);
 const char *names[6]={"01  GATHER","02  WORLD","03  SUSPEND","04  ASCEND","05  FLOURISH","06  RETURN"};
 text(12,226,names[section],1,ink);rect(234,229,74,2,mix(paper,ink,8));rect(234,229,(int)(74*bar/96),2,ink);
 if(bar<6){
   static const CardLine title[]={{"TESSERA",4,8},{"ONE SMALL THING",1,0}};
   text_card(WIDTH/2,88,title,2,paper,ink);
 }else if(bar>=92){
   static const CardLine credits[]={{"PHASE",2,6},{"CODE + MUSIC + DIRECTION",1,5},{"AZURE . CRITIC",1,5},{"LATENT 2026",1,0}};
   text_card(WIDTH/2,146,credits,4,paper,ink);
 }
 unsigned fade=sample<SAMPLE_RATE?(unsigned)((uint64_t)sample*32/SAMPLE_RATE):32;
 if(sample>DURATION_SAMPLES-SAMPLE_RATE*2)fade=(unsigned)((uint64_t)(DURATION_SAMPLES-sample)*32/(SAMPLE_RATE*2));
 if(fade<32)for(unsigned i=0;i<WIDTH*HEIGHT;i++)fb[i]=mix(0,fb[i],fade);
 stats.draw=stamp()-draw;
}
