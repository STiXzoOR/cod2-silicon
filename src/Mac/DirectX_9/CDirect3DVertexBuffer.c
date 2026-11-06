#include "Mac/Metal/shim_dispatch.h"
#include "common_types.h"
#include "imports.h"
#include <stdlib.h>
#include <string.h>
typedef void (*fnptr_t)(void);
extern fnptr_t vtbl_CDirect3DVertexBuffer[];
extern fnptr_t vtbl_CStaticCacheInfo[];
extern void *imp___ZTV15CCacheInfoBlock;

#if defined(COD2_X64)
#include "lp64_buffers.h"
#include <math.h>
#include <stdint.h>
typedef struct {
    GLuint id;
    const void *start;
    UINT32 sizeInBytes, frameCount;
} CBufferFenceNative;
typedef struct { GLuint *begin, *end, *capacity; } CBufferFenceIds;
typedef struct {
    fnptr_t *vtable;
    unsigned char dirty;
} CCacheInfoBlockNative;
typedef struct {
    CCacheInfoBlockNative *begin, *end, *capacity;
} CCacheInfoVectorNative;
typedef struct {
    fnptr_t *vtable;
    UINT32 length;
    const byte *data;
    UINT32 count;
    CCacheInfoVectorNative blocks;
} CStaticCacheInfoNative;
extern CBufferFenceIds CFence_sUnusedFenceIDs;
extern UINT32 COpenGL_sFrameCount;
extern void CFence_CFence(const CFence *, const void *, UINT32, UINT32);
extern void CMemoryBuffer_CMemoryBuffer(const CMemoryBuffer *, UINT32);
extern void CMemoryBuffer_FreeLater(const CMemoryBuffer *, UINT32);
extern void CMemoryBuffer_Recreate(const CMemoryBuffer *);
extern void ZN13CMemoryBufferD1Ev(const CMemoryBuffer *);
extern void CVAOPacket_ReleaseBuffer(const void *, UINT32);
void ZN15CCacheInfoBlockD1Ev(void *);
void ZN15CCacheInfoBlockD0Ev(void *);
void ZN16CStaticCacheInfoD0Ev(void *);
void ZN16CStaticCacheInfoD1Ev(void *);
void CDirect3DVertexBuffer_WaitFence(const CDirect3DVertexBuffer *, const void *, UINT32);
static fnptr_t vtbl_CCacheInfoBlockNative[] = {
    (fnptr_t)ZN15CCacheInfoBlockD1Ev, (fnptr_t)ZN15CCacheInfoBlockD0Ev
};

/* i386 converts unsigned lengths through SSE before ceilf, including the
 * shift/or/double path for lengths with their high bit set. */
static UINT32 CBuffer_CacheBlockEnd(UINT32 length)
{
    float value;
    if (length & 0x80000000U) {
        value = (float)((length >> 1) | (length & 1));
        value = value + value;
    } else {
        value = (float)(int)length;
    }
    return (UINT32)(uint64_t)ceilf(value * 0.0009765625f);
}

static void CBuffer_RecycleFence(CBufferFenceNative *fence)
{
    CBufferFenceIds *ids = &CFence_sUnusedFenceIDs;
    if (ids->end == ids->capacity) {
        size_t count = ids->begin ? (size_t)(ids->end - ids->begin) : 0;
        size_t capacity = count ? count * 2 : 1;
        GLuint *data = malloc(capacity * sizeof(*data));
        if (!data) abort();
        if (count) memcpy(data, ids->begin, count * sizeof(*data));
        free(ids->begin);
        ids->begin = data;
        ids->end = data + count;
        ids->capacity = data + capacity;
    }
    *ids->end++ = fence->id;
#if MAC_METAL_NATIVE
    MacShim_ForgetObject(fence);
#endif
    free(fence);
}

static CBufferFenceNode *CBuffer_EraseFence(CBufferFenceNode *node)
{
    CBufferFenceNode *next = node->next;
    node->prev->next = next;
    next->prev = node->prev;
    free(node);
    return next;
}
#else
typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 lengthBytes;
    byte *data;
    DWORD usage;
} CDirect3DVertexBufferClean;
#endif

ULONG CDirect3DVertexBuffer_AddRef(const CDirect3DVertexBuffer *_this);
void ZN21CDirect3DVertexBufferD0Ev(void *_this);
void ZN21CDirect3DVertexBufferD1Ev(void *_this);
void ZN21CDirect3DVertexBufferD2Ev(void *_this);

ULONG CDirect3DVertexBuffer_AddRef(const CDirect3DVertexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_AddRef", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexBufferClean *vb = (CDirect3DVertexBufferClean *)_this;
    return ++vb->refCount;
}

