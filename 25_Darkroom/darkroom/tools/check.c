#include "darkroom.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "timing.h"
static struct {uint32_t before;uint8_t pixels[WIDTH*HEIGHT];uint32_t after;} page;
int main(void){
 static const unsigned checkpoints[]={240000,720000,1008000,1320000,1728000};uint32_t hashes[5]={0};
 demo_init();page.before=0x1234abcd;page.after=0x98765432;
 for(unsigned i=0;i<=4800;i++){demo_render(page.pixels,i*400);assert(page.before==0x1234abcd&&page.after==0x98765432);for(unsigned j=0;j<5;j++)if(i*400==checkpoints[j])hashes[j]=demo_frame_hash(page.pixels);}
 uint32_t end=demo_frame_hash(page.pixels);demo_init();demo_render(page.pixels,DURATION_SAMPLES);assert(demo_frame_hash(page.pixels)==end);
 for(int j=4;j>=0;j--){
  demo_render(page.pixels,checkpoints[j]);assert(demo_frame_hash(page.pixels)==hashes[j]);
  memset(page.pixels,0xff,sizeof page.pixels);demo_render(page.pixels,checkpoints[j]);assert(demo_frame_hash(page.pixels)==hashes[j]);
 }
 /* Exact fractional PAL boundary ownership, including transitions. */
 for(unsigned t=0;t<=4000;t++){unsigned s=DARKROOM_SAMPLE_AT_TICK(t);assert(darkroom_tick_at_sample(s)==t);if(t)assert(darkroom_tick_at_sample(s-1)==t-1);}
 synth_init();int16_t audio[1994];for(unsigned i=0;i<DURATION_SAMPLES;){unsigned n=DURATION_SAMPLES-i;if(n>997)n=997;synth_render(audio,n);i+=n;}
 uint32_t ah=synth_final_hash();synth_seek(DURATION_SAMPLES);assert(ah==synth_final_hash());
 printf("PASS 4801 frames, guards, complete writes, reverse seek in all effects, PAL boundaries, arbitrary audio blocks; final PCM %08x\n",ah);return 0;
}
