from pathlib import Path
C=Path(__file__).resolve().parents[1]
p=C/'render.h';s=p.read_text().replace('void r_transition(uint32_t sample);','void r_transition(uint32_t sample);\nvoid r_portal(float x,float y,float z,float radius);\nvoid r_portal_reset(void);');p.write_text(s)
p=C/'render.c';s=p.read_text().replace('static unsigned r_env,r_dawn;','static unsigned r_env,r_dawn;\nstatic int clip_x0,clip_y0,clip_x1,clip_y1;');s=s.replace('r_env=0;r_dawn=0;','r_env=0;r_dawn=0;r_portal_reset();');s=s.replace('int ty=clamp(y*64/184,0,63);','int ty=clamp(y*64/((chapter==1||chapter==8||chapter==9)?184:240),0,63);');s=s.replace('            r_page[y*CV_W+x]=c;','''            if(chapter==0)c=mix_color(c,0,255-(unsigned)(75*fminf(1,(float)sample/(CV_BAR*7))));
            if(chapter==4){
                float z=-1.05f-r_camera.cz,rad=z>.05f?.49f*r_camera.focal/z:1000;
                if((x-160.f)*(x-160.f)+(y-120.f)*(y-120.f)<rad*rad)c=mix_color(c,cv_rgb(8,16,24),235);
            }
            r_page[y*CV_W+x]=c;''');s=s.replace('clamp((int)ceilf(a.y-.5f),0,240),ye=clamp((int)ceilf(c.y-.5f),0,240)','clamp((int)ceilf(a.y-.5f),clip_y0,clip_y1),ye=clamp((int)ceilf(c.y-.5f),clip_y0,clip_y1)').replace('clamp((int)ceilf(x0-.5f),0,320),xe=clamp((int)ceilf(x1-.5f),0,320)','clamp((int)ceilf(x0-.5f),clip_x0,clip_x1),xe=clamp((int)ceilf(x1-.5f),clip_x0,clip_x1)');s=s.replace('/* Fixed-point increments;', '''void r_portal_reset(void){clip_x0=clip_y0=0;clip_x1=320;clip_y1=240;}
void r_portal(float x,float y,float z,float radius)
{
    RVertex v=r_transform(x,y,z,0,0,-1,0,0);
    if(v.z<r_camera.near_z)return;
    Screen p=project(v);float r=radius*r_camera.focal/v.z;
    clip_x0=clamp((int)floorf(p.x-r),clip_x0,clip_x1);
    clip_x1=clamp((int)ceilf(p.x+r),clip_x0,clip_x1);
    clip_y0=clamp((int)floorf(p.y-r),clip_y0,clip_y1);
    clip_y1=clamp((int)ceilf(p.y+r),clip_y0,clip_y1);
}
/* Fixed-point increments;''');p.write_text(s)
p=C/'body.c';s=p.read_text().replace('g>=3 && p.y<(g==3?5.3f:7.9f)','g>=3 && (p.y<(g==3?5.3f:7.9f) || fabsf(p.x)<2.3f)');p.write_text(s)
p=C/'scene_chapters.c';s=p.read_text().replace('    for(int i=0;i<4;i++)body_ring(0,17.4f,1.5f+i*1.6f,.78f,1,R_TEXTURE,0);\n    body_ring(0,17.4f,-1.05f,.78f,2,R_CHROME,0);','''    body_ring(0,17.4f,-1.05f,.78f,2,R_CHROME,0);
    r_portal(0,17.4f,-1.05f,.78f*.62f);
    for(int i=0;i<4;i++){
        float z=1.5f+i*1.6f;
        body_ring(0,17.4f,z,.78f,1,R_TEXTURE,0);
        r_portal(0,17.4f,z,.78f*.62f);
    }
    r_portal_reset();''').replace('    body_draw(-1,0);\n    for(int i=0;i<9;i++)','    if(bars>=96)body_draw(-1,0);\n    for(int i=0;i<9;i++)').replace('-4.2f,4.9f+1.8f*t,-8,-.10f+.12f*t','-6.5f+.5f*t,5.1f+1.6f*t,-8,-.28f+.08f*t');p.write_text(s)
