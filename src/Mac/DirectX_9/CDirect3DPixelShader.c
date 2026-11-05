#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 textureUsageMask;
} CDirect3DPixelShaderImpl;

typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 textureUsageMask;
    GLuint programId;
} COpenGLATITextFragmentShaderImpl;

typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 textureUsageMask;
#if defined(COD2_X64)
    const void *source;
#else
    UINT32 pad0xc;
#endif
    UINT32 stageCount;
} COpenGLNVidiaRegisterCombinersProgramImpl;

typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 textureUsageMask;
    void *nameStr;
    void *codeStr;
    GLuint programId;
} COpenGLARBFragmentProgramImpl;
typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3DPixelShader_base[];
extern fnptr_t vtbl_CDirect3DPixelShader_ATI[];
extern fnptr_t vtbl_CDirect3DPixelShader_NVidia[];
extern fnptr_t vtbl_CDirect3DPixelShader_ARB[];

#if defined(COD2_X64)
#if !COD2_APPLE_SDK
extern GLboolean glIsEnabled(GLenum cap);
#endif
extern int CDirect3DDevice_DetachPixelShader(const void *shader);

/* Current native rendering also binds programs outside COpenGL. Query the
 * current context rather than keeping Mac's 0x81c/0x820 singleton cache. */
static int PixelShader_ProgramIsBound(GLenum target, GLuint program)
{
    GLint bound = 0;
    glGetProgramivARB(target, 0x8677, &bound);
    return (GLuint)bound == program;
}

static void PixelShader_EnableProgram(GLenum target, GLuint program)
{
    if (!PixelShader_ProgramIsBound(target, program))
        glBindProgramARB(target, program);
    if (!glIsEnabled(target))
        glEnable(target);
}

static void PixelShader_DisableProgram(GLenum target)
{
    if (glIsEnabled(target))
        glDisable(target);
}
#endif


extern void *imp___ZN7COpenGL7sOpenGLE;
extern int g_fp_enable_count;
extern int g_fp_bind_count;

extern unsigned char *imp___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE;
extern unsigned char *imp___ZN15CDirect3DDevice30mNeedsTransformationValidationE;
extern unsigned char *imp___ZN15CDirect3DDevice29mNeedsRasterizationValidationE;

static void CDirect3DPixelShader_InvalidateDeviceState(void)
{
    if (imp___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE)
        *imp___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE = 1;
    if (imp___ZN15CDirect3DDevice30mNeedsTransformationValidationE)
        *imp___ZN15CDirect3DDevice30mNeedsTransformationValidationE = 1;
    if (imp___ZN15CDirect3DDevice29mNeedsRasterizationValidationE)
        *imp___ZN15CDirect3DDevice29mNeedsRasterizationValidationE = 1;
}

ULONG CDirect3DPixelShader_AddRef(const CDirect3DPixelShader *_this);
HRESULT CDirect3DPixelShader_QueryInterface(const CDirect3DPixelShader *_this, const IID *iid, void **ppvObj);
ULONG CDirect3DPixelShader_Release(const CDirect3DPixelShader *_this);
HRESULT CDirect3DPixelShader_GetDevice(const CDirect3DPixelShader *_this, IDirect3DDevice9 **ppDevice);
HRESULT CDirect3DPixelShader_GetFunction(const CDirect3DPixelShader *_this, void *pDstData, UINT *pSizeOfData);

void ZN20CDirect3DPixelShaderD0Ev(const CDirect3DPixelShader *_this);
void ZN20CDirect3DPixelShaderD1Ev(const CDirect3DPixelShader *_this);
void ZN20CDirect3DPixelShaderD2Ev(const CDirect3DPixelShader *_this);

