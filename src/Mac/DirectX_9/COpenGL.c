#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>

typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_COpenGL[];
extern fnptr_t vtbl_COpenGLVertexProgram[];

#if defined(COD2_X64)
#include "lp64_gl_state.h"
static COpenGLNative s_nativeOpenGL;
#define COpenGL_sOpenGL ((unsigned char *)&s_nativeOpenGL)
void *imp__ZN7COpenGL7sOpenGLE = &s_nativeOpenGL;

unsigned char *COpenGL_TexUnitBase(const COpenGL *_this, UINT32 Unit)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit };
    unsigned char * macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_TexUnitBase", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return (unsigned char *)&((COpenGLNative *)_this)->texUnits[Unit];
}

void *COpenGL_GetVAOBindingList(const COpenGL *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    void * macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_GetVAOBindingList", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    COpenGLBindingList *head = &((COpenGLNative *)_this)->vaoBindings;
    if (!head->next) {
        head->next = head;
        head->prev = head;
    }
    return head;
}
#else
unsigned char __attribute__((visibility("default")))
COpenGL_sOpenGL[4096] = { 0 };

void *imp__ZN7COpenGL7sOpenGLE = COpenGL_sOpenGL;

#define COPENGL_TEXUNIT_STRIDE 304
#define COPENGL_MAX_TEXUNITS 16
static unsigned char COpenGL_texUnitState[COPENGL_MAX_TEXUNITS * COPENGL_TEXUNIT_STRIDE];

unsigned char *COpenGL_TexUnitBase(const COpenGL *_this, UINT32 Unit)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit };
    unsigned char * macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_TexUnitBase", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    unsigned char *p = (unsigned char *)_this;

    if (*(void **)(p + 1628) == 0)
        *(void **)(p + 1628) = COpenGL_texUnitState;
    return (unsigned char *)(*(void **)(p + 1628)) + Unit * COPENGL_TEXUNIT_STRIDE;
}

#endif

#if defined(COD2_X64)
/* Secondary COpenGLTexture base shared by the native texture providers. */
typedef struct {
    void **vtable;
    GLuint *name;
    GLenum wrapS, wrapT, wrapR;
    GLuint border;
    GLenum minFilter, magFilter;
    GLint lastLevel;
    GLfloat anisotropy;
    GLint width, height, depth;
} COpenGLTexturePrefixNative;
_Static_assert(sizeof(COpenGLTexturePrefixNative) == sizeof(COpenGLTextureStateNative), "native texture prefix width");
_Static_assert(offsetof(COpenGLTexturePrefixNative, lastLevel) == offsetof(COpenGLTextureStateNative, lastLevel), "native last-level contract");
_Static_assert(offsetof(COpenGLTexturePrefixNative, anisotropy) == offsetof(COpenGLTextureStateNative, anisotropy), "native anisotropy contract");
_Static_assert(offsetof(COpenGLTexturePrefixNative, depth) == offsetof(COpenGLTextureStateNative, depth), "native texture field extent");

typedef COpenGLVAONative COpenGLVAOPrefixNative;

typedef struct COpenGLBindingNodeNative {
    struct COpenGLBindingNodeNative *next, *prev;
    GLuint *name;
} COpenGLBindingNodeNative;

extern void CMemoryBuffer_Reset(void);
extern void CVAOPacket_Shutdown(void);
extern void CVAOPacket_InitializeGenericVAO(void);
extern void CFence_Shutdown(void);
extern int MacDisplay_GetCardType(void);
extern bool MacDisplay_IsFullscreen(void);
extern bool MacOpenGLUtils_IsGLExtensionSupported(const char *);
extern UINT32 MacOpenGLUtils_GetPCPixelShaderVersion(void);
extern bool MacOpenGLUtils_AreMatricesDifferent4x4(const int *, const int *);
extern void CBaseVA_CBaseVA(const CBaseVA *);
extern void CBaseVA_Reset(const CBaseVA *);
extern void CColorArray_Set(const CColorArray *, GLenum, GLsizei, const void *, GLint);
extern void CNormalArray_Set(const CNormalArray *, GLenum, GLsizei, const void *);
extern void CVertexArray_Set(const CVertexArray *, GLenum, GLsizei, const void *, GLint);
extern void CTexCoordArray_Set(const CTexCoordArray *, GLenum, GLsizei, const void *, GLint);
extern void *imp___ZTV11CColorArray, *imp___ZTV20CSecondaryColorArray;
extern Bool g_InhibitOpenGLErrors;
extern void *imp___ZTV7CBaseVA;
extern void *imp___ZTV12CNormalArray, *imp___ZTV12CVertexArray, *imp___ZTV14CTexCoordArray;

static const GLenum s_textureTargetsNative[3] = { 0xde1, 0x806f, 0x8513 };
static const float s_identityNative[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};
static UINT32 COpenGL_TargetIndexNative(GLenum target)
{
    return target == 0x8513 ? 2 : target == 0x806f ? 1 : 0;
}
static void COpenGL_ClientActiveNative(COpenGLNative *gl, UINT32 unit)
{
    if (gl->clientActiveTexUnit != unit) {
        gl->clientActiveTexUnit = unit;
        glClientActiveTextureARB(0x84c0 + unit);
    }
}
static void COpenGL_ResetArrayNative(CBaseVAImpl *array, void *vtable)
{
    CBaseVA_CBaseVA((const CBaseVA *)array);
    array->vtable = (void **)((char *)vtable + 2 * sizeof(void *));
}
static void COpenGL_EnableArrayNative(CBaseVAImpl *array, GLenum kind,
                                      GLint size, GLenum type, GLsizei stride,
                                      const void *pointer)
{
    if (!array->mEnabled || array->mNeedsValidation) {
        array->mEnabled = 1;
        glEnableClientState(kind);
    }
    if (array->mNeedsValidation || array->mVSize != size ||
        array->mVType != type || array->mStride != stride || array->mpStream != pointer) {
        if (kind == 0x8075)
            CNormalArray_Set((const CNormalArray *)array, type, stride, pointer);
        else if (kind == 0x8076)
            CColorArray_Set((const CColorArray *)array, type, stride, pointer, size);
        else if (kind == 0x8074)
            CVertexArray_Set((const CVertexArray *)array, type, stride, pointer, size);
        else
            CTexCoordArray_Set((const CTexCoordArray *)array, type, stride, pointer, size);
    }
    array->mNeedsValidation = 0;
}
static void COpenGL_DisableArrayNative(CBaseVAImpl *array, GLenum kind)
{
    if (array->mEnabled || array->mNeedsValidation) {
        array->mEnabled = 0;
        glDisableClientState(kind);
    }
    array->mNeedsValidation = 0;
}
static void COpenGL_CopyArrayNative(CBaseVAImpl *dst, const CBaseVAImpl *src)
{
    dst->mEnabled = src->mEnabled;
    dst->mNeedsValidation = 0;
    dst->mVSize = src->mVSize;
    dst->mVType = src->mVType;
    dst->mStride = src->mStride;
    dst->mpStream = src->mpStream;
}
#endif

UINT32 COpenGL_sFrameCount = 0;
bool COpenGL_sDrawFlag = 0;
GLuint COpenGL_sShowTextureID = 0;
GLint g_VAOID = 0;

unsigned int COpenGL_SetColorMaterialEnable(const COpenGL *_this, int Value)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Value };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetColorMaterialEnable", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    if (gl->colorMaterialEnabled != (unsigned char)Value) {
        gl->colorMaterialEnabled = (unsigned char)Value;
        if ((unsigned char)Value) glEnable(0xb57); else glDisable(0xb57);
    }
    return 0;
