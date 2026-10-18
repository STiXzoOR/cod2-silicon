/* Exercise the generated adapters in complete production translation units.
 * No Metal/OpenGL backend is linked: every tested entrypoint must be handled
 * by this boundary hook before its preserved Classic continuation executes. */
#define MAC_SHIM_IMPLEMENTATION
#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !MAC_METAL_NATIVE
#error "This fixture requires the native Apple arm64 generated adapters"
#endif

extern HRESULT CDirect3D_QueryInterface(const void *, const IID *, void **);
extern UINT CDirect3D_GetAdapterCount(const void *);
extern UINT CDirect3D_GetAdapterModeCount(const void *, UINT, D3DFORMAT);
extern HRESULT CDirect3DDevice_SetNPatchMode(const CDirect3DDevice *, float);
extern float CDirect3DDevice_GetNPatchMode(const CDirect3DDevice *);
extern HRESULT CDirect3DDevice_CreateQuery(const CDirect3DDevice *, D3DQUERYTYPE, bool (*)());
extern HRESULT CDirect3DDevice_Clear(const CDirect3DDevice *, DWORD, const D3DRECT *,
                                     DWORD, D3DCOLOR, float, DWORD);

_Static_assert(sizeof(HRESULT) == 4, "HRESULT result storage");
_Static_assert(sizeof(UINT) == 4, "count result storage");
_Static_assert(sizeof(float) == 4, "float result storage");

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "generated dispatch: line %d: %s\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

enum Case { FLOAT_ARGUMENT, FLOAT_RETURN, CALLBACK_ARGUMENT, POINTER_OUTPUT,
            ADAPTER_COUNT, MODE_COUNT, MIXED_ARGUMENTS };
static enum Case currentCase;
static const char *expectedMethod;
static const void *expectedReceiver;
static unsigned int calls;
static uint32_t expectedFloatBits;
static uint32_t returnedBits;
static const IID *expectedIID;
static void **expectedOutput;
static unsigned char outputObject;
static D3DRECT rects[2];
static unsigned int callbackCalls;

static bool queryCallback(void)
{
    ++callbackCalls;
    return 1;
}

static uint32_t bits(const void *value)
{
    uint32_t result;
    memcpy(&result, value, sizeof(result));
    return result;
}

