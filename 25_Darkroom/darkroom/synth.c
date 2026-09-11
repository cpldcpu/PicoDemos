/* Strobo's original Darkroom MOD. Integer four-channel PAL replay by Phase.
 * Implements exactly the effects present in this module: A,C,E1,ED,F.
 * Instruments, notes, swing (alternating speed 8/5), and finetuning are original. */
#include "darkroom.h"
#include "assets.h"
#include "periods.h"
#include "timing.h"
#include <string.h>
typedef struct{const int8_t *data;unsigned length,loop,length_loop,finetune,volume;} Sample;
/* Paula keeps playing the sample whose DMA was triggered even if a later row
 * merely selects another instrument (notably while EDx delays its new DMA).
 * Keep those two pieces of state separate. */
typedef struct{const Sample *selected,*playing;unsigned phase,step,period,volume,fx,param,note;int active,looped;} Channel;
static Sample samples[31];static Channel ch[4];
static unsigned pos,order,row,tick,speed,left,stopped;
static uint64_t tick_fraction;
static uint32_t hash,hash_pos,hash_value,hash_seen,final_hash;
static int32_t lp[2];
_Static_assert(SAMPLE_RATE==DARKROOM_AUDIO_RATE,"music clock must match the audio rate");
_Static_assert(DARKROOM_ORDER_4_SAMPLE==799993u&&DARKROOM_ORDER_6_SAMPLE==1199990u&&
               DARKROOM_ORDER_8_SAMPLE==1599986u&&DARKROOM_F00_SAMPLE==1797581u,
               "PAL timeline constants changed");
static unsigned be(const unsigned char *p){return p[0]*256+p[1];}
static void step(Channel *v){v->step=v->period?(uint32_t)(((uint64_t)3546895*65536/SAMPLE_RATE)/v->period):0;}
static void trigger(Channel *v){
 if(!v->selected||!v->note)return;
 unsigned n=0;while(n<35&&v->note<periods[0][n])n++;
 v->period=periods[v->selected->finetune][n];v->playing=v->selected;
 v->phase=0;v->active=v->playing->length>2;v->looped=0;step(v);
}
static void tracker_tick(void){
 if(stopped)return;
 if(tick==0){
  unsigned pattern=original_module[952+order];
  for(int c=0;c<4;c++){
   const unsigned char *p=original_module+1084+pattern*1024+row*16+c*4;Channel *v=&ch[c];
   unsigned ins=(p[0]&240)|(p[2]>>4);v->note=((p[0]&15)<<8)|p[1];v->fx=p[2]&15;v->param=p[3];
   if(ins){v->selected=&samples[ins-1];v->volume=v->selected->volume;}
   if(v->note&&!(v->fx==14&&(v->param>>4)==13))trigger(v);
   if(v->fx==12)v->volume=v->param>64?64:v->param;
   if(v->fx==14&&(v->param>>4)==1){v->period=v->period>(v->param&15)?v->period-(v->param&15):113;step(v);}
   if(v->fx==15){if(v->param)speed=v->param;else stopped=1;}
  }
 }else for(int c=0;c<4;c++){
  Channel *v=&ch[c];
  if(v->fx==10){int delta=(v->param>>4)?(int)(v->param>>4):-(int)(v->param&15);v->volume=(unsigned)clampi((int)v->volume+delta,0,64);}
  if(v->fx==14&&(v->param>>4)==13&&tick==(v->param&15))trigger(v);
 }
 if(++tick>=speed){tick=0;if(++row==64){row=0;if(++order>=original_module[950])stopped=1;}}
 if(stopped)for(int c=0;c<4;c++)ch[c].active=0;
}
void synth_init(void){
 unsigned offset=1084+7*1024;
 for(int i=0;i<31;i++){
  const unsigned char *p=original_module+20+i*30;Sample *s=&samples[i];
  s->data=(const int8_t*)original_module+offset;s->length=be(p+22)*2;s->finetune=p[24]&15;s->volume=p[25];s->loop=be(p+26)*2;s->length_loop=be(p+28)*2;offset+=s->length;
 }
 memset(ch,0,sizeof ch);memset(lp,0,sizeof lp);pos=order=row=tick=left=stopped=0;tick_fraction=0;speed=6;
 hash=2166136261u;hash_pos=hash_value=hash_seen=final_hash=0;
}
void HOT(synth_render)(int16_t *out,unsigned frames){
 for(unsigned i=0;i<frames;i++){
  if(!left){
   tracker_tick();tick_fraction+=DARKROOM_TICK_SCALED;
   left=(unsigned)(tick_fraction>>31);tick_fraction&=0x7fffffffu;
  }left--;
  int lr[2]={0,0};
  for(int c=0;c<4;c++){
   Channel *v=&ch[c];if(!v->active)continue;const Sample *s=v->playing;unsigned at=v->phase>>16;
   /* The first DMA pass ends at the sample length. Paula then reloads the
    * repeat start/length registers, so every later pass ends at loop+length,
    * which is not necessarily the physical end of the sample. */
   unsigned end=v->looped?s->loop+s->length_loop:s->length;
   if(at>=end){if(s->length_loop>2){at=s->loop+(at-end)%s->length_loop;v->phase=(at<<16)|(v->phase&65535);v->looped=1;}else{v->active=0;continue;}}
   int a=s->data[at]*(int)v->volume;int side=(c==0||c==3)?0:1;
   lr[side]+=a;v->phase+=v->step;
  }
  for(int c=0;c<2;c++){
   /* A500-like 4.4 kHz low-pass. */
   lp[c]+=(lr[c]-lp[c])*22500/32768;int16_t v=(int16_t)clampi(lp[c],-32768,32767);
   out[i*2+c]=v;hash=(hash^(uint16_t)v)*16777619u;
  }
  ++pos;if(pos==DURATION_SAMPLES)__atomic_store_n(&final_hash,hash,__ATOMIC_RELEASE);
  if(pos%SAMPLE_RATE==0){__atomic_store_n(&hash_pos,0,__ATOMIC_RELEASE);__atomic_store_n(&hash_value,hash,__ATOMIC_RELEASE);__atomic_store_n(&hash_pos,pos,__ATOMIC_RELEASE);}
 }
}
uint32_t synth_position(void){return pos;}
uint32_t synth_final_hash(void){return __atomic_load_n(&final_hash,__ATOMIC_ACQUIRE);}
void synth_seek(uint32_t sample){synth_init();int16_t b[512];while(pos<sample){unsigned n=sample-pos;if(n>256)n=256;synth_render(b,n);}}
int synth_hash_latch(uint32_t *p,uint32_t *h){uint32_t a,b,v;do{a=__atomic_load_n(&hash_pos,__ATOMIC_ACQUIRE);v=__atomic_load_n(&hash_value,__ATOMIC_ACQUIRE);b=__atomic_load_n(&hash_pos,__ATOMIC_ACQUIRE);}while(a!=b);if(!a||a==hash_seen)return 0;hash_seen=a;*p=a;*h=v;return 1;}
