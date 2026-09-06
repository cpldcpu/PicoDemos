/* Exact §8 ceiling: 5 layers × 15 × 10 quads = 1500 triangles;
 * 5 × 300 × 60 = 90000 candidate fragments. All layers shade, far first.
 * All four material paths, emissive cells, 256 embers, restricted bloom.
 * Overscan: call this instead of demo_render for device profiling. */
#include "render.h"
#include "song.h"
#include <math.h>
static RVertex vertex(float sx,float sy,float z,float u,float v,float light,float emission)
{
    return (RVertex){(sx-160)*z/190,(120-sy)*z/190,z,light,u,v,emission};
}
void render_material_test(uint16_t *page,uint32_t sample)
{
    r_begin(page,(unsigned)song_section(cv_bar_of(sample)),(RCamera){1,12,0,0,0,0,190});
    r_background(sample);
    float phase=(float)(sample%(CV_BAR*8))/(CV_BAR*8),shift=12*sinf(phase*6.2831853f);
    for(int layer=0;layer<5;layer++){
        float z=8-layer;
        for(int y=0;y<10;y++)for(int x=0;x<15;x++){
            float left=10+x*20,top=90+y*6,u=8+shift,v=8+shift;
            float e=(x==7 && y>=3 && y<=6)?180:0;
            RVertex a=vertex(left,top,z,u,v,80,e),b=vertex(left+20,top,z,u+40,v,210,e);
            RVertex c=vertex(left+20,top+6,z,u+40,v+40,240,e),d=vertex(left,top+6,z,u,v+40,120,e);
            RMaterial mat=(RMaterial)((x+y+layer)%4);
            r_triangle(a,b,c,mat);r_triangle(a,c,d,mat);
        }
    }
    r_embers(sample,256);r_bloom();
    r_inscription("MATERIAL CEILING",18,28,cv_rgb(184,168,144));
    r_inscription("1500 TRI  90000 FILL",18,44,cv_rgb(184,168,144));
    r_finish();
}
