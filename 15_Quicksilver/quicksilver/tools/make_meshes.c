/* make_meshes.c — bake QUICKSILVER's chrome objects into a C header
 * (assets/_packed/meshes.h): per-vertex positions + normals + triangle index
 * lists for a subdivided icosphere, a torus knot, and a rounded cube. The
 * chrome env-map scene reflects the sky off these.
 *
 * Build & run (from quicksilver/):
 *     gcc -O2 -lm tools/make_meshes.c -o tools/make_meshes.exe
 *     ./tools/make_meshes.exe > assets/_packed/meshes.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

typedef struct { float x, y, z; } V;
static V vsub(V a, V b){ return (V){a.x-b.x,a.y-b.y,a.z-b.z}; }
static V vcross(V a, V b){ return (V){a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
static V vnorm(V a){ float l=sqrtf(a.x*a.x+a.y*a.y+a.z*a.z); if(l<1e-9f)l=1e-9f; return (V){a.x/l,a.y/l,a.z/l}; }

#define MAXV 20000
#define MAXT 40000
static V vt[MAXV]; static int nv;
static int tri[MAXT*3]; static int nt;

static void emit(const char *name, V *nrm) {
    /* compute per-vertex normals if not supplied (area-weighted face normals) */
    static V nn[MAXV];
    if (!nrm) {
        for (int i=0;i<nv;i++) nn[i]=(V){0,0,0};
        for (int f=0;f<nt;f++){
            int a=tri[f*3],b=tri[f*3+1],c=tri[f*3+2];
            V fn=vcross(vsub(vt[b],vt[a]),vsub(vt[c],vt[a]));
            nn[a].x+=fn.x;nn[a].y+=fn.y;nn[a].z+=fn.z;
            nn[b].x+=fn.x;nn[b].y+=fn.y;nn[b].z+=fn.z;
            nn[c].x+=fn.x;nn[c].y+=fn.y;nn[c].z+=fn.z;
        }
        for (int i=0;i<nv;i++) nn[i]=vnorm(nn[i]);
        nrm=nn;
    }
    printf("#define %s_NV %d\n#define %s_NT %d\n", name, nv, name, nt);
    printf("static const qs_mvec %s_v[%d] = {\n", name, nv);
    for (int i=0;i<nv;i++) printf("{%.5ff,%.5ff,%.5ff},%s", vt[i].x,vt[i].y,vt[i].z, (i%4==3)?"\n":"");
    printf("};\nstatic const qs_mvec %s_n[%d] = {\n", name, nv);
    for (int i=0;i<nv;i++) printf("{%.5ff,%.5ff,%.5ff},%s", nrm[i].x,nrm[i].y,nrm[i].z, (i%4==3)?"\n":"");
    printf("};\nstatic const uint16_t %s_t[%d] = {\n", name, nt*3);
    for (int i=0;i<nt*3;i++) printf("%d,%s", tri[i], (i%12==11)?"\n":"");
    printf("};\n\n");
}

/* Flip any triangle whose geometric normal points inward (toward the origin),
 * so a star-shaped mesh ends up consistently CCW-outward. Safe for sphere /
 * spike / cube (all star-shaped about the origin); NOT for torus/knot (handled
 * by explicit winding there). */
static void reorient_outward(void){
    for(int f=0;f<nt;f++){
        int a=tri[f*3],b=tri[f*3+1],c=tri[f*3+2];
        V cen={(vt[a].x+vt[b].x+vt[c].x)/3.f,(vt[a].y+vt[b].y+vt[c].y)/3.f,(vt[a].z+vt[b].z+vt[c].z)/3.f};
        V n=vcross(vsub(vt[b],vt[a]),vsub(vt[c],vt[a]));
        if(n.x*cen.x+n.y*cen.y+n.z*cen.z < 0.f){ tri[f*3+1]=c; tri[f*3+2]=b; }
    }
}

