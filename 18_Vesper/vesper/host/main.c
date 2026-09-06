#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "vesper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
static uint16_t pixels[WIDTH*HEIGHT];
static uint8_t rgb24[WIDTH*HEIGHT*3];
static SDL_atomic_t played;
static void callback(void*unused,Uint8*stream,int len){(void)unused;synth_render((int16_t*)stream,(unsigned)len/4);SDL_AtomicSet(&played,(int)synth_position());}
static void convert(void){for(int i=0;i<WIDTH*HEIGHT;i++){rgb24[i*3]=(uint8_t)red(pixels[i]);rgb24[i*3+1]=(uint8_t)green(pixels[i]);rgb24[i*3+2]=(uint8_t)blue(pixels[i]);}}
static void wav_header(FILE*f,uint32_t frames){uint32_t size=frames*4,riff=size+36,rate=SAMPLE_RATE,br=rate*4;uint16_t one=1,two=2,align=4,bits=16;fwrite("RIFF",1,4,f);fwrite(&riff,4,1,f);fwrite("WAVEfmt ",1,8,f);uint32_t n=16;fwrite(&n,4,1,f);fwrite(&one,2,1,f);fwrite(&two,2,1,f);fwrite(&rate,4,1,f);fwrite(&br,4,1,f);fwrite(&align,2,1,f);fwrite(&bits,2,1,f);fwrite("data",1,4,f);fwrite(&size,4,1,f);}
int main(int argc,char**argv){
    int raw=0,headless=0,mute=0,frames=-1,fps=60;float start=0;const char*shot=NULL,*wav=NULL;
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--raw")){raw=1;headless=1;}
        else if(!strcmp(argv[i],"--headless"))headless=1;
        else if(!strcmp(argv[i],"--mute"))mute=1;
        else if(!strcmp(argv[i],"--start")&&i+1<argc)start=(float)atof(argv[++i]);
        else if(!strcmp(argv[i],"--frames")&&i+1<argc)frames=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--fps")&&i+1<argc)fps=atoi(argv[++i]);
        else if(!strcmp(argv[i],"--shot")&&i+1<argc){shot=argv[++i];headless=1;frames=1;}
        else if(!strcmp(argv[i],"--wav")&&i+1<argc){wav=argv[++i];headless=1;}
        else{fprintf(stderr,"VESPER: --start seconds --shot file.ppm --wav file.wav --raw --frames N --fps N --headless --mute\n");return 2;}
    }
    if(start<0||start>=DURATION_SECONDS||fps<1||fps>240||frames< -1){fprintf(stderr,"Invalid time, frame count or frame rate\n");return 2;}
    demo_init();synth_init();
    if(wav){FILE*f=fopen(wav,"wb");if(!f){perror(wav);return 1;}wav_header(f,DURATION_SAMPLES);int16_t b[1024];for(unsigned p=0;p<DURATION_SAMPLES;){unsigned n=DURATION_SAMPLES-p;if(n>512)n=512;synth_render(b,n);if(fwrite(b,4,n,f)!=n)return 1;p+=n;}return fclose(f)!=0;}
#ifdef _WIN32
    if(raw)_setmode(_fileno(stdout),_O_BINARY);
