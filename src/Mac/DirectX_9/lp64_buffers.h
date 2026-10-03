#ifndef COD2_LP64_BUFFERS_H
#define COD2_LP64_BUFFERS_H

/* Shared by resource constructors and draw consumers on the native path. */
typedef struct {
    void **vtable;
    ULONG refCount;
    UINT32 lengthBytes;
    byte *data;
    DWORD usage;
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
} CDirect3DIndexBufferClean;

#endif
