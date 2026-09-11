/* Host-only 60 Hz full-run cost and non-flat image referee. */
#ifdef HOST_BUILD
#include "render.h"
#include "song.h"
#include <stdio.h>
#include <stdint.h>
static struct {uint32_t a;uint16_t page[CV_W*CV_H];uint32_t b;} frame;
int main(void)
{
    unsigned maxtri[10]={0},maxfill[10]={0},at[10]={0},n[10]={0},minfill[10];
    unsigned failures=0;
    for(int i=0;i<10;i++)minfill[i]=UINT32_MAX;
    demo_init();frame.a=0xabcdef01;frame.b=0x12345678;
    for(uint32_t sample=0;sample<CV_TOTAL_SAMPLES;sample+=CV_RATE/60){
        demo_render(frame.page,sample);demo_stats_t s;demo_stats(&s);unsigned c=s.chapter;n[c]++;
        if(s.triangles>maxtri[c])maxtri[c]=s.triangles;
        if(s.fill>maxfill[c]){maxfill[c]=s.fill;at[c]=sample;}
        if(s.fill<minfill[c])minfill[c]=s.fill;
        if(s.fill>90000 || s.triangles>1500){if(failures++<12)fprintf(stderr,"ceiling sample=%u chapter=%u triangles=%u fill=%u\n",sample,c,s.triangles,s.fill);}
        if(sample>=8*CV_BAR && sample<159*CV_BAR){
            unsigned distinct=0;for(int i=1;i<CV_W*CV_H;i++)if(frame.page[i]!=frame.page[0])distinct++;
            if(distinct<1000){fprintf(stderr,"flat sample=%u varying=%u\n",sample,distinct);failures++;}
        }
        if(frame.a!=0xabcdef01||frame.b!=0x12345678)return 2;
    }
    puts("chapter,frames,max_triangles,min_fill,max_fill,max_fill_sample");
    for(int i=0;i<10;i++)printf("%d,%u,%u,%u,%u,%u\n",i,n[i],maxtri[i],minfill[i],maxfill[i],at[i]);
    fprintf(stderr,"60 Hz full-run referee: %u failures\n",failures);
    return failures?1:0;
}
#endif
