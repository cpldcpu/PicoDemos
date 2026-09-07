/* PELAGIC / LATENT / 2026. Phase (GPT-6 Astra).
 * Painted flash-resident environments; animated, textured manta geometry;
 * analytic schools, medusae and foreground plants. Sample-addressed direction.
 * The desktop player and RP2350 compile this same renderer.
 */
#include "pelagic.h"
#include "assets.h"
#include "font8x8.h"
#include "sampling.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define PI 3.14159265358979323846f
#define NX 24
#define NY 16
typedef struct {float x,y,u,v;} Vertex;
static uint16_t *fb;
static float sine[2048];
static Vertex mesh[NY+1][NX+1];
static int sx[WIDTH],light[WIDTH];
static unsigned triangles;
static float t,bar;
static int ray_opacity;
static float sn(float x){return sine[(int)(x*(2048.f/(2*PI)))&2047];}
static float cs(float x){return sn(x+PI*.5f);}
static float sat(float x){return x<0?0:x>1?1:x;}
static float smooth(float x){x=sat(x);return x*x*(3-2*x);}
static float lerp(float a,float b,float x){return a+(b-a)*x;}
static uint32_t hash(uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;return x^(x>>16);}
/* Separate 5-bit channels: bit 5 must stay clear for the board DAC. */
static inline uint16_t blend(uint16_t a,uint16_t b,int n){
    return (uint16_t)((((a&31)*(32-n)+(b&31)*n)>>5)
      | (((((a>>6)&31)*(32-n)+((b>>6)&31)*n)>>5)<<6)
      | (((((a>>11)&31)*(32-n)+((b>>11)&31)*n)>>5)<<11));
}
static void pixel(int x,int y,uint16_t c,int a){
    if((unsigned)x<WIDTH&&(unsigned)y<HEIGHT){int off=y*WIDTH+x;fb[off]=a>=32?c:blend(fb[off],c,a);}
}
static void line(int x,int y,int x2,int y2,uint16_t c,int a){
    int dx=abs(x2-x),sx=x<x2?1:-1,dy=-abs(y2-y),sy=y<y2?1:-1,e=dx+dy;
    int limit=900;while(limit--){pixel(x,y,c,a);if(x==x2&&y==y2)break;int e2=e*2;if(e2>=dy){e+=dy;x+=sx;}if(e2<=dx){e+=dx;y+=sy;}}
}
static void glow(int x,int y,int radius,uint16_t color,int a){
    if(x+radius<0||x-radius>=WIDTH||y+radius<0||y-radius>=HEIGHT)return;
    for(int j=-radius;j<=radius;j++)for(int i=-radius;i<=radius;i++){
        int d=i*i+j*j;if(d<=radius*radius)pixel(x+i,y+j,color,a*(radius*radius-d)/(radius*radius));
    }
}

/* Environment row staging (Overscan, 2026-09-06, at Azure's request).
 *
 * environment() reads about 1.24 texels of a 307,200-byte painted plate for
 * every output pixel, straight out of XIP. Measured on the board, 57% of this
 * pass -- 10.5 ms of every frame in the film -- was flash stall, and none of
 * it overlapped anything: the CPU asked for a texel, waited for the QSPI, did
 * a few cycles of arithmetic, and asked again.
 *
 * The fix is not to read less. It is to read it somewhere else, and earlier.
 * A DMA channel streams the source row for output row y+1 into an SRAM bank
 * while the CPU samples row y out of the other bank. The flash traffic is
 * identical; it just happens underneath the arithmetic instead of in front of
 * it. Two channels, so the dissolve's second plate rides along with the first.
 *
 * Sampling is bit-identical to reading the plate directly, which is what
 * pelagic_check's visual hash is the referee for:
 *
 *   - the whole 480-texel row is staged, so clampi(...,0,479) indexes the
 *     bank exactly as it indexed the plate, edges included;
 *   - filtered, the pair (y0, y0+1) is staged and filtered_color() is called
 *     unmodified with height=2 and the fractional part of an identically
 *     clamped v. That reproduces its own v clamp, its u clamp, its tap
 *     weights and its dy = (y < height-1) ? width : 0 bottom-edge case -- for
 *     which the pair is staged as (319, 319), so dy=480 lands on a copy of
 *     row 319 and reads what dy=0 read before;
 *   - the dissolve stages both plates into separate slots of the bank.
 *
 * On the host there is no DMA and the same banks are filled with memcpy, so
 * there is one code path and the host still decides the hash. If a DMA
 * channel cannot be claimed, or a plate row is not word aligned, the device
 * falls back to that memcpy too: slower, never wrong.
 *
 * `noinline` is load bearing. environment() has always been marked HOT, but
 * it is static and called once, so the compiler inlined it into demo_render()
 * and the section attribute went with the inlined copy -- nothing was in
 * SRAM. This is the trap Phosphor's brief warned about, and the map now shows
 * environment in .time_critical where the marking always said it was.
 */