#else

    (void)_this;
    (void)Value;
    return 0;

#endif
}

unsigned int COpenGL_SetActiveTexUnit(const COpenGL *_this, UINT32 Unit)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetActiveTexUnit", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    if (gl->activeTexUnit != Unit) {
        gl->activeTexUnit = Unit;
        glActiveTextureARB(Unit + 0x84C0);
    }
#else
    unsigned char *p = (unsigned char *)_this;
    if (*(int *)(p + 4) != (int)Unit) {
        *(int *)(p + 4) = (int)Unit;
        glActiveTextureARB(Unit + 0x84C0 );
    }
#endif
    return 0;
}

unsigned int COpenGL_SetDither(const COpenGL *_this, int Value)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Value };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetDither", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    if (gl->ditherEnabled != (unsigned char)Value) {
        gl->ditherEnabled = (unsigned char)Value;
        if ((unsigned char)Value) glEnable(0xbd0); else glDisable(0xbd0);
    }
    return 0;
#else

    (void)_this;
    (void)Value;
    return 0;

#endif
}

unsigned int COpenGL_SetBlendEXT(const COpenGL *_this, int ForceValidation,
                                 GLenum SrcFactorRGB, GLenum DstFactorRGB, GLenum SrcFactorAlpha, GLenum DstFactorAlpha)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ForceValidation, &SrcFactorRGB, &DstFactorRGB, &SrcFactorAlpha, &DstFactorAlpha };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetBlendEXT", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    if (ForceValidation == 0 &&
        SrcFactorRGB == gl->blendSrcRGB && DstFactorRGB == gl->blendDstRGB &&
        SrcFactorAlpha == gl->blendSrcAlpha && DstFactorAlpha == gl->blendDstAlpha)
        return 0;
    gl->blendSrcRGB = SrcFactorRGB;
    gl->blendDstRGB = DstFactorRGB;
    gl->blendSrcAlpha = SrcFactorAlpha;
    gl->blendDstAlpha = DstFactorAlpha;
    glBlendFuncSeparateEXT(SrcFactorRGB, DstFactorRGB, SrcFactorAlpha, DstFactorAlpha);
#else
    unsigned char *p = (unsigned char *)_this;
    if (ForceValidation == 0 &&
        SrcFactorRGB == *(int *)(p + 2084) && DstFactorRGB == *(int *)(p + 2088) &&
        SrcFactorAlpha == *(int *)(p + 2092) && DstFactorAlpha == *(int *)(p + 2096))
        return 0;
    *(int *)(p + 2084) = SrcFactorRGB;
    *(int *)(p + 2088) = DstFactorRGB;
    *(int *)(p + 2092) = SrcFactorAlpha;
    *(int *)(p + 2096) = DstFactorAlpha;
    glBlendFuncSeparateEXT(SrcFactorRGB, DstFactorRGB, SrcFactorAlpha, DstFactorAlpha);
#endif
    return 0;
}

unsigned int COpenGL_SetBlend(const COpenGL *_this, int ForceValidation,
                              GLenum SrcFactorRGB, GLenum DstFactorRGB)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ForceValidation, &SrcFactorRGB, &DstFactorRGB };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetBlend", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    if (ForceValidation == 0 &&
        SrcFactorRGB == gl->blendSrcRGB && DstFactorRGB == gl->blendDstRGB)
        return 0;
    gl->blendSrcRGB = SrcFactorRGB;
    gl->blendDstRGB = DstFactorRGB;
    glBlendFunc(SrcFactorRGB, DstFactorRGB);
#else
    unsigned char *p = (unsigned char *)_this;
    if (ForceValidation == 0 &&
        SrcFactorRGB == *(int *)(p + 2084) && DstFactorRGB == *(int *)(p + 2088))
        return 0;
    *(int *)(p + 2084) = SrcFactorRGB;
    *(int *)(p + 2088) = DstFactorRGB;
    glBlendFunc(SrcFactorRGB, DstFactorRGB);
#endif
    return 0;
}

unsigned int COpenGL_SetVertexProgram(const COpenGL *_this, const COpenGLVertexProgram *pOGLVertexProgramInfo)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pOGLVertexProgramInfo };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetVertexProgram", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    /* The native program provider stores its GLuint first in baseState. */
    GLuint program = *(const GLuint *)pOGLVertexProgramInfo;
    if (gl->vertexProgram != program) {
        gl->vertexProgram = program;
        glBindProgramARB(0x8620, program);
    }
    return 0;
#else

    (void)_this;
    (void)pOGLVertexProgramInfo;
    return 0;

#endif
}

unsigned int COpenGL_SetVARInfo(const COpenGL *_this, const unsigned int *pVAStart, UINT32 Size)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pVAStart, &Size };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetVARInfo", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    if (gl->vertexArrayStart != pVAStart || gl->vertexArraySize != Size) {
        gl->vertexArrayStart = pVAStart;
        gl->vertexArraySize = Size;
        if (gl->supportsVAR)
            glVertexArrayRangeAPPLE(Size, pVAStart);
    }
    return 0;
#else

    (void)_this;
    (void)pVAStart;
    (void)Size;
    return 0;

#endif
}

unsigned int COpenGL_DisableVertexProgramStream(const COpenGL *_this, UINT32 AttribArray)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &AttribArray };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_DisableVertexProgramStream", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    VertexProgramStreamStateNative *array = &((COpenGLNative *)_this)->genericArrays[AttribArray];
    if (array->mEnabled || array->mNeedsValidation) {
        array->mEnabled = array->mNeedsValidation = 0;
        glDisableVertexAttribArrayARB(AttribArray);
    }
    return 0;
#else

    (void)_this;
    (void)AttribArray;
    return 0;

#endif
}

unsigned int COpenGL_CTexUnit_SetTex(const CTexUnit *_this, GLenum Target, const COpenGLTexture *pOpenGLTexInfo)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Target, &pOpenGLTexInfo };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_CTexUnit_SetTex", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CTexUnitNative *unit = (CTexUnitNative *)_this;
    const COpenGLTexturePrefixNative *texture = (const COpenGLTexturePrefixNative *)pOpenGLTexInfo;
    UINT32 index = COpenGL_TargetIndexNative(Target);
    glBindTexture(Target, *texture->name);
    unit->mTexID[index] = texture->name;
    unit->mTexWrapS[index] = texture->wrapS;
    unit->mTexWrapT[index] = texture->wrapT;
    unit->mTexWrapR[index] = texture->wrapR;
    unit->mTexMinFilter[index] = texture->minFilter;
    unit->mTexMagFilter[index] = texture->magFilter;
    unit->mTexAnisotropicFilter[index] = texture->anisotropy;
    unit->mTexLastLevel[index] = texture->lastLevel;
    /* Modern GL can expose image units beyond the fixed-function limit.
     * Their sampler binding is valid; fixed-function target enables are not. */
    if (unit->mIsProgramableOnly)
        return 0;
    if (Target == 0xde1) {
        if (!unit->mTargetEnabled[0]) { glEnable(0xde1); unit->mTargetEnabled[0] = 1; }
        if (unit->mTargetEnabled[1]) { glDisable(0x806f); unit->mTargetEnabled[1] = 0; }
        if (unit->mTargetEnabled[2]) { glDisable(0x8513); unit->mTargetEnabled[2] = 0; }
    } else if (Target == 0x806f) {
        if (!unit->mTargetEnabled[1]) { glEnable(0x806f); unit->mTargetEnabled[1] = 1; }
        if (unit->mTargetEnabled[2]) { glDisable(0x8513); unit->mTargetEnabled[2] = 0; }
    } else if (Target == 0x8513 && !unit->mTargetEnabled[2]) {
        glEnable(0x8513); unit->mTargetEnabled[2] = 1;
    }
    return 0;
