#include "body.h"
#include "song.h"
void scene_hand(uint32_t sample)
{
    body_pose(sample);body_draw(3,2);
    /* Narrow intended emissive bearing, behind the open hand. */
    float kick=(song_drums(cv_step_of(sample))&DR_KICK)?1.f-(float)(sample%CV_STEP)/CV_STEP:0;
    body_ring(-4.2f,7.05f,-.8f,.24f,2,R_FLAT,45+35*kick);
}
/* Framing scaffolds only; the hand is the first finished chapter. */
void scene_body(uint32_t sample,int chapter)
{
    body_pose(sample);
    if(chapter==0){r_wordmark(88,255);return;}
    if(chapter==1){body_draw(1,0);return;}
    body_draw(-1,chapter>=8?0:1);
}
