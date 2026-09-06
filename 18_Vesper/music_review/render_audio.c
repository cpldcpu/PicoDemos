/* Standalone renderer for review copies only. No dependency on video code. */
#include "vesper.h"
#include <stdio.h>
#include <stdlib.h>
static void u16(FILE *f,uint16_t n){fputc(n&255,f);fputc(n>>8,f);}
static void u32(FILE *f,uint32_t n){u16(f,(uint16_t)n);u16(f,(uint16_t)(n>>16));}
int main(int argc,char **argv){
    if(argc!=2){fprintf(stderr,"usage: render_audio output.wav\n");return 2;}
    FILE *f=fopen(argv[1],"wb");if(!f){perror(argv[1]);return 1;}
    fwrite("RIFF",1,4,f);u32(f,DURATION_SAMPLES*4+36);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,1);u16(f,2);u32(f,SAMPLE_RATE);u32(f,SAMPLE_RATE*4);u16(f,4);u16(f,16);
    fwrite("data",1,4,f);u32(f,DURATION_SAMPLES*4);
    synth_init();int16_t buf[1994];
    while(synth_position()<DURATION_SAMPLES){unsigned n=DURATION_SAMPLES-synth_position();if(n>997)n=997;synth_render(buf,n);for(unsigned i=0;i<n*2;i++)u16(f,(uint16_t)buf[i]);}
    if(ferror(f)){fclose(f);return 1;}return fclose(f)!=0;
}
