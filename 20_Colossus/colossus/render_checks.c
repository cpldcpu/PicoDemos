/* Host-only adversarial checks for clipping, depth, bloom and seek purity. */
#ifdef HOST_BUILD
#include "render.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {uint32_t before;uint16_t pixels[320*240];uint32_t after;} guarded;
static uint16_t reference[320*240];
static void begin(void){r_begin(guarded.pixels,2,(RCamera){2,10,0,0,0,0,190});r_background(26*CV_BAR);}
static void triangle(float z,float l,float e){r_triangle((RVertex){-1,-1,z,l,0,0,e},(RVertex){1,-1,z,l,63,0,e},(RVertex){0,1,z,l,31,63,e},R_GOURAUD);}
int main(void)
{
    demo_init();guarded.before=0x76543210;guarded.after=0x01234567;
    /* Seek ordering, including both sides of every bar boundary and end clamps. */
    unsigned seeks=0;
    for(unsigned b=0;b<=160;b++)for(unsigned j=0;j<3;j++){
        uint32_t s=b*CV_BAR+(j==0?0:j==1?CV_BAR/2:CV_BAR-1);
        demo_render(guarded.pixels,s);memcpy(reference,guarded.pixels,sizeof reference);
        demo_render(guarded.pixels,(159-b%160)*CV_BAR+1234);demo_render(guarded.pixels,s);
        assert(!memcmp(reference,guarded.pixels,sizeof reference));seeks++;
    }
    demo_render(guarded.pixels,UINT32_MAX);for(int i=0;i<320*240;i++)assert(guarded.pixels[i]==0);
    /* Non-emissive bright chrome does not bloom. */
    begin();triangle(3,255,0);memcpy(reference,guarded.pixels,sizeof reference);r_bloom();assert(!memcmp(reference,guarded.pixels,sizeof reference));
    /* Hidden emissive geometry submitted after an occluder seeds no glow. */
    begin();triangle(3,100,0);memcpy(reference,guarded.pixels,sizeof reference);triangle(6,255,200);r_bloom();assert(!memcmp(reference,guarded.pixels,sizeof reference));
    /* Depth ordering is independent of submission for separated surfaces. */
    begin();triangle(3,100,0);triangle(6,255,0);memcpy(reference,guarded.pixels,sizeof reference);
    begin();triangle(6,255,0);triangle(3,100,0);assert(!memcmp(reference,guarded.pixels,sizeof reference));
    /* Wholly clipped triangles cannot touch page; crossing triangles must draw. */
    begin();memcpy(reference,guarded.pixels,sizeof reference);triangle(1,255,200);triangle(11,255,200);assert(!memcmp(reference,guarded.pixels,sizeof reference));
    for(int m=0;m<4;m++){
        begin();memcpy(reference,guarded.pixels,sizeof reference);
        r_triangle((RVertex){-1,-1,1,20,0,0,0},(RVertex){1,-1,4,250,63,0,0},(RVertex){0,1,12,100,31,63,0},(RMaterial)m);
        assert(memcmp(reference,guarded.pixels,sizeof reference));
    }
    /* A known z=2 intersection: attributes at both cut edges are exact halves.
       Compare automatic clipping with independently specified clipped geometry. */
    for(int m=0;m<4;m++){
        RVertex a={-2,0,0,0,0,0,0},b={1,-1,4,240,60,0,0},c={1,1,4,120,60,60,0};
        RVertex ab={-.5f,-.5f,2,120,30,0,0},ca={-.5f,.5f,2,60,30,30,0};
        begin();r_triangle(a,b,c,(RMaterial)m);memcpy(reference,guarded.pixels,sizeof reference);
        begin();r_triangle(ab,b,c,(RMaterial)m);r_triangle(ab,c,ca,(RMaterial)m);
        assert(!memcmp(reference,guarded.pixels,sizeof reference));
    }
    for(unsigned j=0;j<16;j++){
        render_material_test(guarded.pixels,24*CV_BAR+j*CV_STEP);demo_stats_t s;demo_stats(&s);
        assert(s.triangles==1500&&s.fill==90000&&s.particles==256);
    }
    assert(guarded.before==0x76543210&&guarded.after==0x01234567);
    printf("PASS: %u seek comparisons, end clamp, bloom mask, hidden source, depth order, near/far clipping and exact cut-edge attributes all materials, 16 exact ceiling frames, page guards\n",seeks);
    return 0;
}
#endif
