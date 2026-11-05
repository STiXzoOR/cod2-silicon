#ifndef COD2_LP64_GL_STATE_H
#define COD2_LP64_GL_STATE_H

#if defined(COD2_X64)
/* Native storage for the live COpenGL implementation. COpenGL is opaque in
 * the reconstructed headers; none of this state is a serialized asset. */
typedef struct {
    void **vtable;
    unsigned char mEnabled;
    unsigned char mNeedsValidation;
    unsigned short padding;
    GLint mVSize;
    GLenum mVType;
    GLsizei mStride;
    const void *mpStream;
} CBaseVAImpl;

/* C++ bool fields are bytes. The engine's C `bool` typedef is int. */
typedef struct {
    unsigned char mNeedsValidation, mEnabled;
    GLint mVSize;
    GLenum mVType;
    GLboolean mNormalized;
    GLsizei mStride;
    const void *mpStream;
} VertexProgramStreamStateNative;

/* Field order verified against the i386 CTexUnit STABS (304 bytes). */
typedef struct {
    unsigned char mIsProgramableOnly;
    unsigned char mTargetEnabled[3];
    const GLuint *mTexID[3];
    GLenum mTexWrapS[3], mTexWrapT[3], mTexWrapR[3];
    GLuint mTexBorderColor[3];
    GLenum mTexMinFilter[3], mTexMagFilter[3];
    GLfloat mTexAnisotropicFilter[3];
    GLint mTexLastLevel[3];
    GLenum mCombinerColorOp;
    GLenum mCombinerColorSource0, mCombinerColorOperand0;
    GLenum mCombinerColorSource1, mCombinerColorOperand1;
    GLenum mCombinerColorSource2, mCombinerColorOperand2;
    GLenum mCombinerAlphaOp;
    GLenum mCombinerAlphaSource0, mCombinerAlphaOperand0;
    GLenum mCombinerAlphaSource1, mCombinerAlphaOperand1;
    GLenum mCombinerAlphaSource2, mCombinerAlphaOperand2;
    GLfloat mCombinerRGBScale, mCombinerAlphaScale;
    GLfloat mTexFactor[4];
    GLfloat mLodBias;
    GLfloat mTexMatrix[16];
    CBaseVAImpl mTexCoordArray;
    unsigned char mTexGenEnable[4];
    GLenum mTexGenMode[4];
} CTexUnitNative;

typedef struct {
    void *next;
    void *prev;
} COpenGLBindingList;

typedef struct {
    UINT32 activeTexUnit;
    GLenum blendSrcRGB, blendDstRGB, blendSrcAlpha, blendDstAlpha;
    CTexUnitNative texUnits[16];
    COpenGLBindingList vaoBindings;
    UINT32 clientActiveTexUnit;
    UINT32 texUnitCount;
    const void *vertexArrayStart;
    UINT32 vertexArraySize;
    unsigned char supportsVAR, supportsAnisotropy, supportsLodBias;
    GLuint vertexProgram;
    GLenum matrixMode;
    COpenGLBindingList textureBindings;
    unsigned char colorMaterialEnabled, ditherEnabled;
    unsigned char supportsATIArrayRevComps;
    unsigned char supportsBlendSubtract, supportsBlendMinMax, supportsBlendSeparate, supportsStencilWrap;
    GLfloat maxAnisotropy;
    LightInfoType lights[8];
    CBaseVAImpl colorArray, secondaryColorArray, normalArray, vertexArray;
    VertexProgramStreamStateNative genericArrays[16];
    GLfloat modelViewMatrix[16];

} COpenGLNative;

/* Flat secondary base used by Texture, CubeTexture and VolumeTexture. The
 * last four bytes of sizeof are tail padding reused by each owner's refcount;
 * access fields individually, never copy this rounded extent into an owner. */
typedef struct {
    void **vtable;
    GLuint *name;
    GLenum wrapS, wrapT, wrapR;
    GLuint border;
    GLenum minFilter, magFilter;
    GLint lastLevel;
    GLfloat anisotropy;
    GLint width, height, depth;
} COpenGLTextureStateNative;
_Static_assert(offsetof(COpenGLTextureStateNative, name) == sizeof(void *), "native texture name pointer");
_Static_assert(offsetof(COpenGLTextureStateNative, lastLevel) == 40, "native texture last level");
_Static_assert(offsetof(COpenGLTextureStateNative, anisotropy) == 44, "native texture anisotropy");
_Static_assert(offsetof(COpenGLTextureStateNative, depth) + sizeof(GLint) == 60, "native texture field extent");

/* Device stages have a widened COM pointer; never use the imported placeholder
 * or the original 164-byte stride as native storage. */
typedef struct {
    unsigned char programmableOnly;
    IDirect3DBaseTexture9 *texture;
    DWORD addressU, addressV, addressW;
    DWORD minFilter, magFilter, mipFilter;
    GLfloat anisotropy, lodBias;
    DWORD borderColor;
    DWORD colorOp, colorArg0, colorArg1, colorArg2;
    DWORD alphaOp, alphaArg0, alphaArg1, alphaArg2;
    DWORD transformFlags, texCoordIndex;
    GLfloat factor[4], matrix[16];
} CTexStageNative;

/* r_init.c publishes fourteen 32-bit words, without an HWND pointer. This is
 * a native renderer protocol, not the imported D3DPRESENT_PARAMETERS layout. */
typedef struct {
    uint32_t width, height, backBufferFormat, backBufferCount;
    uint32_t multiSampleType, multiSampleQuality, swapEffect, windowed;
    uint32_t depthFormat, flags, displayMode, presentationInterval;
    uint32_t refreshRate, reserved;
} NativePresentationParameters14;

static inline int NativePresentation_DepthBits(uint32_t format)
{
    /* Mac Reset jump table at 0x327034 (D3DFORMAT 70 through 80). */
    switch (format) {
    case 70: case 80: return 16;
    case 71: return 32;
    case 75: case 77: return 24;
    default: return 0;
    }
}

void *COpenGL_GetVAOBindingList(const COpenGL *gl);
#endif
#endif
