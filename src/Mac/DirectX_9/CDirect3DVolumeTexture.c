#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>

#if defined(COD2_X64) || COD2_APPLE_SDK
extern void *vtbl_CDirect3DVolumeTexture[30];
static void *vtbl_CDirect3DVolume[13];
#else
void *vtbl_CDirect3DVolumeTexture[30];
#endif

void __ZdlPv(void *ptr);
UINT32 MacOpenGLUtils_GetLevelSizeInBytes(UINT32 Width, UINT32 Height, UINT32 Depth, const D3DFORMAT *f);
UINT32 MacOpenGLUtils_GetNumTextureLevels(UINT32 Width, UINT32 Height, UINT32 Depth);
UINT32 MacOpenGLUtils_GetFormatSizeInBits(const D3DFORMAT *f);
int MacOpenGLUtils_GetOpenGLTextureType(bool *CreateOpenGLResources, GLenum *OpenGLInternalFormat, GLenum *OpenGLFormat, GLenum *OpenGLElementType, D3DFORMAT mFormat);
bool MacOpenGLUtils_IsCompressed(const D3DFORMAT *f);

#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_REPEAT 0x2901
#define GL_TEXTURE_BINDING_3D 0x806A
#define MAX_MIP_LEVELS 16

#if defined(COD2_X64)
void COpenGLTexture_InitNative(void *info, GLuint *name, UINT32 width, UINT32 height, UINT32 depth);
void ZN14COpenGLTextureD1Ev(void *info);
void CDirect3DSurface_SetDirty(void *surface, int dirty);
UINT32 MacOpenGLUtils_GetImageSizeInBytes(UINT32 width, UINT32 height, UINT32 depth, UINT32 levels, const D3DFORMAT *format);
#endif
typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 level;
    UINT32 width;
    UINT32 height;
    UINT32 depth;
    byte *volumeMemory;
    D3DFORMAT format;
    unsigned char isDirty;
    unsigned char pad[3];
    void *owner;
    GLenum glInternalFormat;
    GLenum glFormat;
    GLenum glType;
} CDirect3DVolumeClean;

#if defined(COD2_X64)
void ZN15CDirect3DVolumeD0Ev(const void *volume);
void CDirect3DVolume_UpdateOpenGLSurfaceObject(const void *volume, int recreate);

GLuint COpenGLTexture_GetGLNameNative(const void *info);
#endif
ULONG CDirect3DVolume_AddRef(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return ++((CDirect3DVolumeClean *)_this)->refCount;
}
ULONG CDirect3DVolume_Release(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeClean *volume = (void *)_this;
    ULONG count = --volume->refCount;
    if (!count)
        ((void (*)(const void *))volume->vtable[12])(_this);
    return count;
#else

    return --((CDirect3DVolumeClean *)_this)->refCount;
#endif
}
HRESULT CDirect3DVolume_QueryInterface(const void *_this, const IID *iid, void **ppv)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppv };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;
    *ppv = (void *)_this;
    CDirect3DVolume_AddRef(_this);
    return 0;
}
void ZN15CDirect3DVolumeD1Ev(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN15CDirect3DVolumeD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    ((CDirect3DVolumeClean *)_this)->vtable = vtbl_CDirect3DVolume;
#else

    (void)_this;
#endif
}
void ZN15CDirect3DVolumeD0Ev(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN15CDirect3DVolumeD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    ZN15CDirect3DVolumeD1Ev(_this);
    free((void *)_this);
#else

    __ZdlPv((void *)_this);
#endif
}

HRESULT CDirect3DVolume_GetDesc(const void *_this, D3DVOLUME_DESC *pDesc)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pDesc };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_GetDesc", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeClean *volume = (void *)_this;
    pDesc->Format = volume->format;
    pDesc->Width = volume->width;
    pDesc->Height = volume->height;
    pDesc->Depth = volume->depth;
    return 0;
#else

    CDirect3DVolumeClean *vol = (CDirect3DVolumeClean *)_this;
    pDesc->Format = vol->format;
    pDesc->Type = D3DRTYPE_VOLUME;
    pDesc->Usage = 0;
    pDesc->Pool = 0;
    pDesc->Width = vol->width;
    pDesc->Height = vol->height;
    pDesc->Depth = vol->depth;
    return 0;
#endif
}

