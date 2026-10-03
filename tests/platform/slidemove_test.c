#include "PC/bgame/bg_slidemove.c"
#include <assert.h>
#include <math.h>
static int traces;
void PM_playerTrace(pmove_t *pm, trace_t *trace, const float *start, const float *mins, const float *maxs, const float *end, int entity, int mask)
{
    const float normals[4][3]={{.8f,.6f,0},{.7f,-.7141428f,0},{.5f,.8660254f,0},{.5f,-.8660254f,0}};
    (void)pm;(void)start;(void)mins;(void)maxs;(void)end;(void)entity;(void)mask;
    assert(traces<4);
    memset(trace,0,sizeof(*trace));trace->fraction=.5f;
    memcpy(trace->normal,normals[traces++],sizeof(vec3_t));
}
void PM_AddTouchEnt(pmove_t *pm,int entity){(void)pm;(void)entity;}
const float Vec3NormalizeTo(const float *in,float *out)
{float length=sqrtf(in[0]*in[0]+in[1]*in[1]+in[2]*in[2]);for(int i=0;i<3;i++)out[i]=in[i]/length;return length;}
const float Vec3Normalize(float *v){return Vec3NormalizeTo(v,v);}
void PM_ClipVelocity(const float *in,const float *normal,float *out){(void)normal;memcpy(out,in,sizeof(vec3_t));}
void Vec3Cross(const float *a,const float *b,float *c){(void)a;(void)b;memset(c,0,sizeof(vec3_t));}
int main(void)
{
    playerState_t ps={0};pmove_t pm={0};pml_t pml={0};
    pm.ps=&ps;ps.velocity[0]=100;pml.frametime=.01f;pml.groundPlane=1;pml.groundTrace.normal[2]=1;
    assert(PM_SlideMove(&pm,&pml,0));
    assert(traces==4 && isfinite(ps.origin[0]));
    puts("four collision bumps plus two initial clip planes fit native movement storage: PASS");
}
