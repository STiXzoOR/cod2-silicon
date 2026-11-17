#include "shader_fixed.h"
#include <cassert>
#include <initializer_list>
#include <utility>
#include <cstring>
#include <cstdio>
int main()
{
    MacMetalFixedState fixed;
    MacMetalFixed_Init(&fixed);
    MacMetalState state = {};
    state.viewport[2] = 19; state.viewport[3] = 13;
    for (unsigned matrix : {2u,3u,256u})
        for (unsigned i = 0; i < 4; ++i) state.transform[matrix][i*5] = 1;
    MacMetalARBConstants constants;
    char error[256];
    assert(MacMetalFixed_Pack(&fixed, &state, &constants, error, sizeof(error)));
    state.textureStage[0][1] = 22;
    assert(!MacMetalFixed_Pack(&fixed, &state, &constants, error, sizeof(error)));
    assert(strstr(error,"operation"));
    state.textureStage[0][1] = 4; state.textureStage[0][4] = 1;
    assert(!MacMetalFixed_Pack(&fixed, &state, &constants, error, sizeof(error)));
    struct { float xyz[3]; unsigned color; float uv[2]; } vertex = {{1,2,3},0x80402010,{.25f,.75f}};
    MacMetalARBInput input;
    assert(MacMetalFixed_ExpandFVF(0x142, &vertex, sizeof(vertex), &input));
    assert(input.attrib[0].v[0] == 1 && input.attrib[0].v[3] == 1);
    assert(input.attrib[5].v[0] == 64.f/255 && input.attrib[5].v[3] == 128.f/255);
    assert(input.attrib[8].v[1] == .75f && input.attrib[8].v[3] == 1);
    assert(!MacMetalFixed_ExpandFVF(0x142, &vertex, sizeof(vertex)-1, &input));
    assert(!MacMetalFixed_ExpandFVF(0x146, &vertex, sizeof(vertex), &input));
    uint32_t planeIndex=0;float plane[4]={1,0,0,-.17f};const void *planePointer=plane;
    const void *args[]={nullptr,&planeIndex,&planePointer};int32_t result=-1;
    assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_SetClipPlane",args,&result,sizeof(result)));
    assert(result==0);
    MacMetalFixed_InitDefaults(&state);
    assert(state.render[141]==1 && state.render[142]==1 && state.render[145]==1 && state.render[146]==2);
    assert(state.textureStage[1][1]==1 && state.textureStage[1][11]==1);
    assert(state.transform[256][0]==1 && state.transform[16][15]==1);
    assert(MacMetalFixed_Source());
    // Engine genEyeDirCoords uses camera position (2); the original shim
    // independently supports reflection (3). Normal generation is unreachable.
    for (unsigned generation:{0u,2u,3u}) {
        state.textureStage[0][11]=generation<<16;
        assert(MacMetalFixed_Pack(&fixed,&state,&constants,error,sizeof(error)));
        unsigned packedIndex;memcpy(&packedIndex,&constants.local[2],4);
        assert(packedIndex==generation<<16);
    }
    state.textureStage[0][11]=0x10000;constants.env[0].v[0]=123;
    assert(!MacMetalFixed_Pack(&fixed,&state,&constants,error,sizeof(error)) && constants.env[0].v[0]==123);
    assert(strstr(error,"normal generation"));
    state.textureStage[0][11]=0x40000;
    assert(!MacMetalFixed_Pack(&fixed,&state,&constants,error,sizeof(error)) && strstr(error,"coordinate generation"));
    state.textureStage[0][11]=0;
    for (auto fixture:std::initializer_list<std::pair<unsigned,unsigned>>{{0x152,36},{0x242,32},{0x102,20},{0x142,24}}) {
        unsigned char bytes[64]={};
        assert(MacMetalFixed_ExpandFVF(fixture.first,bytes,fixture.second,&input));
        assert(!MacMetalFixed_ExpandFVF(fixture.first,bytes,fixture.second-1,&input));
    }
    MacMetalFixedMaterial material={};material.power=17;
    const void *materialPointer=&material;const void *materialArgs[]={nullptr,&materialPointer};
    assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_SetMaterial",materialArgs,&result,sizeof(result)) && result==0 && fixed.material.power==17);
    MacMetalFixedLight light={};light.type=3;light.direction[2]=-1;uint32_t lightIndex=0;const void *lightPointer=&light;
    const void *lightArgs[]={nullptr,&lightIndex,&lightPointer};
    assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_SetLight",lightArgs,&result,sizeof(result)) && result==0 && fixed.lights[0].type==3);
    int32_t enabled=1;const void *enableArgs[]={nullptr,&lightIndex,&enabled};
    assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_LightEnable",enableArgs,&result,sizeof(result)) && result==0 && fixed.enabled[0]);
    lightIndex=8;assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_LightEnable",enableArgs,&result,sizeof(result)) && result==int32_t(0x8876086c));
    uint32_t fvf=0x152;const void *fvfArgs[]={nullptr,&fvf};
    assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_SetFVF",fvfArgs,&result,sizeof(result)) && result==0 && fixed.fvf==0x152);
    assert(!MacMetalFixed_Update(&fixed,"unknown",fvfArgs,&result,sizeof(result)));
    assert(MacMetalFixed_Pack(&fixed,&state,&constants,error,sizeof(error)));
    light.type=1;lightIndex=0;assert(MacMetalFixed_Update(&fixed,"CDirect3DDevice_SetLight",lightArgs,&result,sizeof(result)));
    constants.env[0].v[0]=123;
    assert(!MacMetalFixed_Pack(&fixed,&state,&constants,error,sizeof(error)) && constants.env[0].v[0]==123);
    uint32_t invalidTypes[8]={3};assert(!MacMetalFixed_SourceForTypes(invalidTypes));
    puts("PASS fixed contract: stage/coordinate rejection, FVF bytes/bounds/defaults");
}
