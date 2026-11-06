#ifndef COD2_LP64_BUFFERS_H
#define COD2_LP64_BUFFERS_H

#if defined(COD2_X64)
typedef struct {
    void (**vptr)(void);
    byte *allocation;
    byte *data;
    UINT32 length;
    int freedLater;
} CMemoryBufferNative;

typedef struct CBufferFenceNode {
    struct CBufferFenceNode *next, *prev;
    void *fence;
} CBufferFenceNode;

/* Shared by resource constructors and draw consumers on the native path. */
typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 lengthBytes;
    byte *data;
    DWORD usage;
    CMemoryBufferNative memory;
    byte *lockPtr;
    UINT32 lockSize;
    unsigned char isLocked;
    CBufferFenceNode fences;
    void *staticCache;
} CDirect3DVertexBufferClean;

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
    CMemoryBufferNative memory;
} CDirect3DIndexBufferClean;

#endif /* COD2_X64 */
#endif