#endif
    uint32_t start_sample=(uint32_t)(start*SAMPLE_RATE);
    if(frames<0)frames=headless?(int)((DURATION_SECONDS-start)*fps):INT_MAX;
    SDL_Window*w=NULL;SDL_Renderer*r=NULL;SDL_Texture*texture=NULL;SDL_AudioDeviceID device=0;
    if(!headless){
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER)){fprintf(stderr,"SDL: %s\n",SDL_GetError());return 1;}
        w=SDL_CreateWindow("VESPER / LATENT / 2026",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,960,720,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
        r=SDL_CreateRenderer(w,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!r)r=SDL_CreateRenderer(w,-1,SDL_RENDERER_SOFTWARE);
        if(!w||!r){fprintf(stderr,"Video: %s\n",SDL_GetError());return 1;}
        SDL_RenderSetLogicalSize(r,WIDTH,HEIGHT);SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"nearest");
        texture=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGB24,SDL_TEXTUREACCESS_STREAMING,WIDTH,HEIGHT);
        if(!texture){fprintf(stderr,"Texture: %s\n",SDL_GetError());return 1;}
        if(!mute){SDL_AudioSpec spec={0};spec.freq=SAMPLE_RATE;spec.format=AUDIO_S16SYS;spec.channels=2;spec.samples=512;spec.callback=callback;device=SDL_OpenAudioDevice(NULL,0,&spec,NULL,0);if(!device)fprintf(stderr,"Audio: %s (using wall clock)\n",SDL_GetError());}
        synth_seek(start_sample);SDL_AtomicSet(&played,(int)start_sample);if(device)SDL_PauseAudioDevice(device,0);
    }
    uint64_t epoch=SDL_GetPerformanceCounter(),freq=SDL_GetPerformanceFrequency(),total_us=0,max_us=0;unsigned max_tri=0;int rendered=0,quit=0,paused=0;
    uint32_t held=start_sample;
    while(rendered<frames&&!quit){
        uint32_t sample=start_sample+(uint32_t)((uint64_t)rendered*SAMPLE_RATE/fps);
        if(!headless){
            SDL_Event e;while(SDL_PollEvent(&e)){
                if(e.type==SDL_QUIT)quit=1;
                if(e.type==SDL_KEYDOWN){SDL_Keycode k=e.key.keysym.sym;if(k==SDLK_ESCAPE)quit=1;
                    if(k==SDLK_f)SDL_SetWindowFullscreen(w,(SDL_GetWindowFlags(w)&SDL_WINDOW_FULLSCREEN_DESKTOP)?0:SDL_WINDOW_FULLSCREEN_DESKTOP);
                    if(k==SDLK_SPACE){paused=!paused;if(device)SDL_PauseAudioDevice(device,paused);if(!paused){start_sample=held;epoch=SDL_GetPerformanceCounter();}}
                    if(k==SDLK_r||k==SDLK_RIGHT||k==SDLK_LEFT){
                        uint32_t target=k==SDLK_r?0:(uint32_t)clampi((int)held+(k==SDLK_RIGHT?15:-15)*SAMPLE_RATE,0,(DURATION_SECONDS-1)*SAMPLE_RATE);
                        if(device)SDL_LockAudioDevice(device);synth_seek(target);SDL_AtomicSet(&played,(int)target);if(device)SDL_UnlockAudioDevice(device);
                        start_sample=held=target;epoch=SDL_GetPerformanceCounter();rendered=0;
                    }
                }
            }
            if(paused){SDL_Delay(10);continue;}
            sample=device?(uint32_t)SDL_AtomicGet(&played):start_sample+(uint32_t)((SDL_GetPerformanceCounter()-epoch)*SAMPLE_RATE/freq);
            held=sample;
        }
        if(sample>=DURATION_SAMPLES)break;
        uint64_t t=SDL_GetPerformanceCounter();demo_render(pixels,sample);uint64_t us=(SDL_GetPerformanceCounter()-t)*1000000/freq;
        total_us+=us;if(us>max_us)max_us=us;if(demo_triangles()>max_tri)max_tri=demo_triangles();
        if(raw||shot||!headless)convert();
        if(raw&&fwrite(rgb24,1,sizeof rgb24,stdout)!=sizeof rgb24){perror("raw output");return 1;}
        if(shot){FILE*f=fopen(shot,"wb");if(!f){perror(shot);return 1;}fprintf(f,"P6\n%d %d\n255\n",WIDTH,HEIGHT);fwrite(rgb24,1,sizeof rgb24,f);fclose(f);}
        if(!headless){SDL_UpdateTexture(texture,NULL,rgb24,WIDTH*3);SDL_RenderClear(r);SDL_RenderCopy(r,texture,NULL,NULL);SDL_RenderPresent(r);}
        rendered++;
    }
    fprintf(stderr,"VESPER: %d frames, render mean %.2f ms, worst %.2f ms, max %u triangles (HOST measurements)\n",rendered,rendered?(double)total_us/rendered/1000:0,(double)max_us/1000,max_tri);
    if(device)SDL_CloseAudioDevice(device);if(texture)SDL_DestroyTexture(texture);if(r)SDL_DestroyRenderer(r);if(w)SDL_DestroyWindow(w);SDL_Quit();return 0;
}
