#include "pelagic.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static struct {uint32_t before[16];uint16_t pixels[WIDTH*HEIGHT];uint32_t after[16];} guarded;
static int16_t reference[2048],block[2048];
static uint64_t hash_frame(void){uint64_t h=1469598103934665603ull;for(int i=0;i<WIDTH*HEIGHT;i++)h=(h^guarded.pixels[i])*1099511628211ull;return h;}
static void fail(const char *reason,unsigned sample){fprintf(stderr,"FAIL %s at sample %u\n",reason,sample);exit(1);}
int main(void){
    memset(&guarded,0xa5,sizeof guarded);demo_init();synth_init();
    uint64_t first=0,combined=0;unsigned maxtri=0;
    for(unsigned sample=0;sample<=DURATION_SAMPLES;sample+=800){
        demo_render(guarded.pixels,sample);
        for(int i=0;i<16;i++)if(guarded.before[i]!=0xa5a5a5a5||guarded.after[i]!=0xa5a5a5a5)fail("framebuffer guard",sample);
        unsigned lit=0;for(int i=0;i<WIDTH*HEIGHT;i++){if(guarded.pixels[i]&32)fail("invalid DAC bit",sample);lit+=guarded.pixels[i]!=0;}
        if(sample>3*SAMPLE_RATE&&sample<DURATION_SAMPLES-4*SAMPLE_RATE&&lit<10000)fail("unexpected blank frame",sample);
        if(sample==DURATION_SAMPLES&&lit)fail("endpoint must be black",sample);
        if(sample==SAMPLE_RATE*60)first=hash_frame();
        combined^=hash_frame();if(demo_triangles()>maxtri)maxtri=demo_triangles();
    }
    demo_render(guarded.pixels,SAMPLE_RATE*60);if(first!=hash_frame())fail("non deterministic seek",SAMPLE_RATE*60);
    unsigned peak=0;uint64_t ah=1469598103934665603ull;
    const unsigned length=DURATION_SAMPLES+2048;
    /* Host-only reference buffer. Two sequential passes avoid O(n^2) work
     * when the musician supplies a stateful synth with reset-and-replay seek. */
    int16_t *audio=malloc(length*4);if(!audio)fail("allocation",0);
    synth_init();for(unsigned pos=0;pos<length;){unsigned n=length-pos;if(n>512)n=512;synth_render(audio+pos*2,n);pos+=n;}
    const unsigned sizes[4]={1,2,8,997};unsigned cycle=0;
    synth_init();for(unsigned pos=0;pos<length;){
        unsigned n=sizes[cycle++&3];if(n>length-pos)n=length-pos;synth_render(block,n);
        if(memcmp(audio+pos*2,block,n*4))fail("audio block-size mismatch",pos);
        if(synth_position()!=pos+n)fail("audio position",pos);
        for(unsigned i=0;i<n*2;i++){unsigned a=abs(block[i]);if(a>peak)peak=a;ah=(ah^(uint16_t)block[i])*1099511628211ull;if(pos+i/2>=DURATION_SAMPLES&&a)fail("audio endpoint",pos);}
        pos+=n;
    }
    for(unsigned pos=SAMPLE_RATE*9;pos<DURATION_SAMPLES;pos+=SAMPLE_RATE*37){
        synth_seek(pos);synth_render(reference,1024);if(memcmp(reference,audio+pos*2,sizeof reference))fail("audio seek mismatch",pos);
    }
    free(audio);
    printf("PASS 4609 guarded frames; deterministic seek; audio arbitrary blocks and endpoint\n");
    printf("visual_hash=%016llx audio_hash=%016llx max_triangles=%u audio_peak=%u (Phosphor score)\n",(unsigned long long)combined,(unsigned long long)ah,maxtri,peak);
    return 0;
}