#else

    (void)_this;
    (void)Target;
    (void)pOpenGLTexInfo;
    return 0;

#endif
}

unsigned int COpenGL_SetTex(const COpenGL *_this, UINT32 Unit, GLenum Target, const COpenGLTexture *pOpenGLTexInfo)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &Target, &pOpenGLTexInfo };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTex", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CTexUnitNative *unit = &((COpenGLNative *)_this)->texUnits[Unit];
    const COpenGLTexturePrefixNative *texture = (const COpenGLTexturePrefixNative *)pOpenGLTexInfo;
    UINT32 index = COpenGL_TargetIndexNative(Target);
    int stateChanged = Target == 0xde1
        ? (!unit->mTargetEnabled[0] || unit->mTargetEnabled[1] || unit->mTargetEnabled[2])
        : Target == 0x806f ? (!unit->mTargetEnabled[1] || unit->mTargetEnabled[2])
        : Target == 0x8513 ? !unit->mTargetEnabled[2] : 0;
    if (!unit->mTexID[index] || *unit->mTexID[index] != *texture->name || stateChanged) {
        COpenGL_SetActiveTexUnit(_this, Unit);
        COpenGL_CTexUnit_SetTex((const CTexUnit *)unit, Target, pOpenGLTexInfo);
    }
    return 0;
#else

    (void)_this;
    (void)Unit;
    (void)Target;
    (void)pOpenGLTexInfo;
    return 0;

#endif
}

unsigned int COpenGL_SetVertexProgramStreamInfo(const COpenGL *_this, UINT32 AttribArray,
                                                GLint VSize, GLenum VType, int Normalized, GLsizei Stride, const unsigned int *pStream)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &AttribArray, &VSize, &VType, &Normalized, &Stride, &pStream };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetVertexProgramStreamInfo", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    VertexProgramStreamStateNative *array = &gl->genericArrays[AttribArray];
    if (!array->mEnabled || array->mNeedsValidation) {
        array->mEnabled = 1;
        glEnableVertexAttribArrayARB(AttribArray);
    }
    if (AttribArray == 3) gl->normalArray.mNeedsValidation = 1;
    else if (AttribArray == 5) gl->colorArray.mNeedsValidation = 1;
    else if (AttribArray == 0) gl->vertexArray.mNeedsValidation = 1;
    else if (AttribArray > 7 && AttribArray - 8 < gl->texUnitCount)
        gl->texUnits[AttribArray - 8].mTexCoordArray.mNeedsValidation = 1;
    if (array->mNeedsValidation || array->mVSize != VSize || array->mVType != VType ||
        array->mNormalized != (GLboolean)Normalized || array->mStride != Stride || array->mpStream != pStream) {
        array->mVSize = VSize;
        array->mVType = VType;
        array->mNormalized = (GLboolean)Normalized;
        array->mStride = Stride;
        array->mpStream = pStream;
        glVertexAttribPointerARB(AttribArray, VSize, VType, (GLboolean)Normalized, Stride, pStream);
    }
    array->mNeedsValidation = 0;
    return 0;
#else

    (void)_this;
    (void)AttribArray;
    (void)VSize;
    (void)VType;
    (void)Normalized;
    (void)Stride;
    (void)pStream;
    return 0;

#endif
}

unsigned int COpenGL_CTexUnit_Reset(const CTexUnit *_this, int SupportsAnisotropicFiltering,
                                    int SupportsLODBias, int IsProgramableOnly)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &SupportsAnisotropicFiltering, &SupportsLODBias, &IsProgramableOnly };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_CTexUnit_Reset", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CTexUnitNative *unit = (CTexUnitNative *)_this;
    unit->mIsProgramableOnly = (unsigned char)IsProgramableOnly;
    g_InhibitOpenGLErrors = 1;
    glDisable(0xde0);
    unit->mTargetEnabled[0] = 1; glEnable(0xde1); unit->mTexID[0] = NULL;
    unit->mTargetEnabled[1] = 0; glDisable(0x806f); unit->mTexID[1] = NULL;
    unit->mTargetEnabled[2] = 0; glDisable(0x8513); unit->mTexID[2] = NULL;
    g_InhibitOpenGLErrors = 0;
    if (!IsProgramableOnly) {
        memcpy(unit->mTexMatrix, s_identityNative, sizeof(unit->mTexMatrix));
        glLoadIdentity();
        glTexEnvi(0x2300, 0x2200, 0x8570);
    }
    for (unsigned i = 0; i < 4; ++i) unit->mTexFactor[i] = 1.0f;
    glTexEnvfv(0x2300, 0x2201, unit->mTexFactor);
    if (SupportsLODBias) {
        glTexEnvf(0x8500, 0x8501, 0.0f);
        unit->mLodBias = 0.0f;
    }
    for (unsigned i = 0; i < 3; ++i) {
        GLenum target = s_textureTargetsNative[i];
        glTexParameteri(target, 0x2802, 0x2901); unit->mTexWrapS[i] = 0x2901;
        glTexParameteri(target, 0x2803, 0x2901); unit->mTexWrapT[i] = 0x2901;
        glTexParameteri(target, 0x8072, 0x2901); unit->mTexWrapR[i] = 0x2901;
        glTexParameteri(target, 0x2801, 0x2702); unit->mTexMinFilter[i] = 0x2702;
        glTexParameteri(target, 0x2800, 0x2601); unit->mTexMagFilter[i] = 0x2601;
        glTexParameteri(target, 0x813d, 1000); unit->mTexLastLevel[i] = 1000;
        if (SupportsAnisotropicFiltering) {
            glTexParameterf(target, 0x84fe, 1.0f);
            unit->mTexAnisotropicFilter[i] = 1.0f;
        }
    }
    if (IsProgramableOnly) return 0;
    for (unsigned i = 0; i < 4; ++i) {
        unit->mTexGenEnable[i] = 0; glDisable(0xc60 + i);
    }
    for (unsigned i = 0; i < 4; ++i) {
        unit->mTexGenMode[i] = 0x2401; glTexGeni(0x2000 + i, 0x2500, 0x2401);
    }
    glTexEnvi(0x2300, 0x8571, 0x2100); unit->mCombinerColorOp = 0x2100;
    glTexEnvi(0x2300, 0x8580, 0x1702); unit->mCombinerColorSource0 = 0x1702;
    glTexEnvi(0x2300, 0x8590, 0x300); unit->mCombinerColorOperand0 = 0x300;
    glTexEnvi(0x2300, 0x8581, 0x8578); unit->mCombinerColorSource1 = 0x8578;
    glTexEnvi(0x2300, 0x8591, 0x300); unit->mCombinerColorOperand1 = 0x300;
    glTexEnvi(0x2300, 0x8582, 0x8576); unit->mCombinerColorSource2 = 0x8576;
    glTexEnvi(0x2300, 0x8592, 0x302); unit->mCombinerColorOperand2 = 0x302;
    glTexEnvf(0x2300, 0x8573, 1.0f); unit->mCombinerRGBScale = 1.0f;
    glTexEnvi(0x2300, 0x8572, 0x2100); unit->mCombinerAlphaOp = 0x2100;
    glTexEnvi(0x2300, 0x8588, 0x1702); unit->mCombinerAlphaSource0 = 0x1702;
    glTexEnvi(0x2300, 0x8598, 0x302); unit->mCombinerAlphaOperand0 = 0x302;
    glTexEnvi(0x2300, 0x8589, 0x8578); unit->mCombinerAlphaSource1 = 0x8578;
    glTexEnvi(0x2300, 0x8599, 0x302); unit->mCombinerAlphaOperand1 = 0x302;
    glTexEnvi(0x2300, 0x858a, 0x8576); unit->mCombinerAlphaSource2 = 0x8576;
    glTexEnvi(0x2300, 0x859a, 0x302); unit->mCombinerAlphaOperand2 = 0x302;
    glTexEnvf(0x2300, 0xd1c, 1.0f); unit->mCombinerAlphaScale = 1.0f;
    CBaseVA_Reset((const CBaseVA *)&unit->mTexCoordArray);
    COpenGL_DisableArrayNative(&unit->mTexCoordArray, 0x8078);
    return 0;