HRESULT CDirect3DVolume_LockBox(const void *_this, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pLockedVolume, &pBox, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_LockBox", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeClean *volume = (void *)_this;
    (void)pBox;
    (void)Flags;
    pLockedVolume->pBits = volume->volumeMemory;
    if (MacOpenGLUtils_IsCompressed(&volume->format)) {
        UINT32 blocks = (volume->width ? volume->width + 3 : 4) >> 2;
        pLockedVolume->RowPitch = blocks * (volume->format == D3DFMT_DXT1 ? 8 : 16);
    } else {
        pLockedVolume->RowPitch = (volume->width * MacOpenGLUtils_GetFormatSizeInBits(&volume->format)) >> 3;
    }
    /* Image_UploadData writes each native slice into tightly packed backing. */
    UINT32 rows = MacOpenGLUtils_IsCompressed(&volume->format) ? (volume->height + 3) / 4 : volume->height;
    pLockedVolume->SlicePitch = (UINT32)pLockedVolume->RowPitch * rows;
    volume->isDirty = 1;
    return 0;
#else

    CDirect3DVolumeClean *vol = (CDirect3DVolumeClean *)_this;
    UINT32 bpp;
    (void)pBox;
    (void)Flags;

    if (MacOpenGLUtils_IsCompressed(&vol->format)) {
        UINT32 blockSize = (vol->format == D3DFMT_DXT1) ? 8 : 16;
        pLockedVolume->RowPitch = ((vol->width + 3) / 4) * blockSize;
        pLockedVolume->SlicePitch = pLockedVolume->RowPitch * ((vol->height + 3) / 4);
    } else {
        bpp = MacOpenGLUtils_GetFormatSizeInBits(&vol->format);
        pLockedVolume->RowPitch = (vol->width * bpp) >> 3;
        pLockedVolume->SlicePitch = pLockedVolume->RowPitch * vol->height;
    }
    pLockedVolume->pBits = vol->volumeMemory;
    vol->isDirty = 1;
    return 0;
#endif
}

HRESULT CDirect3DVolume_UnlockBox(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_UnlockBox", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)_this;
    return 0;
#else

    (void)_this;
    return 0;
#endif
}
bool CDirect3DVolume_IsDirty(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    bool macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_IsDirty", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeClean *volume = (void *)_this;
    return volume->isDirty || (volume->owner && !COpenGLTexture_GetGLNameNative(volume->owner));
#else

    return ((CDirect3DVolumeClean *)_this)->isDirty;
#endif
}

