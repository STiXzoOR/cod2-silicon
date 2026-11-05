#ifndef COD2_LP64_SURFACE_H
#define COD2_LP64_SURFACE_H
#if defined(COD2_X64)
/* This is the live native surface/secondary texture-base contract, not an
 * overlay of the original 60-byte i386 surface. */
typedef struct {
    COpenGLTextureStateNative *owner;
    GLuint name;
    GLenum target;
    UINT32 level, width, height;
} CDirect3DSurfaceNativeInfo;
int CDirect3DSurface_GetNativeInfo(const void *surface, CDirect3DSurfaceNativeInfo *info);
#endif
#endif