#else

    (void)_this;
    (void)SupportsAnisotropicFiltering;
    (void)SupportsLODBias;
    (void)IsProgramableOnly;
    return 0;

#endif
}

unsigned int COpenGL_SetVAO(const COpenGL *_this, const COpenGLVAO *VAO,
                            int IsFixedFunction, int ForceValidation)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &VAO, &IsFixedFunction, &ForceValidation };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetVAO", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    const COpenGLVAOPrefixNative *vao = (const COpenGLVAOPrefixNative *)VAO;
    if (g_VAOID == (GLint)*vao->mpVAOID && !ForceValidation)
        return 0;
    g_VAOID = (GLint)*vao->mpVAOID;
    glBindVertexArrayAPPLE(*vao->mpVAOID);
    gl->vertexArrayStart = vao->mpVAStart;
    gl->vertexArraySize = vao->mSize;
    if (IsFixedFunction || ForceValidation) {
        COpenGL_CopyArrayNative(&gl->colorArray, &vao->mColorArray);
        COpenGL_CopyArrayNative(&gl->secondaryColorArray, &vao->mSecondaryColorArray);
        COpenGL_CopyArrayNative(&gl->normalArray, &vao->mNormalArray);
        COpenGL_CopyArrayNative(&gl->vertexArray, &vao->mVertexArray);
        for (UINT32 i = 0; i < gl->texUnitCount && !gl->texUnits[i].mIsProgramableOnly; ++i)
            COpenGL_CopyArrayNative(&gl->texUnits[i].mTexCoordArray, &vao->mTexCoordArrays[i]);
        for (unsigned i = 0; i < 16; ++i)
            gl->genericArrays[i].mNeedsValidation = 1;
    }
    if (!IsFixedFunction || ForceValidation) {
        memcpy(gl->genericArrays, vao->mGenericArrays, sizeof(gl->genericArrays));
        for (unsigned i = 0; i < 16; ++i)
            gl->genericArrays[i].mNeedsValidation = 0;
        gl->colorArray.mNeedsValidation = gl->secondaryColorArray.mNeedsValidation = 1;
        gl->normalArray.mNeedsValidation = gl->vertexArray.mNeedsValidation = 1;
        for (UINT32 i = 0; i < gl->texUnitCount && !gl->texUnits[i].mIsProgramableOnly; ++i)
            gl->texUnits[i].mTexCoordArray.mNeedsValidation = 1;
    }
    return 0;
#else

    (void)_this;
    (void)VAO;
    (void)IsFixedFunction;
    (void)ForceValidation;
    return 0;

#endif
}

unsigned int COpenGL_EnableColorArray(const COpenGL *_this, GLint Size, GLenum Type,
                                      GLsizei Stride, const unsigned int *Pointer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Size, &Type, &Stride, &Pointer };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_EnableColorArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    gl->genericArrays[5].mNeedsValidation = 1;
    COpenGL_EnableArrayNative(&gl->colorArray, 0x8076, Size, Type, Stride, Pointer);
    return 0;
#else

    (void)_this;
    (void)Size;
    (void)Type;
    (void)Stride;
    (void)Pointer;
    return 0;

#endif
}

unsigned int COpenGL_DisableColorArray(const COpenGL *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_DisableColorArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGL_DisableArrayNative(&((COpenGLNative *)_this)->colorArray, 0x8076);
    return 0;
#else

    (void)_this;
    return 0;

#endif
}

unsigned int COpenGL_EnableNormalArray(const COpenGL *_this, GLenum Type, GLsizei Stride, const unsigned int *Pointer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Type, &Stride, &Pointer };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_EnableNormalArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    gl->genericArrays[3].mNeedsValidation = 1;
    COpenGL_EnableArrayNative(&gl->normalArray, 0x8075, 3, Type, Stride, Pointer);
    return 0;
#else

    (void)_this;
    (void)Type;
    (void)Stride;
    (void)Pointer;
    return 0;

#endif
}

unsigned int COpenGL_DisableNormalArray(const COpenGL *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_DisableNormalArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGL_DisableArrayNative(&((COpenGLNative *)_this)->normalArray, 0x8075);
    return 0;
#else

    (void)_this;
    return 0;

#endif
}

unsigned int COpenGL_EnableVertexArray(const COpenGL *_this, GLint Size, GLenum Type,
                                       GLsizei Stride, const unsigned int *Pointer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Size, &Type, &Stride, &Pointer };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_EnableVertexArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    gl->genericArrays[0].mNeedsValidation = 1;
    COpenGL_EnableArrayNative(&gl->vertexArray, 0x8074, Size, Type, Stride, Pointer);
    return 0;
#else

    (void)_this;
    (void)Size;
    (void)Type;
    (void)Stride;
    (void)Pointer;
    return 0;

#endif
}

unsigned int COpenGL_SetLight(const COpenGL *_this, UINT32 Light, const LightInfoType *Src)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Light, &Src };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetLight", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    LightInfoType *dst = &((COpenGLNative *)_this)->lights[Light];
    GLenum light = 0x4000 + Light;
    if (memcmp(&dst->Diffuse, &Src->Diffuse, sizeof(dst->Diffuse))) {
        memcpy(&dst->Diffuse, &Src->Diffuse, sizeof(dst->Diffuse));
        glLightfv(light, 0x1201, (const GLfloat *)&Src->Diffuse);
    }
    if (memcmp(&dst->Specular, &Src->Specular, sizeof(dst->Specular))) {
        memcpy(&dst->Specular, &Src->Specular, sizeof(dst->Specular));
        glLightfv(light, 0x1202, (const GLfloat *)&Src->Specular);
    }
    if (memcmp(&dst->Ambient, &Src->Ambient, sizeof(dst->Ambient))) {
        memcpy(&dst->Ambient, &Src->Ambient, sizeof(dst->Ambient));
        glLightfv(light, 0x1200, (const GLfloat *)&Src->Ambient);
    }
#define COPENGL_LIGHT_SCALAR(field, pname) \
    if (memcmp(&dst->field, &Src->field, sizeof(GLfloat))) { \
        dst->field = Src->field; glLightf(light, pname, Src->field); \
    }
    COPENGL_LIGHT_SCALAR(Exponent, 0x1205)
    COPENGL_LIGHT_SCALAR(Cutoff, 0x1206)
    COPENGL_LIGHT_SCALAR(Attenuation0, 0x1207)
    COPENGL_LIGHT_SCALAR(Attenuation1, 0x1208)
    COPENGL_LIGHT_SCALAR(Attenuation2, 0x1209)
#undef COPENGL_LIGHT_SCALAR
    return 0;
#else

    (void)_this;
    (void)Light;
    (void)Src;
    return 0;

#endif
}

