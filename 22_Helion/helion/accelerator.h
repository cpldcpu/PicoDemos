/* Power-of-two indexed texture addressing; no colour arithmetic in the
 * texture loop. INTERP1 POP returns a byte offset and advances U and V.
 * INTERP0 BLEND builds the material palette outside the pixel loop.
 */
#ifndef HELION_ACCELERATOR_H
#define HELION_ACCELERATOR_H
#include <stdint.h>
#include <string.h>
#include "assets.h"
#ifndef HELION_INTERP
#define HELION_INTERP 1
#endif
#ifndef HELION_DMA
#define HELION_DMA 1
#endif
#if defined(PICO_BUILD)
#if HELION_INTERP
#include "hardware/interp.h"
#endif
#include "hardware/dma.h"
#endif
static unsigned addr_bits;
#if !defined(PICO_BUILD) || !HELION_INTERP
static uint32_t au,av,adu,adv;
#endif
static inline void accelerator_init(void){
#if defined(PICO_BUILD) && HELION_INTERP
    interp_claim_lane_mask(interp0,3);interp_claim_lane_mask(interp1,3);
    interp_config c=interp_default_config();interp_config_set_blend(&c,true);interp_set_config(interp0,0,&c);
    c=interp_default_config();interp_config_set_mask(&c,0,7);interp_set_config(interp0,1,&c);
#endif
#if defined(PICO_BUILD) && HELION_DMA
    dma_channel_claim(8);
#endif
}
static inline void texture_config(unsigned bits){
    addr_bits=bits;
#if defined(PICO_BUILD) && HELION_INTERP
    interp_config c=interp_default_config();interp_config_set_shift(&c,16);interp_config_set_mask(&c,0,bits-1);
    interp_config_set_add_raw(&c,true);interp_set_config(interp1,0,&c);
    c=interp_default_config();interp_config_set_shift(&c,16-bits);interp_config_set_mask(&c,bits,bits*2-1);
    interp_config_set_add_raw(&c,true);interp_set_config(interp1,1,&c);interp_set_base(interp1,2,0);
#endif
}
static inline void texture_span(int32_t u,int32_t v,int32_t du,int32_t dv){
#if defined(PICO_BUILD) && HELION_INTERP
    interp_set_accumulator(interp1,0,(uint32_t)u);interp_set_accumulator(interp1,1,(uint32_t)v);
    interp_set_base(interp1,0,(uint32_t)du);interp_set_base(interp1,1,(uint32_t)dv);
#else
    au=(uint32_t)u;av=(uint32_t)v;adu=(uint32_t)du;adv=(uint32_t)dv;
#endif
}
static inline unsigned texture_pop(void){
#if defined(PICO_BUILD) && HELION_INTERP
    return interp_pop_full_result(interp1);
#else
    unsigned mask=(1u<<addr_bits)-1;
    unsigned off=((au>>16)&mask)|(((av>>16)&mask)<<addr_bits);
    au+=adu;av+=adv;return off;
#endif
}
static inline unsigned palette_lerp(unsigned a,unsigned b,unsigned weight){
#if defined(PICO_BUILD) && HELION_INTERP
    interp_set_base(interp0,0,a);interp_set_base(interp0,1,b);interp_set_accumulator(interp0,1,weight);
    return interp_peek_lane_result(interp0,1);
#else
    return (a*(256-weight)+b*weight)>>8;
#endif
}
static inline void background_begin(uint16_t *dst){
#if defined(PICO_BUILD) && HELION_DMA
    dma_channel_config c=dma_channel_get_default_config(8);
    channel_config_set_transfer_data_size(&c,DMA_SIZE_32);channel_config_set_read_increment(&c,true);channel_config_set_write_increment(&c,true);
    dma_channel_configure(8,&c,dst,art_sky,320*240/2,true);
#else
    memcpy(dst,art_sky,320*240*2);
#endif
}
static inline void background_wait(void){
#if defined(PICO_BUILD) && HELION_DMA
    dma_channel_wait_for_finish_blocking(8);
#endif
}
/* Runs against real registers on the board, including negative steps and
 * accumulator wrap. Returns zero only when both SIO contracts hold. */
static inline int accelerator_selftest(void){
    for(unsigned bits=7;bits<=8;bits++){
        texture_config(bits);uint32_t u=0xfffe8000u,v=0x0001ffffu,du=79191,dv=(uint32_t)-17309;
        texture_span((int32_t)u,(int32_t)v,(int32_t)du,(int32_t)dv);
        for(int i=0;i<512;i++){
            unsigned mask=(1u<<bits)-1,expected=((u>>16)&mask)|(((v>>16)&mask)<<bits);
            if(texture_pop()!=expected)return 1;u+=du;v+=dv;
        }
    }
    for(unsigned w=0;w<256;w++)if(palette_lerp(231,17,w)!=(231*(256-w)+17*w)/256)return 2;
    return 0;
}
#endif