void CDirect3DVolume_CreateOpenGLSurfaceObject(const void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("CDirect3DVolume_CreateOpenGLSurfaceObject", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeClean *volume = (void *)_this;
    UINT32 width = volume->width ? volume->width : 1;
    UINT32 height = volume->height ? volume->height : 1;
    UINT32 depth = volume->depth ? volume->depth : 1;
    if (volume->format == D3DFMT_DXT1 || volume->format == D3DFMT_DXT2 ||
        volume->format == D3DFMT_DXT3 || volume->format == D3DFMT_DXT5) {
        UINT32 size = MacOpenGLUtils_GetLevelSizeInBytes(volume->width, volume->height, volume->depth, &volume->format);
        glCompressedTexImage3DARB(GL_TEXTURE_3D, volume->level, volume->glInternalFormat,
                                  volume->width, volume->height, volume->depth, 0, size, volume->volumeMemory);
    } else {
        glTexImage3D(GL_TEXTURE_3D, volume->level, volume->glInternalFormat, width, height, depth,
                     0, volume->glFormat, volume->glType,
                     volume->glInternalFormat == 0x1902 ? NULL : volume->volumeMemory);
    }
#else

    CDirect3DVolumeClean *vol = (CDirect3DVolumeClean *)_this;
    UINT32 w = vol->width ? vol->width : 1;
    UINT32 h = vol->height ? vol->height : 1;
    UINT32 d = vol->depth ? vol->depth : 1;

    if (!vol->volumeMemory)
        return;

    if (MacOpenGLUtils_IsCompressed(&vol->format)) {
        UINT32 dataSize = MacOpenGLUtils_GetLevelSizeInBytes(w, h, d, &vol->format);
        glCompressedTexImage3DARB(GL_TEXTURE_3D, vol->level,
                                  vol->glInternalFormat, w, h, d, 0, dataSize, vol->volumeMemory);
    } else {
        glTexImage3D(GL_TEXTURE_3D, vol->level,
                     vol->glInternalFormat, w, h, d, 0,
                     vol->glFormat, vol->glType, vol->volumeMemory);
    }
#endif
}

void CDirect3DVolume_UpdateOpenGLSurfaceObject(const void *_this, int bRecreate)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &bRecreate };
    if (MacShim_Dispatch("CDirect3DVolume_UpdateOpenGLSurfaceObject", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeClean *volume = (void *)_this;
    if (bRecreate) {
        bool createGL = 1;
        MacOpenGLUtils_GetOpenGLTextureType(&createGL, &volume->glInternalFormat,
                                           &volume->glFormat, &volume->glType, volume->format);
        if (createGL)
            CDirect3DVolume_CreateOpenGLSurfaceObject(volume);
    } else if (volume->format == D3DFMT_DXT1 || volume->format == D3DFMT_DXT2 ||
               volume->format == D3DFMT_DXT3 || volume->format == D3DFMT_DXT5) {
        UINT32 size = MacOpenGLUtils_GetLevelSizeInBytes(volume->width, volume->height, volume->depth, &volume->format);
        if (volume->width > 3 && volume->height > 3 && volume->depth > 3)
            glCompressedTexSubImage3D(GL_TEXTURE_3D, volume->level, 0, 0, 0, volume->width, volume->height,
                                      volume->depth, volume->glInternalFormat, size, volume->volumeMemory);
    } else {
        glTexSubImage3D(GL_TEXTURE_3D, volume->level, 0, 0, 0, volume->width, volume->height,
                        volume->depth, volume->glFormat, volume->glType, volume->volumeMemory);
    }
#else

    CDirect3DVolumeClean *vol = (CDirect3DVolumeClean *)_this;
    UINT32 w = vol->width ? vol->width : 1;
    UINT32 h = vol->height ? vol->height : 1;
    UINT32 d = vol->depth ? vol->depth : 1;

    if (!vol->volumeMemory)
        return;

    if (bRecreate) {
        CDirect3DVolume_CreateOpenGLSurfaceObject(_this);
    } else if (MacOpenGLUtils_IsCompressed(&vol->format)) {
        UINT32 dataSize = MacOpenGLUtils_GetLevelSizeInBytes(w, h, d, &vol->format);
        glCompressedTexSubImage3D(GL_TEXTURE_3D, vol->level, 0, 0, 0,
                                  w, h, d, vol->glInternalFormat, dataSize, vol->volumeMemory);
    } else {
        glTexSubImage3D(GL_TEXTURE_3D, vol->level, 0, 0, 0,
                        w, h, d, vol->glFormat, vol->glType, vol->volumeMemory);
    }
    vol->isDirty = 0;
#endif
}

HRESULT CDirect3DVolume_GetDevice(const void *t, void *p)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &p };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_GetDevice", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)p;
    return 0;
}
HRESULT CDirect3DVolume_SetPrivateData(const void *t, const GUID *g, const void *d, DWORD s, DWORD f)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &g, &d, &s, &f };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_SetPrivateData", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)g;
    (void)d;
    (void)s;
    (void)f;
    return 0;
}
HRESULT CDirect3DVolume_GetPrivateData(const void *t, const GUID *g, void *d, DWORD *s)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &g, &d, &s };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_GetPrivateData", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)g;
    (void)d;
    (void)s;
    return 0;
}
HRESULT CDirect3DVolume_FreePrivateData(const void *t, const GUID *g)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &g };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_FreePrivateData", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)g;
    return 0;
}
HRESULT CDirect3DVolume_GetContainer(const void *t, const IID *r, void **p)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &r, &p };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolume_GetContainer", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)r;
    (void)p;
    return 0;
}

typedef struct {
    void **primaryVtable;
    void **secondaryVtable;
    GLuint *mpTexID;
    GLenum mTexWrapS;
    GLenum mTexWrapT;
    GLenum mTexWrapR;
    GLuint mTexBorderColor;
    GLenum mTexMinFilter;
    GLenum mTexMagFilter;
#if defined(COD2_X64)
    GLint mTexLastLevel;
    GLfloat mTexAniso;
#else
    GLfloat mTexAniso;
    UINT32 reserved;
#endif
    GLint glWidth;
    GLint glHeight;
    GLint glDepth;
    ULONG refCount;
    UINT32 width;
    UINT32 height;
    UINT32 levelCount;
    D3DFORMAT format;
    UINT32 depth;
    void **volumes;
    byte *pixelData;
    GLuint texIDStorage;
} CDirect3DVolumeTextureClean;

#if COD2_APPLE_SDK
GLuint CDirect3DVolumeTexture_GetGLName(const void *texture)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &texture };
    GLuint macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetGLName", texture, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return ((const CDirect3DVolumeTextureClean *)texture)->texIDStorage;
}
#endif