void COpenGLATITextFragmentShader_Enable(const COpenGLATITextFragmentShader *_this);
void COpenGLATITextFragmentShader_Disable(const COpenGLATITextFragmentShader *_this);
void COpenGLATITextFragmentShader_SetConstants(const COpenGLATITextFragmentShader *_this, UINT32 Register, const float *pConstantData, UINT32 Vector4fCount, UINT32 CommandNumber);
void COpenGLATITextFragmentShader_COpenGLATITextFragmentShader(const COpenGLATITextFragmentShader *_this, const void *pSrcData);
void ZN28COpenGLATITextFragmentShaderD0Ev(const COpenGLATITextFragmentShader *_this);
void ZN28COpenGLATITextFragmentShaderD1Ev(const COpenGLATITextFragmentShader *_this);

void COpenGLNVidiaRegisterCombinersProgram_Enable(const COpenGLNVidiaRegisterCombinersProgram *_this);
void COpenGLNVidiaRegisterCombinersProgram_Disable(const COpenGLNVidiaRegisterCombinersProgram *_this);
void COpenGLNVidiaRegisterCombinersProgram_SetConstants(const COpenGLNVidiaRegisterCombinersProgram *_this, UINT32 Register, const float *pConstantData, UINT32 Vector4fCount, UINT32 CommandNumber);
void COpenGLNVidiaRegisterCombinersProgram_COpenGLNVidiaRegisterCombinersProgram(const COpenGLNVidiaRegisterCombinersProgram *_this, const void *pSrcData);
void ZN37COpenGLNVidiaRegisterCombinersProgramD0Ev(const COpenGLNVidiaRegisterCombinersProgram *_this);
void ZN37COpenGLNVidiaRegisterCombinersProgramD1Ev(const COpenGLNVidiaRegisterCombinersProgram *_this);
void ZN37COpenGLNVidiaRegisterCombinersProgramD2Ev(const COpenGLNVidiaRegisterCombinersProgram *_this);

void COpenGLARBFragmentProgram_Enable(const COpenGLARBFragmentProgram *_this);
void COpenGLARBFragmentProgram_Disable(const COpenGLARBFragmentProgram *_this);
void COpenGLARBFragmentProgram_SetConstants(const COpenGLARBFragmentProgram *_this, UINT32 Register, const float *pConstantData, UINT32 Vector4fCount, UINT32 CommandNumber);
void COpenGLARBFragmentProgram_COpenGLARBFragmentProgram(const COpenGLARBFragmentProgram *_this, const string *Name, const string *Code);
void ZN25COpenGLARBFragmentProgramD0Ev(const COpenGLARBFragmentProgram *_this);
void ZN25COpenGLARBFragmentProgramD1Ev(const COpenGLARBFragmentProgram *_this);
void ZN25COpenGLARBFragmentProgramD2Ev(const COpenGLARBFragmentProgram *_this);

ULONG CDirect3DPixelShader_AddRef(const CDirect3DPixelShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DPixelShader_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DPixelShaderImpl *shader;

    shader = (CDirect3DPixelShaderImpl *)_this;
    ++shader->refCount;
    return shader->refCount;
}

HRESULT CDirect3DPixelShader_QueryInterface(const CDirect3DPixelShader *_this, const IID *iid, void **ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DPixelShader_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;

    *ppvObj = (void *)_this;
#if defined(COD2_X64)
    ((ULONG (*)(const void *))(*(void ***)_this)[1])(_this);
#else
    CDirect3DPixelShader_AddRef(_this);
#endif
    return 0;
}

ULONG CDirect3DPixelShader_Release(const CDirect3DPixelShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DPixelShader_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DPixelShaderImpl *shader;
    ULONG refCount;

    shader = (CDirect3DPixelShaderImpl *)_this;
    refCount = --shader->refCount;
    if (!refCount) {

        void **vtable = shader->vtable;
        ((void (*)(const CDirect3DPixelShader *))vtable[6])(_this);
    }

    return refCount;
}

void ZN20CDirect3DPixelShaderD2Ev(const CDirect3DPixelShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20CDirect3DPixelShaderD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DPixelShaderImpl *shader = (CDirect3DPixelShaderImpl *)_this;
    shader->vtable = vtbl_CDirect3DPixelShader_base;
#if defined(COD2_X64)
    CDirect3DDevice_DetachPixelShader(_this);
#endif
    CDirect3DPixelShader_InvalidateDeviceState();
}

