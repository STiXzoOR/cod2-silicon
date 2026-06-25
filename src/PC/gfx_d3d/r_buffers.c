#include "common_types.h"
#include "imports.h"
extern int alwaysfails;

extern const char *va(const char *fmt, ...);
extern void R_FatalInitError(const char *msg);
extern const char *R_ErrorDescription(HRESULT hr);

void *R_AllocStaticVertexBuffer(IDirect3DVertexBuffer9 **vb, int sizeInBytes);
void R_FinishStaticVertexBuffer(IDirect3DVertexBuffer9 *vb);
void R_FreeStaticVertexBuffer(IDirect3DVertexBuffer9 *vb);
void *R_AllocStaticIndexBuffer(IDirect3DIndexBuffer9 **ib, int sizeInBytes);
void R_FinishStaticIndexBuffer(IDirect3DIndexBuffer9 *ib);
void R_FreeStaticIndexBuffer(IDirect3DIndexBuffer9 *ib);

#define VTABLE(obj) (*(void ***)((void *)(obj)))

typedef HRESULT(D3DVTCC *CreateVertexBufferFn)(void *device, UINT length, DWORD usage, DWORD fvf, int pool, IDirect3DVertexBuffer9 **ppVB, void *sharedHandle);
typedef HRESULT(D3DVTCC *CreateIndexBufferFn)(void *device, UINT length, DWORD usage, DWORD fvf, int format, IDirect3DIndexBuffer9 **ppIB, void *sharedHandle);
typedef HRESULT(D3DVTCC *BufferLockFn)(void *buffer, UINT offsetToLock, UINT sizeToLock, void **ppbData, DWORD flags);
typedef HRESULT(D3DVTCC *BufferUnlockFn)(void *buffer);
typedef ULONG(D3DVTCC *ReleaseFn)(void *obj);

void *R_AllocStaticVertexBuffer(IDirect3DVertexBuffer9 **vb, int sizeInBytes)
{
    void *vertexBufferData;
    byte *dxPtr;
    void *device;
    void **vtable;
    HRESULT hr;

    dxPtr = (byte *)imp_dx;
    device = *(void **)(dxPtr + 8);
    vtable = VTABLE(device);

    hr = ((CreateVertexBufferFn)vtable[0x68 / 4])(device, sizeInBytes, 8, 0, 0, vb, NULL);
    if (hr < 0) {
        R_FatalInitError(va("DirectX didn't create a %i-byte vertex buffer: %s\n", sizeInBytes, R_ErrorDescription(hr)));
    }

    hr = ((BufferLockFn)VTABLE(*vb)[0x2c / 4])((void *)*vb, 0, 0, &vertexBufferData, 0);
    if (hr < 0) {
        R_FatalInitError(va("DirectX didn't lock a vertex buffer: %s\n", R_ErrorDescription(hr)));
    }

    return vertexBufferData;
}

void R_FinishStaticVertexBuffer(IDirect3DVertexBuffer9 *vb)
{

    do {
        ((BufferUnlockFn)VTABLE(vb)[0x30 / 4])((void *)vb);
    } while (*(volatile int *)&alwaysfails != 0);
}

void R_FreeStaticVertexBuffer(IDirect3DVertexBuffer9 *vb)
{

    do {
        ((ReleaseFn)VTABLE(vb)[0x08 / 4])((void *)vb);
        vb = NULL;
    } while (*(volatile int *)&alwaysfails != 0);
}

void *R_AllocStaticIndexBuffer(IDirect3DIndexBuffer9 **ib, int sizeInBytes)
{
    void *indexBufferData;
    byte *dxPtr;
    void *device;
    void **devVtable;
    HRESULT hr;

    dxPtr = (byte *)imp_dx;
    device = *(void **)(dxPtr + 8);
    devVtable = VTABLE(device);

    hr = ((CreateIndexBufferFn)devVtable[0x6c / 4])(device, sizeInBytes, 8, 0x65, 0, ib, NULL);
    if (hr < 0) {
        return NULL;
    }

    hr = ((BufferLockFn)VTABLE(*ib)[0x2c / 4])((void *)*ib, 0, 0, &indexBufferData, 0);
    if (hr < 0) {

        ((ReleaseFn)VTABLE(*ib)[0x08 / 4])((void *)*ib);
        return NULL;
    }

    return indexBufferData;
}

void R_FinishStaticIndexBuffer(IDirect3DIndexBuffer9 *ib)
{

    do {
        ((BufferUnlockFn)VTABLE(ib)[0x30 / 4])((void *)ib);
    } while (*(volatile int *)&alwaysfails != 0);
}

void R_FreeStaticIndexBuffer(IDirect3DIndexBuffer9 *ib)
{

    do {
        ((ReleaseFn)VTABLE(ib)[0x08 / 4])((void *)ib);
        ib = NULL;
    } while (*(volatile int *)&alwaysfails != 0);
}
