/* HELION / LATENT / 2026. Phase (GPT-6 Astra).
 * SRAM indexed textures, SIO affine spans, DMA sky transfer overlapped with
 * mesh preparation, painter-sorted environment-mapped solar geometry.
 */
#include "helion.h"
#include "accelerator.h"
#include "font8x8.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define PI 3.14159265358979323846f
#define SEG 36
#define RING 8
#define VMAX (SEG*RING*2)
#define FMAX (SEG*RING*4)
typedef struct {float x,y,z,u,v;} Vertex;
typedef struct {uint16_t a,b,c;int16_t depth;} Face;
typedef struct {int32_t u,v;} UV;
static Vertex vertex[VMAX];
static Face face[FMAX];
static unsigned nf,triangles,texels;
static uint8_t stone[128*128],metal[128*128];
static uint16_t floorpal[16][256],matpal[256];
static UV polar[31][41];
static float sine[2048];
static uint16_t *fb;
static float t,bar,rx,ry,rz;
static DemoProfile profile;
#ifdef PICO_BUILD
#include "pico/time.h"
static uint32_t stamp(void){return time_us_32();}
#else
static uint32_t stamp(void){return 0;}
#endif
DemoProfile demo_profile(void){return profile;}
static float sn(float a){return sine[(int)(a*(2048.f/(2*PI)))&2047];}
static float cs(float a){return sn(a+PI*.5f);}
static float sat(float x){return x<0?0:x>1?1:x;}
static float smooth(float x){x=sat(x);return x*x*(3-2*x);}
/* The same RGB555 blend, done in 5-bit channel space instead of 8-bit.
 * red()/green()/blue() shift a 5-bit field up by three and rgb() shifts it
 * back down by three, and rgb()'s clamps cannot fire (31*32>>5 == 31), so
 * every bit of the result is unchanged -- six clamps and six shifts per
 * blended pixel are not. pixel() is the inner loop of halo(), line(),
 * corona(), orbit(), stars(), text() and title(). -- Overscan */
/* Red (bits 0-4) and blue (11-15) are far enough apart that one 32-bit
 * multiply scales both without their partial products meeting: 31*32 = 992
 * needs ten bits and blue starts at eleven. Green (6-10) takes the second.
 * Each field ends up holding exactly (ca*(32-f)+cb*f)>>5, which is what the
 * three separate channel blends produced, so the result is bit-identical --
 * four multiplies and no clamps in place of six multiplies, six clamps and
 * a dozen shifts. -- Overscan */
typedef uint32_t u32_alias __attribute__((may_alias));
#define RB_MASK 0xf81fu
#define G_MASK  0x07c0u
static uint16_t mixc(uint16_t a,uint16_t b,int f){
    unsigned g=32u-(unsigned)f,h=(unsigned)f;
    unsigned rb=((a&RB_MASK)*g+(b&RB_MASK)*h)>>5;
    unsigned gr=((a&G_MASK)*g+(b&G_MASK)*h)>>5;
    return (uint16_t)((rb&RB_MASK)|(gr&G_MASK));
}
static uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
static void pixel(int x,int y,uint16_t c,int a){if((unsigned)x<WIDTH&&(unsigned)y<HEIGHT){unsigned p=y*WIDTH+x;fb[p]=a>=32?c:mixc(fb[p],c,a);}}
static void line(int x,int y,int xx,int yy,uint16_t c,int a){int dx=abs(xx-x),sx=x<xx?1:-1,dy=-abs(yy-y),sy=y<yy?1:-1,e=dx+dy;int limit=1000;while(limit--){pixel(x,y,c,a);if(x==xx&&y==yy)break;int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}}
/* Same disc, same alphas, same pixels. The square scan tested 29,241 points
 * to light 22,970 of them at radius 85 and paid a bounds check, a row
 * multiply and an integer division on every one. Now each row computes its
 * own span and writes fb directly, and the division by the loop-invariant
 * r*r becomes a multiply-and-shift that is exactly equal over this domain:
 * with M = floor(2^32/rr)+1 and e = M*rr-2^32 <= rr, floor(n*M/2^32) equals
 * floor(n/rr) for every n*e < 2^32, and here n = a*(rr-d) <= 32*rr with
 * rr <= 8192. Outside that guard the divide still runs. -- Overscan */