void ZN20CDirect3DPixelShaderD1Ev(const CDirect3DPixelShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20CDirect3DPixelShaderD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN20CDirect3DPixelShaderD2Ev(_this);
}

void ZN20CDirect3DPixelShaderD0Ev(const CDirect3DPixelShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20CDirect3DPixelShaderD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN20CDirect3DPixelShaderD2Ev(_this);
    free((void *)_this);
}

void COpenGLATITextFragmentShader_Enable(const COpenGLATITextFragmentShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLATITextFragmentShader_Enable", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    PixelShader_EnableProgram(0x8200, ((const COpenGLATITextFragmentShaderImpl *)_this)->programId);
#else
    (void)_this;

    return 0;
#endif
}

void COpenGLATITextFragmentShader_Disable(const COpenGLATITextFragmentShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLATITextFragmentShader_Disable", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    (void)_this;
    PixelShader_DisableProgram(0x8200);
#else
    (void)_this;

    return 0;
#endif
}

void COpenGLATITextFragmentShader_SetConstants(const COpenGLATITextFragmentShader *_this, UINT32 Register, const float *pConstantData, UINT32 Vector4fCount, UINT32 CommandNumber)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Register, &pConstantData, &Vector4fCount, &CommandNumber };
    if (MacShim_Dispatch("COpenGLATITextFragmentShader_SetConstants", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    (void)_this;
    (void)CommandNumber;
    for (UINT32 i = 0; i < Vector4fCount; ++i)
        glProgramEnvParameter4fvARB(0x8200, Register + i, pConstantData + i * 4);
#else
    (void)_this;
    (void)Register;
    (void)pConstantData;
    (void)Vector4fCount;
    (void)CommandNumber;

    return 0;
#endif
}

void COpenGLATITextFragmentShader_COpenGLATITextFragmentShader(const COpenGLATITextFragmentShader *_this, const void *pSrcData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSrcData };
    if (MacShim_Dispatch("COpenGLATITextFragmentShader_COpenGLATITextFragmentShader", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    COpenGLATITextFragmentShaderImpl *shader = (COpenGLATITextFragmentShaderImpl *)_this;
    GLint previousProgram;
    shader->vtable = vtbl_CDirect3DPixelShader_ATI;
    shader->refCount = 1;
    shader->textureUsageMask = 0;
    shader->programId = 0;
    glGetProgramivARB(0x8200, 0x8677, &previousProgram);
    glGenProgramsARB(1, &shader->programId);
    glBindProgramARB(0x8200, shader->programId);
    glProgramStringARB(0x8200, 0x8875, (GLsizei)strlen((const char *)pSrcData), pSrcData);
    glBindProgramARB(0x8200, (GLuint)previousProgram);
#else
    COpenGLATITextFragmentShaderImpl *shader;

    (void)pSrcData;

    shader = (COpenGLATITextFragmentShaderImpl *)_this;
    shader->vtable = vtbl_CDirect3DPixelShader_ATI;
    shader->refCount = 1;
    shader->textureUsageMask = 0;
    shader->programId = 0;

    return 0;
#endif
}

static void COpenGLATITextFragmentShader_DestroyImpl(const COpenGLATITextFragmentShader *_this)
{
    COpenGLATITextFragmentShaderImpl *shader = (COpenGLATITextFragmentShaderImpl *)_this;
    shader->vtable = vtbl_CDirect3DPixelShader_ATI;
#if defined(COD2_X64)
    if (PixelShader_ProgramIsBound(0x8200, shader->programId))
        PixelShader_DisableProgram(0x8200);
    glDeleteProgramsARB(1, &shader->programId);
#endif

    ZN20CDirect3DPixelShaderD2Ev((const CDirect3DPixelShader *)_this);
}

void ZN28COpenGLATITextFragmentShaderD1Ev(const COpenGLATITextFragmentShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN28COpenGLATITextFragmentShaderD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLATITextFragmentShader_DestroyImpl(_this);
}

void ZN28COpenGLATITextFragmentShaderD0Ev(const COpenGLATITextFragmentShader *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN28COpenGLATITextFragmentShaderD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLATITextFragmentShader_DestroyImpl(_this);
    free((void *)_this);
}

void COpenGLNVidiaRegisterCombinersProgram_Enable(const COpenGLNVidiaRegisterCombinersProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLNVidiaRegisterCombinersProgram_Enable", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
    return 0;
}

void COpenGLNVidiaRegisterCombinersProgram_Disable(const COpenGLNVidiaRegisterCombinersProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLNVidiaRegisterCombinersProgram_Disable", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    (void)_this;
    /* NV has no ARB program binding; these are the two enable capabilities. */
    if (glIsEnabled(0x86de) || glIsEnabled(0x8522)) {
        glDisable(0x86de);
        glDisable(0x8522);
    }
#else
    (void)_this;

    return 0;
#endif
}

void COpenGLNVidiaRegisterCombinersProgram_SetConstants(const COpenGLNVidiaRegisterCombinersProgram *_this, UINT32 Register, const float *pConstantData, UINT32 Vector4fCount, UINT32 CommandNumber)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Register, &pConstantData, &Vector4fCount, &CommandNumber };
    if (MacShim_Dispatch("COpenGLNVidiaRegisterCombinersProgram_SetConstants", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    (void)Register;
    (void)Vector4fCount;
    if (((const COpenGLNVidiaRegisterCombinersProgramImpl *)_this)->stageCount <= 2)
        glCombinerParameterfvNV(0x852a + CommandNumber, pConstantData);
    else
        glCombinerStageParameterfvNV(0x8550 + CommandNumber, 0x852a, pConstantData);
#else
    (void)_this;
    (void)Register;
    (void)pConstantData;
    (void)Vector4fCount;
    (void)CommandNumber;

    return 0;
#endif
}

void COpenGLNVidiaRegisterCombinersProgram_COpenGLNVidiaRegisterCombinersProgram(const COpenGLNVidiaRegisterCombinersProgram *_this, const void *pSrcData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSrcData };
    if (MacShim_Dispatch("COpenGLNVidiaRegisterCombinersProgram_COpenGLNVidiaRegisterCombinersProgram", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLNVidiaRegisterCombinersProgramImpl *program;

    program = (COpenGLNVidiaRegisterCombinersProgramImpl *)_this;
    program->vtable = vtbl_CDirect3DPixelShader_NVidia;
    program->refCount = 1;
    program->textureUsageMask = 0;
#if defined(COD2_X64)
    program->source = pSrcData;
#else
    program->pad0xc = (UINT32)(unsigned long)pSrcData;
#endif
    program->stageCount = 0;
    return 0;
}

static void COpenGLNVidiaRegisterCombinersProgram_DestroyImpl(const COpenGLNVidiaRegisterCombinersProgram *_this)
{
    COpenGLNVidiaRegisterCombinersProgramImpl *program;
    program = (COpenGLNVidiaRegisterCombinersProgramImpl *)_this;
    program->vtable = vtbl_CDirect3DPixelShader_NVidia;
#if defined(COD2_X64)
    if (CDirect3DDevice_DetachPixelShader(_this))
        COpenGLNVidiaRegisterCombinersProgram_Disable(_this);
#endif

    ZN20CDirect3DPixelShaderD2Ev((const CDirect3DPixelShader *)_this);
}

void ZN37COpenGLNVidiaRegisterCombinersProgramD2Ev(const COpenGLNVidiaRegisterCombinersProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN37COpenGLNVidiaRegisterCombinersProgramD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLNVidiaRegisterCombinersProgram_DestroyImpl(_this);
}

void ZN37COpenGLNVidiaRegisterCombinersProgramD1Ev(const COpenGLNVidiaRegisterCombinersProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN37COpenGLNVidiaRegisterCombinersProgramD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLNVidiaRegisterCombinersProgram_DestroyImpl(_this);
}

void ZN37COpenGLNVidiaRegisterCombinersProgramD0Ev(const COpenGLNVidiaRegisterCombinersProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN37COpenGLNVidiaRegisterCombinersProgramD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLNVidiaRegisterCombinersProgram_DestroyImpl(_this);
    free((void *)_this);
}

void COpenGLARBFragmentProgram_Enable(const COpenGLARBFragmentProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLARBFragmentProgram_Enable", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    PixelShader_EnableProgram(0x8804, ((const COpenGLARBFragmentProgramImpl *)_this)->programId);
#else
    COpenGLARBFragmentProgramImpl *program = (COpenGLARBFragmentProgramImpl *)_this;
    if (program->programId) {
        glEnable(0x8804);
        glBindProgramARB(0x8804, program->programId);
    }
    return 0;
#endif
}

void COpenGLARBFragmentProgram_Disable(const COpenGLARBFragmentProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("COpenGLARBFragmentProgram_Disable", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    (void)_this;
    PixelShader_DisableProgram(0x8804);
#else
    (void)_this;
    glDisable(0x8804);
    glBindProgramARB(0x8804, 0);
    return 0;
#endif
}

void COpenGLARBFragmentProgram_SetConstants(const COpenGLARBFragmentProgram *_this, UINT32 Register, const float *pConstantData, UINT32 Vector4fCount, UINT32 CommandNumber)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Register, &pConstantData, &Vector4fCount, &CommandNumber };
    if (MacShim_Dispatch("COpenGLARBFragmentProgram_SetConstants", _this, macArguments, NULL, 0))
        return;
#endif

    UINT32 i;
    const float *pf = pConstantData;
    (void)_this;
    (void)CommandNumber;
    for (i = 0; i < Vector4fCount; i++) {
        glProgramEnvParameter4fvARB(0x8804, Register + i, pf);
        pf += 4;
    }
    return 0;
}

void COpenGLARBFragmentProgram_COpenGLARBFragmentProgram(const COpenGLARBFragmentProgram *_this, const string *Name, const string *Code)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Name, &Code };
    if (MacShim_Dispatch("COpenGLARBFragmentProgram_COpenGLARBFragmentProgram", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLARBFragmentProgramImpl *program;
    const char *codeStr;
    int codeLen;

    program = (COpenGLARBFragmentProgramImpl *)_this;
    program->vtable = vtbl_CDirect3DPixelShader_ARB;
    program->refCount = 1;
    program->textureUsageMask = 0;
    program->nameStr = NULL;
    program->codeStr = NULL;
    program->programId = 0;

    codeStr = *(const char **)Code;
    if (!codeStr || !codeStr[0])
        return 0;

    codeLen = strlen(codeStr);

    {
        const char *p = codeStr;
        while ((p = strstr(p, "texture[")) != NULL) {
            p += 8;
            while (*p == ' ')
                p++;
            if (*p >= '0' && *p <= '9') {
                int idx = *p - '0';
                if (idx < 16)
                    program->textureUsageMask |= (1u << idx);
            }
            p++;
        }
    }

    glGenProgramsARB(1, &program->programId);
    glBindProgramARB(0x8804, program->programId);
    glProgramStringARB(0x8804, 0x8875 ,
                       codeLen, codeStr);

    {
        int errorPos = 0;
        glGetIntegerv(0x864B , &errorPos);
        if (errorPos != -1) {
            fprintf(stderr, "[ARB FP] Compile error at position %d in program %u\n",
                    errorPos, program->programId);
        }
    }

    glBindProgramARB(0x8804, 0);

    return 0;
}

static void COpenGLARBFragmentProgram_DestroyImpl(const COpenGLARBFragmentProgram *_this)
{
    COpenGLARBFragmentProgramImpl *program;
    program = (COpenGLARBFragmentProgramImpl *)_this;
    program->vtable = vtbl_CDirect3DPixelShader_ARB;
    if (program->programId) {
        glDeleteProgramsARB(1, &program->programId);
        program->programId = 0;
    }
    ZN20CDirect3DPixelShaderD2Ev((const CDirect3DPixelShader *)_this);
}

void ZN25COpenGLARBFragmentProgramD2Ev(const COpenGLARBFragmentProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN25COpenGLARBFragmentProgramD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLARBFragmentProgram_DestroyImpl(_this);
}

void ZN25COpenGLARBFragmentProgramD1Ev(const COpenGLARBFragmentProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN25COpenGLARBFragmentProgramD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLARBFragmentProgram_DestroyImpl(_this);
}

void ZN25COpenGLARBFragmentProgramD0Ev(const COpenGLARBFragmentProgram *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN25COpenGLARBFragmentProgramD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    COpenGLARBFragmentProgram_DestroyImpl(_this);
    free((void *)_this);
}

HRESULT CDirect3DPixelShader_GetDevice(const CDirect3DPixelShader *_this, IDirect3DDevice9 **ppDevice)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppDevice };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DPixelShader_GetDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    *ppDevice = NULL;
    return 0;
}

