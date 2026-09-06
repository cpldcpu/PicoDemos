#include "../body.h"
#include "../body_layout.h"
#include <stdio.h>
static uint16_t page[320*240];
int main(void){uint32_t s=67*CV_BAR+CV_BAR/2;demo_init();body_pose(s);r_begin(page,4,scene_camera(s,4));r_background(s);demo_stats_t prev={0},now;
for(unsigned i=0;i<sizeof(body_parts)/sizeof(body_parts[0]);i++){BodyPart p=body_parts[i];if(p.group!=0||p.mat==R_FLAT)continue;body_box(p,0);demo_stats(&now);printf("%u y=%.2f fill=%u\n",i,p.y,now.fill-prev.fill);prev=now;}
body_ring(0,17.4f,-1.05f,.78f,2,R_CHROME,0);demo_stats(&now);printf("front ring %u\n",now.fill-prev.fill);prev=now;
for(int i=0;i<4;i++){body_ring(0,17.4f,1.5f+i*1.6f,.78f,1,R_TEXTURE,0);demo_stats(&now);printf("rear ring %u\n",now.fill-prev.fill);prev=now;}return 0;}