#define ENV_W 480
#define ENV_H 320
#if PELAGIC_SMOOTH
#define ENV_ROWS 2                     /* filtered sampling needs y0 and y0+1 */
#else
#define ENV_ROWS 1
#endif
#define ENV_SLOT (ENV_ROWS*ENV_W)
/* Two banks (the CPU samples one while the DMA fills the other) times two
 * slots (the dissolve samples two plates). 3.75 KB point sampled, 7.5 KB
 * filtered, against 107 KB free. */
static uint16_t env_bank[2][2*ENV_SLOT];

#if defined(PICO_BUILD)
#include "hardware/dma.h"
static int env_ch[2]={-1,-1};
static void env_dma_init(void){
    /* Called after video_init() and audio_init(), so scanvideo's low channels
     * and the audio pair are already spoken for. If the board has none left,
     * fall back to memcpy rather than panicking a working demo. */
    for(int i=0;i<2;i++){
        const int ch=dma_claim_unused_channel(false);
        if(ch<0){env_ch[0]=env_ch[1]=-1;return;}
        dma_channel_config c=dma_channel_get_default_config(ch);
        channel_config_set_transfer_data_size(&c,DMA_SIZE_32);
        channel_config_set_read_increment(&c,true);
        channel_config_set_write_increment(&c,true);
        dma_channel_set_config(ch,&c,false);
        env_ch[i]=ch;
    }
}
#endif

static inline void env_fetch(int bank,int slot,const uint16_t *src,int texels){
    uint16_t *dst=env_bank[bank]+slot*ENV_SLOT;
#if defined(PICO_BUILD)
    const int ch=env_ch[slot];
    if(ch>=0&&!((((uintptr_t)src)|((uintptr_t)dst))&3u)&&!(texels&1)){
        dma_channel_set_write_addr(ch,dst,false);
        dma_channel_set_read_addr(ch,src,false);
        dma_channel_set_trans_count(ch,(uint32_t)texels/2u,true);
        return;
    }
#endif
    memcpy(dst,src,(size_t)texels*2u);
}

static inline void env_settle(int bank,int slot,int dup){
#if defined(PICO_BUILD)
    if(env_ch[slot]>=0)dma_channel_wait_for_finish_blocking(env_ch[slot]);
#endif
#if PELAGIC_SMOOTH
    /* The clamped bottom row was fetched once; give the pair its second copy
     * now the transfer has landed. SRAM to SRAM, and only on the few rows a
     * frame where the pan runs off the bottom of the plate. */
    if(dup)memcpy(env_bank[bank]+slot*ENV_SLOT+ENV_W,env_bank[bank]+slot*ENV_SLOT,ENV_W*2u);
#else
    (void)bank;(void)dup;
#endif
    (void)slot;
}

static void __attribute__((noinline)) HOT(environment)(void){
    const uint16_t *a=art_reef,*b=art_abyss;float f=smooth((bar-40)/3.f);
    if(bar>=64){a=art_abyss;b=art_surface;f=smooth((bar-64)/3.f);}
    int mix=(int)(f*32);if(mix==32){a=b;mix=0;}
    float zoom=1.24f+.075f*sn(t*.13f);
    int step=(int)(zoom*65536),xc=(int)((240+20*sn(t*.12f)-160*zoom)*65536);
    float yc=160+16*sn(t*.09f);
    for(int x=0;x<WIDTH;x++){
        sx[x]=(int)(sn(x*.024f+t*.5f)*1.8f*(PELAGIC_SMOOTH?65536.f:1.f));
        light[x]=(int)(sn(x*.031f-t*.36f)*4+sn(x*.071f+t*.15f)*2);
    }
#if defined(PICO_BUILD)
    { static int claimed; if(!claimed){claimed=1;env_dma_init();} }
#endif
    /* One row ahead of the sampler for the whole frame. ENV_ROW_PICK is the
     * only place a source row is chosen, so the point sampled and the
     * filtered path cannot drift apart in how they clamp. */
    int vfrac[2]={0,0},dupb[2]={0,0};
#if PELAGIC_SMOOTH
#define ENV_ROW_PICK(yy,bk) do{ \
        int _v=(int)((yc+((yy)-120)*zoom)*65536); \
        if(_v<0)_v=0; if(_v>(ENV_H-1)*65536)_v=(ENV_H-1)*65536; \
        const int _y0=_v>>16; vfrac[bk]=_v&65535; \
        const int _dup=_y0>=ENV_H-1; dupb[bk]=_dup; \
        const int _n=_dup?ENV_W:2*ENV_W; \
        env_fetch(bk,0,a+_y0*ENV_W,_n); \
        if(mix)env_fetch(bk,1,b+_y0*ENV_W,_n); \
    }while(0)
