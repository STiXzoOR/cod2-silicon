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
    unsigned char pixelsBottomUp;
} CDirect3DSurfaceNativeInfo;
int CDirect3DSurface_GetNativeInfo(const void *surface, CDirect3DSurfaceNativeInfo *info);
void CDirect3DSurface_MarkGPUWritten(void *surface);
#if COD2_APPLE_SDK
HRESULT CDirect3DSurface_BindFloatTarget(void *surface);
HRESULT CDirect3DSurface_CreateDepthStorage(void *surface, UINT32 width, UINT32 height, UINT32 samples);
void CDirect3DSurface_BindDepthStorage(const void *surface);
#endif
#endif
#endif