HRESULT CDirect3DVertexBuffer_QueryInterface(const CDirect3DVertexBuffer *_this, const IID *iid, void **ppvObj)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &iid, &ppvObj };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_QueryInterface", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)iid;
    *ppvObj = (void *)_this;
#if defined(COD2_X64)
    ((ULONG (*)(const void *))((CDirect3DVertexBufferClean *)_this)->vtable[1])(_this);
#else
    CDirect3DVertexBuffer_AddRef(_this);
#endif
    return 0;
}

ULONG CDirect3DVertexBuffer_Release(const CDirect3DVertexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    ULONG macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_Release", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexBufferClean *vb = (CDirect3DVertexBufferClean *)_this;
    ULONG rc = --vb->refCount;
    if (!rc) {

        ((void (*)(void *))vb->vtable[15])((void *)_this);
    }
    return rc;
}

HRESULT CDirect3DVertexBuffer_Lock(const CDirect3DVertexBuffer *_this, UINT OffsetToLock, UINT SizeToLock, void **ppbData, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &OffsetToLock, &SizeToLock, &ppbData, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_Lock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexBufferClean *vb = (CDirect3DVertexBufferClean *)_this;
#if defined(COD2_X64)
    vb->isLocked = 1;
    vb->lockSize = SizeToLock ? SizeToLock : vb->lengthBytes;
    if (vb->memory.vptr) {
        if ((vb->usage & 0x200) && (Flags & 0x2000)) {
            CMemoryBuffer_FreeLater((const CMemoryBuffer *)&vb->memory, 2);
            CMemoryBuffer_Recreate((const CMemoryBuffer *)&vb->memory);
            vb->data = vb->memory.data;
        } else if (!(vb->usage & 0x200)) {
            vb->lockPtr = vb->data + OffsetToLock;
            CDirect3DVertexBuffer_WaitFence(_this, vb->lockPtr, vb->lockSize);
        } else if ((Flags & 1) && vb->staticCache) {
            CStaticCacheInfoNative *cache = vb->staticCache;
            UINT32 end = CBuffer_CacheBlockEnd(OffsetToLock + vb->lockSize);
            for (UINT32 i = OffsetToLock >> 10; i < end; ++i)
                cache->blocks.begin[i].dirty = 1;
        }
    }
    vb->lockPtr = vb->data + OffsetToLock;
    *ppbData = vb->lockPtr;
#else
    (void)SizeToLock;
    (void)Flags;
    *ppbData = vb->data + OffsetToLock;
#endif
    return 0;
}

HRESULT CDirect3DVertexBuffer_Unlock(const CDirect3DVertexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_Unlock", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

HRESULT CDirect3DVertexBuffer_GetDesc(const CDirect3DVertexBuffer *_this, D3DVERTEXBUFFER_DESC *pDesc)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pDesc };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_GetDesc", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    CDirect3DVertexBufferClean *vb = (CDirect3DVertexBufferClean *)_this;
    pDesc->Format = 0;
    pDesc->Type = D3DRTYPE_VERTEXBUFFER;
    pDesc->Usage = vb->usage;
    pDesc->Pool = 0;
    pDesc->Size = vb->lengthBytes;
    pDesc->FVF = 0;
    return 0;
}

void ZN21CDirect3DVertexBufferD1Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN21CDirect3DVertexBufferD1Ev", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DVertexBufferClean *vb = (CDirect3DVertexBufferClean *)_this;
    vb->vtable = vtbl_CDirect3DVertexBuffer;
#if defined(COD2_X64)
    if (vb->memory.vptr) {
        if (vb->staticCache)
            ZN16CStaticCacheInfoD0Ev(vb->staticCache);
        if (!(vb->usage & 0x200))
            CVAOPacket_ReleaseBuffer(vb->memory.data, vb->memory.length);
        CBufferFenceNode *head = &vb->fences;
        for (CBufferFenceNode *node = head->next; node != head; ) {
            CBuffer_RecycleFence(node->fence);
            node = CBuffer_EraseFence(node);
        }
        ZN13CMemoryBufferD1Ev((const CMemoryBuffer *)&vb->memory);
#if MAC_METAL_NATIVE
        /* The member constructor records its interior address separately. */
        MacShim_ForgetObject(&vb->memory);
#endif
    } else {
        free(vb->data);
    }
#else
    free(vb->data);
#endif
    vb->data = NULL;
}

void ZN21CDirect3DVertexBufferD0Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN21CDirect3DVertexBufferD0Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN21CDirect3DVertexBufferD1Ev(_this);
    free(_this);
}

