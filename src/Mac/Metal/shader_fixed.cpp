#include "shader_fixed.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static void Color(float out[4], uint32_t value)
{
    out[0] = ((value >> 16) & 255) / 255.f;
    out[1] = ((value >> 8) & 255) / 255.f;
    out[2] = (value & 255) / 255.f;
    out[3] = (value >> 24) / 255.f;
}
static void Bits(MacMetalFloat4 &out, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    uint32_t words[4] = { x, y, z, w };
    memcpy(&out, words, sizeof(words));
}
static void Floats(MacMetalFloat4 &out, float x, float y, float z, float w)
{
    out = {{x,y,z,w}};
}
static bool Fail(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
    return false;
}
void MacMetalFixed_Init(MacMetalFixedState *state)
{
    if (!state) return;
    memset(state, 0, sizeof(*state));
    for (unsigned i = 0; i < 4; ++i) {
        state->material.diffuse[i] = state->material.ambient[i] = state->material.specular[i] = 1;
    }
    state->material.power = 32;
}
int MacMetalFixed_Update(MacMetalFixedState *state, const char *method, const void *const *args, void *result, size_t size)
{
    if (!state || !method || !args || !result || size != sizeof(int32_t)) return 0;
    int32_t code = 0;
    if (!strcmp(method,"CDirect3DDevice_SetFVF")) {
        state->fvf = *(const uint32_t *)args[1];
    } else if (!strcmp(method,"CDirect3DDevice_SetMaterial")) {
        const void *material = *(const void *const *)args[1];
        if (material) memcpy(&state->material, material, sizeof(state->material));
        else code = (int32_t)0x8876086c;
    } else if (!strcmp(method,"CDirect3DDevice_SetLight")) {
        uint32_t index = *(const uint32_t *)args[1];
        const void *light = *(const void *const *)args[2];
        if (index < 8 && light) memcpy(&state->lights[index], light, sizeof(MacMetalFixedLight));
        else code = (int32_t)0x8876086c;
    } else if (!strcmp(method,"CDirect3DDevice_SetClipPlane")) {
        uint32_t index=*(const uint32_t *)args[1];
        const void *plane=*(const void *const *)args[2];
        if (index<6 && plane) memcpy(state->clipPlanes[index],plane,16);
        else code=(int32_t)0x8876086c;
    } else if (!strcmp(method,"CDirect3DDevice_LightEnable")) {
        uint32_t index = *(const uint32_t *)args[1];
        if (index < 8) state->enabled[index] = *(const int32_t *)args[2] != 0;
        else code = (int32_t)0x8876086c;
    } else return 0;
    memcpy(result, &code, sizeof(code));
    return 1;
}
void MacMetalFixed_InitDefaults(MacMetalState *state)
{
    if (!state) return;
    state->render[137]=state->render[141]=state->render[142]=1;
    state->render[139]=state->render[140]=state->render[143]=state->render[147]=state->render[148]=0;
    state->render[145]=1;state->render[146]=2;
    state->render[60]=0xffffffffu;
    state->render[28]=state->render[34]=state->render[35]=state->render[36]=state->render[48]=0;
    float one=1;memcpy(&state->render[37],&one,4);memcpy(&state->render[38],&one,4);
    state->render[15]=state->render[24]=state->render[29]=state->render[152]=0;state->render[25]=8;
    const unsigned matrices[]={256,2,3,16,17,18,19,20,21,22,23};
    for (unsigned matrix:matrices) {
        memset(state->transform[matrix],0,64);
        for (unsigned i=0;i<4;++i) state->transform[matrix][i*5]=1;
    }
    for (unsigned i=0;i<8;++i) {
        uint32_t *stage=state->textureStage[i];memset(stage,0,33*sizeof(uint32_t));
        stage[1]=i?1:4;stage[2]=2;stage[3]=1;stage[4]=i?1:2;stage[5]=2;stage[6]=1;
        stage[11]=i;stage[26]=stage[27]=stage[28]=1;stage[32]=0xffffffffu;
    }
}
static bool Operation(uint32_t op, bool alpha)
{
    return (op >= 2 && op <= 16) || op == 24 || op == 25 || op == 26 || (!alpha && op >= 18 && op <= 21);
}
int MacMetalFixed_Pack(const MacMetalFixedState *fixed, const MacMetalState *state, MacMetalARBConstants *out, char *error, size_t capacity)
{
    if (!fixed || !state || !out) return Fail(error,capacity,"null fixed state");
    if (!state->viewport[2] || !state->viewport[3]) return Fail(error,capacity,"empty fixed viewport");
    if (state->render[152] & ~63u) return Fail(error,capacity,"invalid clip plane mask");
    if (state->render[35] && state->render[28]) return Fail(error,capacity,"pixel table fog is not source-reachable");
    if (state->render[140] > 3) return Fail(error,capacity,"invalid vertex fog mode");
    if (state->render[15] && (state->render[25] < 1 || state->render[25] > 8)) return Fail(error,capacity,"invalid alpha comparison");
    if (fixed->fvf) {
        unsigned position=fixed->fvf & 0x400e;
        if ((fixed->fvf & 0xb001) || (position!=2 && position!=4 && position!=0x4002) || ((fixed->fvf>>8)&15)>8 || (position==4 && (fixed->fvf&16))) return Fail(error,capacity,"unsupported fixed FVF");
    }
    for (unsigned source=145;source<=148;++source)
        if (state->render[source]>2) return Fail(error,capacity,"invalid material color source");
    MacMetalARBConstants packed = {};
    memcpy(&packed.env[0],state->transform[256],64);
    memcpy(&packed.env[4],state->transform[2],64);
    memcpy(&packed.env[8],state->transform[3],64);
    for (unsigned s = 0; s < 8; ++s) memcpy(&packed.env[12+s*4],state->transform[16+s],64);
    float combined[9] = {};
    for (unsigned i = 0; i < 3; ++i)
        for (unsigned j = 0; j < 3; ++j)
            for (unsigned k = 0; k < 3; ++k) combined[i*3+j] += state->transform[256][i*4+k]*state->transform[2][k*4+j];
    float determinant = combined[0]*(combined[4]*combined[8]-combined[5]*combined[7])-combined[1]*(combined[3]*combined[8]-combined[5]*combined[6])+combined[2]*(combined[3]*combined[7]-combined[4]*combined[6]);
    if (state->render[137] && (!std::isfinite(determinant) || fabsf(determinant) < 1e-20f)) return Fail(error,capacity,"singular lighting transform");
    if (fabsf(determinant) >= 1e-20f) {
        for (unsigned i = 0; i < 3; ++i)
            for (unsigned j = 0; j < 3; ++j) {
                unsigned a=(j+1)%3,b=(j+2)%3,c=(i+1)%3,d=(i+2)%3;
                packed.env[44+i].v[j]=(combined[a*3+c]*combined[b*3+d]-combined[a*3+d]*combined[b*3+c])/determinant;
            }
    }
    Floats(packed.env[47],state->viewport[0],state->viewport[1],state->viewport[2],state->viewport[3]);
    Bits(packed.env[48],state->render[137],state->render[141],state->render[143],state->render[29]);
    Bits(packed.env[49],(fixed->fvf & 0x400e)==4,state->render[28],state->render[140],state->render[48]);
    memcpy(&packed.env[50].v[0], &state->render[36],4);
    memcpy(&packed.env[50].v[1], &state->render[37],4);
    memcpy(&packed.env[50].v[2], &state->render[38],4);
    Color(packed.env[51].v,state->render[34]);
    Color(packed.env[52].v,state->render[60]);
    Bits(packed.env[53],state->render[15],state->render[25],state->render[24] & 255,0);
    Color(packed.env[54].v,state->render[139]);
    memcpy(&packed.env[55],&fixed->material,64);
    Floats(packed.env[59],fixed->material.power,0,0,0);
    Bits(packed.env[60],state->render[142],0,state->render[145],state->render[146]);
    Bits(packed.env[61],state->render[147],state->render[148],0,0);
    for (unsigned i=0;i<8;++i) {
        const auto &light=fixed->lights[i];
        if (state->render[137] && fixed->enabled[i] && light.type!=3) return Fail(error,capacity,"non-directional light is not source-reachable");
        Bits(packed.env[64+i*8],light.type,fixed->enabled[i],0,0);
        memcpy(&packed.env[65+i*8],light.diffuse,16);
        memcpy(&packed.env[66+i*8],light.specular,16);
        memcpy(&packed.env[67+i*8],light.ambient,16);
        Floats(packed.env[68+i*8],light.direction[0],light.direction[1],light.direction[2],0);
    }
    memcpy(&packed.env[128],fixed->clipPlanes,sizeof(fixed->clipPlanes));
    Bits(packed.env[134],state->render[152],0,0,0);
    for (unsigned i=0;i<8;++i) {
        const uint32_t *stage=state->textureStage[i];
        uint32_t color=stage[1] ? stage[1] : (i ? 1 : 4);
        if (color==1) break;
        uint32_t alpha=stage[4] ? stage[4] : 2;
        if (!Operation(color,false) || !Operation(alpha,true)) return Fail(error,capacity,"unsupported fixed stage operation");
        const unsigned indices[6]={2,3,26,5,6,27};
        for (unsigned index:indices) {
            if ((stage[index] & ~0x3fu) || (stage[index] & 15)>6) return Fail(error,capacity,"unsupported fixed stage argument");
        }
        uint32_t coord=stage[11], flags=stage[24];
        if ((coord>>16)==1) return Fail(error,capacity,"camera-space normal generation is not source-reachable");
        if ((coord & 65535)>7 || ((coord>>16)!=0 && (coord>>16)!=2 && (coord>>16)!=3)) return Fail(error,capacity,"unsupported coordinate generation");
        if ((flags & ~0x107u) || ((flags & 7)>4) || ((flags&0x100) && (flags&7)<2)) return Fail(error,capacity,"invalid texture transform count");
        if (stage[28] && stage[28]!=1 && stage[28]!=5) return Fail(error,capacity,"invalid stage result register");
        if (fixed->textureTypes[i]>2) return Fail(error,capacity,"invalid stage texture dimensionality");
        Bits(packed.local[i*4],color,stage[1]?stage[2]:2,stage[1]?stage[3]:1,stage[26]);
        Bits(packed.local[i*4+1],alpha,stage[4]?stage[5]:2,stage[4]?stage[6]:1,stage[27]);
        Bits(packed.local[i*4+2],coord,flags,stage[28]?stage[28]:1,1);
        Color(packed.local[i*4+3].v,stage[32]);
    }
    *out=packed;
    if (error && capacity) error[0]=0;
    return 1;
}
int MacMetalFixed_ExpandFVF(uint32_t fvf, const void *vertex, size_t bytes, MacMetalARBInput *out)
{
    if (!vertex || !out || (fvf & 0xb001)) return 0;
    unsigned position=fvf & 0x400e, count=(fvf>>8)&15;
    if ((position!=2 && position!=4 && position!=0x4002) || count>8 || (fvf&1) || (position==4 && (fvf&16))) return 0;
    size_t need=position==2?12:16;
    if (fvf&16) need+=12;
    if (fvf&32) need+=4;
    if (fvf&64) need+=4;
    if (fvf&128) need+=4;
    unsigned dimensions[8]={};
    for (unsigned i=0;i<count;++i) { unsigned encoded=(fvf>>(16+2*i))&3; dimensions[i]=encoded==0?2:encoded==1?3:encoded==2?4:1; need+=4*dimensions[i]; }
    if (bytes<need) return 0;
    MacMetalARBInput input={};
    for (auto &attribute:input.attrib) attribute.v[3]=1;
    input.attrib[3].v[2]=1;
    for (unsigned i=0;i<4;++i) input.attrib[5].v[i]=1;
    const unsigned char *cursor=(const unsigned char *)vertex;
    memcpy(input.attrib[0].v,cursor,position==2?12:16); cursor+=position==2?12:16;
    if (fvf&16) { memcpy(input.attrib[3].v,cursor,12);cursor+=12; }
    if (fvf&32) { memcpy(input.attrib[4].v,cursor,4);cursor+=4; }
    if (fvf&64) { uint32_t color;memcpy(&color,cursor,4);Color(input.attrib[5].v,color);cursor+=4; }
    if (fvf&128) { uint32_t color;memcpy(&color,cursor,4);Color(input.attrib[6].v,color);cursor+=4; }
    for (unsigned i=0;i<count;++i) { memcpy(input.attrib[8+i].v,cursor,4*dimensions[i]);cursor+=4*dimensions[i]; }
    input.position=input.attrib[0];input.color[0]=input.attrib[5];input.color[1]=input.attrib[6];
    for (unsigned i=0;i<8;++i) input.texcoord[i]=input.attrib[8+i];
    *out=input;
    return 1;
}
static const char *CommonSource = R"MSL(
#include <metal_stdlib>
using namespace metal;
struct ARBInput { float4 attrib[16],texcoord[8],color[2],position,fogcoord; };
struct ARBConstants { float4 env[256],local[256],state[64]; };
struct FixedRaster { float4 position [[position]]; float4 color [[user(color0), center_perspective]]; float4 secondary [[user(color1), center_perspective]]; float4 tc0 [[user(texcoord0), center_perspective]]; float4 tc1 [[user(texcoord1), center_perspective]]; float4 tc2 [[user(texcoord2), center_perspective]]; float4 tc3 [[user(texcoord3), center_perspective]]; float4 tc4 [[user(texcoord4), center_perspective]]; float4 tc5 [[user(texcoord5), center_perspective]]; float4 tc6 [[user(texcoord6), center_perspective]]; float4 tc7 [[user(texcoord7), center_perspective]]; float4 fog [[user(fog), center_perspective]];  };
struct FixedVertex { float4 position [[position]]; float4 color [[user(color0), center_perspective]]; float4 secondary [[user(color1), center_perspective]]; float4 tc0 [[user(texcoord0), center_perspective]]; float4 tc1 [[user(texcoord1), center_perspective]]; float4 tc2 [[user(texcoord2), center_perspective]]; float4 tc3 [[user(texcoord3), center_perspective]]; float4 tc4 [[user(texcoord4), center_perspective]]; float4 tc5 [[user(texcoord5), center_perspective]]; float4 tc6 [[user(texcoord6), center_perspective]]; float4 tc7 [[user(texcoord7), center_perspective]]; float4 fog [[user(fog), center_perspective]]; float clipping [[clip_distance]] [6];  };

