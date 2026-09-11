/* A reviewable piano roll from the same immutable table used on the board. */
#include "song.h"
#include <stdio.h>
int main(void){
 puts("step,voice,note,length,velocity");
 for(unsigned step=0;step<SONG_BARS*16;step++){
   ScoreStep s;song_step(step,&s);
   for(int v=0;v<VOICES;v++)if(s.v[v].note)printf("%u,%d,%u,%u,%u\n",step,v,s.v[v].note,s.v[v].steps,s.v[v].velocity);
 }
 return 0;
}
