/* Darkroom, Stellar (1994). Native reconstruction by Phase.
 * Addresses in comments refer to the unpacked original code hunk.
 * The feedback is evaluated live, using Dweezil's bitplane operations. */
#include "darkroom.h"
#include "assets.h"
#include "timing.h"
#include <string.h>
#include <assert.h>

static uint8_t chip[0x40000] __attribute__((aligned(4)));
static int16_t maps[3][99],sine[1280];
static uint8_t reciprocal[1024];
static unsigned tick,stage,feedback_tick,shown,warp_tick;
static uint32_t random_state,angle,velocity;
static int next_tick;
static demo_stats_t stats;
uint16_t demo_palette[256];
static inline uint16_t rd(unsigned a){assert(a+1<sizeof chip);return (chip[a]<<8)|chip[a+1];}
static inline void wr(unsigned a,uint16_t v){assert(a+1<sizeof chip);chip[a]=v>>8;chip[a+1]=v;}
static inline uint32_t ror(uint32_t v,unsigned n){n&=31;return n?(v>>n)|(v<<(32-n)):v;}
static inline uint16_t ror16(uint16_t v,unsigned n){n&=15;return n?(v>>n)|(v<<(16-n)):v;}
static inline uint16_t minterm(unsigned f,uint16_t a,uint16_t b,uint16_t c){
 switch(f){
 case 0x80:return a&b&c;
 case 0x01:return ~(a|b|c);
 case 0xe6:return (a&c)|(b&~c)|(c&~b);
 case 0x9a:return (b&c)|(c&~a)|(a&~b&~c);
 case 0x89:return (b&c)|(~a&~b&~c);
 case 0xa6:return (a&c)|(c&~b)|(b&~a&~c);
 case 0xf0:return a;
 default:{uint16_t d=0;for(int i=0;i<8;i++)if(f&(1<<i))d|=(i&4?a:~a)&(i&2?b:~b)&(i&1?c:~c);return d;}
 }
}
/* Ascending area blitter, with the original carry across word boundaries.
 * Only the enabled channels are fetched. All uses here have full word masks. */
static void HOT(blit)(unsigned f,int a,int b,int c,int d,int am,int bm,int cm,int dm,unsigned shift,int width,int height){
 a&=~1;b&=~1;c&=~1;d&=~1;
 uint16_t prev=0;
 for(int y=0;y<height;y++){
  for(int x=0;x<width;x++){
   uint16_t av=rd(a),bv=0,cv=0; a+=2;
   uint16_t aa=shift?(uint16_t)(((uint32_t)prev<<16|av)>>shift):av;prev=av;
   if(f!=0xf0){bv=rd(b);cv=rd(c);b+=2;c+=2;}
   wr(d,minterm(f,aa,bv,cv));d+=2;
  }
  a+=am;b+=bm;c+=cm;d+=dm;
 }
 stats.spans++;
}
/* $264/$38a: four-plane saturating increment/decrement using carry masks. */
static void HOT(feedback_mix)(int odd){
 int src=odd?0x1d000:0x13000,dst=odd?0x13000:0x1d000;
 blit(odd?1:0x80,src+0x2800,src+0x5000,src+0x7800,0x28000,0,0,0,0,0,20,256);
 blit(odd?0x89:0xe6,0x28000,0x10000,src,dst,0,8,0,0,0,20,256);
 for(int p=0;p<3;p++)blit(odd?0xa6:0x9a,src+p*0x2800,dst+p*0x2800,src+(p+1)*0x2800,dst+(p+1)*0x2800,0,0,0,0,0,20,256);
 shown=dst;
}
/* $222: bit-reversed subpixel jitter, 11x9 block transform, noise seed. */
static void HOT(feedback)(unsigned sample){
 unsigned f=++feedback_tick,v=f&31,jitter=0;
 for(int i=0;i<5;i++)jitter=(jitter<<1)|((v>>i)&1);
 unsigned pt=darkroom_tick_at_sample(sample);
 int mode=pt>=1248?1:pt>=806?2:0; /* Orders 3 / late order 1. */
 random_state=ror((random_state&0xffff0000)|((random_state+f)&65535),5)^0x1dc4;
 uint32_t r=random_state;
 for(int i=0;i<4;i++){
  int p=0x117b3+i*48;
  if(mode==1)chip[p]=chip[p+1]=0;
  else{chip[p]^=(uint8_t)r;r=(r<<16)|(r>>16);chip[p+1]^=(uint8_t)r; r=ror((r&0xffff0000)|((r+0x10000+(i+1)*48)&65535),1);}
 }
 int a=0xf3f8,d=0x28000,offset=(int)jitter*385-1,k=0;
 for(int y=0;y<9;y++){
  a+=0x62c;d+=0x62c;
  for(int x=0;x<11;x++){
   int16_t q=(int16_t)(maps[mode][k++]+offset);
   unsigned sh=15-(q&15);
   blit(0xf0,a+(q>>3),0,0,d,42,0,0,42,sh,3,32);
   a-=4;d-=4;
  }
 }
 int displacement=(int)jitter*24+(jitter>>4);
 blit(0xf0,0x28c0a-displacement*2,0,0,0x10000,0,0,0,0,jitter&15,24,256);
 feedback_mix((f-1)&1);
}
/* $67e..$93e: three age planes, a horizontal/vertical one-bit shear.
 * The original title is injected one scanline per tick. */