static void HOT(halo)(float cx,float cy,float radius,uint16_t c,int a){
    int r=(int)radius;if(r<1)return;
    const int rr=r*r,ox=(int)cx,oy=(int)cy;
    const uint32_t magic=(uint32_t)(0x100000000ull/(uint32_t)rr)+1u;
    const int exact=rr<=8192;
    for(int y=-r;y<=r;y++){
        int py=oy+y;if((unsigned)py>=HEIGHT)continue;
        int rem=rr-y*y;if(rem<=0)continue;
        int half=(int)sqrtf((float)rem);
        while(half*half>=rem)half--;
        while((half+1)*(half+1)<rem)half++;
        int x0=-half,x1=half;
        if(ox+x0<0)x0=-ox;
        if(ox+x1>WIDTH-1)x1=WIDTH-1-ox;
        uint16_t *row=fb+(unsigned)py*WIDTH+ox;
        for(int x=x0;x<=x1;x++){
            uint32_t n=(uint32_t)(a*(rem-x*x));
            int alpha=exact?(int)(((uint64_t)n*magic)>>32):(int)(n/(uint32_t)rr);
            row[x]=alpha>=32?c:mixc(row[x],c,alpha);
        }
    }
}
/* rx/ry/rz change once a frame, not once a vertex: the twelve table lookups
 * per rotate() were 24 per vertex over 1,152 records. Cached here; sn()/cs()
 * are pure table reads, so the floats that come out are the same bits. */
static float crx,srx,cry,sry,crz,srz;
static void set_rotation(float ax,float ay,float az){
    rx=ax;ry=ay;rz=az;
    crx=cs(rx);srx=sn(rx);cry=cs(ry);sry=sn(ry);crz=cs(rz);srz=sn(rz);
}
static void HOT(rotate)(float *x,float *y,float *z){
    float a=*y*crx-*z*srx;*z=*y*srx+*z*crx;*y=a;
    a=*x*cry+*z*sry;*z=-*x*sry+*z*cry;*x=a;
    a=*x*crz-*y*srz;*y=*x*srz+*y*crz;*x=a;
}
/* Painter order for 1,152 records with 16-bit keys. qsort cost ~11,700
 * indirect comparator calls; two stable counting passes cost 2*nf moves.
 * Both passes fill their buckets from 255 downwards, so the result is
 * descending by depth -- farthest first -- and stable, where qsort was not.
 * Triangles whose quantised depth is exactly equal can therefore come out in
 * a different order than before; their relative depth was never defined. */
