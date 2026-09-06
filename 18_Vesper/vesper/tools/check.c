/* End-to-end checks: all 7200 frames, output guards, seek stability, score
 * boundaries, and complete stereo synthesis with incompatible block sizes. */
#include "vesper.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static struct {uint32_t before[16];uint16_t image[WIDTH*HEIGHT];uint32_t after[16];} guarded;
static uint64_t hash(const void *data,size_t n,uint64_t h){const unsigned char*p=data;while(n--)h=(h^*p++)*1099511628211ull;return h;}
static int fail(const char *s){fprintf(stderr,"FAIL: %s\n",s);return 1;}
static uint64_t audio_hash(unsigned block){
    synth_init();int16_t b[2048];uint64_t h=14695981039346656037ull;
    while(synth_position()<DURATION_SAMPLES){unsigned n=DURATION_SAMPLES-synth_position();if(n>block)n=block;synth_render(b,n);h=hash(b,n*4,h);}return h;
}
int main(void){
    static const unsigned bars[]={0,8,20,32,40,56};
    for(int i=0;i<6;i++){if(score_at(bars[i]*BEAT_SAMPLES*4).chapter!=i)return fail("chapter start");if(i&&score_at(bars[i]*BEAT_SAMPLES*4-1).chapter!=i-1)return fail("chapter boundary");}
    uint64_t a=audio_hash(1),b=audio_hash(997);if(a!=b)return fail("audio block independence");
    printf("PASS stereo synth: 2,880,000 samples/channel, blocks 1 and 997: %016llx\n",(unsigned long long)a);
    int16_t silence[64];synth_render(silence,32);for(int i=0;i<64;i++)if(silence[i])return fail("sound after ending");
    demo_init();memset(guarded.before,0x5a,sizeof guarded.before);memset(guarded.after,0x5a,sizeof guarded.after);
    uint64_t all=14695981039346656037ull,frame44=0;unsigned max_tri=0;
    for(unsigned f=0;f<=7200;f++){
        demo_render(guarded.image,f*400);
        for(int k=0;k<16;k++)if(guarded.before[k]!=0x5a5a5a5a||guarded.after[k]!=0x5a5a5a5a)return fail("framebuffer guard");
        for(int k=0;k<WIDTH*HEIGHT;k++)if(guarded.image[k]&32)return fail("invalid DAC bit");
        if(f>60&&f<7080){unsigned sum=0;for(int k=0;k<WIDTH*HEIGHT;k+=17)sum+=red(guarded.image[k])+green(guarded.image[k])+blue(guarded.image[k]);if(sum<2000)return fail("unexpected black frame");}
        uint64_t h=hash(guarded.image,sizeof guarded.image,14695981039346656037ull);
        if(f==44*60)frame44=h;all=hash(&h,sizeof h,all);if(demo_triangles()>max_tri)max_tri=demo_triangles();
        if(f==7200)for(int k=0;k<WIDTH*HEIGHT;k++)if(guarded.image[k])return fail("ending not black");
    }
    demo_render(guarded.image,44*SAMPLE_RATE);if(hash(guarded.image,sizeof guarded.image,14695981039346656037ull)!=frame44)return fail("visual seek independence");
    printf("PASS 7201 frames including black endpoint, guards, DAC layout, nonblank scenes, seek: %016llx; max %u triangles\n",(unsigned long long)all,max_tri);
    return 0;
}