static const uint8_t warp_modes[8][2]={{5,255},{2,0},{7,255},{0,0},{1,255},{6,0},{3,255},{4,0}};
static unsigned planes[4];
static uint8_t title_fade[16][16];
static uint32_t spread[16]={0x00000000,0x01000000,0x00010000,0x01010000,0x00000100,0x01000100,0x00010100,0x01010100,0x00000001,0x01000001,0x00010001,0x01010001,0x00000101,0x01000101,0x00010101,0x01010101};
static void HOT(warp)(unsigned sample){
 unsigned old=planes[0];planes[0]=planes[1];planes[1]=planes[2];planes[2]=planes[3];planes[3]=old;
 warp_tick++;unsigned bit=warp_modes[warp_tick&7][0],mask=warp_modes[warp_tick&7][1];
 /* $84e: shift each scanline one pixel left or right. */
 for(int y=0;y<256;y++){
  int right=(((255-y)^mask)>>bit)&1;
  for(int x=0;x<20;x++){
   unsigned a=planes[0]+y*40+x*2;uint16_t v=rd(a);
   wr(0x12828+y*40+x*2,right?(v>>1)|(rd(a-2)<<15):(v<<1)|(rd(a+2)>>15));
  }
 }
 /* $8ca: independently choose the preceding or next row for each bit. */
 uint16_t select[20];
 for(int x=0;x<20;x++){
  uint16_t sel=0;for(int b=0;b<16;b++)sel=(sel<<1)|(((((224+x*16+b)^mask)>>bit)&1)?0:1);
  select[x]=sel;
 }
 for(int y=0;y<256;y++)for(int x=0;x<20;x++){
  uint16_t above=rd(0x12800+y*40+x*2),below=rd(0x12850+y*40+x*2),sel=select[x];
  wr(planes[1]+y*40+x*2,(above&sel)|(below&~sel));
 }
 /* $7c4 pulse on tracker row phase $40. */
 unsigned pos=darkroom_tick_at_sample(sample)%DARKROOM_ORDER_TICKS;unsigned row=(pos/13)*2+(pos%13>=8);
 if((row&31)==16)for(int y=0;y<32;y++)memset(chip+planes[1]+0x1192+y*40,255,4);
 unsigned line=warp_tick&31;
 for(int x=0;x<40;x++){
  uint8_t v=original_title[line*40+x];chip[planes[1]+0x1180+line*40+x]|=v;chip[0x1c850+0x1180+line*40+x]=v;
 }
}
static void sine_table(int short_table){
 uint32_t phase=short_table?0x3243f8:0,inc=short_table?0x6487f0:0xc90fe;
 int n=short_table?32:256;
 for(int i=0;i<n;i++){
  uint32_t d0=phase>>16;phase+=inc;int d2=d0;
  uint32_t d1=((d0*d0)<<4)>>16;
  d0=((d0&65535)*(d1&65535))/0x1800;d2-=d0&65535;
  d0=((d0&65535)*(d1&65535))/0x5000;d2+=d0&65535;
  d0=((d0&65535)*(d1&65535))/0xa800;d2-=d0&65535;
  d2=(int16_t)d2>>(short_table?2:4);
  sine[i]=sine[n*2-1-i]=sine[n*4+i]=(int16_t)d2;
  sine[n*2+i]=sine[n*4-1-i]=(int16_t)-d2;
 }
}
static void enter(unsigned s){
 memset(chip+0x10000,0,sizeof chip-0x10000);stage=s;
 if(s==1){planes[0]=0x10000;planes[1]=0x15050;planes[2]=0x17850;planes[3]=0x1a050;warp_tick=0;}
 if(s==2){sine_table(0);angle=0;velocity=0x80000;}
 if(s==3){sine_table(1);tick=0;}
}
/* Palette banks: blue 0..15, title 16..31, original sparkle 32..95,
 * closing copper 96..103. Fixed banks make page ownership race-free. */
