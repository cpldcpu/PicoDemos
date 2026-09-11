/* ONE SMALL THING -- an original 96-bar score by Phase.
 * D minor / F major, 150 BPM. Notes and rests are authored, never random.
 * The eight-bar question returns in new orchestration; the answer opens up
 * in the globe and flower. Close-position upper triads avoid muddy bass. */
#include "song.h"
#include <string.h>
typedef struct { uint8_t bar, step, note, length; } Tune;
static const Tune question[] = {
 {0,0,69,3},{0,4,74,5},{0,10,77,2},{0,12,76,2},{0,14,74,2},
 {1,0,72,3},{1,4,69,7},{1,14,65,2},
 {2,0,65,3},{2,4,70,5},{2,10,74,2},{2,12,72,2},{2,14,70,2},
 {3,0,69,3},{3,4,65,7},
 {4,0,69,3},{4,4,72,5},{4,10,77,3},{4,14,72,2},
 {5,0,69,3},{5,4,67,3},{5,8,65,6},
 {6,0,67,3},{6,4,72,5},{6,10,76,2},{6,12,74,2},{6,14,72,2},
 {7,0,67,3},{7,4,64,5},{7,12,69,3}
};
static const Tune answer[] = {
 {0,0,77,6},{0,8,76,2},{0,10,74,5},
 {1,0,72,3},{1,4,69,3},{1,8,74,6},
 {2,0,77,6},{2,8,74,3},{2,12,70,3},
 {3,0,69,3},{3,4,70,3},{3,8,74,6},
 {4,0,77,6},{4,8,79,3},{4,12,77,3},
 {5,0,76,3},{5,4,72,3},{5,8,69,6},
 {6,0,76,6},{6,8,74,3},{6,12,72,3},
 {7,0,67,6},{7,8,69,3},{7,12,72,3}
};
/* Dm, Bb, F, C: two bars each. F-major return rotates the progression and
 * the corresponding melody by four bars, preserving every voice/chord link. */
static const uint8_t roots[4]={38,34,41,36};
static const uint8_t chords[4][3]={{62,65,69},{62,65,70},{60,65,69},{60,64,67}};
void song_init(void){}
int song_section(unsigned bar){return bar>=96?6:(int)(bar/16);}
static Note note(unsigned n,unsigned len,unsigned vel){Note v={(uint8_t)n,(uint8_t)len,(uint8_t)vel};return v;}
void song_step(unsigned absolute_step,ScoreStep *o){
 memset(o,0,sizeof *o); unsigned bar=absolute_step/16,step=absolute_step%16;
 if(bar>=96)return;
 /* An actual cadence, not a fade halfway through the four-chord loop:
  * F -- Bb -- C -- Dm, with the final D exposed on the wooden mallet. */
 if(bar>=92){
   static const uint8_t cadence[4]={2,1,3,0},melody[4]={65,70,72,74};
   if(step==0){
     unsigned ch=cadence[bar-92];
     for(int i=0;i<3;i++)o->v[CHORD1+i]=note(chords[ch][i],15,58);
     o->v[MALLET]=note(melody[bar-92],14,88);
   }
   return;
 }
 unsigned phrase=(bar+(bar>=64&&bar<88?4:0))%8,ch=phrase/2;
 int full=(bar>=16&&bar<48)||(bar>=64&&bar<88), broken=bar>=48&&bar<64;
 if(bar>=8&&bar<92){
   if(step==0||(!broken&&(step==6||step==10||step==14)))
     o->v[BASS]=note(roots[ch]+(step==14&&bar%2?12:0),step==0?4:2,step==0?112:92);
 }
 if((step==0&&bar%2==0)||(full&&step==10)){
   for(int i=0;i<3;i++)o->v[CHORD1+i]=note(chords[ch][i],broken?24:step==0?10:4,broken?72:full?64:48);
 }
 const Tune *t=(bar>=32&&bar<48)||(bar>=72&&bar<88)?answer:question;
 unsigned count=t==answer?sizeof answer/sizeof *answer:sizeof question/sizeof *question;
 for(unsigned i=0;i<count;i++)if(t[i].bar==phrase&&t[i].step==step){
   if(bar<8||broken||bar>=88){
     if((bar<4&&step!=0&&step!=4)||(bar>=92&&step!=0))continue;
     o->v[MALLET]=note(t[i].note, t[i].length, bar>=92?58:92);
   }else o->v[BRASS]=note(t[i].note,t[i].length,104);
 }
 if(full){
   if(step==0||step==8||step==6||(step==14&&bar%2))o->kick=step==6?92:122;
   if(step==4||step==12)o->snare=112;
   if(step%2==0)o->hat=step%4==2?75:43;
   if(step%2&&bar>=32)o->hat=24+(step==15?17:0);
   if(step==11&&bar%2)o->rim=54;
   if(bar%8==7&&step==15)o->snare=48;
 }else if(bar>=8&&bar<16){if(step==0||step==8)o->kick=100;if(step==2||step==10)o->hat=55;}
 else if(broken&&bar>=56){if(step==0)o->kick=86;if(step==12)o->rim=75;if(step%4==2)o->hat=40;}
 else if(bar>=88&&bar<92){if(step==0)o->kick=90;}
}
unsigned song_accent(uint32_t sample){
 unsigned step=sample/STEP_SAMPLES;
 for(unsigned back=0;back<16&&back<=step;back++){
   ScoreStep s;song_step(step-back,&s);
   if(s.v[BRASS].note||s.v[MALLET].note)return (step-back)*STEP_SAMPLES;
 }
 return sample>STEP_SAMPLES*16?sample-STEP_SAMPLES*16:0;
}
