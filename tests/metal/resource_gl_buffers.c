/* GL buffers have CPU backing only. Execute their real constructors/locks as
 * the byte readback oracle without selecting the Metal dispatch continuation. */
#include "common_types.h"
#include "imports.h"
#include "Mac/Metal/shim_dispatch.h"
#include <assert.h>
#include <string.h>
#undef MAC_METAL_NATIVE
#define MAC_METAL_NATIVE 0
#define CDirect3DVertexBuffer_CDirect3DVertexBuffer ResourceGL_VertexCtor
#define CDirect3DIndexBuffer_CDirect3DIndexBuffer ResourceGL_IndexCtor
#define vtbl_CDirect3DVertexBuffer ResourceGL_VertexTable
#define vtbl_CDirect3DIndexBuffer ResourceGL_IndexTable
#include "Mac/DirectX_9/CDirect3DVertexBuffer.c"
#include "Mac/DirectX_9/CDirect3DIndexBuffer.c"

void ResourceGL_BufferRead(unsigned int format, const unsigned char *source,
                           unsigned int capacity, unsigned int offset,
                           unsigned int length, unsigned char *out)
{
    void *locked = NULL;
    if (format) {
        CDirect3DIndexBufferClean buffer;
        ResourceGL_IndexCtor(&buffer, capacity, format, 0x208, 0);
        memcpy(buffer.data, source, capacity);
        assert(!CDirect3DIndexBuffer_Lock((void *)&buffer, offset, length, &locked, 0x1001));
        memcpy(out, locked, length);
        assert(!CDirect3DIndexBuffer_Unlock((void *)&buffer));
        ZN20CDirect3DIndexBufferD1Ev((void *)&buffer);
    } else {
        CDirect3DVertexBufferClean buffer;
        ResourceGL_VertexCtor(&buffer, capacity, 0x208, 0);
        memcpy(buffer.data, source, capacity);
        assert(!CDirect3DVertexBuffer_Lock((void *)&buffer, offset, length, &locked, 0x1001));
        memcpy(out, locked, length);
        assert(!CDirect3DVertexBuffer_Unlock((void *)&buffer));
        ZN21CDirect3DVertexBufferD1Ev(&buffer);
    }
}