unsigned int COpenGL_EnableTexCoordArray(const COpenGL *_this, UINT32 Unit, GLint Size,
                                         GLenum Type, GLsizei Stride, const unsigned int *Pointer)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &Size, &Type, &Stride, &Pointer };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_EnableTexCoordArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    CBaseVAImpl *array = &gl->texUnits[Unit].mTexCoordArray;
    gl->genericArrays[Unit + 8].mNeedsValidation = 1;
    if (!array->mEnabled || array->mNeedsValidation || array->mVSize != Size ||
        array->mVType != Type || array->mStride != Stride || array->mpStream != Pointer)
        COpenGL_ClientActiveNative(gl, Unit);
    COpenGL_EnableArrayNative(array, 0x8078, Size, Type, Stride, Pointer);
    return 0;
#else

    (void)_this;
    (void)Unit;
    (void)Size;
    (void)Type;
    (void)Stride;
    (void)Pointer;
    return 0;

#endif
}

unsigned int COpenGL_DisableTexCoordArray(const COpenGL *_this, UINT32 Unit)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_DisableTexCoordArray", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    CBaseVAImpl *array = &gl->texUnits[Unit].mTexCoordArray;
    if (array->mEnabled || array->mNeedsValidation)
        COpenGL_ClientActiveNative(gl, Unit);
    COpenGL_DisableArrayNative(array, 0x8078);
    return 0;
#else

    (void)_this;
    (void)Unit;
    return 0;

#endif
}

#if defined(COD2_X64)
unsigned int COpenGL_SetTexBorderColor(const COpenGL *_this, UINT32 Unit, GLenum Target, GLuint Color)
{
    CTexUnitNative *unit = &((COpenGLNative *)_this)->texUnits[Unit];
    UINT32 index = COpenGL_TargetIndexNative(Target);
    if (unit->mTexBorderColor[index] != Color) {
        COpenGL_SetActiveTexUnit(_this, Unit);
        GLfloat color[4] = {
            (GLfloat)((Color >> 16) & 255) * (1.0f / 255.0f),
            (GLfloat)((Color >> 8) & 255) * (1.0f / 255.0f),
            (GLfloat)(Color & 255) * (1.0f / 255.0f),
            (GLfloat)(Color >> 24) * (1.0f / 255.0f)
        };
        glTexParameterfv(Target, 0x1004, color);
        unit->mTexBorderColor[index] = Color;
    }
    return 0;
}
#endif

unsigned int COpenGL_SetTexMatrix(const COpenGL *_this, UINT32 Unit, const float *m)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &m };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexMatrix", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    CTexUnitNative *unit = &gl->texUnits[Unit];
    if (MacOpenGLUtils_AreMatricesDifferent4x4((const int *)unit->mTexMatrix, (const int *)m)) {
        memcpy(unit->mTexMatrix, m, sizeof(unit->mTexMatrix));
        COpenGL_SetActiveTexUnit(_this, Unit);
        if (gl->matrixMode != 0x1702) {
            gl->matrixMode = 0x1702;
            glMatrixMode(0x1702);
        }
        glLoadMatrixf(unit->mTexMatrix);
    }
    return 0;
#else

    (void)_this;
    (void)Unit;
    (void)m;
    return 0;

#endif
}

unsigned int COpenGL_SetTexMatrixIdentity(const COpenGL *_this, UINT32 Unit)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexMatrixIdentity", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    CTexUnitNative *unit = &gl->texUnits[Unit];
    if (MacOpenGLUtils_AreMatricesDifferent4x4((const int *)unit->mTexMatrix, (const int *)s_identityNative)) {
        memcpy(unit->mTexMatrix, s_identityNative, sizeof(unit->mTexMatrix));
        COpenGL_SetActiveTexUnit(_this, Unit);
        if (gl->matrixMode != 0x1702) {
            gl->matrixMode = 0x1702;
            glMatrixMode(0x1702);
        }
        glLoadIdentity();
    }
    return 0;
#else

    (void)_this;
    (void)Unit;
    return 0;

#endif
}

unsigned int COpenGL_Shutdown(const COpenGL *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_Shutdown", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    memcpy(gl->modelViewMatrix, s_identityNative, sizeof(gl->modelViewMatrix));
    glFinish(); CMemoryBuffer_Reset();
    for (UINT32 i = 0; i < gl->texUnitCount; ++i) COpenGL_SetActiveTexUnit(_this, i);
    /* Regenerated embedded names keep their address after shutdown. */
    for (UINT32 i = 0; i < 16; ++i)
        for (unsigned target = 0; target < 3; ++target)
            gl->texUnits[i].mTexID[target] = NULL;
    COpenGLBindingNodeNative *textures = (COpenGLBindingNodeNative *)&gl->textureBindings;
    for (COpenGLBindingNodeNative *node = textures->next; node != textures; node = node->next) {
        glDeleteTextures(1, node->name); *node->name = 0;
    }
    COpenGLBindingNodeNative *vaos = (COpenGLBindingNodeNative *)COpenGL_GetVAOBindingList(_this);
    for (COpenGLBindingNodeNative *node = vaos->next; node != vaos; node = node->next) {
        glDeleteVertexArraysAPPLE(1, node->name); *node->name = 0;
    }
    CVAOPacket_Shutdown(); CFence_Shutdown();
    if (gl->supportsVAR) {
        glDisableClientState(0x851d); glVertexArrayRangeAPPLE(0, NULL);
        gl->vertexArrayStart = NULL; gl->vertexArraySize = 0;
    }
    return 0;
#else

    (void)_this;
    return 0;

#endif
}

unsigned int COpenGL_SetTexGenEnable(const COpenGL *_this, UINT32 Unit,
                                     int Enable, int EnableReflection, int EnableNormalMap)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit, &Enable, &EnableReflection, &EnableNormalMap };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_SetTexGenEnable", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CTexUnitNative *unit = &((COpenGLNative *)_this)->texUnits[Unit];
    unsigned count = Enable ? ((EnableReflection || EnableNormalMap) ? 3 : 4) : 0;
    GLenum mode = EnableReflection ? 0x8512 : EnableNormalMap ? 0x8511 : 0x2401;
    for (unsigned i = 0; i < count; ++i) {
        if (!unit->mTexGenEnable[i]) {
            COpenGL_SetActiveTexUnit(_this, Unit);
            unit->mTexGenEnable[i] = 1;
            glEnable(0xc60 + i);
        }
        if (unit->mTexGenMode[i] != mode) {
            COpenGL_SetActiveTexUnit(_this, Unit);
            unit->mTexGenMode[i] = mode;
            glTexGeni(0x2000 + i, 0x2500, mode);
        }
    }
    for (unsigned i = count; i < 4; ++i) {
        if (unit->mTexGenEnable[i]) {
            COpenGL_SetActiveTexUnit(_this, Unit);
            unit->mTexGenEnable[i] = 0;
            glDisable(0xc60 + i);
        }
    }
    return 0;
#else

    (void)_this;
    (void)Unit;
    (void)Enable;
    (void)EnableReflection;
    (void)EnableNormalMap;
    return 0;

#endif
}

unsigned int COpenGL_DisableTexUnit(const COpenGL *_this, UINT32 Unit)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Unit };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_DisableTexUnit", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CTexUnitNative *unit = &((COpenGLNative *)_this)->texUnits[Unit];
    for (unsigned i = 0; i < 3; ++i) {
        if (unit->mTargetEnabled[i]) {
            COpenGL_SetActiveTexUnit(_this, Unit);
            glDisable(s_textureTargetsNative[i]);
            unit->mTargetEnabled[i] = 0;
        }
    }
    return 0;
#else

    (void)_this;
    (void)Unit;
    return 0;

#endif
}