ULONG CDirect3DVolumeTexture_AddRef(const CDirect3DVolumeTexture *_this);
void ZN22CDirect3DVolumeTextureD0Ev(void *_this);
void ZN22CDirect3DVolumeTextureD1Ev(void *_this);

ULONG CDirect3DVolumeTexture_AddRef(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return ++((CDirect3DVolumeTextureClean *)_this)->refCount;
}

HRESULT CDirect3DVolumeTexture_QueryInterface(const CDirect3DVolumeTexture *_this, const IID *iid, void **ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    (void)iid;
    *ppvObj = (void *)_this;
    ((ULONG (*)(const void *))(*(void ***)_this)[1])(_this);
    return 0;
#else

    (void)iid;
    *ppvObj = (void *)_this;
    CDirect3DVolumeTexture_AddRef(_this);
    return 0;
#endif
}

ULONG CDirect3DVolumeTexture_Release(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    ULONG count = --tex->refCount;
    if (!count)
        ((void (*)(void *))tex->primaryVtable[23])(tex);
    return count;
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    ULONG rc = --tex->refCount;
    if (!rc)
        ZN22CDirect3DVolumeTextureD0Ev((void *)_this);
    return rc;
#endif
}

HRESULT CDirect3DVolumeTexture_GetLevelDesc(const CDirect3DVolumeTexture *_this, UINT Level, D3DVOLUME_DESC *pDesc)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Level, &pDesc };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetLevelDesc", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    void *volume;
    HRESULT result = ((HRESULT (*)(const void *, UINT, void **))(*(void ***)_this)[18])(_this, Level, &volume);
    if (result < 0)
        return (HRESULT)0x8876086cu;
    result = ((HRESULT (*)(void *, D3DVOLUME_DESC *))(*(void ***)volume)[8])(volume, pDesc);
    ((ULONG (*)(void *))(*(void ***)volume)[2])(volume);
    return result;
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    if (Level >= tex->levelCount)
        return -1;
    return CDirect3DVolume_GetDesc(tex->volumes[Level], pDesc);
#endif
}

HRESULT CDirect3DVolumeTexture_GetVolumeLevel(const CDirect3DVolumeTexture *_this, UINT Level, void **ppVolumeLevel)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Level, &ppVolumeLevel };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetVolumeLevel", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    if (Level >= tex->levelCount)
        return (HRESULT)0x8876086cu;
    void *volume = tex->volumes[Level];
    *ppVolumeLevel = volume;
    ((ULONG (*)(void *))(*(void ***)volume)[1])(volume);
    return 0;
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    if (Level >= tex->levelCount)
        return -1;
    CDirect3DVolume_AddRef(tex->volumes[Level]);
    *ppVolumeLevel = tex->volumes[Level];
    return 0;
#endif
}

HRESULT CDirect3DVolumeTexture_LockBox(const CDirect3DVolumeTexture *_this, UINT Level, D3DLOCKED_BOX *pLockedVolume, const D3DBOX *pBox, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Level, &pLockedVolume, &pBox, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_LockBox", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    if (Level >= tex->levelCount)
        return (HRESULT)0x8876086cu;
    void *volume = tex->volumes[Level];
    return ((HRESULT (*)(void *, D3DLOCKED_BOX *, const D3DBOX *, DWORD))(*(void ***)volume)[9])(volume, pLockedVolume, pBox, Flags);
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    if (Level >= tex->levelCount)
        return -1;
    return CDirect3DVolume_LockBox(tex->volumes[Level], pLockedVolume, pBox, Flags);
#endif
}

HRESULT CDirect3DVolumeTexture_UnlockBox(const CDirect3DVolumeTexture *_this, UINT Level)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Level };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_UnlockBox", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    if (Level >= tex->levelCount)
        return (HRESULT)0x8876086cu;
    void *volume = tex->volumes[Level];
    HRESULT result = ((HRESULT (*)(void *))(*(void ***)volume)[10])(volume);
    CDirect3DVolumeClean *child = volume;
    if (result >= 0 && child->isDirty && child->volumeMemory && child->owner) {
        GLuint name = COpenGLTexture_GetGLNameNative(child->owner);
        if (name) {
            GLint previous;
            glGetIntegerv(GL_TEXTURE_BINDING_3D, &previous);
            glBindTexture(GL_TEXTURE_3D, name);
            CDirect3DVolume_UpdateOpenGLSurfaceObject(child, 1);
            child->isDirty = 0;
            glBindTexture(GL_TEXTURE_3D, previous);
        }
    }
    return result;
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    CDirect3DVolumeClean *vol;
    if (Level >= tex->levelCount)
        return -1;
    vol = (CDirect3DVolumeClean *)tex->volumes[Level];

    if (vol->isDirty) {
        int prevTex = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &prevTex);
        glBindTexture(GL_TEXTURE_3D, tex->texIDStorage);
        CDirect3DVolume_UpdateOpenGLSurfaceObject(vol, 0);
        glBindTexture(GL_TEXTURE_3D, prevTex);
    }
    return 0;