static Face facetmp[FMAX];
static void HOT(sort_faces)(void){
    unsigned count[256],sum,i;
    memset(count,0,sizeof count);
    for(i=0;i<nf;i++)count[(unsigned)((uint16_t)(face[i].depth^0x8000))&255u]++;
    sum=0;for(int k=255;k>=0;k--){unsigned c=count[k];count[k]=sum;sum+=c;}
    for(i=0;i<nf;i++)facetmp[count[(unsigned)((uint16_t)(face[i].depth^0x8000))&255u]++]=face[i];
    memset(count,0,sizeof count);
    for(i=0;i<nf;i++)count[(unsigned)((uint16_t)(facetmp[i].depth^0x8000))>>8]++;
    sum=0;for(int k=255;k>=0;k--){unsigned c=count[k];count[k]=sum;sum+=c;}
    for(i=0;i<nf;i++)face[count[(unsigned)((uint16_t)(facetmp[i].depth^0x8000))>>8]++]=facetmp[i];
}
static void HOT(prepare_geometry)(float scale,int kind){
    nf=0;set_rotation(.5f+sn(t*.21f)*.45f,t*.31f,t*.11f);
    for(int shell=0;shell<2;shell++){
        int base=shell*SEG*RING;
        for(int i=0;i<SEG;i++)for(int j=0;j<RING;j++){
            float u=i*(2*PI/SEG),v=j*(2*PI/RING),cu=cs(u),su=sn(u),cv=cs(v),sv=sn(v);
            float tooth=kind==1?.28f*cs(u*6+t*.5f):.16f*cs(u*3);
            float radius=(shell?.76f:1.18f)+tooth,tube=shell?.17f:.22f;
            float x=(radius+tube*cv)*cu,y=(radius+tube*cv)*su,z=tube*sv+.22f*sn(u*3+t*.35f);
            float nx=cv*cu,ny=cv*su,nz=sv;
            if(shell){float q=y;y=z;z=-q;q=ny;ny=nz;nz=-q;}
            rotate(&x,&y,&z);rotate(&nx,&ny,&nz);
            z+=4.4f;float p=205*scale/z;
            vertex[base+i*RING+j]=(Vertex){160+x*p,116-y*p,z,(nx*.48f+.5f)*127,(ny*.48f+.5f)*127};
        }
        for(int i=0;i<SEG;i++)for(int j=0;j<RING;j++){
            int a=base+i*RING+j,b=base+((i+1)%SEG)*RING+j,c=base+((i+1)%SEG)*RING+(j+1)%RING,d=base+i*RING+(j+1)%RING;
            face[nf++]=(Face){a,b,c,(int16_t)((vertex[a].z+vertex[b].z+vertex[c].z)*1024)};
            face[nf++]=(Face){a,c,d,(int16_t)((vertex[a].z+vertex[c].z+vertex[d].z)*1024)};
        }
    }
    sort_faces();
}
static void HOT(raster)(Vertex a,Vertex b,Vertex c){
    if(a.y>b.y){Vertex q=a;a=b;b=q;}if(b.y>c.y){Vertex q=b;b=c;c=q;}if(a.y>b.y){Vertex q=a;a=b;b=q;}
    float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
    if(fabsf(area)<.08f||c.y<0||a.y>=HEIGHT)return;
    float inv=1/area,ux=((b.u-a.u)*(c.y-a.y)-(c.u-a.u)*(b.y-a.y))*inv,uy=((b.x-a.x)*(c.u-a.u)-(c.x-a.x)*(b.u-a.u))*inv;
    float vx=((b.v-a.v)*(c.y-a.y)-(c.v-a.v)*(b.y-a.y))*inv,vy=((b.x-a.x)*(c.v-a.v)-(c.x-a.x)*(b.v-a.v))*inv;
    int du=(int)(ux*65536),dv=(int)(vx*65536),ys=clampi((int)ceilf(a.y-.5f),0,HEIGHT),ye=clampi((int)ceilf(c.y-.5f),0,HEIGHT);
    float ls=(c.x-a.x)/(c.y-a.y+1e-9f),up=(b.x-a.x)/(b.y-a.y+1e-9f),down=(c.x-b.x)/(c.y-b.y+1e-9f);
    for(int y=ys;y<ye;y++){
        float fy=y+.5f,x0=a.x+(fy-a.y)*ls,x1=fy<b.y?a.x+(fy-a.y)*up:b.x+(fy-b.y)*down;
        if(x0>x1){float q=x0;x0=x1;x1=q;}
        int xs=clampi((int)ceilf(x0-.5f),0,WIDTH),xe=clampi((int)ceilf(x1-.5f),0,WIDTH);if(xs>=xe)continue;
        int u=(int)((a.u+ux*(xs+.5f-a.x)+uy*(fy-a.y))*65536),v=(int)((a.v+vx*(xs+.5f-a.x)+vy*(fy-a.y))*65536);
        texture_span(u,v,du,dv);uint16_t *out=fb+y*WIDTH+xs;
        for(int x=xs;x<xe;x++)*out++=matpal[metal[texture_pop()]];
        texels+=(unsigned)(xe-xs);
    }
    triangles++;
}
static void HOT(draw_geometry)(void){uint32_t begin=stamp();texture_config(7);for(unsigned i=0;i<nf;i++)raster(vertex[face[i].a],vertex[face[i].b],vertex[face[i].c]);profile.mesh+=stamp()-begin;}
static void palettes(void){
    unsigned shift=(unsigned)(smooth((bar-54)/4)*smooth((66-bar)/2)*240);
    for(unsigned i=0;i<256;i++){
        unsigned r=clampi((int)i*2,0,255),g=clampi((int)i*3/2-35,0,255),b=clampi((int)i-65,0,255);
        matpal[i]=rgb(palette_lerp(r,clampi((int)i+45,0,255),shift),palette_lerp(g,i,shift),palette_lerp(b,clampi((int)i*2,0,255),shift));
    }
}
static void HOT(plain)(void){
    uint32_t begin=stamp();
    texture_config(7);float angle=.22f*sn(t*.13f),co=cs(angle),si=sn(angle),speed=t*28;
    for(int y=117;y<HEIGHT;y++){
        float z=1600.f/(y-108),step=z*.025f;
        int du=(int)(co*step*65536),dv=(int)(si*step*65536);
        int u=(int)((-160*co*step+32*sn(t*.2f))*65536),v=(int)((z*2+speed-160*si*step)*65536);
        texture_span(u,v,du,dv);const uint16_t *pal=floorpal[clampi((y-117)/7,0,15)];
        uint16_t *out=fb+y*WIDTH;for(int x=0;x<WIDTH;x++)*out++=pal[stone[texture_pop()]];
    }
    texels+=320*123;
    profile.field+=stamp()-begin;
}
static int unwrap(int delta){const int turn=128*65536;if(delta>turn/2)delta-=turn;if(delta< -turn/2)delta+=turn;return delta;}
static void HOT(tunnel)(void){
    uint32_t begin=stamp();
    texture_config(7);int spin=(int)(t*9*65536),travel=(int)(t*43*65536);
    for(int y=0;y<HEIGHT;y++){
        int gy=y>>3,f=y&7;
        for(int gx=0;gx<40;gx++){
            UV a=polar[gy][gx],b=polar[gy+1][gx],c=polar[gy][gx+1],d=polar[gy+1][gx+1];
            int u=a.u+unwrap(b.u-a.u)*f/8,v=a.v+(b.v-a.v)*f/8;
            int u2=c.u+unwrap(d.u-c.u)*f/8,v2=c.v+(d.v-c.v)*f/8;
            u+=(int)(sn(v/65536.f*.036f+t)*4*65536);u2+=(int)(sn(v2/65536.f*.036f+t)*4*65536);
            texture_span(u*3+spin,v*4+travel,unwrap(u2-u)*3/8,(v2-v)*4/8);
            int distance=abs(gx*8-160)+abs(y-120),shade=clampi(distance/10,0,15);
            uint16_t *out=fb+y*WIDTH+gx*8;const uint16_t *pal=floorpal[shade];
            for(int x=0;x<8;x++)*out++=pal[stone[texture_pop()]];
        }
    }
    texels+=WIDTH*HEIGHT;
    profile.field+=stamp()-begin;
}
static void stars(int rush){
    for(unsigned i=0;i<150;i++){
        uint32_t h=hash(i+91);float z=fmodf((h&1023)*.007f-t*(rush?1.6f:.25f),7.f);if(z<0)z+=7;z+=.5f;
        float px=((int)((h>>10)&1023)-512)*.46f,py=((int)((h>>20)&1023)-512)*.32f;
        int x=(int)(160+px/z),y=(int)(120+py/z);uint16_t c=i%4?rgb(214,158,129):rgb(199,161,248);
        pixel(x,y,c,12);if(rush)line(x,y,(int)(160+px/(z+.16f)),(int)(120+py/(z+.16f)),c,8);
    }
}
static void corona(float radius,float intensity){
    uint16_t gold=rgb(255,179,86),white=rgb(255,233,173);int alpha=(int)(intensity*24);
    for(int ring=0;ring<7;ring++){
        int px=0,py=0;
        for(int i=0;i<=180;i++){
            float a=i*(2*PI/180),r=radius+ring*1.5f+(2+ring*.7f)*sn(a*9+t*.7f+ring*.3f)+2*sn(a*17-t);
            int x=(int)(160+cs(a)*r),y=(int)(116+sn(a)*r*.86f);
            if(i)line(px,py,x,y,ring<2?white:gold,alpha/(1+ring/2));px=x;py=y;
        }
    }
}
static void orbit(void){
    for(int band=0;band<3;band++){
        int px=0,py=0;
        for(int j=0;j<=128;j++){
            float a=j*(2*PI/128),x=cs(a)*(1.8f+band*.12f),y=sn(a)*(1.8f+band*.12f),z=0;
            float b=band*PI/3+t*.12f;z=y*sn(b);y*=cs(b);rotate(&x,&y,&z);float k=220/(z+4.7f);
            int xx=(int)(160+x*k),yy=(int)(116-y*k);
            if(j)line(px,py,xx,yy,rgb(234,161+band*25,135+band*35),11);px=xx;py=yy;
            if(j%16==0)halo(xx,yy,3,rgb(255,222,183),20);
        }
    }
}
static void text(int y,const char *s,int spacing,int a){int x=(WIDTH-((int)strlen(s)*(7+spacing)-spacing))/2;while(*s){const uint8_t *g=font8x8_glyph(*s++);for(int j=0;j<7;j++)for(int i=0;i<7;i++)if(g[j]&(128>>i))pixel(x+i,y+j,rgb(231,205,181),a);x+=7+spacing;}}
static void title(int y,int a){for(int j=0;j<48;j++)for(int i=0;i<280;i++){int q=art_title[j*280+i]*a/255;if(q){pixel(21+i,y+j+2,rgb(0,0,0),q);pixel(20+i,y+j,rgb(255,228,177),q);}}}
void demo_init(void){
    accelerator_init();
    if(accelerator_selftest()){
#ifdef PICO_BUILD
        panic("HELION SIO self-test failed");
#else
        abort();
#endif
    }
    for(int i=0;i<2048;i++)sine[i]=sinf(i*(2*PI/2048));
    for(int y=0;y<128;y++)for(int x=0;x<128;x++){
        float a=sn(x*(2*PI/128)*3+2*sn(y*(2*PI/128)*2))+cs(y*(2*PI/128)*3+sn(x*(2*PI/128)*2));
        float vein=expf(-fabsf(a)*8),panel=(x%32==0||y%32==0)?.35f:0;
        stone[y*128+x]=(uint8_t)clampi((int)(20+190*vein+panel*60+16*sn(x*.2f)*cs(y*.17f)),0,255);
        float nx=(x-63.5f)/64,ny=(y-63.5f)/64,nz=sqrtf(fmaxf(0,1-nx*nx-ny*ny));
        float spec=powf(fmaxf(0,nx*.30f-ny*.4f+nz*.86f),28);
        float strip=expf(-fabsf(nx*.75f+ny*.35f-.22f)*22);
        metal[y*128+x]=(uint8_t)clampi((int)(30+65*nz+95*spec+135*strip+28*sn(ny*12)),0,255);
    }
    for(int p=0;p<16;p++)for(int i=0;i<256;i++){
        float f=(p+1)/16.f;int r=clampi(i*2,0,255),g=clampi(i*3/2-60,0,255),b=clampi(i-80,0,255);
        if(i<75){r=i;g=i/3;b=i+12;}
        floorpal[p][i]=rgb((int)(r*f),(int)(g*f),(int)(b*f));
    }
    for(int y=0;y<=30;y++)for(int x=0;x<=40;x++){
        float dx=x*8-160.f,dy=y*8-120.f,r=sqrtf(dx*dx+dy*dy);
        polar[y][x]=(UV){(int32_t)((atan2f(dy,dx)*(128/(2*PI)))*65536),(int32_t)(fminf(128,1600/(r+8))*65536)};
    }
}
unsigned demo_triangles(void){return triangles;}
unsigned demo_texels(void){return texels;}
Score score_at(uint32_t sample){float b=sample/(float)BEAT_SAMPLES;int bar=(int)(b/4);return (Score){b,1-(b-floorf(b)),b/4-bar,bar,bar<8?0:bar<24?1:bar<40?2:bar<56?3:bar<64?4:bar<72?5:6};}
void HOT(demo_render)(uint16_t *pixels,uint32_t sample){
    fb=pixels;triangles=texels=0;if(sample>=DURATION_SAMPLES){memset(fb,0,WIDTH*HEIGHT*2);return;}
    uint32_t start=stamp();memset(&profile,0,sizeof profile);
    t=sample/(float)SAMPLE_RATE;bar=sample/(float)(BEAT_SAMPLES*4);
    int use_sky=bar<40||bar>=64;
    if(use_sky)background_begin(fb);palettes();
    float scale=bar<24?.35f+smooth((bar-8)/16)*.8f:1.27f+.09f*sn(t*.6f);
    if(bar>=72)scale=.7f*(1-smooth((bar-74)/4));
    if(bar>=8&&bar<40)prepare_geometry(scale,bar>=24);
    else if(bar>=56&&bar<64)prepare_geometry(1.15f,1);
    profile.prepare=stamp()-start;uint32_t wait=stamp();if(use_sky)background_wait();profile.wait=stamp()-wait;
    if(bar<8){stars(0);corona(36+smooth(bar/8)*13,.8f);halo(160,116,40,rgb(217,75,19),9);int a=(int)(32*smooth((t-1)/2)*smooth((16-t)/2));text(62,"L A T E N T",0,a);title(87,a);text(178,"A STAR REMEMBERS ITS FIRE",0,a);text(214,"PHASE / 2026",1,a);}
    else if(bar<24){plain();stars(0);halo(160,116,54,rgb(249,88,12),8);corona(35+smooth((bar-8)/16)*28,.5f);draw_geometry();}
    else if(bar<40){stars(0);halo(160,116,85,rgb(247,71,20),6);orbit();draw_geometry();corona(92,.35f);}
    else if(bar<56){tunnel();stars(1);halo(160,120,16,rgb(255,210,148),22);}
    else if(bar<64){tunnel();orbit();draw_geometry();stars(0);}
    else if(bar<72){
        stars(0);float r=48+sn(t*.3f)*3;halo(160,116,r+22,rgb(234,85,24),10);corona(r,1);
        for(int y=-(int)r+3;y<r-3;y++)for(int x=-(int)r+3;x<r-3;x++)if(x*x+y*y<(r-3)*(r-3))pixel(160+x,116+y,rgb(2,1,8),32);
        if(bar>67)text(202,"EVEN DARKNESS HAS A HEART",0,(int)(24*smooth(bar-67)*smooth(72-bar)));
    }else{
        stars(0);int a=(int)(32*smooth((bar-72)/2));title(58,a);
        text(118,"CODE + DIRECTION  PHASE",0,a);
        text(136,"MUSIC  PHOSPHOR",0,a);
        text(154,"MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1",0,a);
        text(176,"FOR AZURE / FOR LATENT",0,a);
        text(204,"UNTIL THE NEXT SUN",1,a);
    }
    /* Short exposure dips join the different optical spaces; absolute sample
     * time means slow frames skip ahead rather than change the production. */
    float exposure=smooth(t/2)*smooth((160-t)/4);
    const float cuts[]={16,48,80,112,128,144};
    for(unsigned i=0;i<sizeof cuts/sizeof cuts[0];i++){float d=fabsf(t-cuts[i]);if(d<.6f)exposure*=.18f+.82f*smooth(d/.6f);}
    int f=(int)(exposure*32);
    if(f<32){
        /* mixc(0,p,f) is (channel*f)>>5 in each of the three 5-bit fields, so
         * so the same two-multiply field trick mixc() uses fades the whole
         * page with no table and no clamps. The extra framebuffer pass cost
         * 12.6 ms of an 18 ms frame at the cuts; its bytes are unchanged. */
        unsigned h=(unsigned)f;
        if(!((uintptr_t)fb&3u)){
            /* Both pages are 4-byte aligned, so the pass runs a word at a
             * time: red and green scale in place in the doubled masks, and
             * blue is brought down to bits 0-4 first because 31*31<<27 would
             * not fit. Same three fields, same >>5, same bytes out. */
            u32_alias *w=(u32_alias*)fb;
            for(int i=0;i<WIDTH*HEIGHT/2;i++){
                uint32_t q=w[i];
                uint32_t r=(((q&0x001f001fu)*h)>>5)&0x001f001fu;
                uint32_t g=(((q&0x07c007c0u)*h)>>5)&0x07c007c0u;
                uint32_t b=(((((q>>11)&0x001f001fu)*h)>>5)&0x001f001fu)<<11;
                w[i]=r|g|b;
            }
        }else for(int i=0;i<WIDTH*HEIGHT;i++){
            unsigned q=fb[i];
            fb[i]=(uint16_t)(((((q&RB_MASK)*h)>>5)&RB_MASK)|((((q&G_MASK)*h)>>5)&G_MASK));
        }
    }
    profile.other=stamp()-start-profile.prepare-profile.wait-profile.field-profile.mesh;
}
