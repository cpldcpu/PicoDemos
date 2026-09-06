/* VESPER: "A machine for the blue hour". Original 64-bar score in D minor.
 * Integer oscillators, FM bell, detuned pads, kick, snare, hats, stereo delay.
 * No samples, allocations, transcendental functions or float in the audio loop.
 * All sequencing uses sample positions: render block size cannot change notes.
 */
#include "vesper.h"
#include <math.h>
#include <string.h>
static int16_t wave[2048],delay_l[8192],delay_r[8192];
static uint32_t notes[128],position,rng,phase[12],inc[12],kick_phase;
static int32_t bass_env,bell_env,kick_env,snare_env,hat_env,bass_lp,pad_lp;
static int32_t dc_l,dc_r;
static unsigned delay_pos,last_step;
static int32_t bell_decay,bell_attack;
static int sinwave(uint32_t p){return wave[p>>21];}
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static int scale_note(unsigned bar){static const int root[8]={38,38,34,34,41,41,36,36};return root[(bar/2)%8];}
void synth_init(void){
    for(int i=0;i<2048;i++)wave[i]=(int16_t)(sinf(i*6.28318530718f/2048)*32767);
    for(int i=0;i<128;i++)notes[i]=(uint32_t)(440.0*pow(2.0,(i-69)/12.0)*4294967296.0/SAMPLE_RATE);
    memset(delay_l,0,sizeof delay_l);memset(delay_r,0,sizeof delay_r);memset(phase,0,sizeof phase);memset(inc,0,sizeof inc);
    bell_attack=0;
    position=0;rng=0x7a31u;kick_phase=0;delay_pos=0;last_step=~0u;
    bass_env=bell_env=kick_env=snare_env=hat_env=bass_lp=pad_lp=dc_l=dc_r=0;
}
/* Proposal 01: "Canticle". Explicit D-natural-minor note choices.
 * Four related phrases, each answering the same opening motif.
 * MIDI 0 denotes a rest, not a voice kill: the previous note decays naturally.
 * Harmony and all scene/drum boundaries remain at the original sample times.
 */