#endif
}

HRESULT CDirect3DVolumeTexture_AddDirtyBox(const CDirect3DVolumeTexture *_this, const D3DBOX *pBox)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pBox };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_AddDirtyBox", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    (void)pBox;
    for (UINT32 level = 0; level < tex->levelCount; ++level)
        ((CDirect3DVolumeClean *)tex->volumes[level])->isDirty = 1;
    return 0;
#else

    (void)_this;
    (void)pBox;
    return 0;
#endif
}
DWORD CDirect3DVolumeTexture_SetLOD(const CDirect3DVolumeTexture *_this, DWORD LODNew)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &LODNew };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_SetLOD", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)LODNew;
    return 0;
}
DWORD CDirect3DVolumeTexture_GetLOD(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetLOD", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}
DWORD CDirect3DVolumeTexture_GetLevelCount(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetLevelCount", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    return ((CDirect3DVolumeTextureClean *)_this)->levelCount;
}
D3DRESOURCETYPE CDirect3DVolumeTexture_GetType(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    D3DRESOURCETYPE macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetType", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return D3DRTYPE_VOLUMETEXTURE;
}
HRESULT CDirect3DVolumeTexture_SetAutoGenFilterType(const CDirect3DVolumeTexture *_this, D3DTEXTUREFILTERTYPE f)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &f };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_SetAutoGenFilterType", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)f;
    return 0;
}
D3DTEXTUREFILTERTYPE CDirect3DVolumeTexture_GetAutoGenFilterType(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    D3DTEXTUREFILTERTYPE macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetAutoGenFilterType", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}
void CDirect3DVolumeTexture_GenerateMipSubLevels(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GenerateMipSubLevels", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

void ZN22CDirect3DVolumeTextureD1Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN22CDirect3DVolumeTextureD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = _this;
    tex->primaryVtable = vtbl_CDirect3DVolumeTexture;
    extern void *vtbl_CDirect3DVolumeTexture_secondary[];
    tex->secondaryVtable = vtbl_CDirect3DVolumeTexture_secondary;
    for (UINT32 level = 0; level < tex->levelCount; ++level) {
        void *volume = tex->volumes[level];
        ((ULONG (*)(void *))(*(void ***)volume)[2])(volume);
    }
    free(tex->volumes);
    free(tex->pixelData);
    ZN14COpenGLTextureD1Ev(&tex->secondaryVtable);
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    UINT32 i;
    if (tex->volumes) {
        for (i = 0; i < tex->levelCount; i++)
            if (tex->volumes[i])
                __ZdlPv(tex->volumes[i]);
        free(tex->volumes);
        tex->volumes = NULL;
    }
    if (tex->texIDStorage) {
        glDeleteTextures(1, &tex->texIDStorage);
        tex->texIDStorage = 0;
    }
    free(tex->pixelData);
    tex->pixelData = NULL;
#endif
}

void ZN22CDirect3DVolumeTextureD0Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN22CDirect3DVolumeTextureD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN22CDirect3DVolumeTextureD1Ev(_this);
    free(_this);
}
void ZN22CDirect3DVolumeTextureD2Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN22CDirect3DVolumeTextureD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN22CDirect3DVolumeTextureD1Ev(_this);
}

