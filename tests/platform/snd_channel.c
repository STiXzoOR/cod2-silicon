#include "PC/snd.c"
#include <assert.h>
snd_local_t g_snd;
void CG_GetEntityOrientation(int n, float *origin, float (*axis)[3])
{
    assert(n == 7);
    origin[0]=10; origin[1]=20; origin[2]=30;
    memset(axis, 0, 9*sizeof(float));
    axis[0][1]=1; axis[1][0]=-1; axis[2][2]=1;
}
int main(void)
{
    snd_alias_t alias = {0};
    vec3_t origin = {12,23,34};
    alias.flags = 1 << 7;
    g_snd.time=100; g_snd.looptime=90; g_snd.paused=1; g_snd.pauseSettings[1]=1;
    SND_SetChannelInfo(32,2048,&alias,&alias,0.25f,NULL,0.8f,1.2f,2,44100,200,20,5,1,0);
    snd_channel_info_t *c=&g_snd.chaninfo[32];
    assert(c->pAlias0==&alias && c->pAlias1==&alias);
    assert(c->endtime==280 && c->looptime==90 && c->startDelay==5);
    assert(c->paused && c->master && c->baserate==44100 && c->srcChannelCount==2);
    alias.flags=0;
    SND_SetChannelInfo(2,7,&alias,&alias,0,origin,1,1,1,22050,100,0,0,0,1);
    assert(g_snd.chaninfo[2].offset[0]==3 && g_snd.chaninfo[2].offset[1]==-2 && g_snd.chaninfo[2].offset[2]==4);
    puts("native sound channel aliases, timing, pause and rotated entity offset: PASS");
}