void ZN21CDirect3DVertexBufferD2Ev(void *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("ZN21CDirect3DVertexBufferD2Ev", _this, macArguments, NULL, 0))
        return;
#endif

    ZN21CDirect3DVertexBufferD1Ev(_this);
}

void CDirect3DVertexBuffer_CDirect3DVertexBuffer(const CDirect3DVertexBuffer *_this, UINT32 Length, DWORD Usage, D3DPOOL Pool)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &Length, &Usage, &Pool };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_CDirect3DVertexBuffer", _this, macArguments, NULL, 0))
        return;
#endif

    CDirect3DVertexBufferClean *vb = (CDirect3DVertexBufferClean *)_this;
    (void)Pool;
    vb->vtable = vtbl_CDirect3DVertexBuffer;
    vb->refCount = 1;
    vb->lengthBytes = Length;
#if defined(COD2_X64)
    vb->lockPtr = NULL;
    vb->lockSize = 0;
    vb->isLocked = 0;
    vb->fences.next = vb->fences.prev = &vb->fences;
    vb->fences.fence = NULL;
    vb->staticCache = NULL;
#if MAC_METAL_NATIVE
    if (MacShim_Metal()) {
        memset(&vb->memory, 0, sizeof(vb->memory));
        vb->data = (byte *)calloc(1, Length);
    } else
#endif
    {
        CMemoryBuffer_CMemoryBuffer((const CMemoryBuffer *)&vb->memory, Length);
        vb->data = vb->memory.data;
        /* Preserve the native allocator's defined initial bytes. */
        if (vb->data) memset(vb->data, 0, Length);
        if (Usage & 0x400000) {
            CStaticCacheInfoNative *cache = malloc(sizeof(*cache));
            if (!cache) abort();
            cache->vtable = vtbl_CStaticCacheInfo;
            cache->length = Length;
            cache->data = vb->data;
            cache->count = CBuffer_CacheBlockEnd(Length);
            cache->blocks.begin = malloc((size_t)cache->count * sizeof(*cache->blocks.begin));
            if (cache->count && !cache->blocks.begin) abort();
            cache->blocks.end = cache->blocks.capacity = cache->count
                ? cache->blocks.begin + cache->count : cache->blocks.begin;
            for (UINT32 i = 0; i < cache->count; ++i) {
                cache->blocks.begin[i].vtable = vtbl_CCacheInfoBlockNative;
                cache->blocks.begin[i].dirty = 1;
            }
            vb->staticCache = cache;
        }
    }
#else
    vb->data = (byte *)calloc(1, Length);
#endif
    vb->usage = Usage;
}

HRESULT CDirect3DVertexBuffer_GetDevice(const CDirect3DVertexBuffer *_this, IDirect3DDevice9 **ppDevice)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &ppDevice };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_GetDevice", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)ppDevice;
    return 0;
}

HRESULT CDirect3DVertexBuffer_SetPrivateData(const CDirect3DVertexBuffer *_this, const GUID *refguid, const void *pData, DWORD SizeOfData, DWORD Flags)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &refguid, &pData, &SizeOfData, &Flags };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_SetPrivateData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)refguid;
    (void)pData;
    (void)SizeOfData;
    (void)Flags;
    return 0;
}

HRESULT CDirect3DVertexBuffer_GetPrivateData(const CDirect3DVertexBuffer *_this, const GUID *refguid, void *pData, DWORD *pSizeOfData)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &refguid, &pData, &pSizeOfData };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_GetPrivateData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)refguid;
    (void)pData;
    (void)pSizeOfData;
    return 0;
}

HRESULT CDirect3DVertexBuffer_FreePrivateData(const CDirect3DVertexBuffer *_this, const GUID *refguid)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &refguid };
    HRESULT macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_FreePrivateData", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)refguid;
    return 0;
}

DWORD CDirect3DVertexBuffer_SetPriority(const CDirect3DVertexBuffer *_this, DWORD PriorityNew)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &PriorityNew };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_SetPriority", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    (void)PriorityNew;
    return 0;
}

DWORD CDirect3DVertexBuffer_GetPriority(const CDirect3DVertexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    DWORD macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_GetPriority", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return 0;
}

void CDirect3DVertexBuffer_PreLoad(const CDirect3DVertexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_PreLoad", _this, macArguments, NULL, 0))
        return;
#endif

    (void)_this;
}

D3DRESOURCETYPE CDirect3DVertexBuffer_GetType(const CDirect3DVertexBuffer *_this)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this };
    D3DRESOURCETYPE macResult = { 0 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_GetType", _this, macArguments, &macResult, sizeof(macResult)))
        return macResult;
#endif

    (void)_this;
    return D3DRTYPE_VERTEXBUFFER;
}