void CDirect3DVolumeTexture_CDirect3DVolumeTexture(const CDirect3DVolumeTexture *_this, UINT32 Width, UINT32 Height, UINT32 Depth, UINT32 Levels, DWORD Usage, D3DFORMAT Format)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Width, &Height, &Depth, &Levels, &Usage, &Format };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_CDirect3DVolumeTexture", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    (void)Usage;
    memset(tex, 0, sizeof(*tex));
    COpenGLTexture_InitNative(&tex->secondaryVtable, &tex->texIDStorage, Width, Height, Depth);
    tex->primaryVtable = vtbl_CDirect3DVolumeTexture;
    extern void *vtbl_CDirect3DVolumeTexture_secondary[];
    tex->secondaryVtable = vtbl_CDirect3DVolumeTexture_secondary;
    tex->width = Width;
    tex->height = Height;
    tex->depth = Depth;
    tex->format = Format;
    tex->levelCount = Levels ? Levels : MacOpenGLUtils_GetNumTextureLevels(Width, Height, Depth);
    tex->volumes = calloc(tex->levelCount, sizeof(*tex->volumes));
    GLint previous;
    /* Native GL's binding query is 0x806a; Mac erroneously queried 0x806f. */
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &previous);
    glBindTexture(GL_TEXTURE_3D, tex->texIDStorage);
    UINT32 totalSize = 0, allocationWidth = Width, allocationHeight = Height, allocationDepth = Depth;
    for (UINT32 level = 0; level < tex->levelCount; ++level) {
        totalSize += MacOpenGLUtils_GetLevelSizeInBytes(allocationWidth ? allocationWidth : 1,
                                                       allocationHeight ? allocationHeight : 1,
                                                       allocationDepth ? allocationDepth : 1, &Format);
        allocationWidth >>= 1;
        allocationHeight >>= 1;
        allocationDepth >>= 1;
    }
    tex->pixelData = calloc(1, totalSize ? totalSize : 1);
    UINT32 offset = 0, width = Width, height = Height, depth = Depth;
    for (UINT32 level = 0; level < tex->levelCount; ++level) {
        if (!width) width = 1;
        if (!height) height = 1;
        if (!depth) depth = 1;
        CDirect3DVolumeClean *volume = calloc(1, sizeof(*volume));
        volume->vtable = vtbl_CDirect3DVolume;
        volume->level = level;
        volume->width = width;
        volume->height = height;
        volume->depth = depth;
        volume->format = Format;
        volume->volumeMemory = tex->pixelData + offset;
        volume->owner = &tex->secondaryVtable;
        bool createGL = 1;
        MacOpenGLUtils_GetOpenGLTextureType(&createGL, &volume->glInternalFormat,
                                           &volume->glFormat, &volume->glType, Format);
        if (createGL)
            CDirect3DVolume_CreateOpenGLSurfaceObject(volume);
        ++volume->refCount;
        tex->volumes[level] = volume;
        offset += MacOpenGLUtils_GetLevelSizeInBytes(width, height, depth, &Format);
        width >>= 1;
        height >>= 1;
        depth >>= 1;
    }
#if COD2_APPLE_SDK
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, tex->levelCount - 1);
#endif
    glBindTexture(GL_TEXTURE_3D, previous);
    tex->refCount = 1;
#else

    CDirect3DVolumeTextureClean *tex = (CDirect3DVolumeTextureClean *)_this;
    UINT32 i, w, h, d, totalSize, offset;
    bool createGL = 0;
    GLenum internalFmt = 0, glFmt = 0, glType = 0;
    int prevTex = 0;
    (void)Usage;

    memset(tex, 0, sizeof(*tex));
    tex->primaryVtable = vtbl_CDirect3DVolumeTexture;
    tex->mpTexID = &tex->texIDStorage;
    tex->mTexWrapS = GL_REPEAT;
    tex->mTexWrapT = GL_REPEAT;
    tex->mTexMinFilter = GL_LINEAR;
    tex->mTexMagFilter = GL_LINEAR;
    tex->mTexAniso = 1.0f;
    tex->glWidth = Width;
    tex->glHeight = Height;
    tex->glDepth = Depth;
    tex->refCount = 1;
    tex->width = Width;
    tex->height = Height;
    tex->depth = Depth;
    tex->format = Format;

    if (Levels == 0)
        tex->levelCount = MacOpenGLUtils_GetNumTextureLevels(Width, Height, Depth);
    else
        tex->levelCount = Levels;
    if (tex->levelCount > MAX_MIP_LEVELS)
        tex->levelCount = MAX_MIP_LEVELS;

    MacOpenGLUtils_GetOpenGLTextureType(&createGL, &internalFmt, &glFmt, &glType, Format);

    glGenTextures(1, &tex->texIDStorage);
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &prevTex);
    glBindTexture(GL_TEXTURE_3D, tex->texIDStorage);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