static void palette(void){
 for(unsigned i=0;i<16;i++)demo_palette[i]=rgb(0,0,i*17);
 for(unsigned i=0;i<16;i++){
  unsigned a=0xd6+i*4,c=(original_copper[a]<<8)|original_copper[a+1];
  demo_palette[16+i]=rgb(((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17);
 }
 for(int i=0;i<64;i++){
  unsigned c=0;
  if(i>=24&&i<40)c=i-24;
  else if(i>=40&&i<47){static const unsigned p[]={0x22f,0x44f,0x88f,0xaaf,0xccf,0xeef,0xfff};c=p[i-40];}
  else if(i>=47&&i<53)c=0xfff;
  else if(i>=53&&i<58){static const unsigned p[]={0xeef,0xccf,0xaaf,0x88f,0x11e};c=p[i-53];}
  else if(i>=58)c=12-(i-58)*2;
  demo_palette[32+i]=rgb(((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17);
 }
 for(int i=0;i<8;i++){unsigned a=0x13a+i*4,c=(original_copper[a]<<8)|original_copper[a+1];demo_palette[96+i]=rgb(((c>>8)&15)*17,((c>>4)&15)*17,(c&15)*17);}
 unsigned used=104;
 for(int fade=0;fade<16;fade++)for(int i=0;i<16;i++){
  unsigned a=0xd6+i*4,c=(original_copper[a]<<8)|original_copper[a+1];
  uint16_t value=rgb(clampi((int)((c>>8)&15)-fade,0,15)*17,clampi((int)((c>>4)&15)-fade,0,15)*17,clampi((int)(c&15)-fade,0,15)*17);
  unsigned k=0;while(k<used&&demo_palette[k]!=value)k++;assert(k<256);
  if(k==used)demo_palette[used++]=value;title_fade[fade][i]=k;
 }

}
static void HOT(sparkle)(uint8_t *page){
 angle+=velocity;velocity+=0x800;
 unsigned phase=angle>>16;int ox[5],oy[5];
 for(int i=0;i<5;i++){ox[i]=sine[(phase&0x7fe)/2];oy[i]=sine[((phase&0x7fe)+512)/2];phase=(phase>>1)+0x780;}
 uint8_t xs[320],ys[256];
 for(int x=0;x<320;x++){unsigned v=0;for(int i=0;i<5;i++)v+=reciprocal[clampi(0x160+ox[i]+x,0,1023)];xs[x]=(v&255)>>4;}
 for(int y=0;y<256;y++){unsigned v=0;for(int i=0;i<5;i++)v+=reciprocal[clampi(0x180+oy[i]+y,0,1023)];ys[y]=(v&255)>>2;}
 for(int y=0;y<HEIGHT;y++){int sy=y*256/HEIGHT;for(int x=0;x<WIDTH;x++)page[y*WIDTH+x]=32+((ys[sy]+xs[x]*4)&63);}
}
/* $1022/$1094: 64 original ray directions and rotating line patterns.
 * The line blitter writes a fixed 128/160-pixel major-axis length. */
static void HOT(ray_line)(int dx,int dy,uint16_t pattern){
 int ax=dx<0?-dx:dx,ay=dy<0?-dy:dy;if(!(ax|ay))return;
 int steep=ay>=ax,major=steep?ay:ax,minor=steep?ax:ay;
 int err=2*minor-major,x=160,y=128,n=steep?128:160;
 for(int i=0;i<n;i++){
  if((pattern&(0x8000u>>(i&15)))&&x>=0&&x<320&&y>=0&&y<256)chip[0x10000+y*40+(x>>3)]|=128>>(x&7);
  if(err>=0){if(steep)x+=dx<0?-1:1;else y+=dy<0?-1:1;err-=2*major;}
  if(steep)y+=dy<0?-1:1;else x+=dx<0?-1:1;err+=2*minor;
 }
}
static void HOT(rays)(void){
 memset(chip+0x10000,0,10240);unsigned f=++tick;
 uint32_t pattern=0xffff;if(f<=64)pattern=ror(pattern,(64-f)>>2);
 unsigned a=(f-32)*2,b=a;pattern=ror16(pattern,a);
 int centre=f;if(!(centre&256))centre=(uint16_t)(~centre+256);centre=(int8_t)(centre-128)*16;
 for(int i=0;i<64;i++){
  int dx=centre+sine[(a&254)/2]+sine[(b&254)/2];
  int dy=sine[((a&254)+64)/2]+sine[((b&254)+64)/2];
  ray_line(dx,dy,pattern);pattern=ror16(pattern,1);a=(a&~255)|((a+4)&255);b=(b&~255)|((b+8)&255);
 }
}
void demo_init(void){
 memset(chip,0,sizeof chip);memset(&stats,0,sizeof stats);palette();
 int dx[3]={383,-1,384},dy[3]={385,384,1};
 for(int m=0;m<3;m++)for(int y=0;y<9;y++)for(int x=0;x<11;x++)maps[m][y*11+x]=dx[m]*(5-x)+dy[m]*(4-y);
 for(int i=0;i<512;i++)reciprocal[i]=reciprocal[1023-i]=256/(516-i);
 stage=tick=feedback_tick=warp_tick=0;next_tick=0;random_state=0xdeadbeef;shown=0x13000;
}
void HOT(demo_render)(uint8_t *page,uint32_t sample){
 stats.spans=0;
 unsigned target=darkroom_tick_at_sample(sample);
 if((int)target<next_tick-1)demo_init();
 /* PAL effect clock; output transport remains standard 60 Hz VGA. */
 while(next_tick<=(int)target){
  unsigned t=DARKROOM_SAMPLE_AT_TICK(next_tick),s=next_tick<1664?0:next_tick<2496?1:next_tick<3328?2:3;
  if(s!=stage)enter(s);
  if(s==0){/* The original's large four-channel blits update about 12.5 times/s. */if(!(next_tick&3))feedback(t);}
  else if(s==1)warp(t);
  else if(s==2)sparkle(page);
  else rays();
  next_tick++;
 }
 if(stage==2){/* Re-present without advancing the effect on duplicated VGA fields. */
  angle-=velocity-0x800;velocity-=0x800;sparkle(page);
 }else {
  unsigned addresses[4]={0,0,0,0};uint32_t base=0;
  if(stage==0)for(int i=0;i<4;i++)addresses[i]=shown+i*0x2800;
  if(stage==1){for(int i=0;i<3;i++)addresses[i]=planes[i];addresses[3]=0x1c850;base=0x10101010;}
  for(int y=0;y<HEIGHT;y++){
   unsigned sy=y*256/HEIGHT,off=sy*40;
   uint8_t *out=page+y*WIDTH;
   for(int x=0;x<40;x++){
    unsigned a,b,c,d;uint32_t lo,hi;
    if(stage<2){
     a=chip[addresses[0]+off+x];b=chip[addresses[1]+off+x];c=chip[addresses[2]+off+x];d=chip[addresses[3]+off+x];
     lo=base|spread[a>>4]|(spread[b>>4]<<1)|(spread[c>>4]<<2)|(spread[d>>4]<<3);
     hi=base|spread[a&15]|(spread[b&15]<<1)|(spread[c&15]<<2)|(spread[d&15]<<3);

    }else{
     a=chip[0x10000+off+x];b=0;int top=128-clampi((int)tick,0,19);
     if((int)sy>=top&&(int)sy<top+38)b=original_credits[(sy-top)*40+x];
     lo=0x60606060|spread[a>>4]*3|(spread[b>>4]<<2);hi=0x60606060|spread[a&15]*3|(spread[b&15]<<2);
    }
    memcpy(out+x*8,&lo,4);memcpy(out+x*8+4,&hi,4);
   }
  }
 }
 if(stage==1&&target>=2444){unsigned fade=clampi((int)target-2444+1,0,15);for(unsigned i=0;i<WIDTH*HEIGHT;i++)page[i]=title_fade[fade][page[i]-16];}
 stats.section=stage;stats.tiles=0;
}
unsigned demo_triangles(void){return 0;}
void demo_stats(demo_stats_t *out){*out=stats;}