unsigned int COpenGL_ReleaseVAOBinding(const COpenGL *_this, const GLuint *TexID)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &TexID };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_ReleaseVAOBinding", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLBindingNodeNative *head = (COpenGLBindingNodeNative *)COpenGL_GetVAOBindingList(_this);
    for (COpenGLBindingNodeNative *node = head->next; node != head; node = node->next) {
        if (*node->name == *TexID) {
            glDeleteVertexArraysAPPLE(1, TexID);
            free(node->name);
            node->prev->next = node->next;
            node->next->prev = node->prev;
            free(node);
            break;
        }
    }
    return 0;
#else

    (void)_this;
    (void)TexID;
    return 0;

#endif
}

#if defined(COD2_X64)
void COpenGL_RegisterTextureBinding(const COpenGL *_this, GLuint *name)
{
    COpenGLBindingNodeNative *head = (COpenGLBindingNodeNative *)&((COpenGLNative *)_this)->textureBindings;
    if (!head->next) head->next = head->prev = head;
    for (COpenGLBindingNodeNative *existing = head->next; existing != head; existing = existing->next)
        if (existing->name == name) return;
    COpenGLBindingNodeNative *node = (COpenGLBindingNodeNative *)malloc(sizeof(*node));
    if (!node) abort();
    node->name = name; node->next = head; node->prev = head->prev;
    head->prev->next = node; head->prev = node;
}
#endif

unsigned int COpenGL_ReleaseTextureBinding(const COpenGL *_this, const GLuint *TexID)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &TexID };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_ReleaseTextureBinding", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    /* Native texture providers own embedded names, so no name allocation is freed. */
    for (UINT32 unit = 0; unit < 16; ++unit)
        for (unsigned target = 0; target < 3; ++target)
            if (gl->texUnits[unit].mTexID[target] == TexID)
                gl->texUnits[unit].mTexID[target] = NULL;
    COpenGLBindingNodeNative *head = (COpenGLBindingNodeNative *)&gl->textureBindings;
    if (head->next) for (COpenGLBindingNodeNative *node = head->next; node != head; node = node->next) {
        if (node->name == TexID) {
            node->prev->next = node->next; node->next->prev = node->prev;
            free(node); break;
        }
    }
    if (*TexID) {
        glDeleteTextures(1, TexID);
        *(GLuint *)TexID = 0;
    }
    return 0;
#else

    (void)_this;
    (void)TexID;
    return 0;

#endif
}

unsigned int COpenGL_Init(const COpenGL *_this, UINT32 Buffers, UINT32 MaxTextureUnits, UINT32 MaxTextureImageUnits)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Buffers, &MaxTextureUnits, &MaxTextureImageUnits };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_Init", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    COpenGL_sFrameCount = 0;
    memcpy(gl->modelViewMatrix, s_identityNative, sizeof(gl->modelViewMatrix));
    gl->supportsBlendSubtract = MacOpenGLUtils_IsGLExtensionSupported("GL_EXT_blend_subtract");
    gl->supportsBlendMinMax = MacOpenGLUtils_IsGLExtensionSupported("GL_EXT_blend_minmax");
    gl->supportsATIArrayRevComps = MacOpenGLUtils_IsGLExtensionSupported("GL_ATI_array_rev_comps_in_4_bytes");
    gl->supportsBlendSeparate = MacOpenGLUtils_IsGLExtensionSupported("GL_EXT_blend_func_separate");
    gl->supportsStencilWrap = MacOpenGLUtils_IsGLExtensionSupported("GL_EXT_stencil_wrap");
    gl->supportsAnisotropy = MacOpenGLUtils_IsGLExtensionSupported("GL_EXT_texture_filter_anisotropic");
    if (gl->supportsAnisotropy) glGetFloatv(0x84ff, &gl->maxAnisotropy);
    gl->supportsLodBias = MacOpenGLUtils_IsGLExtensionSupported("GL_EXT_texture_lod_bias");
    gl->supportsVAR = MacOpenGLUtils_IsGLExtensionSupported("GL_APPLE_vertex_array_object") &&
        MacOpenGLUtils_IsGLExtensionSupported("GL_APPLE_fence") &&
        MacOpenGLUtils_IsGLExtensionSupported("GL_APPLE_element_array");
    gl->vertexArrayStart = NULL; gl->vertexArraySize = 0;
    if (gl->supportsATIArrayRevComps) glVertexArrayParameteriAPPLE(0x897c, 1);
    if (gl->supportsVAR) {
        glVertexArrayRangeAPPLE(0, NULL); glEnableClientState(0x851d);
        glVertexArrayParameteriAPPLE(0x851f, 0x85bf);
    }
    glPixelStorei(0xcf5, 1); glPixelStorei(0xd05, 1); glPixelStorei(0x85b2, 0);
    glDisable(0xbc0); glAlphaFunc(0x207, 0.0f); glBlendFunc(1, 0);
    gl->blendSrcRGB = gl->blendSrcAlpha = 1; gl->blendDstRGB = gl->blendDstAlpha = 0;
    GLint clips; glGetIntegerv(0xd32, &clips);
    GLdouble plane[4] = {0,0,0,0};
    for (GLint i = 0; i < clips; ++i) { glDisable(0x3000 + i); glClipPlane(0x3000 + i, plane); }
    glColorMask(1,1,1,1); glColor4f(1,1,1,1);
    if (gl->supportsBlendSubtract || gl->supportsBlendMinMax) glBlendEquationEXT(0x8006);
    glDisable(0xb44); glEnable(0xb71); glDepthFunc(0x203); glDepthMask(1); glDepthRange(0,1);
    glDisable(0xbd0); glClearColor(0,0,0,0);
    glDrawBuffer(0x409); glClear(0x4000); glDrawBuffer(0x40a); glClear(0x4000);
    glDrawBuffer(0x405); glClear(0x4000);
    if (MacDisplay_IsFullscreen()) { glDrawBuffer(0x404); glClear(0x4000); }
    glDrawBuffer(Buffers == 1 ? 0x404 : 0x405);
    glDisable(0xd80); glDisable(0xbe2); glDisable(0xbf2);
    gl->ditherEnabled = 0; glDisable(0xbd0); glDisable(0xb60); glFogi(0xb65, 0x2601); glDisable(0xbf1);
    if (!gl->supportsATIArrayRevComps) glEnable(0xb50);
    GLint lightCount; glGetIntegerv(0xd31, &lightCount);
    /* The Mac owns eight light cache records regardless of the reported limit. */
    for (unsigned i = 0; i < 8; ++i) {
        LightInfoType *light = &gl->lights[i];
        memset(light, 0, sizeof(*light));
        ((GLfloat *)&light->Ambient)[3] = 1.0f;
        ((GLfloat *)&light->Diffuse)[3] = ((GLfloat *)&light->Specular)[3] = 1.0f;
        if (!i) for (unsigned j = 0; j < 4; ++j)
            ((GLfloat *)&light->Diffuse)[j] = ((GLfloat *)&light->Specular)[j] = 1.0f;
        light->Cutoff = 180.0f; light->Attenuation0 = 1.0f;
        glDisable(0x4000 + i); COpenGL_SetLight(_this, i, light);
    }
    glDisable(0xb20); glDisable(0xb24);
    for (unsigned i = 0; i < 9; ++i) glDisable(0xd90 + i);
    for (unsigned i = 0; i < 9; ++i) glDisable(0xdb0 + i);
    glEnable(0xba1); glDisable(0xb10); glDisable(0x8037); glPolygonOffset(0,0);
    glDisable(0x2a02); glDisable(0x2a01); glDisable(0xb41); glDisable(0xb42); glDisable(0xc11);
    glHint(0xc54, 0x1101); glHint(0xc52, 0x1101); glHint(0xc50, 0x1102);
    glHint(0xc51, 0x1101); glHint(0xc53, 0x1101);
    GLfloat zero[4] = {0,0,0,0}, one[4] = {1,1,1,1};
    glLightModelfv(0xb53, zero); glLightModeli(0xb52, 0);
    glMatrixMode(0x1700); glLoadIdentity(); glMatrixMode(0x1701); glLoadIdentity();
    glMaterialfv(0x408,0x1200,zero); glMaterialfv(0x408,0x1201,one);
    glMaterialfv(0x408,0x1202,zero); glMaterialfv(0x408,0x1600,one); glMaterialf(0x408,0x1601,0);
    glPointSize(1); glPolygonMode(0x408,0x1b02); glShadeModel(0x1d01);
    gl->activeTexUnit = gl->clientActiveTexUnit = 0;
    glActiveTextureARB(0x84c0); glClientActiveTextureARB(0x84c0); glDisable(0xb90);
    gl->texUnitCount = MaxTextureUnits > MaxTextureImageUnits ? MaxTextureUnits : MaxTextureImageUnits;
    /* Native storage reserves the sixteen image units exposed by the selected GL backend. */
    if (gl->texUnitCount > 16) abort();
    for (UINT32 i = 0; i < gl->texUnitCount; ++i) {
        COpenGL_SetActiveTexUnit(_this, i);
        if (i < MaxTextureUnits) COpenGL_ClientActiveNative(gl, i);
        gl->matrixMode = i < MaxTextureUnits ? 0x1702 : 0x1700;
        glMatrixMode(gl->matrixMode);
        COpenGL_ResetArrayNative(&gl->texUnits[i].mTexCoordArray, imp___ZTV14CTexCoordArray);
        COpenGL_CTexUnit_Reset((const CTexUnit *)&gl->texUnits[i], gl->supportsAnisotropy,
                               gl->supportsLodBias, i >= MaxTextureUnits);
    }
    CBaseVA_Reset((const CBaseVA *)&gl->colorArray); COpenGL_DisableArrayNative(&gl->colorArray,0x8076);
    CBaseVA_Reset((const CBaseVA *)&gl->secondaryColorArray); COpenGL_DisableArrayNative(&gl->secondaryColorArray,0x845e);
    CBaseVA_Reset((const CBaseVA *)&gl->vertexArray); COpenGL_DisableArrayNative(&gl->vertexArray,0x8074);
    CBaseVA_Reset((const CBaseVA *)&gl->normalArray); COpenGL_DisableArrayNative(&gl->normalArray,0x8075);
    glDisableClientState(0x8079); glDisableClientState(0x8077);
    gl->matrixMode = 0x1700; glMatrixMode(0x1700);
    glDisable(0xb57); gl->colorMaterialEnabled = 0;
    gl->vertexProgram = 0;
    for (unsigned i = 0; i < 16; ++i) {
        VertexProgramStreamStateNative *array = &gl->genericArrays[i];
        array->mNeedsValidation = 1; array->mEnabled = 0; array->mVSize = 4;
        array->mVType = 0x1406; array->mNormalized = 0; array->mStride = 0; array->mpStream = NULL;
    }
    glDisable(0x8620);
    for (unsigned i = 0; i < 16; ++i) {
        glDisableVertexAttribArrayARB(i); glVertexAttribPointerARB(i,4,0x1406,0,0,NULL);
    }
    int card = MacDisplay_GetCardType();
    if (card == 2) { glDisable(0x86de); glDisable(0x8522); }
    else if (card == 1) glDisable(0x8200);
    if (MacOpenGLUtils_GetPCPixelShaderVersion() > 0xffff01ffU) glDisable(0x8804);
    COpenGL_SetActiveTexUnit(_this, 0);
    g_VAOID = -1;
    CVAOPacket_InitializeGenericVAO();
    return 0;
