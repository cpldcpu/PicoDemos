/* ONE SMALL THING / Phase. Integer stereo synthesis, no audio assets.
 * Muted brass (four harmonic partials), rounded octave bass, soft accordion
 * triads, a wooden 2:1 FM mallet, membrane kick and dry snare/shaker/rim.
 * A 24-frame private block bounds DSP spikes independently of pull size.
 * All tables are integer-derived: host and Cortex-M33 emit identical PCM. */
#include "song.h"
#include <string.h>
#define BLOCK 24u
#define DELAY 8192u
typedef struct { uint32_t phase, inc, age, gate; int32_t env; uint8_t vel; } Voice;
static Voice voices[VOICES];
static int16_t sine[1024],delay[DELAY],room[1297],buffer[BLOCK*2];
static uint32_t pos,generated,read_at,noise,delay_at,room_at;
static uint32_t kick_phase,kick_age,snare_age,hat_age,rim_age;
static int kick_vel,snare_vel,hat_vel,rim_vel;
static int32_t noise_lp,dc_x[2],dc_y[2];
static uint32_t hash,hash_pos,hash_value,hash_seen;
static uint32_t final_hash;
/* Octave 10 phase increments, rounded once offline, divided by octaves. */
static const uint32_t top[12]={1498230996u,1587320447u,1681707432u,1781706960u,1887652769u,1999898444u,2118818594u,2244810104u,2378293459u,2519714147u,2669544147u,2828283503u};
static uint32_t pitch(unsigned midi){return top[midi%12]>>(10-midi/12);}
static inline int32_t osc(uint32_t phase){return sine[phase>>22];}
static inline int32_t mul(int32_t a,int32_t b){return (int32_t)(((int64_t)a*b)>>15);}
static inline uint32_t rnd(void){noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;return noise;}
static void trigger(Voice *v,Note n){if(!n.note)return;v->inc=pitch(n.note);v->age=0;v->gate=n.steps*STEP_SAMPLES;v->vel=n.velocity;/* continuous phase and envelope prevent retrigger clicks */}
static int32_t envelope(Voice *v,int instrument){
 uint32_t age=v->age,attack=instrument==BRASS?360:instrument>=CHORD1&&instrument<=CHORD3?700:48;
 int32_t target;
 if(age>=v->gate)target=mul(v->env,instrument==MALLET?31800:instrument==BRASS?30000:31300);
 else if(age<attack)target=(int32_t)(age*32700/attack);
 else if(instrument==MALLET)target=(int32_t)(32700u*1700u/(1700u+age));
 else if(instrument>=CHORD1&&instrument<=CHORD3)target=(int32_t)(29000u*6000u/(6000u+age));
 else target=instrument==BRASS?24000:27500;
 if(target<8)target=0;return target;
}
static void HOT(make_block)(void){
 if(generated%STEP_SAMPLES==0){
   ScoreStep s;song_step(generated/STEP_SAMPLES,&s);
   for(int i=0;i<VOICES;i++)trigger(&voices[i],s.v[i]);
   if(s.kick){kick_age=0;kick_phase=0;kick_vel=s.kick;}
   if(s.snare){snare_age=0;snare_vel=s.snare;}
   if(s.hat){hat_age=0;hat_vel=s.hat;}
   if(s.rim){rim_age=0;rim_vel=s.rim;}
 }
 int32_t increment[VOICES],target[VOICES];
 for(int i=0;i<VOICES;i++){target[i]=envelope(&voices[i],i);increment[i]=(target[i]-voices[i].env)/(int)BLOCK;}
 for(unsigned n=0;n<BLOCK;n++){
   int32_t l=0,r=0,send=0;
   for(int i=0;i<VOICES;i++){
     Voice *v=&voices[i];v->phase+=v->inc;v->env+=increment[i];
     if(!v->env)continue;
     int32_t a=osc(v->phase),tone;
     if(i==BASS)tone=(a*3+osc(v->phase*2u))/4;
     else if(i==BRASS)tone=(a*8+osc(v->phase*2u)*3+osc(v->phase*3u)*2+osc(v->phase*4u))/14;
     else if(i==MALLET){int32_t fm=mul(osc(v->phase*2u),v->env);tone=osc(v->phase+(uint32_t)(fm*3200));}
     else tone=(a*4+osc(v->phase*2u)+osc(v->phase*3u))/6;
     int gain=i==BASS?7000:i==BRASS?6700:i==MALLET?6200:2100;
     int32_t s=mul(mul(tone,v->env),gain)*v->vel/128;
     int pan=i==CHORD1?9:i==CHORD3?23:i==MALLET?19:i==BRASS?14:16;
     l+=s*(32-pan)/24;r+=s*pan/24;
     if(i!=BASS)send+=s/3;
   }
   int32_t ns=(int32_t)(rnd()>>16)-32768;noise_lp+=(ns-noise_lp)/4;int32_t hp=ns-noise_lp;
   if(kick_age<6000){
     unsigned hz=49+155*240u/(240+kick_age);kick_phase+=(uint32_t)((uint64_t)hz*4294967296ull/SAMPLE_RATE);
     int32_t e=(int32_t)((uint64_t)(6000-kick_age)*(6000-kick_age)*11000/36000000);
     int32_t k=mul(osc(kick_phase),e)*kick_vel/128;l+=k;r+=k;
   }
   if(snare_age<3600){
     int32_t e=(int32_t)((3600-snare_age)*(3600-snare_age)/5000);
     int32_t s=mul(hp*3/4+osc(snare_age*32212255u)/4,e)*snare_vel/128;l+=s;r+=s;
   }
   if(hat_age<1300){int32_t h=mul(hp,(int32_t)(1300-hat_age)*2)*hat_vel/128;l+=h*3/4;r+=h;}
   if(rim_age<600){int32_t s=mul(osc(rim_age*340018244u), (600-(int32_t)rim_age)*7)*rim_vel/128;l+=s;r+=s/2;}
   kick_age++;snare_age++;hat_age++;rim_age++;
   int32_t dl=delay[(delay_at-7200u)&(DELAY-1)],dr=delay[(delay_at-4800u)&(DELAY-1)];
   delay[delay_at]=clampi(send+(dl+dr)/5,-25000,25000);delay_at=(delay_at+1)&(DELAY-1);
   int32_t rev=room[room_at];room[room_at]=(int16_t)clampi(send/2+rev*3/5,-20000,20000);if(++room_at==1297)room_at=0;
   l+=dl/2+rev/3;r+=dr/2-rev/4;
   int32_t lr[2]={l,r};
   uint32_t time=generated+n;int32_t fade=time<SAMPLE_RATE/20?(int32_t)(time*32768/(SAMPLE_RATE/20)):32768;
   if(time>DURATION_SAMPLES-SAMPLE_RATE*3)fade=time>=DURATION_SAMPLES?0:(int32_t)((DURATION_SAMPLES-time)*32768u/(SAMPLE_RATE*3));
   for(int c=0;c<2;c++){
     /* Truncate toward zero in the DC feedback. An arithmetic right shift
      * rounds every negative residue down and builds a -0.5% DC offset. */
     int32_t y=lr[c]-dc_x[c]+(int32_t)((int64_t)dc_y[c]*32680/32768);dc_x[c]=lr[c];dc_y[c]=y;
     buffer[n*2+c]=(int16_t)clampi(mul(y*3/2,fade),-32767,32767);
   }
 }
 for(int i=0;i<VOICES;i++){voices[i].env=target[i];voices[i].age+=BLOCK;}
 generated+=BLOCK;read_at=0;
}
void synth_init(void){
 memset(voices,0,sizeof voices);memset(delay,0,sizeof delay);memset(room,0,sizeof room);memset(dc_x,0,sizeof dc_x);memset(dc_y,0,sizeof dc_y);
 for(int i=0;i<1024;i++){
   int x=i&511;int64_t q=(int64_t)x*(512-x);int32_t v=(int32_t)(16*q*32767/(5*512*512-4*q));sine[i]=(int16_t)(i<512?v:-v);
 }
 pos=generated=delay_at=room_at=0;read_at=BLOCK;noise=0x1ceba11u;noise_lp=0;
 kick_age=snare_age=hat_age=rim_age=10000;kick_vel=snare_vel=hat_vel=rim_vel=0;kick_phase=0;
 hash=2166136261u;hash_pos=hash_value=hash_seen=final_hash=0;
}
void HOT(synth_render)(int16_t *out,unsigned frames){
 for(unsigned i=0;i<frames;i++){
   if(read_at==BLOCK)make_block();
   int16_t l=buffer[read_at*2],r=buffer[read_at*2+1];read_at++;
   out[i*2]=l;out[i*2+1]=r;hash=(hash^(uint16_t)l)*16777619u;hash=(hash^(uint16_t)r)*16777619u;
   ++pos;
   if(pos==DURATION_SAMPLES)__atomic_store_n(&final_hash,hash,__ATOMIC_RELEASE);
   if(pos%SAMPLE_RATE==0){
     /* Seqlock: the reader cannot pair one second's position with another
      * second's value, even if it is interrupted between the two loads. */
     __atomic_store_n(&hash_pos,0,__ATOMIC_RELEASE);
     __atomic_store_n(&hash_value,hash,__ATOMIC_RELEASE);
     __atomic_store_n(&hash_pos,pos,__ATOMIC_RELEASE);
   }
 }
}
uint32_t synth_position(void){return pos;}
uint32_t synth_final_hash(void){return __atomic_load_n(&final_hash,__ATOMIC_ACQUIRE);}
void synth_seek(uint32_t sample){synth_init();int16_t b[512];while(pos<sample){unsigned n=sample-pos;if(n>256)n=256;synth_render(b,n);}}
int synth_hash_latch(uint32_t *p,uint32_t *h){
 uint32_t a,b,v;do{a=__atomic_load_n(&hash_pos,__ATOMIC_ACQUIRE);v=__atomic_load_n(&hash_value,__ATOMIC_ACQUIRE);b=__atomic_load_n(&hash_pos,__ATOMIC_ACQUIRE);}while(a!=b);
 if(!a||a==hash_seen)return 0;hash_seen=a;*p=a;*h=v;return 1;
}
