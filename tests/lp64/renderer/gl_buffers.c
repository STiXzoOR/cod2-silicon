#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "Mac/DirectX_9/lp64_buffers.h"

_Static_assert(sizeof(GLuint) == 4, "GL object names have four-byte slots");
_Static_assert(sizeof(GLint) == 4, "GL output integers have four-byte slots");
_Static_assert(sizeof(GLenum) == 4 && sizeof(GLsizei) == 4, "GL scalar ABI");
_Static_assert(sizeof(GLsizeiptr) == sizeof(void *), "GL buffer sizes retain pointer width");
_Static_assert(sizeof(GLintptr) == sizeof(void *), "GL buffer offsets retain pointer width");
_Static_assert(sizeof(DWORD) == 4 && sizeof(D3DCOLOR) == 4, "D3D words and packed vertex colors");
_Static_assert(sizeof(ULONG) == 4 && sizeof(HRESULT) == 4, "D3D reference counts and signed results");

#include "Mac/DirectX_9/CDirect3DVertexBuffer.c"
#include "Mac/DirectX_9/CDirect3DIndexBuffer.c"
#include "Mac/DirectX_9/CMemoryBuffer.c"

int main(void)
{
    CDirect3DVertexBufferClean vb;
    CDirect3DIndexBufferClean ib;
    CMemoryBufferImpl buffer;
    void *locked;

    CDirect3DVertexBuffer_CDirect3DVertexBuffer((void *)&vb, 64, 0, 0);
    CDirect3DVertexBuffer_Lock((void *)&vb, 17, 4, &locked, 0);
    assert(locked == vb.data + 17);
    assert((uintptr_t)locked > UINT32_MAX);
    ZN21CDirect3DVertexBufferD1Ev(&vb);
    CDirect3DIndexBuffer_CDirect3DIndexBuffer((void *)&ib, 64, D3DFMT_INDEX16, 0, 0);
    CDirect3DIndexBuffer_Lock((void *)&ib, 6, 4, &locked, 0);
    assert(locked == ib.data + 6 && ib.indexSizeBytes == 2);
    ZN20CDirect3DIndexBufferD1Ev((void *)&ib);

    CMemoryBuffer_CMemoryBuffer((void *)&buffer, 65);
    assert(buffer.vptr == vtbl_CMemoryBuffer);
    assert(((uintptr_t)buffer.data & 31) == 0);
    CMemoryBuffer_FreeLater((void *)&buffer, 1);
    assert(CMemoryBuffer_sMemoryDesignatedForDelayedFree == 65);
    CMemoryBuffer_Update();
    assert(CMemoryBuffer_sMemoryDesignatedForDelayedFree == 65);
    CMemoryBuffer_Update();
    assert(CMemoryBuffer_sMemoryDesignatedForDelayedFree == 0);
    CMemoryBuffer_Recreate((void *)&buffer);
    CMemoryBuffer_FreeLater((void *)&buffer, 10);
    CMemoryBuffer_Reset();
    assert(CMemoryBuffer_sMemoryDesignatedForDelayedFree == 0);
    puts("renderer GL ABI, buffer locks and delayed-free list: passed");
    return 0;
}