#if COD2_APPLE_SDK
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, tex->levelCount - 1);
#endif
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    totalSize = 0;
    w = Width;
    h = Height;
    d = Depth;
    for (i = 0; i < tex->levelCount; i++) {
        totalSize += MacOpenGLUtils_GetLevelSizeInBytes(w ? w : 1, h ? h : 1, d ? d : 1, &Format);
        w >>= 1;
        h >>= 1;
        d >>= 1;
    }
    tex->pixelData = (byte *)calloc(1, totalSize ? totalSize : 1);

    tex->volumes = (void **)calloc(tex->levelCount, sizeof(void *));
    offset = 0;
    w = Width;
    h = Height;
    d = Depth;
    for (i = 0; i < tex->levelCount; i++) {
        UINT32 lw = w ? w : 1, lh = h ? h : 1, ld = d ? d : 1;
        UINT32 levelSize = MacOpenGLUtils_GetLevelSizeInBytes(lw, lh, ld, &Format);
        CDirect3DVolumeClean *vol = (CDirect3DVolumeClean *)calloc(1, sizeof(CDirect3DVolumeClean));
#if COD2_APPLE_SDK
        vol->vtable = vtbl_CDirect3DVolume;
#endif
        vol->refCount = 1;
        vol->level = i;
        vol->width = lw;
        vol->height = lh;
        vol->depth = ld;
        vol->volumeMemory = tex->pixelData + offset;
        vol->format = Format;
        vol->glInternalFormat = internalFmt;
        vol->glFormat = glFmt;
        vol->glType = glType;
        tex->volumes[i] = vol;

        if (createGL)
            CDirect3DVolume_CreateOpenGLSurfaceObject(vol);

        offset += levelSize;
        w >>= 1;
        h >>= 1;
        d >>= 1;
    }

    glBindTexture(GL_TEXTURE_3D, prevTex);
#endif
}

void CDirect3DVolumeTexture_UpdateOpenGLSurfaces(const CDirect3DVolumeTexture *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_UpdateOpenGLSurfaces", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    extern void CDirect3DVolumeTexture_UpdateOpenGLSurfacesNative(const CDirect3DVolumeTexture *, int);
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    GLint previous;
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &previous);
    glBindTexture(GL_TEXTURE_3D, tex->texIDStorage);
    CDirect3DVolumeTexture_UpdateOpenGLSurfacesNative(_this, 0);
    glBindTexture(GL_TEXTURE_3D, previous);
#else

#if COD2_APPLE_SDK
    CDirect3DVolumeTextureClean *tex = (void *)_this;
    GLint previous;
    glGetIntegerv(GL_TEXTURE_BINDING_3D, &previous);
    glBindTexture(GL_TEXTURE_3D, tex->texIDStorage);
    for (UINT32 level = 0; level < tex->levelCount; ++level) {
        CDirect3DVolumeClean *volume = tex->volumes[level];
        if (volume->isDirty)
            CDirect3DVolume_UpdateOpenGLSurfaceObject(volume, 0);
    }
    glBindTexture(GL_TEXTURE_3D, previous);
#else
    (void)_this;
#endif
#endif
}

HRESULT CDirect3DVolumeTexture_GetDevice(const CDirect3DVolumeTexture *t, void *p)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &p };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetDevice", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)p;
    return 0;
}
HRESULT CDirect3DVolumeTexture_SetPrivateData(const CDirect3DVolumeTexture *t, const GUID *g, const void *d, DWORD s, DWORD f)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &g, &d, &s, &f };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_SetPrivateData", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)g;
    (void)d;
    (void)s;
    (void)f;
    return 0;
}
HRESULT CDirect3DVolumeTexture_GetPrivateData(const CDirect3DVolumeTexture *t, const GUID *g, void *d, DWORD *s)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &g, &d, &s };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetPrivateData", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)g;
    (void)d;
    (void)s;
    return 0;
}
HRESULT CDirect3DVolumeTexture_FreePrivateData(const CDirect3DVolumeTexture *t, const GUID *g)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &g };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_FreePrivateData", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)g;
    return 0;
}
DWORD CDirect3DVolumeTexture_SetPriority(const CDirect3DVolumeTexture *t, DWORD p)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t, &p };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_SetPriority", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    (void)p;
    return 0;
}
DWORD CDirect3DVolumeTexture_GetPriority(const CDirect3DVolumeTexture *t)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_GetPriority", t, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)t;
    return 0;
}
void CDirect3DVolumeTexture_PreLoad(const CDirect3DVolumeTexture *t)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &t };
    if (MacShim_Dispatch("CDirect3DVolumeTexture_PreLoad", t, macArguments, NULL, 0))
        return;
