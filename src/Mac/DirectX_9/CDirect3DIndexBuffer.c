#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>
typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3DIndexBuffer[];

#if defined(COD2_X64)
#include "lp64_buffers.h"
#else
typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 lengthBytes;
    byte *data;
    UINT32 indexSizeBytes;
    DWORD usage;
    byte *lockPtr;
    UINT32 lockSize;
    unsigned char isLocked;
} CDirect3DIndexBufferClean;
#endif

ULONG CDirect3DIndexBuffer_AddRef(const CDirect3DIndexBuffer *_this);
void ZN20CDirect3DIndexBufferD0Ev(const CDirect3DIndexBuffer *_this);
void ZN20CDirect3DIndexBufferD1Ev(const CDirect3DIndexBuffer *_this);

ULONG CDirect3DIndexBuffer_AddRef(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DIndexBufferClean *ib = (CDirect3DIndexBufferClean *)_this;
    return ++ib->refCount;
}

HRESULT CDirect3DIndexBuffer_QueryInterface(const CDirect3DIndexBuffer *_this, const IID *iid, LPVOID *ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;
    *ppvObj = (LPVOID)_this;
    CDirect3DIndexBuffer_AddRef(_this);
    return 0;
}

ULONG CDirect3DIndexBuffer_Release(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DIndexBufferClean *ib = (CDirect3DIndexBufferClean *)_this;
    ULONG rc = --ib->refCount;
    if (!rc) {
        ZN20CDirect3DIndexBufferD0Ev(_this);
    }
    return rc;
}

void ZN20CDirect3DIndexBufferD1Ev(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20CDirect3DIndexBufferD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DIndexBufferClean *ib = (CDirect3DIndexBufferClean *)_this;
    ib->vtable = vtbl_CDirect3DIndexBuffer;
    free(ib->data);
    ib->data = NULL;
}

void ZN20CDirect3DIndexBufferD0Ev(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN20CDirect3DIndexBufferD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN20CDirect3DIndexBufferD1Ev(_this);
    free((void *)_this);
}

HRESULT CDirect3DIndexBuffer_Lock(const CDirect3DIndexBuffer *_this, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &OffsetToLock, &SizeToLock, &ppbData, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_Lock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DIndexBufferClean *ib = (CDirect3DIndexBufferClean *)_this;
    (void)Flags;
    ib->isLocked = 1;
    ib->lockSize = SizeToLock ? SizeToLock : ib->lengthBytes;
    ib->lockPtr = ib->data + OffsetToLock;
    *ppbData = ib->lockPtr;
    return 0;
}

HRESULT CDirect3DIndexBuffer_Unlock(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_Unlock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

HRESULT CDirect3DIndexBuffer_GetDesc(const CDirect3DIndexBuffer *_this, D3DINDEXBUFFER_DESC *pDesc)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pDesc };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_GetDesc", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DIndexBufferClean *ib = (CDirect3DIndexBufferClean *)_this;
    pDesc->Format = ib->indexSizeBytes == 2 ? D3DFMT_INDEX16 : D3DFMT_INDEX32;
    pDesc->Type = D3DRTYPE_INDEXBUFFER;
    pDesc->Usage = ib->usage;
    pDesc->Pool = 0;
    pDesc->Size = ib->lengthBytes;
    return 0;
}

void CDirect3DIndexBuffer_CDirect3DIndexBuffer(const CDirect3DIndexBuffer *_this, UINT32 Length, D3DFORMAT Format, DWORD Usage, D3DPOOL Pool)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Length, &Format, &Usage, &Pool };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_CDirect3DIndexBuffer", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DIndexBufferClean *ib = (CDirect3DIndexBufferClean *)_this;
    (void)Pool;
    ib->vtable = vtbl_CDirect3DIndexBuffer;
    ib->refCount = 1;
    ib->lengthBytes = (Length + 3) & ~3U;
    ib->data = (byte *)calloc(1, ib->lengthBytes);

    ib->indexSizeBytes = (Format == D3DFMT_INDEX16) ? 2 : 4;
    ib->usage = Usage;
    ib->lockPtr = NULL;
    ib->lockSize = 0;
    ib->isLocked = 0;
}

HRESULT CDirect3DIndexBuffer_GetDevice(const CDirect3DIndexBuffer *_this, IDirect3DDevice9 **ppDevice)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppDevice };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_GetDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppDevice;
    return 0;
}

HRESULT CDirect3DIndexBuffer_SetPrivateData(const CDirect3DIndexBuffer *_this, const GUID *refguid, const void *pData, DWORD SizeOfData, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &refguid, &pData, &SizeOfData, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_SetPrivateData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)refguid;
    (void)pData;
    (void)SizeOfData;
    (void)Flags;
    return 0;
}

HRESULT CDirect3DIndexBuffer_GetPrivateData(const CDirect3DIndexBuffer *_this, const GUID *refguid, void *pData, DWORD *pSizeOfData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &refguid, &pData, &pSizeOfData };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_GetPrivateData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)refguid;
    (void)pData;
    (void)pSizeOfData;
    return 0;
}

HRESULT CDirect3DIndexBuffer_FreePrivateData(const CDirect3DIndexBuffer *_this, const GUID *refguid)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &refguid };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_FreePrivateData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)refguid;
    return 0;
}

DWORD CDirect3DIndexBuffer_SetPriority(const CDirect3DIndexBuffer *_this, DWORD PriorityNew)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PriorityNew };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_SetPriority", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PriorityNew;
    return 0;
}

DWORD CDirect3DIndexBuffer_GetPriority(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_GetPriority", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

void CDirect3DIndexBuffer_PreLoad(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_PreLoad", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

D3DRESOURCETYPE CDirect3DIndexBuffer_GetType(const CDirect3DIndexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    D3DRESOURCETYPE macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DIndexBuffer_GetType", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return D3DRTYPE_INDEXBUFFER;
}

fnptr_t vtbl_CDirect3DIndexBuffer[] = { (fnptr_t)CDirect3DIndexBuffer_QueryInterface, (fnptr_t)CDirect3DIndexBuffer_AddRef, (fnptr_t)CDirect3DIndexBuffer_Release, (fnptr_t)CDirect3DIndexBuffer_GetDevice, (fnptr_t)CDirect3DIndexBuffer_SetPrivateData, (fnptr_t)CDirect3DIndexBuffer_GetPrivateData, (fnptr_t)CDirect3DIndexBuffer_FreePrivateData, (fnptr_t)CDirect3DIndexBuffer_SetPriority, (fnptr_t)CDirect3DIndexBuffer_GetPriority, (fnptr_t)CDirect3DIndexBuffer_PreLoad, (fnptr_t)CDirect3DIndexBuffer_GetType, (fnptr_t)CDirect3DIndexBuffer_Lock, (fnptr_t)CDirect3DIndexBuffer_Unlock, (fnptr_t)CDirect3DIndexBuffer_GetDesc, (fnptr_t)ZN20CDirect3DIndexBufferD1Ev, (fnptr_t)ZN20CDirect3DIndexBufferD0Ev };