void CDirect3DVertexBuffer_WaitFence(const CDirect3DVertexBuffer *_this, const void *pStart, UINT32 SizeInBytes)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pStart, &SizeInBytes };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_WaitFence", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    CDirect3DVertexBufferClean *vb = (void *)_this;
    if (!vb->memory.vptr) return;
    CBufferFenceNode *head = &vb->fences;
    UINT32 frame = COpenGL_sFrameCount;
    for (CBufferFenceNode *node = head->next; node != head; ) {
        CBufferFenceNative *fence = node->fence;
        if ((UINT32)(frame - fence->frameCount) <= 1) {
            uintptr_t start = (uintptr_t)pStart;
            uintptr_t fenceStart = (uintptr_t)fence->start;
            if (start + SizeInBytes <= fenceStart || start >= fenceStart + fence->sizeInBytes) {
                node = node->next;
                continue;
            }
            glFinishFenceAPPLE(fence->id);
        }
        CBuffer_RecycleFence(fence);
        node = CBuffer_EraseFence(node);
    }
#else
    (void)_this;
    (void)pStart;
    (void)SizeInBytes;
#endif
}

void CDirect3DVertexBuffer_SetFence(const CDirect3DVertexBuffer *_this, const void *pStart, UINT32 SizeInBytes, const CDirect3DVertexBuffer *_this_3)
{
#if MAC_METAL_NATIVE
    const void *macArguments[] = { &_this, &pStart, &SizeInBytes, &_this_3 };
    if (MacShim_Dispatch("CDirect3DVertexBuffer_SetFence", _this, macArguments, NULL, 0))
        return;
#endif

#if defined(COD2_X64)
    CDirect3DVertexBufferClean *vb = (void *)_this;
    (void)_this_3;
    if (!vb->memory.vptr) return;
    CBufferFenceNode *head = &vb->fences;
    UINT32 frame = COpenGL_sFrameCount;
    for (CBufferFenceNode *node = head->next; node != head; ) {
        CBufferFenceNative *fence = node->fence;
        if ((UINT32)(frame - fence->frameCount) <= 1) {
            if (fence->start != pStart || fence->sizeInBytes != SizeInBytes) {
                node = node->next;
                continue;
            }
            CBuffer_RecycleFence(fence);
            CBuffer_EraseFence(node);
            break;
        }
        CBuffer_RecycleFence(fence);
        node = CBuffer_EraseFence(node);
    }
    CBufferFenceNative *fence = malloc(sizeof(*fence));
    if (!fence) abort();
#if MAC_METAL_NATIVE
    MacShim_RecordObject(fence, 0);
#endif
    CFence_CFence((const CFence *)fence, pStart, SizeInBytes, frame);
    CBufferFenceNode *node = malloc(sizeof(*node));
    if (!node) abort();
    node->fence = fence;
    node->next = head->next;
    node->prev = head;
    head->next->prev = node;
    head->next = node;
#else
    (void)_this;
    (void)pStart;
    (void)SizeInBytes;
    (void)_this_3;
#endif
}

void ZN16CStaticCacheInfoD0Ev(void *_this)
{
#if defined(COD2_X64)
    ZN16CStaticCacheInfoD1Ev(_this);
#endif
    free(_this);
}

#if defined(COD2_X64)
void ZN16CStaticCacheInfoD1Ev(void *_this)
{
    CStaticCacheInfoNative *cache = _this;
    cache->vtable = vtbl_CStaticCacheInfo;
    for (CCacheInfoBlockNative *block = cache->blocks.begin; block != cache->blocks.end; ++block)
        ((void (*)(void *))block->vtable[0])(block);
    free(cache->blocks.begin);
}
#else
void ZN16CStaticCacheInfoD1Ev(void *_this)
{
    (void)_this;
}
#endif

#if defined(COD2_X64)
void ZN16CStaticCacheInfoD2Ev(void *_this)
{
    ZN16CStaticCacheInfoD1Ev(_this);
}
#else
void ZN16CStaticCacheInfoD2Ev(void *_this)
{
    (void)_this;
}
#endif

void CStaticCacheInfo_Flush(const CStaticCacheInfo *_this, void (*pStart)(), void (*pEnd)())
{
#if defined(COD2_X64)
    CStaticCacheInfoNative *cache = (void *)_this;
    UINT32 start = (UINT32)((uintptr_t)pStart - (uintptr_t)cache->data) >> 10;
    UINT32 end = CBuffer_CacheBlockEnd((UINT32)((uintptr_t)pEnd - (uintptr_t)cache->data));
    for (UINT32 i = start; i < end; ++i) {
        if (cache->blocks.begin[i].dirty) {
            glFlushVertexArrayRangeAPPLE(1024, cache->data + (i << 10));
            cache->blocks.begin[i].dirty = 0;
        }
    }
#else
    (void)_this;
    (void)pStart;
    (void)pEnd;
#endif
}