#endif

    (void)t;
}

#if defined(COD2_X64) || COD2_APPLE_SDK
/* D3D9 COM slots: the engine uploads through LockBox/UnlockBox, not direct calls. */
void *vtbl_CDirect3DVolumeTexture[30] = {
    (void *)CDirect3DVolumeTexture_QueryInterface,
    (void *)CDirect3DVolumeTexture_AddRef,
    (void *)CDirect3DVolumeTexture_Release,
    (void *)CDirect3DVolumeTexture_GetDevice,
    (void *)CDirect3DVolumeTexture_SetPrivateData,
    (void *)CDirect3DVolumeTexture_GetPrivateData,
    (void *)CDirect3DVolumeTexture_FreePrivateData,
    (void *)CDirect3DVolumeTexture_SetPriority,
    (void *)CDirect3DVolumeTexture_GetPriority,
    (void *)CDirect3DVolumeTexture_PreLoad,
    (void *)CDirect3DVolumeTexture_GetType,
    (void *)CDirect3DVolumeTexture_SetLOD,
    (void *)CDirect3DVolumeTexture_GetLOD,
    (void *)CDirect3DVolumeTexture_GetLevelCount,
    (void *)CDirect3DVolumeTexture_SetAutoGenFilterType,
    (void *)CDirect3DVolumeTexture_GetAutoGenFilterType,
    (void *)CDirect3DVolumeTexture_GenerateMipSubLevels,
    (void *)CDirect3DVolumeTexture_GetLevelDesc,
    (void *)CDirect3DVolumeTexture_GetVolumeLevel,
    (void *)CDirect3DVolumeTexture_LockBox,
    (void *)CDirect3DVolumeTexture_UnlockBox,
    (void *)CDirect3DVolumeTexture_AddDirtyBox,
    (void *)ZN22CDirect3DVolumeTextureD1Ev,
    (void *)ZN22CDirect3DVolumeTextureD0Ev
};
static void *vtbl_CDirect3DVolume[13] = {
    (void *)CDirect3DVolume_QueryInterface,
    (void *)CDirect3DVolume_AddRef,
    (void *)CDirect3DVolume_Release,
    (void *)CDirect3DVolume_GetDevice,
    (void *)CDirect3DVolume_SetPrivateData,
    (void *)CDirect3DVolume_GetPrivateData,
    (void *)CDirect3DVolume_FreePrivateData,
    (void *)CDirect3DVolume_GetContainer,
    (void *)CDirect3DVolume_GetDesc,
    (void *)CDirect3DVolume_LockBox,
    (void *)CDirect3DVolume_UnlockBox,
    (void *)ZN15CDirect3DVolumeD1Ev,
    (void *)ZN15CDirect3DVolumeD0Ev
};
#endif

#if defined(COD2_X64)
void CDirect3DVolumeTexture_UpdateOpenGLSurfacesNative(const CDirect3DVolumeTexture *_this, int recreate)
{
    CDirect3DVolumeTextureClean *tex = (void *)_this;
#if COD2_APPLE_SDK
    if (recreate)
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, tex->levelCount - 1);
#endif
    for (UINT32 level = 0; level < tex->levelCount; ++level) {
        CDirect3DVolumeClean *volume = tex->volumes[level];
        if (recreate || CDirect3DVolume_IsDirty(volume)) {
            CDirect3DVolume_UpdateOpenGLSurfaceObject(volume, recreate != 0);
            volume->isDirty = 0;
        }
    }
}
static void CDirect3DVolumeTexture_SecondaryD1(void *info)
{
    ZN22CDirect3DVolumeTextureD1Ev((void **)info - 1);
}
static void CDirect3DVolumeTexture_SecondaryD0(void *info)
{
    ZN22CDirect3DVolumeTextureD0Ev((void **)info - 1);
}
extern void COpenGLTexture_UpdateOpenGLSurfaces(const COpenGLTexture *);
void *vtbl_CDirect3DVolumeTexture_secondary[] = {
    (void *)CDirect3DVolumeTexture_SecondaryD1, (void *)CDirect3DVolumeTexture_SecondaryD0,
    (void *)COpenGLTexture_UpdateOpenGLSurfaces
};
_Static_assert(sizeof(CDirect3DVolumeTextureClean) <= 0x16c, "device volume allocation");
_Static_assert(__builtin_offsetof(CDirect3DVolumeTextureClean, refCount) == 3 * sizeof(void *) + 11 * sizeof(UINT32), "native texture prefix extent");
#endif
