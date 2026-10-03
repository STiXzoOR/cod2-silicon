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
} COpenGLNative;

void *COpenGL_GetVAOBindingList(const COpenGL *gl);
#endif
#endif