template<typename V> void fixed_set_tc(thread V &v,uint i,float4 tc) { switch(i) { case 0:v.tc0=tc;break;case 1:v.tc1=tc;break;case 2:v.tc2=tc;break;case 3:v.tc3=tc;break;case 4:v.tc4=tc;break;case 5:v.tc5=tc;break;case 6:v.tc6=tc;break;case 7:v.tc7=tc;break; } }
float4 fixed_matrix(float4 v, constant float4 *m)
{
    return v.x*m[0]+v.y*m[1]+v.z*m[2]+v.w*m[3];
}
float3 fixed_normal(float3 n, constant ARBConstants &c)
{
    return float3(dot(n,c.env[44].xyz),dot(n,c.env[45].xyz),dot(n,c.env[46].xyz));
}
float3 fixed_safe_normal(float3 n) { float q=dot(n,n);return q>0?n*rsqrt(q):float3(0); }
float4 fixed_material(uint source,float4 value,float4 diffuse,float4 specular,bool colorVertex)
{
    return colorVertex&&source==1?diffuse:colorVertex&&source==2?specular:value;
}
vertex FixedVertex fixed_vertex(uint id [[vertex_id]], device const ARBInput *input [[buffer(0)]],constant ARBConstants &c [[buffer(1)]])
{
    ARBInput a=input[id]; FixedVertex o;
    uint4 flags=as_type<uint4>(c.env[48]),fog=as_type<uint4>(c.env[49]);
    float4 world=fixed_matrix(a.attrib[0],c.env);
    float4 view=fixed_matrix(world,c.env+4);
    uint mask=as_type<uint4>(c.env[134]).x;
    for(uint i=0;i<6;i++)o.clipping[i]=(mask&(1u<<i))?dot(world,c.env[128+i]):1;
    float3 normal=fixed_normal(a.attrib[3].xyz,c);
    if(flags.z) normal=fixed_safe_normal(normal);
    o.position=fixed_matrix(view,c.env+8);
    if(fog.x) {
        float4 viewport=c.env[47];float rhw=a.attrib[0].w;
        float w=rhw!=0?1/rhw:1;
        o.position=float4((a.attrib[0].x-viewport.x+.5f)*2/viewport.z-1,1-(a.attrib[0].y-viewport.y+.5f)*2/viewport.w,a.attrib[0].z,1)*w;
    }
    o.color=a.attrib[5];o.secondary=a.attrib[6];
    if(flags.x && !fog.x) {
        uint4 sources=as_type<uint4>(c.env[60]);uint4 extra=as_type<uint4>(c.env[61]);
        float4 diffuse=fixed_material(sources.z,c.env[55],a.attrib[5],a.attrib[6],flags.y!=0);
        float4 specular=fixed_material(sources.w,c.env[56],a.attrib[5],a.attrib[6],flags.y!=0);
        float4 ambient=fixed_material(extra.x,c.env[57],a.attrib[5],a.attrib[6],flags.y!=0);
        float4 emissive=fixed_material(extra.y,c.env[58],a.attrib[5],a.attrib[6],flags.y!=0);
        float3 d=emissive.xyz+c.env[54].xyz*ambient.xyz,s=float3(0);
        float3 viewer=sources.x?fixed_safe_normal(-view.xyz):float3(0,0,-1);
        for(uint i=0;i<8;i++) {
            uint base=64+i*8;uint4 light=as_type<uint4>(c.env[base]);
            if(!light.y) continue;
            float3 direction=-fixed_matrix(float4(c.env[base+4].xyz,0),c.env+4).xyz;
            direction=fixed_safe_normal(direction);
            float ndl=max(dot(normal,direction),0.0f);
            d+=ambient.xyz*c.env[base+3].xyz+diffuse.xyz*c.env[base+1].xyz*ndl;
            if(flags.w && ndl>0) {
                float3 h=fixed_safe_normal(direction+viewer);
                float power=c.env[59].x;
                float intensity=power>0?pow(max(dot(normal,h),0.0f),power):1;
                s+=specular.xyz*c.env[base+2].xyz*intensity;
            }
        }
        o.color=float4(saturate(d),diffuse.a);o.secondary=float4(saturate(s),a.attrib[6].a);
    }
    for(uint i=0;i<8;i++) {
        uint4 stage=as_type<uint4>(c.local[i*4+2]);
        float4 tc=a.attrib[8+(stage.x&7)];
        if(stage.x>>16==2) tc=view;
        else if(stage.x>>16==3) tc=float4(reflect(fixed_safe_normal(view.xyz),fixed_safe_normal(normal)),1);
        if(stage.y&7) tc=fixed_matrix(tc,c.env+12+i*4);
        fixed_set_tc(o,i,tc);
    }
    o.fog=float4(a.attrib[6].a);
    if(fog.z&&!fog.x) {
        float distance=fog.w?length(view.xyz):abs(view.z),density=c.env[50].z;
        if(fog.z==1) o.fog=float4(exp(-density*distance));
        else if(fog.z==2) o.fog=float4(exp(-density*density*distance*distance));
        else { float span=c.env[50].y-c.env[50].x;o.fog=float4(span!=0?(c.env[50].y-distance)/span:float(distance<c.env[50].y)); }
        o.fog=saturate(o.fog);
    }
    return o;
}
vertex FixedVertex fixed_post_vertex(uint id [[vertex_id]],device const ARBInput *input [[buffer(0)]])
{
    ARBInput a=input[id];FixedVertex o={};o.position=a.attrib[0];o.color=a.attrib[5];o.secondary=a.attrib[6];
    for(uint i=0;i<8;i++) fixed_set_tc(o,i,a.attrib[8+i]);
    for(uint i=0;i<6;i++)o.clipping[i]=1;
    o.fog=float4(1);return o;
}
float4 fixed_argument(uint arg,float4 current,float4 diffuse,float4 specular,float4 temporary,float4 texture,float4 factor,float4 constantColor)
{
    uint kind=arg&15;float4 result=kind==0?diffuse:kind==1?current:kind==2?texture:kind==3?factor:kind==4?specular:kind==5?temporary:constantColor;
    if(arg&32) result=float4(result.a);
    if(arg&16) result=1-result;
    return result;
}
float4 fixed_operation(uint op,float4 a,float4 b,float4 zero,float4 diffuse,float4 texture,float4 factor,float4 current)
{
    switch(op) {
    case 2:return a;
    case 3:return b;
    case 4:return a*b;
    case 5:return 2*a*b;
    case 6:return 4*a*b;
    case 7:return a+b;
    case 8:return a+b-.5f;
    case 9:return 2*(a+b-.5f);
    case 10:return a-b;
    case 11:return a+b*(1-a);
    case 12:return a*diffuse.a+b*(1-diffuse.a);
    case 13:return a*texture.a+b*(1-texture.a);
    case 14:return a*factor.a+b*(1-factor.a);
    case 15:return a+b*(1-texture.a);
    case 16:return a*current.a+b*(1-current.a);
    case 18:return a+a.a*b;
    case 19:return a*b+a.a;
    case 20:return a+(1-a.a)*b;
    case 21:return (1-a)*b+a.a;
    case 24:return float4(4*dot(a.xyz-.5f,b.xyz-.5f));
    case 25:return zero+a*b;
    case 26:return zero*a+(1-zero)*b;
    default:return float4(0);
    }
}
bool fixed_alpha(float a,uint function,uint reference)
{
    float r=float(reference)/255;
    switch(function) {
    case 1:return false;case 2:return a<r;case 3:return a==r;case 4:return a<=r;
    case 5:return a>r;case 6:return a!=r;case 7:return a>=r;default:return true;
    }
}
float4 fixed_finish(FixedRaster in,float4 result,constant ARBConstants &c)
{
    if(as_type<uint4>(c.env[48]).w) result.rgb=saturate(result.rgb+in.secondary.rgb);
    if(as_type<uint4>(c.env[49]).y) result.rgb=mix(c.env[51].rgb,result.rgb,saturate(in.fog.x));
    uint4 alpha=as_type<uint4>(c.env[53]);
    if(alpha.x&&!fixed_alpha(result.a,alpha.y,alpha.z)) discard_fragment();
    return result;
}
float2 fixed_uv(float4 tc,uint flags)
{
    uint count=flags&7;
    if(flags&256) return tc.xy/tc[count-1];
    return tc.xy;
}
float3 fixed_uv3(float4 tc,uint flags)
{
    uint count=flags&7;
    if(flags&256) return tc.xyz/tc[count-1];
    return tc.xyz;
}
fragment float4 fixed_colored(FixedRaster in [[stage_in]],constant ARBConstants &c [[buffer(1)]]) { return fixed_finish(in,in.color,c); }
fragment float4 fixed_solid(FixedRaster in [[stage_in]],constant ARBConstants &c [[buffer(1)]]) { return c.env[52]; }
fragment float4 fixed_copy(FixedRaster in [[stage_in]],constant ARBConstants &c [[buffer(1)]],texture2d<float> texture [[texture(0)]],sampler sample [[sampler(0)]])
{
    return texture.sample(sample,fixed_uv(in.tc0,as_type<uint4>(c.local[2]).y));
}
fragment float4 fixed_gamma(FixedRaster in [[stage_in]],constant ARBConstants &c [[buffer(1)]],texture2d<float> texture [[texture(0)]],sampler sample [[sampler(0)]])
{
    float4 value=texture.sample(sample,in.tc0.xy);
    uint3 indices=uint3(floor(saturate(value.rgb)*255+.5f));
    return float4(c.local[indices.r].r,c.local[indices.g].g,c.local[indices.b].b,value.a);
}
)MSL";
char *MacMetalFixed_SourceForTypes(const uint32_t types[8])
{
    if (!types) return nullptr;
    std::string source=CommonSource;
    source+="fragment float4 fixed_fragment(FixedRaster in [[stage_in]],constant ARBConstants &c [[buffer(1)]]";
    for(unsigned i=0;i<8;++i) {
        if(types[i]>2) return nullptr;
        source+=","+std::string(types[i]==1?"texturecube":types[i]==2?"texture3d":"texture2d")+"<float> t"+std::to_string(i)+" [[texture("+std::to_string(i)+")]],sampler s"+std::to_string(i)+" [[sampler("+std::to_string(i)+")]]";
    }
    source+=") { float4 textures[8];\n";
    for(unsigned i=0;i<8;++i) {
        std::string n=std::to_string(i);
        source+="textures["+n+"]=float4(1); if(as_type<uint4>(c.local["+std::to_string(i*4+2)+"]).w) textures["+n+"]=t"+n+".sample(s"+n+","+(types[i]==0?"fixed_uv":"fixed_uv3")+"(in.tc"+n+",as_type<uint4>(c.local["+std::to_string(i*4+2)+"]).y));\n";
    }
    source+=R"MSL(
    float4 current=in.color,temporary=float4(0),factor=c.env[52];
    for(uint i=0;i<8;i++) {
        uint4 rgb=as_type<uint4>(c.local[i*4]),alpha=as_type<uint4>(c.local[i*4+1]),stage=as_type<uint4>(c.local[i*4+2]);
        if(!stage.w) break;
        float4 texture=textures[i],constantColor=c.local[i*4+3];
        float4 a=fixed_argument(rgb.y,current,in.color,in.secondary,temporary,texture,factor,constantColor);
        float4 b=fixed_argument(rgb.z,current,in.color,in.secondary,temporary,texture,factor,constantColor);
        float4 z=fixed_argument(rgb.w,current,in.color,in.secondary,temporary,texture,factor,constantColor);
        float4 aa=fixed_argument(alpha.y,current,in.color,in.secondary,temporary,texture,factor,constantColor);
        float4 ab=fixed_argument(alpha.z,current,in.color,in.secondary,temporary,texture,factor,constantColor);
        float4 az=fixed_argument(alpha.w,current,in.color,in.secondary,temporary,texture,factor,constantColor);
        float4 color=fixed_operation(rgb.x,a,b,z,in.color,texture,factor,current);
        color.a=fixed_operation(alpha.x,aa,ab,az,in.color,texture,factor,current).a;
        color=saturate(color);
        if(stage.z==5) temporary=color;else current=color;
    }
    return fixed_finish(in,current,c);
}
)MSL";
    char *result=(char *)malloc(source.size()+1);
    if(result) memcpy(result,source.c_str(),source.size()+1);
    return result;
}
const char *MacMetalFixed_Source(void)
{
    static const char *source=[](){ uint32_t types[8]={};return MacMetalFixed_SourceForTypes(types); }();
    return source;
}