#else

    (void)_this;
    (void)Buffers;
    (void)MaxTextureUnits;
    (void)MaxTextureImageUnits;
    return 0;

#endif
}

unsigned int COpenGL_COpenGL(const COpenGL *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGL_COpenGL", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    COpenGL_ResetArrayNative(&gl->colorArray, imp___ZTV11CColorArray);
    COpenGL_ResetArrayNative(&gl->secondaryColorArray, imp___ZTV20CSecondaryColorArray);
    COpenGL_ResetArrayNative(&gl->normalArray, imp___ZTV12CNormalArray);
    COpenGL_ResetArrayNative(&gl->vertexArray, imp___ZTV12CVertexArray);
    gl->vaoBindings.next = gl->vaoBindings.prev = &gl->vaoBindings;
    gl->textureBindings.next = gl->textureBindings.prev = &gl->textureBindings;
    gl->vertexProgram = 0;
    memcpy(gl->modelViewMatrix, s_identityNative, sizeof(gl->modelViewMatrix));
    gl->texUnitCount = 0;
    for (unsigned i = 0; i < 16; ++i) {
        VertexProgramStreamStateNative *array = &gl->genericArrays[i];
        array->mNeedsValidation = 1; array->mEnabled = 0;
        array->mVSize = 4; array->mVType = 0x1406; array->mNormalized = 0;
        array->mStride = 0; array->mpStream = NULL;
    }
    return 0;
#else

    (void)_this;
    return 0;

#endif
}

void ZN7COpenGLD1Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN7COpenGLD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    COpenGLNative *gl = (COpenGLNative *)_this;
    COpenGLBindingNodeNative *heads[2] = {
        (COpenGLBindingNodeNative *)&gl->vaoBindings,
        (COpenGLBindingNodeNative *)&gl->textureBindings
    };
    for (unsigned i = 0; i < 2; ++i) {
        COpenGLBindingNodeNative *head = heads[i], *node = head->next;
        while (node && node != head) {
            COpenGLBindingNodeNative *next = node->next;
            free(node); node = next;
        }
        head->next = head->prev = head;
    }
    gl->texUnitCount = 0;
#else

    (void)_this;

#endif
}

void ZN7COpenGLD0Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN7COpenGLD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    ZN7COpenGLD1Ev(_this);
    free(_this);
#else

    (void)_this;

#endif
}

void ZN7COpenGLD2Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN7COpenGLD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    ZN7COpenGLD1Ev(_this);
#else

    (void)_this;

#endif
}

#if defined(COD2_X64)
static void __static_initialization_and_destruction_0(int initialize, int priority)
{
    if (priority == 0xffff) {
        if (initialize == 1) COpenGL_COpenGL((const COpenGL *)&s_nativeOpenGL);
        else if (initialize == 0) ZN7COpenGLD1Ev(&s_nativeOpenGL);
    }
}
#endif

#if defined(COD2_X64)
__attribute__((destructor))
#endif
static void GLOBAL__D__ZN7COpenGL7sOpenGLE(void)
{
#if defined(COD2_X64)
    __static_initialization_and_destruction_0(0, 0xffff);
#endif

}

#ifdef _MSC_VER
COD2_CONSTRUCTOR(GLOBAL__I__ZN7COpenGL7sOpenGLE)
#else
__attribute__((constructor)) void GLOBAL__I__ZN7COpenGL7sOpenGLE(void)
#endif
{

#if defined(COD2_X64)
    __static_initialization_and_destruction_0(1, 0xffff);
#else
    COpenGL_COpenGL((const COpenGL *)COpenGL_sOpenGL);
#endif
}

#if defined(COD2_X64)
/* Native CDirect3DVertexShader stores a GLuint at the start of baseState.
 * Draw consumers read that word directly; string labels and stream state are
 * supplied separately by the native shader-cache and vertex-declaration path. */