static const uint8_t melody[4][4][8]={
    { /* D minor: call, answer, variation, handoff to B-flat. */
        {74, 0,77,76,74, 0,69, 0},
        {72, 0,69, 0,74, 0, 0, 0},
        {74, 0,77,76,74,72,69, 0},
        {69, 0,72,69,67, 0,65, 0}
    },
    { /* B-flat major: D and F are held in common with the pad. */
        {74, 0,77,74,70, 0,65, 0},
        {72,74,70, 0,65, 0, 0, 0},
        {74, 0,77,74,72, 0,70, 0},
        {70, 0,69,67,65, 0,67, 0}
    },
    { /* F major. E appears as a short passing tone, resolving down to C. */
        {72, 0,77,76,72, 0,69, 0},
        {67,69,72, 0,69, 0, 0, 0},
        {72, 0,74,72,69, 0,65, 0},
        {69, 0,67,65,67, 0,64, 0}
    },
    { /* C major: the final E resolves down to D at the loop boundary. */
        {76, 0,74,72,67, 0,64, 0},
        {67, 0,72, 0,76, 0, 0, 0},
        {74, 0,72,69,67, 0,64, 0},
        {67, 0,69, 0,72, 0,76, 0}
    }
};
static const uint8_t pad_voicing[4][4]={
    {50,57,62,65}, /* D3 A3 D4 F4 */
    {53,58,62,65}, /* F3 Bb3 D4 F4: two common upper voices */
    {53,57,60,65}, /* F3 A3 C4 F4 */
    {48,55,60,64}  /* C3 G3 C4 E4 */
};
static void sequence(unsigned step){
    unsigned bar=step/16,s=step%16,chord=(bar/4)%4;
    int root=scale_note(bar),drums=bar>=8&&bar<56&&!(bar>=32&&bar<36);
    if(s==0){
        for(int i=0;i<4;i++){
            int n=pad_voicing[chord][i];
            inc[i*2]=notes[n];inc[i*2+1]=notes[n]+notes[n]/650;
        }
    }
    if(drums&&(s%4==0||(bar%4==3&&s==14))){kick_env=30000;kick_phase=0;}
    if(drums&&(s==4||s==12||(bar%8==7&&(s==14||s==15))))snare_env=15000;
    if(drums&&(s%2==0||bar>=40)){hat_env=s%4==2?8500:3500;}
    if(bar>=4&&bar<60&&(s%2==0)){
        static const int bass_pattern[8]={0,0,12,0,0,7,0,12};
        inc[8]=notes[root-12+bass_pattern[s/2]];bass_env=s%4==2?14000:9000;
    }
    if((bar>=2&&bar<32&&(s%2==0))||(bar>=36&&bar<60&&(s%2==0))||(bar>=32&&bar<36&&s%4==0)){
        int note=melody[chord][bar%4][s/2];
        if(note){
            inc[9]=notes[note];inc[10]=notes[note]*2;
            bell_env=bar<8?6000:bar>=40?8500:7200;
            phase[9]=phase[10]=0;bell_attack=0;
            bell_decay=bar>=32&&bar<36?32762:32758;
        }
    }
}
static int16_t limit(int32_t x){
    /* Gentle bounded saturation, with exact integer arithmetic. */
    int32_t a=x<0?-x:x;if(a>24000)x=(x<0?-1:1)*(24000+(a-24000)/4);
    return (int16_t)clampi(x,-32760,32760);
}
void HOT(synth_render)(int16_t *out,unsigned frames){
    for(unsigned f=0;f<frames;f++){
        if(position>=DURATION_SAMPLES){out[f*2]=out[f*2+1]=0;position++;continue;}
        unsigned step=(unsigned)((uint64_t)position*4/BEAT_SAMPLES);
        if(step!=last_step){sequence(step);last_step=step;}
        for(int i=0;i<11;i++)phase[i]+=inc[i];
        int32_t pad=0;for(int i=0;i<8;i++){int32_t saw=(int32_t)(phase[i]>>16)-32768;pad+=saw/16;}
        pad_lp+=(pad-pad_lp)/48;
        int duck=32768-kick_env/2;
        int32_t bass=(int32_t)(phase[8]>>16)-32768;bass_lp+=(bass-bass_lp)/7;
        bass=((bass_lp*bass_env)>>15)*duck/32768;
        int fm=(sinwave(phase[10])*bell_env)>>15;
        int32_t bell=sinwave(phase[9]+(uint32_t)(fm*8000));bell=(bell*bell_env)>>15;
        if(bell_attack<32768)bell_attack+=512;
        bell=(bell*bell_attack)>>15;
        kick_phase+=8000000u+(uint32_t)kick_env*1400u;
        int32_t kick=(sinwave(kick_phase)*kick_env)>>15;
        int noise=(int16_t)(random32()>>16);
        int32_t snare=((noise*snare_env)>>15)+((sinwave(phase[0]*3)*snare_env)>>16);
        int32_t hat=((noise*hat_env)>>15);
        int32_t pad_gain=(pad_lp*duck)>>15;
        int32_t dry=kick+snare/2+hat/3+bass/2+pad_gain/2;
        int32_t dl=delay_l[(delay_pos+8192-5625)&8191],dr=delay_r[(delay_pos+8192-2813)&8191];
        int32_t send=bell+pad_lp/3;
        delay_l[delay_pos]=limit(send+dr*2/5);delay_r[delay_pos]=limit(send+dl*2/5);
        delay_pos=(delay_pos+1)&8191;
        int32_t l=dry+bell*3/4+dl/2,r=dry+bell*3/4+dr/2;
        dc_l+=(l-dc_l)/1024;dc_r+=(r-dc_r)/1024;l-=dc_l;r-=dc_r;
        int32_t gain=32768;
        if(position<SAMPLE_RATE*2u)gain=(int32_t)((uint64_t)position*32768/(SAMPLE_RATE*2u));
        if(position>DURATION_SAMPLES-SAMPLE_RATE*5u)gain=(int32_t)((uint64_t)(DURATION_SAMPLES-position)*32768/(SAMPLE_RATE*5u));
        out[f*2]=limit((int32_t)(((int64_t)l*gain)>>15));out[f*2+1]=limit((int32_t)(((int64_t)r*gain)>>15));
        bass_env=(bass_env*32720)>>15;bell_env=(bell_env*bell_decay)>>15;kick_env=(kick_env*32717)>>15;
        snare_env=(snare_env*32695)>>15;hat_env=(hat_env*32630)>>15;
        position++;
    }
}
uint32_t synth_position(void){return position;}
void synth_seek(uint32_t target){
    synth_init();int16_t scratch[512];while(position<target){unsigned n=target-position;if(n>256)n=256;synth_render(scratch,n);}
}
