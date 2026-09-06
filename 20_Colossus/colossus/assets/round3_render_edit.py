from pathlib import Path
C=Path(__file__).resolve().parents[1];p=C/'render.c';s=p.read_text()
s=s.replace('static RCamera r_camera;','static RCamera r_camera;\nstatic unsigned r_env,r_dawn;\nstatic uint16_t r_chrome_palette[256];')
s=s.replace('glow_x0=80;','r_env=0;r_dawn=0;\n    glow_x0=80;')
a=s.index('void r_background(');b=s.index('RVertex r_transform',a)
s=s[:a]+'''static uint16_t mix_color(uint16_t a,uint16_t b,unsigned t)
{
    unsigned u=255-t;
    return cv_rgb(((a&31)*u+(b&31)*t)*8/255,(((a>>6)&31)*u+((b>>6)&31)*t)*8/255,(((a>>11)&31)*u+((b>>11)&31)*t)*8/255);
}
void r_environment(unsigned mode){r_env=mode;}
static float contact(float wx,float wz,float x,float z,float rx,float rz)
{
    float dx=(wx-x)/rx,dz=(wz-z)/rz;
    return fmaxf(0,1-dx*dx-dz*dz);
}
void r_background(uint32_t sample)
{
    float dawn=fminf(1,fmaxf(0,((float)sample/CV_BAR-128)/7));
    r_dawn=(unsigned)(255*dawn);
    /* Both maps use the dusk normal-index topology; dawn is a palette morph. */
    for(int i=0;i<256;i++)r_chrome_palette[i]=mix_color(dusk_matcap_palette[i],dawn_matcap_palette[i],r_dawn);
    unsigned chapter=r_stats.chapter;
    for(int y=0;y<CV_H;y++){
        int ty=clamp(y*64/184,0,63);
        for(int x=0;x<CV_W;x++){
            int idx=dusk_sky_pixels[ty*256+x*256/320];
            uint16_t c=mix_color(dusk_sky_palette[idx],dawn_sky_palette[idx],r_dawn);
            /* Inside chambers retain a dark environmental field, no false floor. */
            if(chapter==3||chapter==5 || (chapter==6 && sample<98*CV_BAR))
                c=mix_color(c,cv_rgb(16,24,32),210);
            r_page[y*CV_W+x]=c;
        }
    }
    if(chapter!=1 && chapter!=8 && chapter!=9)return;
    /* Level world plane: per-row reciprocal projection; x stepping is exact.
       Shallow fog converges to the same painted sky row above the horizon. */
    int horizon=184;
    float c=cosf(r_camera.yaw),sn=sinf(r_camera.yaw);
    for(int y=horizon;y<CV_H;y++){
        float z=r_camera.cy*r_camera.focal/(y-120.f),v=z*4;
        unsigned haze=(unsigned)(210*fmaxf(0,1-(y-horizon)/20.f));
        for(int x=0;x<CV_W;x+=8){
            float xx=(x-160)*z/r_camera.focal,dx=z/r_camera.focal;
            for(int j=0;j<8;j++,xx+=dx){
                float wx=r_camera.cx+xx*c-z*sn,wz=r_camera.cz+xx*sn+z*c;
                int tex=stone_pixels[((int)v&63)*64+((int)(wx*4)&63)];
                uint16_t col=stone_palette[tex];
                float shadow=.32f*contact(wx,wz,0,0,5,2.5f);
                shadow=fmaxf(shadow,.86f*contact(wx,wz,-1.6f,-.85f,1.75f,1.8f));
                shadow=fmaxf(shadow,.86f*contact(wx,wz,1.6f,.3f,1.65f,1.55f));
                col=mix_color(col,cv_rgb(8,16,24),(unsigned)(255*shadow));
                int idx=dusk_sky_pixels[63*256+(x+j)*256/320];
                uint16_t fog=mix_color(dusk_sky_palette[idx],dawn_sky_palette[idx],r_dawn);
                r_page[y*320+x+j]=mix_color(col,fog,haze);
            }
        }
    }
}
'''+s[b:]
s=s.replace('SPAN_LOOP(dusk_matcap_palette[dusk_matcap_pixels[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]])','SPAN_LOOP(r_env==2?warm_environment_palette[warm_environment_pixels[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]]:r_chrome_palette[dusk_matcap_pixels[clamp(s.v>>16,0,63)*64+clamp(s.u>>16,0,63)]])')
s=s.replace('r_shades[3][(diagnostic_tile[((s.v>>16)&63)*64+((s.u>>16)&63)]*clamp(s.l>>16,0,255))>>8]','r_shades[1][(bronze_wear_pixels[((s.v>>16)&63)*64+((s.u>>16)&63)]*(100+clamp(s.l>>16,0,255)*155/255))>>8]')
# Correct emissive color, which formerly seeded orange bloom from a black surface.
s=s.replace('if(s.e>0)glow_seed(off,s.e>>16);','if(s.e>0){int e=clamp(s.e>>16,0,255);r_page[off]=cv_rgb(130+e/2,50+e/3,24+e/8);glow_seed(off,e);}')
a=s.index('void r_embers(');b=s.index('void r_inscription(',a)
s=s[:a]+'''void r_embers(uint32_t sample,unsigned count)
{
    if(count>256)count=256;
    r_stats.particles=count;
    for(unsigned i=0;i<count;i++){
        uint32_t life=CV_RATE*(5+i%6),t=sample+hash(i)%life,epoch=t/life,age=t%life,h=hash(i+epoch*1021);
        float f=(float)age/life;
        int x=(int)(h%320)+(int)(sinf(f*5+i)*12),y=235-(int)(f*(85+(h%55))),q=16+(int)((h>>16)%220);
        unsigned cls=i%3;int size=cls==2?3:cls==1?2:1;
        for(int yy=0;yy<size;yy++)for(int xx=0;xx<size;xx++){
            int px=x+xx,py=y+yy;
            if((unsigned)px>=320||(unsigned)py>=240||q<=r_depth[py][px])continue;
            int sx=xx*16/size,sy=yy*16/size;
            int idx=cls*256+sy*16+sx;unsigned packed=ember_stamps_pixels[idx/2];
            int stamp=(idx&1)?packed&15:packed>>4;
            int e=(int)((70+cls*70)*sinf(f*3.14159265f))*stamp/15;
            if(e<12)continue;
            r_page[py*320+px]=cv_rgb(50+e,28+e*2/3,16+e/4);
        }
    }
}
/* A common foreground rib pair at each substitution, under local lit dust.
   Stateless screen-space occluder; only one chapter is ever rendered. */
void r_transition(uint32_t sample)
{
    static const unsigned boundaries[]={8,24,40,56,72,88,112,128,144};
    for(unsigned i=0;i<sizeof boundaries/sizeof boundaries[0];i++){
        float dt=((float)sample-boundaries[i]*CV_BAR)/CV_RATE;
        if(fabsf(dt)>1.5f)continue;
        float t=1-fabsf(dt)/1.5f;t=t*t*(3-2*t);
        if(i==8)t*=.35f; /* The coda holds the exact reveal camera/world. */
        float z=r_camera.near_z*1.1f;
        for(int rib=0;rib<2;rib++){
            float x= rib?232:64,w=18*t;
            RVertex a={(x-160)*z/r_camera.focal,120*z/r_camera.focal,z,130,0,0,0};
            RVertex b=a,c=a,d=a;b.x+=(w*z/r_camera.focal);c.x=b.x;c.y=-120*z/r_camera.focal;d.y=c.y;
            a.u=d.u=0;b.u=c.u=63;a.v=b.v=0;c.v=d.v=63;
            r_triangle(a,b,c,R_TEXTURE);r_triangle(a,c,d,R_TEXTURE);
        }
        for(int y=0;y<240;y++)for(int x=0;x<320;x++){
            float band=fmaxf(0,1-fabsf(x-150.f-(y-120)*.35f)/195.f);
            unsigned noise=hash((unsigned)(x/3)+(unsigned)(y/3)*107)&31;
            unsigned opacity=(unsigned)(t*band*(205+noise));
            r_page[y*320+x]=mix_color(r_page[y*320+x],cv_rgb(120+noise,112+noise/2,96),opacity);
        }
        break;
    }
}
'''+s[b:]
s=s.replace('    for(;*text;text++,x+=8){unsigned ch=(unsigned char)*text;','''    int advance=strlen(text)>38?6:8;
    for(;*text;text++,x+=advance){unsigned ch=(unsigned char)*text;
        if(ch=='~')ch='.';''')