static float floatFromBits(uint32_t value)
{
    float result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void expect(enum Case test, const char *method, const void *receiver,
                   uint32_t result)
{
    currentCase = test;
    expectedMethod = method;
    expectedReceiver = receiver;
    returnedBits = result;
    calls = 0;
}

static int invoke(const char *method, const void *receiver,
                  const void *const *arguments, void *result, size_t size)
{
    ++calls;
    REQUIRE(calls == 1);
    REQUIRE(!strcmp(method, expectedMethod));
    REQUIRE(receiver == expectedReceiver);
    REQUIRE(arguments != NULL);
    REQUIRE(arguments[0] != NULL);
    REQUIRE(arguments[0] != receiver);
    REQUIRE(*(const void *const *)arguments[0] == expectedReceiver);
    REQUIRE(result != NULL);
    REQUIRE(size == sizeof(uint32_t));
    REQUIRE((uintptr_t)result % _Alignof(uint32_t) == 0);
    REQUIRE(bits(result) == 0); /* The generated return slot is initialized. */

    switch (currentCase) {
    case FLOAT_ARGUMENT:
        REQUIRE(arguments[1] != arguments[0]);
        REQUIRE(bits(arguments[1]) == expectedFloatBits);
        break;
    case FLOAT_RETURN:
    case ADAPTER_COUNT:
        break;
    case CALLBACK_ARGUMENT: {
        REQUIRE(*(const D3DQUERYTYPE *)arguments[1] == (D3DQUERYTYPE)0x13579);
        bool (*callback)() = *(bool (*const *)())arguments[2];
        REQUIRE(callback == queryCallback);
        REQUIRE(callback() == 1);
        break;
    }
    case POINTER_OUTPUT: {
        REQUIRE(*(const IID *const *)arguments[1] == expectedIID);
        REQUIRE(*(void **const *)arguments[2] == expectedOutput);
        REQUIRE(arguments[2] != expectedOutput);
        void **output = *(void **const *)arguments[2];
        *output = &outputObject;
        break;
    }
    case MODE_COUNT:
        REQUIRE(*(const UINT *)arguments[1] == 0x12345678u);
        REQUIRE(*(const D3DFORMAT *)arguments[2] == (D3DFORMAT)0x2a);
        break;
    case MIXED_ARGUMENTS:
        REQUIRE(*(const DWORD *)arguments[1] == 2u);
        REQUIRE(*(const D3DRECT *const *)arguments[2] == rects);
        REQUIRE(*(const DWORD *)arguments[3] == 7u);
        REQUIRE(*(const D3DCOLOR *)arguments[4] == 0x8a13cd57u);
        REQUIRE(bits(arguments[5]) == expectedFloatBits);
        REQUIRE(*(const DWORD *)arguments[6] == 0xfedcba98u);
        break;
    }
    memcpy(result, &returnedBits, sizeof(returnedBits));
    return 1;
}

MacShimBoundaryHooks MacShim_Hooks = { invoke, NULL };

/* Clear's real continuation refers to these symbols. Resolve only those live
 * references without linking a GL library or creating any graphics context. */
static void unexpectedGL(void)
{
    fputs("generated dispatch: Classic GL continuation executed\n", stderr);
    exit(1);
}
void glClearColor(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{ (void)r; (void)g; (void)b; (void)a; unexpectedGL(); }
void glClearDepth(GLdouble depth) { (void)depth; unexpectedGL(); }
void glClearStencil(GLint stencil) { (void)stencil; unexpectedGL(); }
void glColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{ (void)r; (void)g; (void)b; (void)a; unexpectedGL(); }
void glDepthMask(GLboolean mask) { (void)mask; unexpectedGL(); }
void glStencilMask(GLuint mask) { (void)mask; unexpectedGL(); }
void glClear(GLbitfield mask) { (void)mask; unexpectedGL(); }

int main(void)
{
    unsigned char direct3DObject[16] = { 0 };
    unsigned char deviceObject[16] = { 0 };
    const CDirect3DDevice *device = (const CDirect3DDevice *)(void *)deviceObject;
    const uint32_t floatPatterns[] = {
        0x80000000u, /* negative zero */
        0x7fc12345u, /* quiet NaN with payload */
        0x3eaaaaabu, /* nonintegral finite value */
        0xff800000u, /* negative infinity */
    };
    for (size_t i = 0; i < sizeof(floatPatterns) / sizeof(floatPatterns[0]); ++i) {
        expectedFloatBits = floatPatterns[i];
        expect(FLOAT_ARGUMENT, "CDirect3DDevice_SetNPatchMode", device, 0x87654321u);
        HRESULT status = CDirect3DDevice_SetNPatchMode(device, floatFromBits(expectedFloatBits));
        REQUIRE(calls == 1);
        REQUIRE(bits(&status) == 0x87654321u);

        expect(FLOAT_RETURN, "CDirect3DDevice_GetNPatchMode", device, floatPatterns[i]);
        float result = CDirect3DDevice_GetNPatchMode(device);
        REQUIRE(calls == 1);
        REQUIRE(bits(&result) == floatPatterns[i]);
    }

    expect(CALLBACK_ARGUMENT, "CDirect3DDevice_CreateQuery", device, 0x81234567u);
    HRESULT status = CDirect3DDevice_CreateQuery(device, (D3DQUERYTYPE)0x13579, queryCallback);
    REQUIRE(calls == 1 && callbackCalls == 1);
    REQUIRE(bits(&status) == 0x81234567u);

    IID iid = { 0 };
    void *output = NULL;
    expectedIID = &iid;
    expectedOutput = &output;
    expect(POINTER_OUTPUT, "CDirect3D_QueryInterface", direct3DObject, 0x88776655u);
    status = CDirect3D_QueryInterface(direct3DObject, &iid, &output);
    REQUIRE(calls == 1 && output == &outputObject);
    REQUIRE(bits(&status) == 0x88776655u);

    expect(ADAPTER_COUNT, "CDirect3D_GetAdapterCount", direct3DObject, 0xfedcba98u);
    REQUIRE(CDirect3D_GetAdapterCount(direct3DObject) == 0xfedcba98u);
    REQUIRE(calls == 1);
    expect(MODE_COUNT, "CDirect3D_GetAdapterModeCount", direct3DObject, 0x89abcdefu);
    REQUIRE(CDirect3D_GetAdapterModeCount(direct3DObject, 0x12345678u,
                                        (D3DFORMAT)0x2a) == 0x89abcdefu);
    REQUIRE(calls == 1);

    expectedFloatBits = 0x3eaaaaabu;
    expect(MIXED_ARGUMENTS, "CDirect3DDevice_Clear", device, 0x80001234u);
    status = CDirect3DDevice_Clear(device, 2, rects, 7, 0x8a13cd57u,
                                   floatFromBits(expectedFloatBits), 0xfedcba98u);
    REQUIRE(calls == 1);
    REQUIRE(bits(&status) == 0x80001234u);
    puts("Generated production dispatch: typed arguments, callback, output, HRESULT, count and float bits pass");
    return 0;
}