unsigned int COpenGLVertexProgram_COpenGLVertexProgram(const COpenGLVertexProgram *_this, const char *pSrcData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSrcData };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGLVertexProgram_COpenGLVertexProgram", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif
    GLuint *program = (GLuint *)_this;
    GLint previousProgram;
    *program = 0;
    if (!pSrcData || pSrcData[0] != '!' || pSrcData[1] != '!')
        return 0;
    glGetProgramivARB(0x8620, 0x8677, &previousProgram);
    glGenProgramsARB(1, program);
    glBindProgramARB(0x8620, *program);
    glProgramStringARB(0x8620, 0x8875, (GLsizei)strlen(pSrcData), pSrcData);
    glBindProgramARB(0x8620, (GLuint)previousProgram);
    return 0;
}

static void COpenGLVertexProgram_Destroy(void *_this)
{
    extern unsigned char *imp___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE;
    extern unsigned char *imp___ZN15CDirect3DDevice30mNeedsTransformationValidationE;
    extern unsigned char *imp___ZN15CDirect3DDevice29mNeedsRasterizationValidationE;
    GLuint program = *(GLuint *)_this;
    GLint currentProgram;
    glGetProgramivARB(0x8620, 0x8677, &currentProgram);
    if ((GLuint)currentProgram == program)
        glDisable(0x8620);
    glDeleteProgramsARB(1, &program);
    if (imp___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE)
        *imp___ZN15CDirect3DDevice28mNeedsVertexShaderValidationE = 1;
    if (imp___ZN15CDirect3DDevice30mNeedsTransformationValidationE)
        *imp___ZN15CDirect3DDevice30mNeedsTransformationValidationE = 1;
    if (imp___ZN15CDirect3DDevice29mNeedsRasterizationValidationE)
        *imp___ZN15CDirect3DDevice29mNeedsRasterizationValidationE = 1;
}

void ZN20COpenGLVertexProgramD1Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20COpenGLVertexProgramD1Ev", _this, macArguments, NULL, 0))
        return;
#endif
    COpenGLVertexProgram_Destroy(_this);
}

void ZN20COpenGLVertexProgramD0Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20COpenGLVertexProgramD0Ev", _this, macArguments, NULL, 0))
        return;
#endif
    COpenGLVertexProgram_Destroy(_this);
    free(_this);
}

void ZN20COpenGLVertexProgramD2Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20COpenGLVertexProgramD2Ev", _this, macArguments, NULL, 0))
        return;
#endif
    COpenGLVertexProgram_Destroy(_this);
}
#else
unsigned int COpenGLVertexProgram_COpenGLVertexProgram(const COpenGLVertexProgram *_this, const char *pSrcData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pSrcData };
    unsigned int macResult = { 0 };
    if (MacShim_Dispatch("COpenGLVertexProgram_COpenGLVertexProgram", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif


    GLuint *progIdPtr = (GLuint *)_this;

    if (!pSrcData || pSrcData[0] != '!' || pSrcData[1] != '!') {
        *progIdPtr = 0;
        return 0;
    }

    {
        int len = strlen(pSrcData);
        int errorPos = -1;

        glGenProgramsARB(1, progIdPtr);
        glBindProgramARB(0x8620 , *progIdPtr);
        glProgramStringARB(0x8620, 0x8875 ,
                           len, pSrcData);

        glGetIntegerv(0x864B , &errorPos);
        if (errorPos != -1) {
            fprintf(stderr, "[ARB VP] Compile error at position %d in program %u\n",
                    errorPos, *progIdPtr);
        }

        glBindProgramARB(0x8620, 0);
    }

    return 0;
}

void ZN20COpenGLVertexProgramD1Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20COpenGLVertexProgramD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

void ZN20COpenGLVertexProgramD0Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20COpenGLVertexProgramD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

void ZN20COpenGLVertexProgramD2Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20COpenGLVertexProgramD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

#endif

void ZN7CBaseVAD1Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
#else
    (void)_this;

#endif
}
void ZN7CBaseVAD0Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
    free(_this);
#else
    (void)_this;

#endif
}

unsigned int CColorArray_Enable(const CColorArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 1;
    glEnableClientState(0x8076);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CColorArray_Disable(const CColorArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 0;
    glDisableClientState(0x8076);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CSecondaryColorArray_Enable(const CSecondaryColorArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 1;
    glEnableClientState(0x845e);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CSecondaryColorArray_Disable(const CSecondaryColorArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 0;
    glDisableClientState(0x845e);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CNormalArray_Enable(const CNormalArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 1;
    glEnableClientState(0x8075);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CNormalArray_Disable(const CNormalArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 0;
    glDisableClientState(0x8075);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CVertexArray_Enable(const CVertexArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 1;
    glEnableClientState(0x8074);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CVertexArray_Disable(const CVertexArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 0;
    glDisableClientState(0x8074);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CTexCoordArray_Enable(const CTexCoordArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 1;
    glEnableClientState(0x8078);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}
unsigned int CTexCoordArray_Disable(const CTexCoordArray *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->mEnabled = 0;
    glDisableClientState(0x8078);
    return 0;
#else
    (void)_this;
    return 0;

#endif
}

void ZN14CTexCoordArrayD1Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
#else
    (void)_this;

#endif
}
void ZN14CTexCoordArrayD0Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
    free(_this);
#else
    (void)_this;

#endif
}
void ZN11CColorArrayD0Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
    free(_this);
#else
    (void)_this;

#endif
}
void ZN11CColorArrayD1Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
#else
    (void)_this;

#endif
}
void ZN20CSecondaryColorArrayD0Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
    free(_this);
#else
    (void)_this;

#endif
}
void ZN20CSecondaryColorArrayD1Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
#else
    (void)_this;

#endif
}
void ZN12CVertexArrayD0Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
    free(_this);
#else
    (void)_this;

#endif
}
void ZN12CVertexArrayD1Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
#else
    (void)_this;

#endif
}
void ZN12CNormalArrayD0Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
    free(_this);
#else
    (void)_this;

#endif
}
void ZN12CNormalArrayD1Ev(void *_this)
{

#if defined(COD2_X64)
    ((CBaseVAImpl *)_this)->vtable = (void **)((char *)imp___ZTV7CBaseVA + 2 * sizeof(void *));
#else
    (void)_this;

#endif
}

#if defined(COD2_X64)
void *ZNSt4listIPmSaIS0_EE5eraseESt14_List_iteratorIS0_E(void *list, void *position)
{
    extern void __ZNSt15_List_node_base6unhookEv(void *);
    extern void __ZdlPv(void *);
    void *next = *(void **)position;
    (void)list;
    __ZNSt15_List_node_base6unhookEv(position);
    __ZdlPv(position);
    return next;
}
#else
void ZNSt4listIPmSaIS0_EE5eraseESt14_List_iteratorIS0_E(void) {}
#endif
void ZNSt6vectorIN7COpenGL8CTexUnitESaIS1_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS1_S3_EERKS1_(void) {}
void ZNSt6vectorIN7COpenGL8CTexUnitESaIS1_EE5eraseEN9__gnu_cxx17__normal_iteratorIPS1_S3_EES7_(void) {}

fnptr_t vtbl_COpenGL[] = { (fnptr_t)ZN7COpenGLD1Ev, (fnptr_t)ZN7COpenGLD0Ev };
fnptr_t vtbl_COpenGLVertexProgram[] = { (fnptr_t)ZN20COpenGLVertexProgramD1Ev, (fnptr_t)ZN20COpenGLVertexProgramD0Ev };