/* ---- icosphere ---------------------------------------------------------- */
static int midcache_a[MAXT*3], midcache_b[MAXT*3], midcache_m[MAXT*3], nmid;
static int midpoint(int a,int b){
    int lo=a<b?a:b, hi=a<b?b:a;
    for(int i=0;i<nmid;i++) if(midcache_a[i]==lo&&midcache_b[i]==hi) return midcache_m[i];
    V m=vnorm((V){(vt[a].x+vt[b].x)*.5f,(vt[a].y+vt[b].y)*.5f,(vt[a].z+vt[b].z)*.5f});
    vt[nv]=m; int id=nv++; midcache_a[nmid]=lo;midcache_b[nmid]=hi;midcache_m[nmid]=id;nmid++;
    return id;
}
static void gen_icosphere_geom(int subdiv){
    nv=0;nt=0;nmid=0;
    float t=(1.f+sqrtf(5.f))*.5f;
    V base[12]={{-1,t,0},{1,t,0},{-1,-t,0},{1,-t,0},{0,-1,t},{0,1,t},{0,-1,-t},{0,1,-t},{t,0,-1},{t,0,1},{-t,0,-1},{-t,0,1}};
    for(int i=0;i<12;i++) vt[nv++]=vnorm(base[i]);
    int f0[20*3]={0,11,5,0,5,1,0,1,7,0,7,10,0,10,11,1,5,9,5,11,4,11,10,2,10,7,6,7,1,8,3,9,4,3,4,2,3,2,6,3,6,8,3,8,9,4,9,5,2,4,11,6,2,10,8,6,7,9,8,1};
    for(int i=0;i<20*3;i++) tri[i]=f0[i]; nt=20;
    for(int s=0;s<subdiv;s++){
        int newtri[MAXT*3]; int n2=0; nmid=0;
        for(int f=0;f<nt;f++){
            int a=tri[f*3],b=tri[f*3+1],c=tri[f*3+2];
            int ab=midpoint(a,b),bc=midpoint(b,c),ca=midpoint(c,a);
            int q[4*3]={a,ab,ca, b,bc,ab, c,ca,bc, ab,bc,ca};
            for(int k=0;k<12;k++) newtri[n2*3+k]=q[k]; n2+=4;
        }
        memcpy(tri,newtri,sizeof(int)*n2*3); nt=n2;
    }
}
static void gen_icosphere(const char *name,int subdiv){
    gen_icosphere_geom(subdiv);
    static V nrm[MAXV]; for(int i=0;i<nv;i++) nrm[i]=vt[i];   /* sphere normals = positions */
    emit(name, nrm);
}

/* ---- torus knot --------------------------------------------------------- */
static void gen_knot(const char *name,int along,int around,int P,int Q,float tube){
    nv=0;nt=0;
    for(int i=0;i<along;i++){
        float u=(float)i/along*2*M_PI;
        float pu=P*u, qu=Q*u;
        float r=2.f+cosf(qu);
        V c={r*cosf(pu), r*sinf(pu), -sinf(qu)};
        /* tangent (finite diff) */
        float u2=u+0.001f; float r2=2.f+cosf(Q*u2);
        V c2={r2*cosf(P*u2), r2*sinf(P*u2), -sinf(Q*u2)};
        V T=vnorm(vsub(c2,c));
        V N=vnorm(vcross(T,(V){0,0,1}));
        V B=vnorm(vcross(T,N));
        for(int j=0;j<around;j++){
            float v=(float)j/around*2*M_PI;
            V off={ (N.x*cosf(v)+B.x*sinf(v)), (N.y*cosf(v)+B.y*sinf(v)), (N.z*cosf(v)+B.z*sinf(v)) };
            vt[nv++]=(V){ c.x+tube*off.x, c.y+tube*off.y, c.z+tube*off.z };
        }
    }
    for(int i=0;i<along;i++)for(int j=0;j<around;j++){
        int i1=(i+1)%along, j1=(j+1)%around;
        int a=i*around+j, b=i1*around+j, d=i*around+j1, e=i1*around+j1;
        tri[nt*3]=a;tri[nt*3+1]=d;tri[nt*3+2]=b;nt++;   /* CCW-outward winding */
        tri[nt*3]=d;tri[nt*3+1]=e;tri[nt*3+2]=b;nt++;
    }
    /* scale to ~unit */
    float mx=0; for(int i=0;i<nv;i++){float l=sqrtf(vt[i].x*vt[i].x+vt[i].y*vt[i].y+vt[i].z*vt[i].z); if(l>mx)mx=l;}
    for(int i=0;i<nv;i++){vt[i].x/=mx;vt[i].y/=mx;vt[i].z/=mx;}
    emit(name, NULL);
}

/* ---- spiky stellated sphere: icosphere with radial spikes -------------- */
static void gen_spike(const char *name,int subdiv,int freq,float amp){
    gen_icosphere_geom(subdiv);   /* fills vt/tri/nv/nt, unit sphere */
    static V nrm[MAXV];
    for(int i=0;i<nv;i++){
        V d=vt[i];
        /* spike factor from a high-frequency function of direction */
        float s=1.f + amp*powf(fabsf(sinf(d.x*freq)*sinf(d.y*freq)*sinf(d.z*freq)),0.5f);
        vt[i]=(V){d.x*s,d.y*s,d.z*s};
    }
    (void)nrm;
    emit(name,NULL);   /* recompute smooth-ish normals from faces */
}

