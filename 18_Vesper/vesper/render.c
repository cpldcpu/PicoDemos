/* VESPER / LATENT. Code and direction: Phase (GPT-6 Astra).
 * Solid geometry, 8-bit reciprocal depth, Gouraud materials.
 * No recorded frames, external textures, GPU, or per-pixel ray marching.
 * Work scales with visible spans. Every triangle is clipped at the near plane.
 */
#include "vesper.h"
#include "font8x8.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#define PI 3.14159265358979323846f
#define TOP 22
#define BOTTOM 218
typedef struct {float x,y,z;} V3;
typedef struct {float x,y,z,l;} Vertex;
typedef struct {float x,y,q,l;} Screen;
static uint16_t *fb;
static uint8_t depth[WIDTH*HEIGHT];
static uint16_t material[6][256];
static uint8_t glow[60][80], blur[60][80];
static float sine[2048];
static Vertex mesh[81][13];
static unsigned triangles;
static float time_s, rot_x,rot_y,rot_z,centre_y,cam_z;
static Score score;
static int box_view;
static int cull;
static float sn(float a){return sine[(int)(a*(2048.f/(2*PI)))&2047];}
static float cs(float a){return sn(a+PI*.5f);}
static float mix(float a,float b,float t){return a+(b-a)*t;}
static float sat(float a){return a<0?0:a>1?1:a;}
static float smooth(float a){a=sat(a);return a*a*(3-2*a);}
static V3 vec(float x,float y,float z){return (V3){x,y,z};}
static V3 add(V3 a,V3 b){return vec(a.x+b.x,a.y+b.y,a.z+b.z);}
static V3 sub(V3 a,V3 b){return vec(a.x-b.x,a.y-b.y,a.z-b.z);}
static V3 mul(V3 a,float s){return vec(a.x*s,a.y*s,a.z*s);}
static V3 cross(V3 a,V3 b){return vec(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static V3 norm(V3 a){float r=1.f/sqrtf(a.x*a.x+a.y*a.y+a.z*a.z+1e-12f);return mul(a,r);}
static V3 rotate(V3 p){
    float c=cs(rot_x),s=sn(rot_x),v=p.y*c-p.z*s;p.z=p.y*s+p.z*c;p.y=v;
    c=cs(rot_y);s=sn(rot_y);v=p.x*c+p.z*s;p.z=-p.x*s+p.z*c;p.x=v;
    c=cs(rot_z);s=sn(rot_z);v=p.x*c-p.y*s;p.y=p.x*s+p.y*c;p.x=v;
    return p;
}
static Vertex vertex(V3 p,float l){return (Vertex){p.x,p.y,p.z,l};}
static void pixel(int x,int y,uint16_t c){if((unsigned)x<WIDTH&&y>=TOP&&y<BOTTOM)fb[y*WIDTH+x]=c;}
static void line(int x,int y,int x2,int y2,uint16_t c){
    int dx=abs(x2-x),sx=x<x2?1:-1,dy=-abs(y2-y),sy=y<y2?1:-1,e=dx+dy;
    int limit=1400;while(limit--){pixel(x,y,c);if(x==x2&&y==y2)break;int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}
}
static void HOT(raster)(Screen a,Screen b,Screen c,int mat){
    if(a.y>b.y){Screen t=a;a=b;b=t;}if(b.y>c.y){Screen t=b;b=c;c=t;}if(a.y>b.y){Screen t=a;a=b;b=t;}
    float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
    if(fabsf(area)<.05f||c.y<TOP||a.y>=BOTTOM)return;
    float inv=1.f/area;
    float qx=((b.q-a.q)*(c.y-a.y)-(c.q-a.q)*(b.y-a.y))*inv;
    float qy=((b.x-a.x)*(c.q-a.q)-(c.x-a.x)*(b.q-a.q))*inv;
    float lx=((b.l-a.l)*(c.y-a.y)-(c.l-a.l)*(b.y-a.y))*inv;
    float ly=((b.x-a.x)*(c.l-a.l)-(c.x-a.x)*(b.l-a.l))*inv;
    int dq=(int)(qx*65536),dl=(int)(lx*65536);
    int ys=clampi((int)ceilf(a.y-.5f),TOP,BOTTOM),ye=clampi((int)ceilf(c.y-.5f),TOP,BOTTOM);
    float long_s=(c.x-a.x)/(c.y-a.y+1e-9f);
    float upper=(b.x-a.x)/(b.y-a.y+1e-9f),lower=(c.x-b.x)/(c.y-b.y+1e-9f);
    const uint16_t *pal=material[mat];
    for(int y=ys;y<ye;y++){
        float fy=y+.5f, x0=a.x+(fy-a.y)*long_s;
        float x1=fy<b.y?a.x+(fy-a.y)*upper:b.x+(fy-b.y)*lower;
        if(x0>x1){float t=x0;x0=x1;x1=t;}
        int xs=clampi((int)ceilf(x0-.5f),0,WIDTH),xe=clampi((int)ceilf(x1-.5f),0,WIDTH);
        if(xs>=xe)continue;
        int q=(int)((a.q+qx*(xs+.5f-a.x)+qy*(fy-a.y))*65536);
        int l=(int)((a.l+lx*(xs+.5f-a.x)+ly*(fy-a.y))*65536);
        int off=y*WIDTH+xs;
        for(int x=xs;x<xe;x++,off++,q+=dq,l+=dl){
            int z=q>>16;if(z>=depth[off]){depth[off]=(uint8_t)clampi(z,0,255);fb[off]=pal[clampi(l>>16,0,255)];}
        }
    }
    triangles++;
}
static Screen project(Vertex v){float iz=1.f/v.z;return (Screen){160+v.x*190*iz,centre_y-v.y*190*iz,384*iz,v.l};}
static void tri(Vertex a,Vertex b,Vertex c,int mat){
    Vertex in[4]={a,b,c},out[5];int n=0;
    for(int i=0;i<3;i++){
        Vertex p=in[i],q=in[(i+1)%3];int ip=p.z>=1.6f,iq=q.z>=1.6f;
        if(ip)out[n++]=p;
        if(ip!=iq){float t=(1.6f-p.z)/(q.z-p.z);out[n++]=(Vertex){mix(p.x,q.x,t),mix(p.y,q.y,t),1.6f,mix(p.l,q.l,t)};}
    }
    for(int i=1;i<n-1;i++){
        Screen p=project(out[0]),q=project(out[i]),r=project(out[i+1]);
        if(cull&&(q.x-p.x)*(r.y-p.y)-(r.x-p.x)*(q.y-p.y)<=0)continue;
        raster(p,q,r,mat);
    }
}
static void quad(Vertex a,Vertex b,Vertex c,Vertex d,int m){tri(a,b,c,m);tri(a,c,d,m);}
static float light(V3 n){
    /* Two long rectangular softboxes and a grazing edge: analytic matcap. */
    float strip=sat(1-fabsf(n.x*.55f+n.y*.83f-.32f)*5.f);
    float strip2=sat(1-fabsf(n.y-.72f)*12.f);
    float rim=1-fabsf(n.z);rim*=rim;
    return 25+105*sat(n.y*.6f-n.x*.4f+n.z*.25f)+145*strip*strip+130*strip2+70*rim;
}
static void setup_object(float x,float y,float z,float distance){rot_x=x;rot_y=y;rot_z=z;cam_z=distance;}
static Vertex object_vertex(V3 p,V3 n){p=rotate(p);p.z+=cam_z;return vertex(p,light(rotate(n)));}
static void knot(float morph,float radius,int mat){
    enum{U=80,V=12};
    for(int i=0;i<=U;i++){
        float u=i*(2*PI/U);
        float rr=1.36f+.46f*cs(3*u)*morph;
        V3 p=vec(rr*cs(2*u),rr*sn(2*u),.68f*sn(3*u)*morph);
        /* Ring becomes a trefoil without changing mesh topology. */
        float w=morph; p.x=mix(1.65f*cs(u),p.x,w);p.y=mix(1.65f*sn(u),p.y,w);
        float du=.005f,uu=u+du,rr2=1.36f+.46f*cs(3*uu)*morph;
        V3 p2=vec(mix(1.65f*cs(uu),rr2*cs(2*uu),w),mix(1.65f*sn(uu),rr2*sn(2*uu),w),.68f*sn(3*uu)*morph);
        V3 tangent=norm(sub(p2,p)),normal=norm(cross(tangent,vec(0,0,1))),binormal=norm(cross(normal,tangent));
        for(int j=0;j<=V;j++){
            float v=j*(2*PI/V);V3 n=add(mul(normal,cs(v)),mul(binormal,sn(v)));
            mesh[i][j]=object_vertex(add(p,mul(n,radius)),n);
        }
    }
    cull=1;
    for(int i=0;i<U;i++)for(int j=0;j<V;j++){
        int band=score.chapter==2&&((i+(int)(score.beat*3))%20<2);
        quad(mesh[i][j],mesh[i+1][j],mesh[i+1][j+1],mesh[i][j+1],band?0:mat);
    }
    cull=0;
}
static void box(V3 p,V3 size,int mat,float lum){
    Vertex v[8];for(int i=0;i<8;i++){V3 a=vec(p.x+((i&1)?size.x:-size.x),p.y+((i&2)?size.y:-size.y),p.z+((i&4)?size.z:-size.z));if(box_view){a=rotate(a);a.z+=cam_z;a.y+=.5f;}v[i]=vertex(a,lum);}
    static const uint8_t faces[6][4]={{0,2,3,1},{4,5,7,6},{0,4,6,2},{1,3,7,5},{2,6,7,3},{0,1,5,4}};
    static const float shade[6]={.65f,.35f,.45f,.8f,1.f,.25f};
    cull=1;
    for(int f=0;f<6;f++){Vertex a=v[faces[f][0]],b=v[faces[f][1]],c=v[faces[f][2]],d=v[faces[f][3]];a.l=b.l=c.l=d.l=lum*shade[f];quad(a,b,c,d,mat);}
    cull=0;
}
static void ring(float z,float radius,float twist,int mat,float brightness){
    for(int i=0;i<12;i++){
        float a=i*(2*PI/12)+twist,b=(i+1)*(2*PI/12)+twist;
        float cx=.65f*sn(z*.10f+time_s*.4f),cy=.35f*cs(z*.16f+time_s*.3f);
        Vertex p=vertex(vec(cx+cs(a)*radius,cy+sn(a)*radius,z),brightness);
        Vertex q=vertex(vec(cx+cs(b)*radius,cy+sn(b)*radius,z),brightness);
        Vertex r=vertex(vec(cx+cs(b)*(radius+.14f),cy+sn(b)*(radius+.14f),z),brightness*.65f);
        Vertex s=vertex(vec(cx+cs(a)*(radius+.14f),cy+sn(a)*(radius+.14f),z),brightness*.65f);
        quad(p,q,r,s,mat);
        p.z+=.18f;q.z+=.18f;quad(s,r,q,p,2);
    }
}
static void background(int chapter){
    memset(depth,0,sizeof depth);memset(fb,0,WIDTH*TOP*2);memset(fb+WIDTH*BOTTOM,0,WIDTH*(HEIGHT-BOTTOM)*2);
    for(int y=TOP;y<BOTTOM;y++){
        int horizon=160-abs(y-132);int base=clampi(horizon/7,0,24);
        for(int x=0;x<WIDTH/2;x++){
            int v=clampi(base-abs(x-160)/18,0,24),d=((x&1)^((y&1)<<1));
            uint16_t color=rgb(v/2+d,v+d+2,v*2+d+4);
            fb[y*WIDTH+x]=color;fb[y*WIDTH+WIDTH-1-x]=color;
        }
    }
    uint32_t rng=7183;
    for(int i=0;i<90;i++){
        rng=rng*1664525u+1013904223u;float x=(int)(rng&1023)-512.f;
        rng=rng*1664525u+1013904223u;float y=(int)(rng&511)-255.f;
        float z=3.f+fmodf(i*.773f+30-time_s*(chapter==1?2.f:.2f),30.f);
        int sx=160+(int)(x/z),sy=110+(int)(y/z),v=30+(int)(120/z);
        pixel(sx,sy,rgb(v*.6f,v*.8f,v));
    }
}
static void reflection(void){
    for(int y=177;y<BOTTOM;y++){
        int dy=y-177,sy=176-dy*2,shift=(int)(sn(dy*.55f+time_s*2)*(.3f+dy*.055f));
        int strength=150-dy*2;
        for(int x=0;x<WIDTH;x++){
            int sx=clampi(x+shift,0,WIDTH-1);uint16_t c=fb[sy*WIDTH+sx];
            int grid=((x-160)*30/(dy+8))%100;int g=(abs(grid)<2)?5:0;
            fb[y*WIDTH+x]=rgb((red(c)*strength)>>8,((green(c)*strength)>>8)+g+2,((blue(c)*strength)>>8)+g+5);
        }
    }
}
static void HOT(bloom)(void){
    for(int y=0;y<60;y++)for(int x=0;x<80;x++){
        int m=0;for(int j=0;j<4;j++)for(int i=0;i<4;i++){uint16_t p=fb[(y*4+j)*WIDTH+x*4+i];int l=red(p)+green(p)+blue(p);if(l>m)m=l;}
        glow[y][x]=(uint8_t)clampi((m-270)/2,0,200);
    }
    for(int pass=0;pass<2;pass++){
        for(int y=1;y<59;y++)for(int x=1;x<79;x++)blur[y][x]=(glow[y][x-1]+glow[y][x]*2+glow[y][x+1])/4;
        for(int y=1;y<59;y++)for(int x=1;x<79;x++)glow[y][x]=(blur[y-1][x]+blur[y][x]*2+blur[y+1][x])/4;
    }
    int warm=score.chapter==2||score.chapter==5;
    for(int y=TOP;y<BOTTOM;y++)for(int x=0;x<WIDTH;x++){
        int gx=x>>2,gy=y>>2,gx1=gx<79?gx+1:gx,gy1=gy<59?gy+1:gy;
        int a=glow[gy][gx]*(4-(x&3))+glow[gy][gx1]*(x&3);
        int b=glow[gy1][gx]*(4-(x&3))+glow[gy1][gx1]*(x&3);
        int v=(a*(4-(y&3))+b*(y&3))>>5;
        uint16_t p=fb[y*WIDTH+x];fb[y*WIDTH+x]=rgb(red(p)+(warm?v:v/2),green(p)+v,blue(p)+(warm?v/2:v));
    }
}
static void text(int x,int y,const char *s,int scale,int spacing,uint16_t color){
    while(*s){const uint8_t *glyph=font8x8_glyph(*s++);for(int j=0;j<7;j++)for(int i=0;i<7;i++)if(glyph[j]&(128>>i))for(int b=0;b<scale;b++)for(int a=0;a<scale;a++)pixel(x+i*scale+a,y+j*scale+b,color);x+=7*scale+spacing;}
}
static void centered(int y,const char*s,int size,int space,uint16_t c){text((WIDTH-((int)strlen(s)*(7*size+space)-space))/2,y,s,size,space,c);}
/* Bespoke wide geometric wordmark, drawn as strokes rather than bitmap scaling. */
static void wordmark(int y,float reveal){
    static const int8_t paths[6][18]={
        {0,0,12,28,12,28,24,0,-1},
        {24,0,0,0,0,0,0,28,0,28,24,28,0,14,19,14,-1},
        {24,0,0,0,0,0,0,14,0,14,24,14,24,14,24,28,-1},
        {0,28,0,0,0,0,24,0,24,0,24,14,24,14,0,14,-1},
        {24,0,0,0,0,0,0,28,0,28,24,28,0,14,19,14,-1},
        {0,28,0,0,0,0,24,0,24,0,24,14,24,14,0,14,-1}};
    int color=(int)(235*reveal);uint16_t c=rgb(color,color,color*.9f);
    for(int k=0;k<6;k++){
        int ox=30+k*45;for(int j=0;j<17&&paths[k][j]>=0;j+=4){int x0=ox+paths[k][j],y0=y+paths[k][j+1],x1=ox+paths[k][j+2],y1=y+paths[k][j+3];line(x0,y0,x1,y1,c);line(x0+1,y0,x1+1,y1,c);}
        if(k==2)line(ox,y+28,ox+24,y+28,c);
        if(k==5){line(ox+11,y+14,ox+25,y+28,c);line(ox+12,y+14,ox+26,y+28,c);}
    }
}
static void cathedral(void){
    float travel=fmodf(time_s*2.6f,3.f);
    for(int k=14;k>=0;k--){float z=3+k*3.f-travel;
        for(int side=-1;side<=1;side+=2){float h=3.2f+.8f*sn(k*.7f+time_s*.3f);
            box(vec(side*3.5f,h*.5f-1.5f,z),vec(.38f,h*.5f,.45f),2,170/(1+z*.025f));
            box(vec(side*3.06f,h*.5f-1.5f,z-.47f),vec(.038f,h*.45f,.035f),0,330/(1+z*.012f)+score.pulse*35);
        }
        ring(z,3.1f,.15f*sn(time_s*.3f+k*.2f),k%4==0?1:0,310/(1+z*.025f));
    }
}
static void city(void){
    /* 169 separate solid columns; their roofs play the same sixteenth-note wave. */
    box_view=1;setup_object(-.62f,.48f+sn(time_s*.15f)*.25f,.06f*sn(time_s*.2f),12.5f);
    for(int iz=12;iz>=0;iz--)for(int ix=-6;ix<=6;ix++){
        float x=ix*.83f,z=iz*.87f-5.f,r=sqrtf(x*x+z*z);
        float h=.18f+1.3f*(.5f+.5f*sn(r*1.6f-score.beat*.85f));
        V3 p=vec(x,-1.8f+h*.5f,z);
        box(p,vec(.28f,h*.5f,.28f),ix%3==0?1:0,130+60*h);
    }
    box_view=0;
}
static void iris(void){
    /* A mechanical flower: 28 tapered blades in three dimensions. */
    int count=28;float open=.55f+.3f*sn((time_s-60)*.28f);
    for(int k=0;k<count;k++){
        float a=k*(2*PI/count)+time_s*.15f;
        for(int j=0;j<9;j++){
            float u=j/9.f,v=(j+1)/9.f;
            Vertex q[4];for(int m=0;m<4;m++){
                float w=m>=2?v:u,edge=(m==0||m==3)?-1.f:1.f;
                float r=.5f+w*2.4f,aa=a+w*open+edge*(.045f+.035f*sn(w*PI));
                V3 p=vec(r*cs(aa),r*sn(aa),.7f*sn(w*PI+time_s*.7f)+edge*.16f);
                q[m]=object_vertex(p,norm(vec(cs(aa)*.5f,sn(aa)*.5f,.6f+edge*.4f)));
            }
            quad(q[0],q[1],q[2],q[3],k%4==0?1:0);
        }
    }
}
static void shards(void){
    float unfold=smooth((time_s-75)/10)*(.65f+.35f*sn(time_s*.33f));
    for(int k=0;k<100;k++){
        float a=k*2.399963f+time_s*.25f,y=1-2*(k+.5f)/100.f,r=sqrtf(1-y*y);
        V3 n=vec(r*cs(a),y,r*sn(a));float radius=1.8f+unfold*(.3f+.6f*sn(k*7.f+time_s));
        V3 p=mul(n,radius),side=norm(cross(n,vec(0,1,0))),up=cross(side,n);
        Vertex q[4];q[0]=object_vertex(add(p,mul(up,.27f)),n);q[1]=object_vertex(add(p,mul(side,.19f)),n);
        q[2]=object_vertex(sub(p,mul(up,.27f)),n);q[3]=object_vertex(sub(p,mul(side,.19f)),n);
        quad(q[0],q[1],q[2],q[3],k%7==0?1:0);
    }
}
void demo_init(void){
    for(int i=0;i<2048;i++)sine[i]=sinf(i*2*PI/2048);
    for(int i=0;i<256;i++){
        float t=i/255.f,h=sat((t-.40f)/.60f);h*=h;
        material[0][i]=rgb(8+65*t+215*h,18+180*t+85*h,26+200*t+79*h);
        material[1][i]=rgb(22+220*t+90*h,14+146*t+170*h,20+70*t+215*h);
        material[2][i]=rgb(5+41*t,8+54*t,15+70*t);
        material[3][i]=rgb(30+130*t+90*h,5+60*t+180*h,32+171*t+50*h);
        material[4][i]=rgb(i,i,i);material[5][i]=rgb(i/2,i/2,i);
    }
}
void demo_render(uint16_t *pixels,uint32_t sample){
    fb=pixels;score=score_at(sample);time_s=(float)sample/SAMPLE_RATE;triangles=0;centre_y=111;
    background(score.chapter);
    if(score.chapter==0){
        centre_y=104;setup_object(.3f+time_s*.035f,.4f,time_s*.08f,5.5f-time_s*.025f);knot(0,.10f,1);reflection();bloom();
        float f=smooth((time_s-1)*.5f);wordmark(93,f);
        centered(70,"L A T E N T",1,1,rgb(115*f,165*f,170*f));
        centered(142,"A MACHINE FOR THE BLUE HOUR",1,0,rgb(110*f,135*f,145*f));
        centered(198,"PHASE  .  AZURE  .  2026",1,1,rgb(80*f,100*f,115*f));
    }else if(score.chapter==1){
        centre_y=115;cathedral();reflection();bloom();
        if(time_s<19)centered(196,"I   THE NAVE",1,2,rgb(120,185,190));
    }else if(score.chapter==2){
        ring(8.8f,3.75f,time_s*.065f,0,185+score.pulse*20);
        setup_object(time_s*.21f,time_s*.29f,.25f*sn(time_s*.27f),5.1f+.5f*sn(time_s*.32f));
        knot(.92f,.25f+.025f*score.pulse,1);reflection();bloom();
        if(time_s<41)centered(196,"II   THE RELIQUARY",1,1,rgb(195,160,110));
    }else if(score.chapter==3){
        setup_object(.28f*sn(time_s*.4f),.28f*cs(time_s*.3f),0,7.4f);iris();reflection();bloom();
        if(time_s<64)centered(196,"III   BLOOM IN THE STATIC",1,0,rgb(120,190,195));
    }else if(score.chapter==4){
        /* Alternating four-bar phrases, deliberate musical cuts. */
        if((score.bar/4)&1){centre_y=94;city();bloom();}
        else{setup_object(time_s*.15f,time_s*.11f,0,5.5f);shards();reflection();bloom();}
        if(time_s<79)centered(196,"IV   ALL THINGS RESONATE",1,0,rgb(120,195,200));
    }else{
        float f=smooth((time_s-105)/9);
        setup_object(.25f,.4f+time_s*.06f,time_s*.1f,7.2f);knot(1-f,.17f,1);reflection();bloom();
        if(time_s>109){
            wordmark(92,smooth((time_s-109)*.5f));
            centered(139,"CODE DIRECTION MUSIC  PHASE",1,0,rgb(170,180,170));
            centered(155,"MODEL  GPT-6 ASTRA",1,1,rgb(120,145,155));
            centered(171,"CRITIC  AZURE",1,1,rgb(120,145,155));
            centered(195,"VESPER . LATENT . 2026",1,1,rgb(190,170,130));
        }
    }
    /* Editorial frame, no performance counters inside the production. */
    uint16_t dim=rgb(35,60,67);line(18,TOP,301,TOP,dim);line(18,BOTTOM-1,301,BOTTOM-1,dim);
    float fade=smooth(time_s*.7f)*(1-smooth((time_s-117)/3));
    if(fade<.999f){int f=(int)(fade*256);for(int i=0;i<WIDTH*HEIGHT;i++){uint16_t p=fb[i];fb[i]=rgb((red(p)*f)>>8,(green(p)*f)>>8,(blue(p)*f)>>8);}}
}
unsigned demo_triangles(void){return triangles;}
