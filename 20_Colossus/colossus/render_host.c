/* Phase's temporary PPM dumper; superseded by Overscan's tools/capture.c. */
#ifdef HOST_BUILD
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t page[CV_W*CV_H],reference[CV_W*CV_H];
int main(int argc,char **argv){
    uint32_t sample=argc>1?(uint32_t)strtoul(argv[1],0,0):26*CV_BAR;demo_init();
    if(argc>3 && !strcmp(argv[3],"stress"))render_material_test(page,sample);else demo_render(page,sample);
    memcpy(reference,page,sizeof page);demo_render(page,97*CV_BAR);
    if(argc>3 && !strcmp(argv[3],"stress"))render_material_test(page,sample);else demo_render(page,sample);
    if(memcmp(page,reference,sizeof page)){fprintf(stderr,"seek mismatch\n");return 2;}
    FILE *f=fopen(argc>2?argv[2]:"hand.ppm","wb");if(!f)return 1;fprintf(f,"P6\n320 240\n255\n");
    for(int i=0;i<CV_W*CV_H;i++){uint16_t p=page[i];unsigned char rgb[3]={(p&31)*255/31,((p>>6)&31)*255/31,((p>>11)&31)*255/31};fwrite(rgb,1,3,f);}if(fclose(f))return 1;
    demo_stats_t s;demo_stats(&s);fprintf(stderr,"sample=%u triangles=%u fill=%u embers=%u chapter=%u seek=identical\n",sample,s.triangles,s.fill,s.particles,s.chapter);return 0;
}
#endif