#if defined(COD2_X64)
void ZN15CCacheInfoBlockD1Ev(void *_this)
{
    ((CCacheInfoBlockNative *)_this)->vtable = vtbl_CCacheInfoBlockNative;
}
#else
void ZN15CCacheInfoBlockD1Ev(void *_this)
{
    (void)_this;
}
#endif

void ZN15CCacheInfoBlockD0Ev(void *_this)
{
#if defined(COD2_X64)
    ZN15CCacheInfoBlockD1Ev(_this);
#endif
    free(_this);
}

void *ZNSt4listIP6CFenceSaIS1_EE5eraseESt14_List_iteratorIS1_E(void *list, void *pos)
{
#if defined(COD2_X64)
    (void)list;
    return CBuffer_EraseFence(pos);
#else
    (void)list;
    return pos;
#endif
}

void ZNSt6vectorI15CCacheInfoBlockSaIS0_EEC1Em(void *vec, unsigned long n)
{
#if defined(COD2_X64)
    CCacheInfoVectorNative *blocks = vec;
    if (n > 0x1fffffffUL) abort();
    blocks->begin = malloc(n * sizeof(*blocks->begin));
    if (n && !blocks->begin) abort();
    blocks->end = blocks->capacity = n ? blocks->begin + n : blocks->begin;
    for (size_t i = 0; i < n; ++i) {
        blocks->begin[i].vtable = vtbl_CCacheInfoBlockNative;
        blocks->begin[i].dirty = 1;
    }
#else
    (void)vec;
    (void)n;
#endif
}

void ZNSt6vectorImSaImEE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPmS1_EERKm(void *vec, void *pos, const unsigned long *val)
{
#if defined(COD2_X64)
    CBufferFenceIds *ids = vec;
    GLuint value = *(const GLuint *)val;
    size_t count = ids->begin ? (size_t)(ids->end - ids->begin) : 0;
    size_t index = ids->begin ? (size_t)((GLuint *)pos - ids->begin) : 0;
    if (ids->end == ids->capacity) {
        size_t capacity = count ? count * 2 : 1;
        if (count == 0x3fffffffU || capacity > 0x3fffffffU) abort();
        GLuint *data = malloc(capacity * sizeof(*data));
        if (!data) abort();
        if (index) memcpy(data, ids->begin, index * sizeof(*data));
        if (count != index) memcpy(data + index + 1, ids->begin + index, (count - index) * sizeof(*data));
        free(ids->begin);
        ids->begin = data;
        ids->end = data + count;
        ids->capacity = data + capacity;
    } else {
        memmove(ids->begin + index + 1, ids->begin + index, (count - index) * sizeof(*ids->begin));
    }
    ids->begin[index] = value;
    ++ids->end;
#else
    (void)vec;
    (void)pos;
    (void)val;
#endif
}

fnptr_t vtbl_CStaticCacheInfo[] = { (fnptr_t)ZN16CStaticCacheInfoD1Ev, (fnptr_t)ZN16CStaticCacheInfoD0Ev };
fnptr_t vtbl_CDirect3DVertexBuffer[] = { (fnptr_t)CDirect3DVertexBuffer_QueryInterface, (fnptr_t)CDirect3DVertexBuffer_AddRef, (fnptr_t)CDirect3DVertexBuffer_Release, (fnptr_t)CDirect3DVertexBuffer_GetDevice, (fnptr_t)CDirect3DVertexBuffer_SetPrivateData, (fnptr_t)CDirect3DVertexBuffer_GetPrivateData, (fnptr_t)CDirect3DVertexBuffer_FreePrivateData, (fnptr_t)CDirect3DVertexBuffer_SetPriority, (fnptr_t)CDirect3DVertexBuffer_GetPriority, (fnptr_t)CDirect3DVertexBuffer_PreLoad, (fnptr_t)CDirect3DVertexBuffer_GetType, (fnptr_t)CDirect3DVertexBuffer_Lock, (fnptr_t)CDirect3DVertexBuffer_Unlock, (fnptr_t)CDirect3DVertexBuffer_GetDesc, (fnptr_t)ZN21CDirect3DVertexBufferD1Ev, (fnptr_t)ZN21CDirect3DVertexBufferD0Ev };