#else
#define ENV_ROW_PICK(yy,bk) do{ \
        const int _y0=clampi((int)(yc+((yy)-120)*zoom),0,ENV_H-1); \
        (void)vfrac;(void)dupb; \
        env_fetch(bk,0,a+_y0*ENV_W,ENV_W); \
        if(mix)env_fetch(bk,1,b+_y0*ENV_W,ENV_W); \
    }while(0)
#endif
    ENV_ROW_PICK(0,0);
    for(int y=0;y<HEIGHT;y++){
        const int bank=y&1;
        env_settle(bank,0,dupb[bank]); if(mix)env_settle(bank,1,dupb[bank]);
        if(y+1<HEIGHT)ENV_ROW_PICK(y+1,1-bank);
        const uint16_t *ra=env_bank[bank],*rb=env_bank[bank]+ENV_SLOT;
#if PELAGIC_SMOOTH
        const int v=vfrac[bank];
#endif
        int shift=(int)((2.3f*sn(y*.051f+t*.62f)+sn(y*.11f-t*.4f))*65536);
        int u=xc+shift;
        int ly=(int)(sn(y*.045f+t*.5f)*5),off=y*WIDTH;
        for(int x=0;x<WIDTH;x++,u+=step){
#if PELAGIC_SMOOTH
            uint16_t c=filtered_color(ra,ENV_W,2,u+sx[x],v);
            if(mix)c=blend(c,filtered_color(rb,ENV_W,2,u+sx[x],v),mix);
#else
            int xx=clampi((u>>16)+sx[x],0,479);
            uint16_t c=ra[xx];if(mix)c=blend(c,rb[xx],mix);
#endif
            /* Sparse moving caustic crests, quantized gently in the native DAC. */
            int shine=light[x]+ly; if(shine>6)c=blend(c,rgb(126,229,240),(shine-6)/2);
            fb[off+x]=c;
        }
    }
#undef ENV_ROW_PICK
}

/* Affine UV interpolation over a fine deforming mesh. Pixel-centre top-left
 * coverage keeps adjacent triangles watertight. No depth buffer is needed:
 * each ray is a single-valued surface and creatures are submitted far to near.
 */
