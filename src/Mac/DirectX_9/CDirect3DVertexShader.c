#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    void **vtablePrimary;
    void **vtableSecondary;
    unsigned char baseState[0x18c];
    ULONG refCount;
    unsigned char isBound;
} CDirect3DVertexShaderImpl;
typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3DVertexShader[];
extern fnptr_t vtbl_CDirect3DVertexShader_secondary[];

ULONG CDirect3DVertexShader_AddRef(const CDirect3DVertexShader *_this);
void ZN21CDirect3DVertexShaderD1Ev(const CDirect3DVertexShader *_this);
void ZN21CDirect3DVertexShaderD0Ev(const CDirect3DVertexShader *_this);
HRESULT CDirect3DVertexShader_QueryInterface(const CDirect3DVertexShader *_this, const IID *iid, unsigned char **ppvObj);
ULONG CDirect3DVertexShader_Release(const CDirect3DVertexShader *_this);
HRESULT CDirect3DVertexShader_GetDevice(const CDirect3DVertexShader *_this, IDirect3DDevice9 **ppDevice);
unsigned char CDirect3DVertexShader_CDirect3DVertexShader(const CDirect3DVertexShader *_this, const char *pSrcData);
HRESULT CDirect3DVertexShader_GetFunction(const CDirect3DVertexShader *_this, UINT *pSizeOfData);

extern unsigned int COpenGLVertexProgram_COpenGLVertexProgram(const COpenGLVertexProgram *_this, const char *pSrcData);
#if defined(COD2_X64)
extern void ZN20COpenGLVertexProgramD2Ev(void *_this);
#else
extern void ZN20COpenGLVertexProgramD2Ev(void);
#endif

#if defined(COD2_X64)
GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    GLuint macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_GetProgramId", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return *(const GLuint *)((const CDirect3DVertexShaderImpl *)_this)->baseState;
}
#else
#if COD2_APPLE_SDK
GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    GLuint macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_GetProgramId", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return *(const GLuint *)((const CDirect3DVertexShaderImpl *)_this)->baseState;
}
#endif
#endif

ULONG CDirect3DVertexShader_AddRef(const CDirect3DVertexShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexShaderImpl *shader;

    shader = (CDirect3DVertexShaderImpl *)_this;
    ++shader->refCount;
    return shader->refCount;
}

HRESULT CDirect3DVertexShader_QueryInterface(const CDirect3DVertexShader *_this, const IID *iid, unsigned char **ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;

    *ppvObj = (unsigned char *)_this;
    CDirect3DVertexShader_AddRef(_this);
    return 0;
}

ULONG CDirect3DVertexShader_Release(const CDirect3DVertexShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexShaderImpl *shader;
    ULONG refCount;

    shader = (CDirect3DVertexShaderImpl *)_this;
    refCount = --shader->refCount;
    if (!refCount) {
        ZN21CDirect3DVertexShaderD0Ev(_this);
    }

    return refCount;
}

void ZN21CDirect3DVertexShaderD1Ev(const CDirect3DVertexShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN21CDirect3DVertexShaderD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DVertexShaderImpl *shader;

    shader = (CDirect3DVertexShaderImpl *)_this;
    shader->vtablePrimary = vtbl_CDirect3DVertexShader;
    shader->vtableSecondary = vtbl_CDirect3DVertexShader_secondary;

}

void ZN21CDirect3DVertexShaderD0Ev(const CDirect3DVertexShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN21CDirect3DVertexShaderD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN21CDirect3DVertexShaderD1Ev(_this);
    free((void *)_this);
}

unsigned char CDirect3DVertexShader_CDirect3DVertexShader(const CDirect3DVertexShader *_this, const char *pSrcData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSrcData };
    unsigned char macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_CDirect3DVertexShader", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexShaderImpl *shader;

    shader = (CDirect3DVertexShaderImpl *)_this;
    memset(shader->baseState, 0, sizeof(shader->baseState));

    COpenGLVertexProgram_COpenGLVertexProgram(
        (const COpenGLVertexProgram *)shader->baseState, pSrcData);
    shader->vtablePrimary = vtbl_CDirect3DVertexShader;
    shader->vtableSecondary = vtbl_CDirect3DVertexShader_secondary;
    shader->isBound = 0;
    shader->refCount = 1;
    return 0;
}

HRESULT CDirect3DVertexShader_GetDevice(const CDirect3DVertexShader *_this, IDirect3DDevice9 **ppDevice)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppDevice };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_GetDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppDevice;
    return 0;
}

HRESULT CDirect3DVertexShader_GetFunction(const CDirect3DVertexShader *_this, UINT *pSizeOfData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSizeOfData };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexShader_GetFunction", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pSizeOfData;
    return 0;
}

#if defined(COD2_X64)
void ZThn4_N21CDirect3DVertexShaderD0Ev(void *p) {
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &p };
    if (MacShim_Dispatch("ZThn4_N21CDirect3DVertexShaderD0Ev", p, macArguments, NULL, 0))
        return;
#endif
 ZN21CDirect3DVertexShaderD0Ev((char *)p - offsetof(CDirect3DVertexShaderImpl, vtableSecondary)); }
void ZThn4_N21CDirect3DVertexShaderD1Ev(void *p) {
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &p };
    if (MacShim_Dispatch("ZThn4_N21CDirect3DVertexShaderD1Ev", p, macArguments, NULL, 0))
        return;
#endif
 ZN21CDirect3DVertexShaderD1Ev((char *)p - offsetof(CDirect3DVertexShaderImpl, vtableSecondary)); }
#else
void ZThn4_N21CDirect3DVertexShaderD0Ev(void *p) {
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &p };
    if (MacShim_Dispatch("ZThn4_N21CDirect3DVertexShaderD0Ev", p, macArguments, NULL, 0))
        return;
#endif
 ZN21CDirect3DVertexShaderD0Ev((char *)p - 4); }
void ZThn4_N21CDirect3DVertexShaderD1Ev(void *p) {
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &p };
    if (MacShim_Dispatch("ZThn4_N21CDirect3DVertexShaderD1Ev", p, macArguments, NULL, 0))
        return;
#endif
 ZN21CDirect3DVertexShaderD1Ev((char *)p - 4); }
#endif
fnptr_t vtbl_CDirect3DVertexShader[] = { (fnptr_t)CDirect3DVertexShader_QueryInterface, (fnptr_t)CDirect3DVertexShader_AddRef, (fnptr_t)CDirect3DVertexShader_Release, (fnptr_t)CDirect3DVertexShader_GetDevice, (fnptr_t)CDirect3DVertexShader_GetFunction, (fnptr_t)ZN21CDirect3DVertexShaderD1Ev, (fnptr_t)ZN21CDirect3DVertexShaderD0Ev };
fnptr_t vtbl_CDirect3DVertexShader_secondary[] = { (fnptr_t)ZThn4_N21CDirect3DVertexShaderD1Ev, (fnptr_t)ZThn4_N21CDirect3DVertexShaderD0Ev };