HRESULT CDirect3DPixelShader_GetFunction(const CDirect3DPixelShader *_this, void *pDstData, UINT *pSizeOfData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pDstData, &pSizeOfData };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DPixelShader_GetFunction", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)pDstData;
    *pSizeOfData = 0;
    return 0;
}

fnptr_t vtbl_CDirect3DPixelShader_base[] = { (fnptr_t)CDirect3DPixelShader_QueryInterface, (fnptr_t)CDirect3DPixelShader_AddRef, (fnptr_t)CDirect3DPixelShader_Release, (fnptr_t)CDirect3DPixelShader_GetDevice, (fnptr_t)CDirect3DPixelShader_GetFunction, (fnptr_t)ZN20CDirect3DPixelShaderD1Ev, (fnptr_t)ZN20CDirect3DPixelShaderD0Ev };
fnptr_t vtbl_CDirect3DPixelShader_ATI[] = { (fnptr_t)CDirect3DPixelShader_QueryInterface, (fnptr_t)CDirect3DPixelShader_AddRef, (fnptr_t)CDirect3DPixelShader_Release, (fnptr_t)CDirect3DPixelShader_GetDevice, (fnptr_t)CDirect3DPixelShader_GetFunction, (fnptr_t)ZN28COpenGLATITextFragmentShaderD1Ev, (fnptr_t)ZN28COpenGLATITextFragmentShaderD0Ev, (fnptr_t)COpenGLATITextFragmentShader_Enable, (fnptr_t)COpenGLATITextFragmentShader_Disable, (fnptr_t)COpenGLATITextFragmentShader_SetConstants };
fnptr_t vtbl_CDirect3DPixelShader_NVidia[] = { (fnptr_t)CDirect3DPixelShader_QueryInterface, (fnptr_t)CDirect3DPixelShader_AddRef, (fnptr_t)CDirect3DPixelShader_Release, (fnptr_t)CDirect3DPixelShader_GetDevice, (fnptr_t)CDirect3DPixelShader_GetFunction, (fnptr_t)ZN37COpenGLNVidiaRegisterCombinersProgramD1Ev, (fnptr_t)ZN37COpenGLNVidiaRegisterCombinersProgramD0Ev, (fnptr_t)COpenGLNVidiaRegisterCombinersProgram_Enable, (fnptr_t)COpenGLNVidiaRegisterCombinersProgram_Disable, (fnptr_t)COpenGLNVidiaRegisterCombinersProgram_SetConstants };
fnptr_t vtbl_CDirect3DPixelShader_ARB[] = { (fnptr_t)CDirect3DPixelShader_QueryInterface, (fnptr_t)CDirect3DPixelShader_AddRef, (fnptr_t)CDirect3DPixelShader_Release, (fnptr_t)CDirect3DPixelShader_GetDevice, (fnptr_t)CDirect3DPixelShader_GetFunction, (fnptr_t)ZN25COpenGLARBFragmentProgramD1Ev, (fnptr_t)ZN25COpenGLARBFragmentProgramD0Ev, (fnptr_t)COpenGLARBFragmentProgram_Enable, (fnptr_t)COpenGLARBFragmentProgram_Disable, (fnptr_t)COpenGLARBFragmentProgram_SetConstants };
