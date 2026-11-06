#include "metal_core.h"
#include "shim_dispatch.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

_Static_assert(sizeof(MacMetalFloat4) == 16, "float4 ABI");
_Static_assert(_Alignof(MacMetalFloat4) == 16, "float4 alignment");
_Static_assert(sizeof(MacMetalARBInput) == 448, "ARBInput ABI");
_Static_assert(offsetof(MacMetalARBConstants, local) == 4096, "local namespace");
_Static_assert(offsetof(MacMetalARBConstants, state) == 8192, "state namespace");
_Static_assert(sizeof(MacMetalARBConstants) == 9216, "ARBConstants ABI");
_Static_assert(offsetof(MacMetalARBOutput, killed) == 208, "killed ABI");
_Static_assert(sizeof(MacMetalARBOutput) == 224, "ARBOutput stride");
static void retire(void *context) { ++*(int *)context; }
static int metal(const char *name, const void *object, const void *const *args, void *result, size_t size)
{
    (void)name; (void)object; (void)args; assert(size == sizeof(int));
    *(int *)result = 29; return 1;
}
int main(void)
{
    MacMetalARBConstants vs = { 0 }, ps = { 0 };
    float input[] = { -0.0f, -2.0f, INFINITY, NAN, 1, 2, 3, 4 };
    assert(MacMetal_WriteRegisters(vs.env, 256, 254, input, 2));
    assert(!memcmp(vs.env[254].v, input, sizeof(input)));
    assert(ps.env[254].v[1] == 0 && vs.local[254].v[1] == 0);
    MacMetalARBConstants before = vs;
    assert(!MacMetal_WriteRegisters(vs.env, 256, UINT32_MAX, input, 1));
    assert(!MacMetal_WriteRegisters(vs.env, 256, 255, input, 2));
    assert(!memcmp(&before, &vs, sizeof(vs)));
    assert(MacMetal_WriteRegisters(vs.env, 256, 256, NULL, 0));
    assert(!MacMetal_WriteRegisters(vs.env, 256, 0, NULL, 1));
    int releasedA = 0, releasedB = 0;
    MacMetalRetirement *queue = MacMetalRetirement_Create();
    assert(MacMetalRetirement_Add(queue, 7, retire, &releasedA));
    assert(MacMetalRetirement_Add(queue, 3, retire, &releasedB));
    MacMetalRetirement_Collect(queue, 2); assert(!releasedA && !releasedB);
    MacMetalRetirement_Collect(queue, 3); assert(!releasedA && releasedB == 1);
    MacMetalRetirement_Collect(queue, 6); assert(!releasedA && releasedB == 1);
    MacMetalRetirement_Collect(queue, 7); assert(releasedA == 1 && releasedB == 1);
    MacMetalRetirement_Collect(queue, 100); assert(releasedA == 1 && releasedB == 1);
    MacMetalRetirement_Destroy(queue);
    int object = 0, result = -1;
    MacShimBackendOps ops = { metal };
    assert(!MacShim_IsMetal());
    assert(MacShim_InstallBackend(1, &ops));
    assert(!MacShim_InstallBackend(2, &ops));
    assert(!MacShim_Invoke("method", &object, NULL, &result, sizeof(result)));
    MacShim_RecordObject(&object, 0);
    MacShim_SelectRenderer(1);
    assert(!MacShim_Invoke("method", &object, NULL, &result, sizeof(result)));
    MacShim_ForgetObject(&object);
    assert(MacShim_Invoke("method", &object, NULL, &result, sizeof(result)) && result == 29);
    MacShim_RecordObject(&object, 1); MacShim_SelectRenderer(0);
    assert(MacShim_Invoke("method", &object, NULL, &result, sizeof(result)));
    MacShim_ForgetObject(&object);
    assert(!MacShim_Invoke("method", &object, NULL, &result, sizeof(result)));
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    /* Actual generated constructor/destructor names must pin Classic objects
     * and release that identity before the same address belongs to Metal. */
    assert(!MacShim_Dispatch("CDirect3DTexture_CDirect3DTexture", &object,
                             NULL, NULL, 0));
    MacShim_SelectRenderer(1);
    assert(!MacShim_Dispatch("CDirect3DTexture_GetLevelCount", &object,
                             NULL, &result, sizeof(result)));
    assert(!MacShim_Dispatch("ZN16CDirect3DTextureD0Ev", &object, NULL, NULL, 0));
    assert(MacShim_Dispatch("CDirect3DTexture_GetLevelCount", &object,
                            NULL, &result, sizeof(result)) && result == 29);
    MacShim_RecordObject(&object, 1);
    MacShim_SelectRenderer(0);
    assert(MacShim_Dispatch("CDirect3DTexture_GetLevelCount", &object,
                            NULL, &result, sizeof(result)));
    MacShim_ForgetObject(&object);
#endif
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    int many[2048];
    for (unsigned int i = 0; i < 2048; ++i) MacShim_RecordObject(&many[i], i & 1);
    for (unsigned int i = 0; i < 2048; ++i) {
        MacShim_SelectRenderer(!(i & 1));
        assert(MacShim_Invoke("method", &many[i], NULL, &result, sizeof(result)) == (int)(i & 1));
    }
    for (unsigned int i = 0; i < 2048; ++i) MacShim_ForgetObject(&many[i]);
    for (unsigned int i = 0; i < 2048; ++i) {
        MacShim_RecordObject(&many[i], !(i & 1));
        MacShim_SelectRenderer(i & 1);
        assert(MacShim_Invoke("method", &many[i], NULL, &result, sizeof(result)) == (int)!(i & 1));
        MacShim_ForgetObject(&many[i]);
    }
#endif
    puts("Metal core: dispatch ownership, retirement and register ABI pass");
}
