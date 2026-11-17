#include "metal_states.h"
#include "metal_state_cache.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <cstdlib>
#include <new>

static size_t allocations;
void *operator new(size_t size)
{
    ++allocations;
    if (void *memory = malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept { free(memory); }

struct CollisionHash {
    size_t operator()(const MacMetalPipelineKey &) const { return 0; }
};
int main()
{
    MacMetalState raw;
    MacMetalStates_Defaults(&raw);
    MacMetalNormalizedState state;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(state.render[7] == 1 && state.render[23] == 4);
    assert(state.sampler[0].address[0] == 1 && state.sampler[0].anisotropy == 1);
    assert(state.transform[256][0] == 1 && state.transform[256][15] == 1);
    raw.render[147] = 2; // AMBIENTMATERIALSOURCE is an enum, not a boolean.
    raw.render[141] = 7; raw.render[157] = 7;
    raw.render[154] = 0x80000000; // POINTSIZE is a DWORD carrying float bits.
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(state.render[147] == 2 && state.render[141] == 1 && state.render[157] == 1 && state.render[154] == 0);
    raw.render[154] = 0x7fc00000;
    assert(!MacMetalStates_Normalize(&raw, &state));
    raw.render[154] = 0;
    MacMetalPipelineRecipe recipe = {};
    recipe.vertex.registerABI = recipe.fragment.registerABI = MAC_METAL_ABI_VERSION;
    recipe.vertex.compilerOptions = recipe.fragment.compilerOptions = MAC_METAL_STRICT_MATH;
    strcpy(recipe.vertex.entryPoint, "vs"); strcpy(recipe.fragment.entryPoint, "ps");
    strcpy(recipe.vertex.translatorRevision, "synthetic-1"); strcpy(recipe.fragment.translatorRevision, "synthetic-1");
    recipe.colorFormats[0] = 80; recipe.samples = 1; recipe.topology = 3;
    MacMetalKeyStorage a, b;
    assert(MacMetalStates_PipelineKey(&state, &recipe, &a));
    assert(a.length == 502);
    auto golden = MacMetalStates_KeyView(&a);
    assert(MacMetalStates_KeyHash(&golden) == UINT64_C(8489815314101775408));
    assert(a.bytes[0] == 'M' && a.bytes[1] == 'P' && a.bytes[4] == 1 && a.bytes[5] == 0);
    raw.offsets[0] = 432; raw.viewport[2] = 1024; raw.render[22] = 3;
    raw.transform[256][0] = -0.0f;
    assert(MacMetalStates_Normalize(&raw, &state) && !std::signbit(state.transform[256][0]));
    assert(MacMetalStates_PipelineKey(&state, &recipe, &b));
    assert(MacMetalStates_KeyEqual(&a, &b)); // Dynamic state is not baked.
    recipe.vertex.source[0] = 1;
    assert(MacMetalStates_PipelineKey(&state, &recipe, &b) && !MacMetalStates_KeyEqual(&a, &b));
    MacMetalByteCache<int, CollisionHash> cache;
    cache.insert(MacMetalStates_KeyView(&a), 11); cache.insert(MacMetalStates_KeyView(&b), 22);
    assert(*cache.find(MacMetalStates_KeyView(&a)) == 11);
    assert(*cache.find(MacMetalStates_KeyView(&b)) == 22 && cache.size() == 2);
    size_t beforeLookups = allocations;
    assert(*cache.find(MacMetalStates_KeyView(&a)) == 11);
    assert(*cache.find(MacMetalStates_KeyView(&b)) == 22);
    assert(allocations == beforeLookups && "Ready PSO/depth/sampler lookups must not allocate temporary keys");
    b.schemaVersion++;
    assert(!cache.find(MacMetalStates_KeyView(&b)));
    recipe.vertex.source[0] = 0;
    recipe.attributeCount = 2; recipe.attributes[0] = { 1, 0, 16, 31, 1 }; recipe.attributes[1] = { 0, 0, 0, 31, 0 }; recipe.strides[0] = 32;
    assert(MacMetalStates_PipelineKey(&state, &recipe, &a));
    std::swap(recipe.attributes[0], recipe.attributes[1]);
    assert(MacMetalStates_PipelineKey(&state, &recipe, &b) && MacMetalStates_KeyEqual(&a, &b));
    recipe.attributes[1].colorOrder++;
    assert(MacMetalStates_PipelineKey(&state, &recipe, &b) && !MacMetalStates_KeyEqual(&a, &b));
    recipe.attributes[1].colorOrder--;
    MacMetalPipelineRecipe invalidFetch = recipe;
    invalidFetch.hardwareVertexFetch = 1; invalidFetch.attributeCount = 1; invalidFetch.attributes[0].attribute = 31;
    assert(!MacMetalStates_PipelineKey(&state, &invalidFetch, &b)); // Metal descriptors have slots 0..30.
    raw.render[27] = 5; raw.render[206] = 1; raw.render[207] = 5; raw.render[208] = 6;
    assert(MacMetalStates_Normalize(&raw, &state) && state.render[27] == 1);
    assert(MacMetalStates_PipelineKey(&state, &recipe, &a));
    raw.render[207] = 2;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_PipelineKey(&state, &recipe, &b) && !MacMetalStates_KeyEqual(&a, &b));
    raw.render[27] = 0;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_PipelineKey(&state, &recipe, &a));
    raw.render[207] = 5;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_PipelineKey(&state, &recipe, &b) && MacMetalStates_KeyEqual(&a, &b));
    assert(MacMetalStates_DepthKey(&state, &a));
    raw.render[57] = 123; // Reference is dynamic.
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_DepthKey(&state, &b) && MacMetalStates_KeyEqual(&a, &b));
    raw.render[52] = 1; raw.render[185] = 1; raw.render[189] = 2;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_DepthKey(&state, &b) && !MacMetalStates_KeyEqual(&a, &b));
    raw.sampler[0][10] = 99; raw.sampler[0][6] = 3;
    assert(MacMetalStates_Normalize(&raw, &state) && state.sampler[0].anisotropy == 16);
    assert(MacMetalStates_SamplerKey(&state.sampler[0], &a));
    raw.sampler[0][8] = 0x80000000;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_SamplerKey(&state.sampler[0], &b) && MacMetalStates_KeyEqual(&a, &b));
    raw.sampler[0][6] = 2; raw.sampler[0][5] = 3; raw.sampler[0][10] = 8;
    assert(MacMetalStates_Normalize(&raw, &state) && state.sampler[0].anisotropy == 8 && state.sampler[0].magFilter == 3);
    assert(MacMetalStates_SamplerKey(&state.sampler[0], &a));
    raw.sampler[0][5] = 2;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_SamplerKey(&state.sampler[0], &b) && !MacMetalStates_KeyEqual(&a, &b));
    raw.sampler[0][6] = 3;
    assert(MacMetalStates_Normalize(&raw, &state));
    assert(MacMetalStates_SamplerKey(&state.sampler[0], &b) && !MacMetalStates_KeyEqual(&a, &b));
    raw.maxDepth = std::numeric_limits<float>::quiet_NaN();
    assert(!MacMetalStates_Normalize(&raw, &state));
    raw.maxDepth = 1;
    uint32_t type = 23, value = 0; void *device = nullptr;
    const void *args[] = { &device, &type, &value }; int32_t result = 0;
    assert(MacMetalStates_Update(&raw, "CDirect3DDevice_SetRenderState", args, &result, sizeof(result)));
    assert(result == (int32_t)0x8876086c && raw.render[23] == 4);
    value = 3;
    assert(MacMetalStates_Update(&raw, "CDirect3DDevice_SetRenderState", args, &result, sizeof(result)));
    assert(!result && raw.render[23] == 3);
    float matrix[16] = {}; matrix[0] = -0.0f; void *matrixPointer = matrix; type = 256;
    const void *transformArgs[] = { &device, &type, &matrixPointer };
    assert(MacMetalStates_Update(&raw, "CDirect3DDevice_SetTransform", transformArgs, &result, sizeof(result)) && !result);
    assert(std::signbit(raw.transform[256][0])); // Getters keep submitted raw bits.
    assert(MacMetalStates_Normalize(&raw, &state) && !std::signbit(state.transform[256][0]));
    MacMetalState zero = {}; zero.color[0] = &device;
    value = 1; type = 7;
    assert(MacMetalStates_Update(&zero, "CDirect3DDevice_SetRenderState", args, &result, sizeof(result)) && !result);
    assert(zero.color[0] == &device && zero.render[23] == 4 && zero.sampler[0][1] == 1);
    assert(MacMetalStates_Normalize(&zero, &state));
    puts("Metal states: canonical floats, defaults, dynamic separation, complete keys and collision equality pass");
}