/* ---- twisted torus (supertoroid with a twist) -------------------------- */
static void gen_torus(const char *name,int along,int around,float R,float r,int twist){
    nv=0;nt=0;
    for(int i=0;i<along;i++){
        float u=(float)i/along*2*M_PI;
        for(int j=0;j<around;j++){
            float v=(float)j/around*2*M_PI + twist*u;   /* twist couples v to u */
            float cr=R + r*cosf(v);
            vt[nv++]=(V){ cr*cosf(u), r*sinf(v), cr*sinf(u) };
        }
    }
    for(int i=0;i<along;i++)for(int j=0;j<around;j++){
        int i1=(i+1)%along,j1=(j+1)%around;
        int a=i*around+j,b=i1*around+j,d=i*around+j1,e=i1*around+j1;
        tri[nt*3]=a;tri[nt*3+1]=d;tri[nt*3+2]=b;nt++;   /* CCW-outward winding */
        tri[nt*3]=d;tri[nt*3+1]=e;tri[nt*3+2]=b;nt++;
    }
    float mx=0; for(int i=0;i<nv;i++){float l=sqrtf(vt[i].x*vt[i].x+vt[i].y*vt[i].y+vt[i].z*vt[i].z); if(l>mx)mx=l;}
    for(int i=0;i<nv;i++){vt[i].x/=mx;vt[i].y/=mx;vt[i].z/=mx;}
    emit(name,NULL);
}

/* ---- rounded cube (spherified subdivided cube) -------------------------- */
static void gen_roundcube(int grid,float round){
    nv=0;nt=0;
    int faces[6][3]={{0,1,2},{0,2,1},{1,2,0},{1,0,2},{2,0,1},{2,1,0}}; (void)faces;
    /* 6 faces, each a grid of (grid+1)^2 verts */
    int base[6]={0};
    for(int f=0;f<6;f++){
        base[f]=nv;
        int axis=f/2, sign=(f&1)?-1:1;
        for(int a=0;a<=grid;a++)for(int b=0;b<=grid;b++){
            float ua=(float)a/grid*2-1, ub=(float)b/grid*2-1;
            V p;
            if(axis==0) p=(V){sign, ua, ub};
            else if(axis==1) p=(V){ua, sign, ub};
            else p=(V){ua, ub, sign};
            V s=vnorm(p);
            vt[nv++]=(V){ p.x*(1-round)+s.x*round, p.y*(1-round)+s.y*round, p.z*(1-round)+s.z*round };
        }
    }
    for(int f=0;f<6;f++){
        for(int a=0;a<grid;a++)for(int b=0;b<grid;b++){
            int row=grid+1;
            int p0=base[f]+a*row+b, p1=base[f]+(a+1)*row+b, p2=base[f]+a*row+b+1, p3=base[f]+(a+1)*row+b+1;
            tri[nt*3]=p0;tri[nt*3+1]=p1;tri[nt*3+2]=p2;nt++;
            tri[nt*3]=p2;tri[nt*3+1]=p1;tri[nt*3+2]=p3;nt++;
        }
    }
    reorient_outward();    /* make all 6 faces consistently CCW-outward */
    emit("CUBE", NULL);
}

int main(void){
    printf("/* GENERATED by tools/make_meshes.c — do not edit. Chrome objects for envmap3d. */\n");
    printf("#ifndef QS_MESHES_H\n#define QS_MESHES_H\n#include <stdint.h>\n");
    printf("typedef struct { float x,y,z; } qs_mvec;\n\n");
    gen_icosphere("ICO", 3);          /* 642 v / 1280 t  — chrome ball     */
    gen_knot("KNOT", 64,18,2,3,0.55f);/* 1152 v / 2304 t — trefoil         */
    gen_roundcube(10,0.55f);          /* 726 v / 1200 t  — rounded cube    */
    gen_knot("KNOT2", 100,16,3,5,0.32f);/* (3,5) knot — intricate          */
    gen_spike("SPIKE", 3, 6, 0.55f);  /* spiky stellated chrome ball       */
    gen_torus("TORUS", 80,24,0.66f,0.34f, 3); /* twisted torus             */
    printf("#endif\n");
    fprintf(stderr,"meshes generated\n");
    return 0;
}