s=s.replace('for(int xx=0;xx<8;xx++){\n            int bit=(oy+yy)*128+ox+xx;','for(int xx=0;xx<advance;xx++){\n            int bit=(oy+yy)*128+ox+(advance==6?1:0)+xx;')
a=s.index('    if(chapter==2){',s.index('void demo_render'));b=s.index('    if(sample>CV_TOTAL',a)
s=s[:a]+'''    r_begin(page,chapter,scene_camera(sample,chapter));r_background(sample);
    if(chapter==2)scene_hand(sample);else scene_body(sample,chapter);
    unsigned energy=(unsigned)song_energy(cv_bar_of(sample));
    unsigned count=64+energy/4;
    if(chapter==5)count=96+energy/8;
    if(chapter==9)count=64;
    r_embers(sample,count);r_bloom();r_transition(sample);scene_titles(sample,chapter);
'''+s[b:]
s=s.replace('void r_finish(void){}','''void r_end_inscription(int top)
{
    for(int y=0;y<32;y++)for(int x=0;x<320;x++){
        int i=y*320+x;unsigned v=end_inscription_pixels[i/2],idx=(i&1)?v&15:v>>4;
        if(idx && (unsigned)(top+y)<240)r_page[(top+y)*320+x]=end_inscription_palette[idx];
    }
}
void r_finish(void){}''')
p.write_text(s)
# Reserve before allocation; renderer rows only.
p=C/'LEDGER.md';s=p.read_text().replace('| `r_shades[4][256]` | 2048','| `r_shades[4][256]` + dawn chrome palette `[256]` | 2560').replace('| Renderer context, stats, bounds, alignment (ceiling) |','| Renderer context, stats, bounds, alignment (ceiling; includes environment, dawn and curl scalars) |');p.write_text(s)