static void HOT(triangle)(Vertex a,Vertex b,Vertex c){
    if(a.y>b.y){Vertex q=a;a=b;b=q;}if(b.y>c.y){Vertex q=b;b=c;c=q;}if(a.y>b.y){Vertex q=a;a=b;b=q;}
    float area=(b.x-a.x)*(c.y-a.y)-(c.x-a.x)*(b.y-a.y);
    if(fabsf(area)<.035f||c.y<0||a.y>=HEIGHT)return;
    float inv=1.f/area;
    float ux=((b.u-a.u)*(c.y-a.y)-(c.u-a.u)*(b.y-a.y))*inv;
    float uy=((b.x-a.x)*(c.u-a.u)-(c.x-a.x)*(b.u-a.u))*inv;
    float vx=((b.v-a.v)*(c.y-a.y)-(c.v-a.v)*(b.y-a.y))*inv;
    float vy=((b.x-a.x)*(c.v-a.v)-(c.x-a.x)*(b.v-a.v))*inv;
    int du=(int)(ux*65536),dv=(int)(vx*65536);
    int ys=clampi((int)ceilf(a.y-.5f),0,HEIGHT),ye=clampi((int)ceilf(c.y-.5f),0,HEIGHT);
    float ls=(c.x-a.x)/(c.y-a.y+1e-9f),up=(b.x-a.x)/(b.y-a.y+1e-9f),down=(c.x-b.x)/(c.y-b.y+1e-9f);
    for(int y=ys;y<ye;y++){
        float fy=y+.5f,x0=a.x+(fy-a.y)*ls,x1=fy<b.y?a.x+(fy-a.y)*up:b.x+(fy-b.y)*down;
        if(x0>x1){float q=x0;x0=x1;x1=q;}
        int xs=clampi((int)ceilf(x0-.5f),0,WIDTH),xe=clampi((int)ceilf(x1-.5f),0,WIDTH);
        if(xs>=xe)continue;
        int u=(int)((a.u+ux*(xs+.5f-a.x)+uy*(fy-a.y))*65536);
        int v=(int)((a.v+vx*(xs+.5f-a.x)+vy*(fy-a.y))*65536);
        int off=y*WIDTH+xs;
#if PELAGIC_INTERP
        texture_span(u,v,du,dv);
        for(int x=xs;x<xe;x++,off++){
#if PELAGIC_SMOOTH
            unsigned uf=(texture_u()>>8)&255,vf=(texture_v()>>8)&255;
#endif
            uint32_t packed=texture_pop();
            int iu=(int)(packed&65535),iv=(int)(packed>>16);
#else
        for(int x=xs;x<xe;x++,off++,u+=du,v+=dv){
            int iu=u>>16,iv=v>>16;
#if PELAGIC_SMOOTH
            unsigned uf=((unsigned)u>>8)&255,vf=((unsigned)v>>8)&255;
#endif
#endif
            if((unsigned)iu>=384||(unsigned)iv>=256)continue;
#if PELAGIC_SMOOTH
            fb[off]=filtered_rgba(art_ray_color,art_ray_alpha,384,256,(unsigned)iu,(unsigned)iv,uf,vf,fb[off],ray_opacity);
#else
            int uv=iv*384+iu,alpha=(art_ray_alpha[uv]*ray_opacity)>>8;
            if(alpha>0)fb[off]=blend(fb[off],art_ray_color[uv],alpha);
#endif
        }
    }
    triangles++;
}
static void manta(float x,float y,float scale,float bank,float phase,int opacity){
    float co=cs(bank),si=sn(bank);ray_opacity=opacity;
    for(int j=0;j<=NY;j++)for(int i=0;i<=NX;i++){
        float u=i*(2.f/NX)-1,v=j*(2.f/NY)-1,wing=fabsf(u);
        float flap=sn(t*1.72f+phase-wing*1.65f);
        float xx=u*(.91f+.09f*cs(t*1.72f+phase));
        float yy=v*.67f+wing*wing*flap*.22f;
        float zz=wing*wing*flap*.42f;
        /* A flexible tail swims behind the relatively rigid central spine. */
        if(v>.28f){float tail=(v-.28f)/.72f;xx+=tail*tail*.075f*sn(t*2.7f+v*5+phase);}
        float perspective=1.f/(1.f+zz*.28f+v*.08f);
        mesh[j][i]=(Vertex){x+(xx*co-yy*si)*160*scale*perspective,
            y+(xx*si+yy*co)*160*scale*perspective,i*(383.f/NX),j*(255.f/NY)};
    }
    for(int j=0;j<NY;j++)for(int i=0;i<NX;i++){
        triangle(mesh[j][i],mesh[j][i+1],mesh[j+1][i+1]);
        triangle(mesh[j][i],mesh[j+1][i+1],mesh[j+1][i]);
    }
}

static void plankton(int front){
    int count=front?45:100;
    for(int i=0;i<count;i++){
        uint32_t h=hash((unsigned)i+front*800u);float speed=front?9.f:3.f;
        float y=fmodf((h&1023)*.31f-t*speed,280.f);if(y<0)y+=280;
        int x=(int)(((h>>10)%360)-20.f+8*sn(t*.32f+i));
        int yy=(int)y-20;uint16_t color=i%5?rgb(103,191,204):rgb(251,201,145);
        if(front&&i%4==0)glow(x,yy,3,color,8);
        pixel(x,yy,color,front?22:8);
        if(front&&i%3==0)pixel(x,yy+1,color,8);
    }
}
static void schools(void){
    int alpha=bar<8?5:bar<24?18:9;
    for(int group=0;group<3;group++)for(int i=0;i<54;i++){
        float ph=i*.069f+t*(.16f+group*.025f)+group*2.1f;
        float z=2.5f+sn(ph)*1.2f,spread=(int)(hash(i+group*120)&255)/255.f;
        float xx=sn(ph*1.3f+group)*230+(spread-.5f)*56;
        float yy=cs(ph*.8f)*74+(spread-.5f)*30+group*8-12;
        int x=(int)(160+xx/z),y=(int)(118+yy/z+8*sn(t*.14f));
        int len=(int)(6/z)+1,dir=cs(ph*1.3f+group)>0?1:-1;
        uint16_t color=group==1?rgb(170,219,207):rgb(38,120,153);
        line(x,y,x+len*dir,y-1,color,alpha);
        pixel(x-dir,y+(sn(t*8+i)>0?1:-1),color,alpha*3/4);
    }
}
static void currents(void){
    for(int k=0;k<7;k++){
        int px=-20,py=0;
        for(int j=0;j<60;j++){
            float x=j*6-20.f,y=170+k*9+14*sn(x*.011f+t*.35f+k*.24f);
            if(j)line(px,py,(int)x,(int)y,rgb(72,173,185),2);
            px=(int)x;py=(int)y;
        }
    }
}
static void medusa(float x,float y,float size,float phase,int alpha){
    float pulse=.87f+.13f*sn(t*2+phase);uint16_t c=rgb(164,195,249),warm=rgb(235,167,218);
    int radius=(int)(size*22*pulse);
    glow((int)x,(int)y,radius,c,alpha/3);
    /* Translucent bell, lit at the rim; ribs and tentacles remain live curves. */
    for(int j=-radius;j<=0;j++)for(int i=-radius;i<=radius;i++){
        float q=(i*i+j*j*2.8f)/(radius*radius);
        if(q<1){int a=(int)(alpha*(.07f+.22f*q*q));pixel((int)x+i,(int)y+j,warm,a);}
    }
    for(int ring=0;ring<4;ring++){
        int px=0,py=0;
        for(int k=0;k<=32;k++){
            float a=PI*k/32,rx=cs(a)*radius*(1-ring*.12f);
            float ry=-sn(a)*radius*.58f+ring*size*2;
            int xx=(int)(x+rx),yy=(int)(y+ry);
            if(k)line(px,py,xx,yy,ring%2?warm:c,alpha/(ring+1));px=xx;py=yy;
        }
    }
    for(int k=0;k<7;k++){
        int px=(int)(x+(k-3)*radius*.26f),py=(int)y;
        for(int j=1;j<25;j++){
            float yy=j*size*2,xx=x+(k-3)*radius*.26f+sn(j*.21f-t*1.4f+phase+k*.9f)*j*size*.19f;
            int iy=(int)(y+yy);line(px,py,(int)xx,iy,c,alpha*(26-j)/32);px=(int)xx;py=iy;
        }
    }
}
static void bloom(int front){
    float strength=smooth((bar-54)/3)*smooth((65-bar)/2);
    if(strength<=0)return;
    for(int i=0;i<380;i++){
        float q=i/380.f,angle=q*PI*12-t*.65f,z=sn(angle);
        if((z>0)!=front)continue;
        float radius=35+q*103;
        int x=(int)(160+cs(angle)*radius),y=(int)(128+(q-.5f)*145+z*radius*.20f);
        int a=(int)(strength*(10+(z+1)*6));uint16_t c=i%4?rgb(110,206,232):rgb(244,198,165);
        pixel(x,y,c,a);if(i%9==0)glow(x,y,3,c,a/3);
    }
}
static void text(int x,int y,const char *s,int spacing,uint16_t color,int a){
    while(*s){const uint8_t *g=font8x8_glyph(*s++);for(int j=0;j<7;j++)for(int i=0;i<7;i++)if(g[j]&(128>>i))pixel(x+i,y+j,color,a);x+=7+spacing;}
}
static void centered(int y,const char*s,int spacing,uint16_t c,int a){text((WIDTH-((int)strlen(s)*(7+spacing)-spacing))/2,y,s,spacing,c,a);}
static void title(int y,int a){
    uint16_t c=rgb(239,232,207);
    for(int j=0;j<48;j++)for(int i=0;i<280;i++){
        int alpha=(art_title_alpha[j*280+i]*a)>>8;
        if(alpha){pixel(i+21,y+j+2,rgb(1,16,33),alpha/2);pixel(i+20,y+j,c,alpha);}
    }
}
static void typography(void){
    if(bar<8){
        int a=(int)(32*smooth((t-1)/2)*smooth((15.36f-t)/2));
        centered(74,"L A T E N T",0,rgb(139,205,211),a);
        title(91,a);
        line(136,149,184,149,rgb(176,199,190),a/2);
        centered(165,"A JOURNEY BELOW THE LIGHT",0,rgb(171,207,210),a*3/4);
        centered(206,"PHASE  /  2026",1,rgb(149,188,198),a*3/4);
    }
    if(bar>=72){
        int a=(int)(32*smooth((bar-72)/1.5f));
        /* The surface plate is bright. A graduated blue veil gives the endcard
         * its own quiet exposure while the living scene continues underneath. */
        for(int y=62;y<HEIGHT;y++){
            int shade=(int)(a*.65f*smooth((y-62)/42.f));
            for(int x=0;x<WIDTH;x++)fb[y*WIDTH+x]=blend(fb[y*WIDTH+x],rgb(4,30,48),shade);
        }
        title(78,a);
        centered(138,"CODE + DIRECTION  PHASE",0,rgb(212,231,224),a);
        centered(155,"MUSIC  PHOSPHOR",0,rgb(212,231,224),a);
        centered(172,"MODELS  GPT-6 ASTRA + CLAUDE FABLE 5.1",0,rgb(174,212,214),a);
        centered(189,"FOR AZURE / FOR LATENT",0,rgb(174,212,214),a);
        centered(212,"THE OCEAN REMEMBERS",1,rgb(237,212,172),a);
    }
}
void demo_init(void){texture_init();for(int i=0;i<2048;i++)sine[i]=sinf(i*(2*PI/2048));}
unsigned demo_triangles(void){return triangles;}
Score score_at(uint32_t sample){float beat=sample/(float)BEAT_SAMPLES;int b=(int)(beat/4);return (Score){beat,1-(beat-floorf(beat)),beat/4-b,b,b<8?0:b<24?1:b<40?2:b<48?3:b<64?4:b<72?5:6};}
void demo_render(uint16_t *pixels,uint32_t sample){
    fb=pixels;triangles=0;
    if(sample>=DURATION_SAMPLES){memset(fb,0,WIDTH*HEIGHT*2);return;}
    t=sample/(float)SAMPLE_RATE;bar=sample/(float)(BEAT_SAMPLES*4);
    environment();currents();plankton(0);schools();bloom(0);
    if(bar>=8&&bar<72){
        float arrival=smooth((bar-8)/4),close=smooth((bar-24)/3),dive=smooth((bar-40)/8),rise=smooth((bar-64)/7);
        float scale=lerp(.27f,.89f,close)-dive*.24f+rise*.36f;
        float x=160+sn(t*.16f)*35+(1-arrival)*210;
        float y=123+sn(t*.23f)*8+dive*8-rise*67;
        float bank=.20f*sn(t*.21f)+dive*.27f-rise*.48f;
        /* Small companions live on a more distant plane. */
        manta(70+sn(t*.1f)*18,73+cs(t*.15f)*13,.17f,-.15f,2,10);
        manta(248+cs(t*.13f)*22,96+sn(t*.19f)*14,.12f,.28f,4,9);
        if(bar>46&&bar<68){
            int a=(int)(24*smooth((bar-46)/2)*smooth((68-bar)/3));
            float encounter=smooth((bar-48)/2)*smooth((57-bar)/3);
            medusa(46+sn(t*.17f)*12,87+sn(t*.32f)*10,.80f+encounter*.9f,0,a);
            medusa(269+sn(t*.21f)*14,58+sn(t*.27f)*15,1.05f,3,a);
            medusa(214+sn(t*.19f)*11,170+sn(t*.31f)*12,.43f,1,a/2);
        }
        manta(x,y,scale,bank,0,32);
    }
    if(bar>=72){float leave=smooth((bar-72)/5);manta(175+leave*95,65-leave*100,.45f*(1-leave*.6f),-.35f,0,(int)(24*(1-leave)));}
    bloom(1);plankton(1);typography();
    float fade=smooth(t/2.3f)*smooth((DURATION_SECONDS-t)/3.5f);
    int level=(int)(fade*32);
    if(level<32)for(int i=0;i<WIDTH*HEIGHT;i++)fb[i]=blend(0,fb[i],level);
}
