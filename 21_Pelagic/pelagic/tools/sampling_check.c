#include "pelagic.h"
#include "sampling.h"
#include <stdio.h>
#include <stdlib.h>
#define REQUIRE(c) do{if(!(c)){fprintf(stderr,"sampling check failed at line %d\n",__LINE__);return 1;}}while(0)
int main(void){
    texture_init();
    const int starts[][4]={{0,0,65536,32768},{383*65536,255*65536,-17003,-23004},{-1,-65537,7919,-997},{2147400000,-2147400000,12345,-45678}};
    for(unsigned k=0;k<sizeof starts/sizeof starts[0];k++){
        uint32_t u=(uint32_t)starts[k][0],v=(uint32_t)starts[k][1];
        texture_span(starts[k][0],starts[k][1],starts[k][2],starts[k][3]);
        for(int n=0;n<10000;n++){
            REQUIRE(texture_u()==u&&texture_v()==v);
            REQUIRE(texture_pop()==((u>>16)|(v&0xffff0000u)));
            u+=(uint32_t)starts[k][2];v+=(uint32_t)starts[k][3];
        }
    }
    uint16_t solid[4]={rgb(96,160,224),rgb(96,160,224),rgb(96,160,224),rgb(96,160,224)};
    for(int u=0;u<65536;u+=257)for(int v=0;v<65536;v+=257)REQUIRE(filtered_color(solid,2,2,u,v)==solid[0]);
    uint16_t corners[4]={rgb(0,0,0),rgb(248,0,0),rgb(0,248,0),rgb(0,0,248)};
    REQUIRE(filtered_color(corners,2,2,-65536,-65536)==corners[0]);
    REQUIRE(filtered_color(corners,2,2,999999,999999)==corners[3]);
    REQUIRE(filtered_color(corners,2,2,32768,32768)==rgb(64,64,64));
    uint16_t rgba[4]={rgb(248,0,0),rgb(0,0,248),rgb(248,0,0),rgb(0,0,248)};
    uint8_t alpha[4]={0,255,0,255};
    uint16_t c=filtered_rgba(rgba,alpha,2,2,0,0,128,128,0,32);
    REQUIRE(red(c)==0&&green(c)==0&&blue(c)==120); /* no invisible-red fringe */
    REQUIRE(filtered_rgba(rgba,alpha,2,2,0,0,0,0,solid[0],32)==solid[0]);
    REQUIRE(filtered_rgba(rgba,alpha,2,2,1,1,255,255,solid[0],0)==solid[0]);
    puts("PASS coordinate POP wrap/negative steps, constant filtering, clamped edges, colour midpoint, premultiplied alpha");
    return 0;
}
