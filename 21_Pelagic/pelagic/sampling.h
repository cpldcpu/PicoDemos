/* Optional texture filtering and core-0 SIO coordinate walker.
 * INTERP does coordinate extraction/stepping, NOT four-tap colour filtering.
 * Textures have non-power-of-two strides, so FULL packs integer U,V; the
 * CPU applies the actual stride. No padded duplicate textures are needed.
 */
#ifndef PELAGIC_SAMPLING_H
#define PELAGIC_SAMPLING_H
#include <stdint.h>
#ifndef PELAGIC_SMOOTH
#define PELAGIC_SMOOTH 0
#endif
#ifndef PELAGIC_INTERP
#define PELAGIC_INTERP 0
#endif
#if PELAGIC_INTERP && defined(PICO_BUILD)
#include "hardware/interp.h"
static inline void texture_init(void){
    static int claimed;
    if(!claimed){interp_claim_lane_mask(interp0,3);claimed=1;}
    interp_config c=interp_default_config();
    interp_config_set_shift(&c,16);interp_config_set_mask(&c,0,15);
    interp_config_set_add_raw(&c,true);interp_set_config(interp0,0,&c);
    c=interp_default_config();interp_config_set_mask(&c,16,31);
    interp_config_set_add_raw(&c,true);interp_set_config(interp0,1,&c);
    interp_set_base(interp0,2,0);
}
static inline void texture_span(int u,int v,int du,int dv){
    interp_set_accumulator(interp0,0,(uint32_t)u);interp_set_accumulator(interp0,1,(uint32_t)v);
    interp_set_base(interp0,0,(uint32_t)du);interp_set_base(interp0,1,(uint32_t)dv);
}
static inline uint32_t texture_u(void){return interp_get_accumulator(interp0,0);}
static inline uint32_t texture_v(void){return interp_get_accumulator(interp0,1);}
static inline uint32_t texture_pop(void){return interp_pop_full_result(interp0);}
#else
/* Host model of precisely the configured datapath: unsigned 32-bit wrapping
 * accumulators, shift/mask FULL, ADD_RAW lane feedback on every POP. */
static uint32_t texture_accum[2],texture_step[2];
static inline void texture_init(void){}
static inline void texture_span(int u,int v,int du,int dv){texture_accum[0]=(uint32_t)u;texture_accum[1]=(uint32_t)v;texture_step[0]=(uint32_t)du;texture_step[1]=(uint32_t)dv;}
static inline uint32_t texture_u(void){return texture_accum[0];}
static inline uint32_t texture_v(void){return texture_accum[1];}
static inline uint32_t texture_pop(void){
    uint32_t r=(texture_accum[0]>>16)|(texture_accum[1]&0xffff0000u);
    texture_accum[0]+=texture_step[0];texture_accum[1]+=texture_step[1];return r;
}
#endif
static inline void tap_weights(unsigned uf,unsigned vf,unsigned w[4]){
    w[0]=(256-uf)*(256-vf);w[1]=uf*(256-vf);w[2]=(256-uf)*vf;w[3]=uf*vf;
}
static inline uint16_t filtered_color(const uint16_t *image,int width,int height,int u,int v){
    if(u<0)u=0;if(v<0)v=0;
    if(u>(width-1)*65536)u=(width-1)*65536;
    if(v>(height-1)*65536)v=(height-1)*65536;
    int x=u>>16,y=v>>16,dx=x<width-1,dy=y<height-1?width:0,off=y*width+x;
    unsigned w[4];tap_weights(((unsigned)u>>8)&255,((unsigned)v>>8)&255,w);
    uint16_t c[4]={image[off],image[off+dx],image[off+dy],image[off+dy+dx]};
    unsigned r=32768,g=32768,b=32768;
    for(int i=0;i<4;i++){r+=(c[i]&31)*w[i];g+=((c[i]>>6)&31)*w[i];b+=((c[i]>>11)&31)*w[i];}
    return (uint16_t)((r>>16)|((g>>16)<<6)|((b>>16)<<11));
}
/* Input integer coordinates already bounds-checked by the triangle loop.
 * Premultiply before filtering, then composite without unpremultiplication.
 * RGB in transparent texels cannot bleed into the visible silhouette.
 */
static inline uint16_t filtered_rgba(const uint16_t *image,const uint8_t *alpha,int width,int height,
                                    unsigned x,unsigned y,unsigned uf,unsigned vf,uint16_t dst,unsigned opacity){
    unsigned dx=x<(unsigned)width-1,dy=y<(unsigned)height-1?(unsigned)width:0,off=y*width+x;
    unsigned offsets[4]={off,off+dx,off+dy,off+dy+dx},w[4];tap_weights(uf,vf,w);
    uint32_t a=32768,r=32768,g=32768,b=32768;
    for(int i=0;i<4;i++){
        unsigned wa=w[i]*alpha[offsets[i]];uint16_t c=image[offsets[i]];
        a+=wa;r+=(c&31)*wa;g+=((c>>6)&31)*wa;b+=((c>>11)&31)*wa;
    }
    unsigned remain=8192-(a>>16)*opacity;
    r=((r>>16)*opacity+(dst&31)*remain+4096)>>13;
    g=((g>>16)*opacity+((dst>>6)&31)*remain+4096)>>13;
    b=((b>>16)*opacity+((dst>>11)&31)*remain+4096)>>13;
    return (uint16_t)(r|(g<<6)|(b<<11));
}
#endif
