#if COD2_APPLE_SDK
#include "lp64_shader_state.h"
extern GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *shader);

static int MacShader_Diagnostics(void)
{
    static int enabled = -1;
    if (enabled < 0) enabled = getenv("COD2_MAC_SHADER_DIAGNOSTICS") != NULL;
    return enabled;
}

static void MacShader_CheckGL(const char *operation)
{
    if (MacShader_Diagnostics()) {
        GLenum error = glGetError();
        static int reports;
        if (error && reports++ < 48)
            fprintf(stderr, "Native ARB %s GL error: 0x%x\n", operation, error);
    }
}

static int MacShader_DrawEnabled(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("COD2_MAC_SHADER_CACHE") != NULL && CDirect3DDevice_UsePrograms();
    return enabled;
}

static UINT MacShader_Attribute(const D3DVERTEXELEMENT9 *element)
{
    switch (element->Usage) {
    case 0: case 9: return 0;
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    case 5: return 8 + element->UsageIndex;
    case 6: return 7;
    case 7: return 6;
    case 10: return 5 + element->UsageIndex;
    default: return 16;
    }
}

/* Bind the actual D3D declaration/samplers. The fixed-function approximation
 * below replaces texture units and omits tangent/binormal inputs. */
static HRESULT MacShader_DrawIndexed(DeviceImpl *dev, INT baseVertex, UINT minVertex, UINT vertexCount,
                                      UINT startIndex, UINT primitiveCount)
{
    const CDirect3DVertexDeclarationImpl *decl = (const void *)g_activeVertexDeclaration;
    const CDirect3DIndexBufferClean *indices = (const void *)dev->indexBuffer;
    GLuint program = g_activeVertexShader ? CDirect3DVertexShader_GetProgramId((const void *)g_activeVertexShader) : 0;
    UINT enabled = 0;
    void **pixelVtable;

    if (!program || !dev->pixelShader || !decl || !indices || !indices->data ||
        indices->indexSizeBytes != 2 ||
        ((uint64_t)startIndex + (uint64_t)primitiveCount * 3) * 2 > indices->lengthBytes)
        return (HRESULT)0x8876086c;
    if (MacShader_Diagnostics()) {
        static int draws;
        if (draws++ < 8)
            fprintf(stderr, "Native ARB draw: program=%u attributes=%u texture0=%u target=%x triangles=%u\n",
                    program, decl->elementCount, CDirect3DDevice_GetTextureGLId(g_boundTextures[0]),
                    CDirect3DDevice_GetTextureTarget(g_boundTextures[0]), primitiveCount);
    }
    MacShader_CheckGL("before draw");
#if defined(COD2_X64)
    int nativePackets = 1;
    for (UINT i = 0; i + 1 < decl->elementCount; ++i)
        if (decl->elements[i].Type > 7) nativePackets = 0;
    if (nativePackets) {
        *(Bool *)imp__ZN7COpenGL9sDrawFlagE = 1;
        DeviceStreamsNative *state = CDirect3DDevice_StreamsNative((const void *)dev);
        state->low = (UINT32)baseVertex + minVertex;
        state->high = state->low + vertexCount - 1;
        if (*(const Bool *)imp_g_special) {
            const UINT16 *element = (const void *)(indices->data + startIndex * 2);
            UINT low = 0xffffffff, high = 0;
            for (UINT i = 0; i < primitiveCount * 3; ++i) {
                if (element[i] < low) low = element[i];
                if (element[i] > high) high = element[i];
            }
            state->low = (UINT32)baseVertex + low;
            state->high = (UINT32)baseVertex + high;
        }
        glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
        glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        CDirect3DDevice_ValidateTransformation((const void *)dev, baseVertex, state->high - state->low + 1,
                                             indices->data + startIndex * 2, primitiveCount * 3);
        for (UINT i = 0; i < 16; ++i)
            if (state->programmable[i].used) enabled |= 1u << i;
        CDirect3DDevice_mNeedsTransformationValidation = 0;
    } else {
#endif
    glBindVertexArrayAPPLE(0);
#if defined(COD2_X64)
    *(GLint *)imp_g_VAOID = -1;
#endif
    glBindBufferARB(GL_ARRAY_BUFFER_ARB, 0);
    glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_NORMAL_ARRAY);
    for (UINT i = 0; i < 8; ++i) {
        glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glClientActiveTextureARB(GL_TEXTURE0_ARB);
    MacShader_CheckGL("clear arrays");
    for (UINT i = 0; i < 16; ++i)
        glDisableVertexAttribArrayARB(i);

    for (UINT i = 0; i + 1 < decl->elementCount; ++i) {
        const D3DVERTEXELEMENT9 *element = &decl->elements[i];
        UINT attribute = MacShader_Attribute(element);
        const CDirect3DVertexBufferClean *buffer;
        GLint components;
        GLenum type;
        GLboolean normalized = GL_FALSE;
        const byte *pointer;
        UINT stride;

        if (attribute >= 16 || element->Stream >= 16 || !dev->streams[element->Stream])
            continue;
        buffer = (const void *)dev->streams[element->Stream];
        stride = dev->streamStrides[element->Stream];
#if defined(COD2_X64)
        pointer = buffer->data + dev->streamOffsets[element->Stream] +
            (intptr_t)baseVertex * stride + element->Offset;
#else
        pointer = buffer->data + dev->streamOffsets[element->Stream] + baseVertex * stride + element->Offset;
#endif
        if (element->Type <= 3) {
            components = element->Type + 1;
            type = GL_FLOAT;
        } else if (element->Type == 4 || element->Type == 5 || element->Type == 8) {
            components = 4;
            type = GL_UNSIGNED_BYTE;
            normalized = element->Type != 5;
            if (element->Type == 4) {
                const D3DVERTEXELEMENT9 *position = CDirect3DDevice_FindVertexElement(element->Stream, 0, 0);
                int order = MacShader_ColorByteOrder(stride, element->Offset, position && position->Type == 3 ? 4 : 3);
                /* NULL means the interleaved array is already RGBA (or nothing is
                 * indexed): keep the original pointer and stride. */
                const byte *converted = CDirect3DDevice_ConvertColorArray(
#if defined(COD2_X64)
                    buffer->data + dev->streamOffsets[element->Stream] + (intptr_t)baseVertex * stride,
#else
                    buffer->data + dev->streamOffsets[element->Stream] + baseVertex * stride,
#endif
                    stride, element->Offset, minVertex + vertexCount, order,
                    (const unsigned short *)((const byte *)indices->data + startIndex * 2), primitiveCount * 3);
                if (converted) {
                    pointer = converted;
                    stride = 0;
                }
            }
        } else if (element->Type == 6 || element->Type == 7 || element->Type == 9 || element->Type == 10) {
            components = element->Type == 6 || element->Type == 9 ? 2 : 4;
            type = GL_SHORT;
            normalized = element->Type >= 9;
        } else if (element->Type == 11 || element->Type == 12) {
            components = element->Type == 11 ? 2 : 4;
            type = GL_UNSIGNED_SHORT;
            normalized = GL_TRUE;
        } else {
            return (HRESULT)0x8876086c;
        }
        glVertexAttribPointerARB(attribute, components, type, normalized, stride, pointer);
        glEnableVertexAttribArrayARB(attribute);
        enabled |= 1u << attribute;
    }
#if defined(COD2_X64)
    }
#endif
    for (UINT i = 1; i < 16; ++i) {
        if (!(enabled & (1u << i)))
            glVertexAttrib4fARB(i, 0, 0, 0, 1);
    }
    MacShader_CheckGL("vertex declaration");

#if defined(COD2_X64)
    if (nativePackets) {
        CDirect3DDevice_ValidateRasterization((const void *)dev, baseVertex,
                                             (UINT32)baseVertex + dev->streamState->high - dev->streamState->low + 1);
        CDirect3DDevice_mNeedsRasterizationValidation = 0;
    } else {
#endif
    for (UINT i = 0; i < 16; ++i) {
        IDirect3DBaseTexture9 *texture = g_boundTextures[i];
        GLenum target = CDirect3DDevice_GetTextureTarget(texture);
        glActiveTextureARB(GL_TEXTURE0_ARB + i);
        MacShader_CheckGL("texture unit");
        CDirect3DDevice_UpdateTextureIfNeeded(texture);
        MacShader_CheckGL("texture upload");
        GLuint textureId = CDirect3DDevice_GetTextureGLId(texture);
        glBindTexture(target, textureId);
        if (MacShader_Diagnostics()) {
            GLenum error = glGetError();
            static int bindReports;
            if (error && bindReports++ < 16)
                fprintf(stderr, "Native texture bind: unit=%u target=%x texture=%p id=%u error=%x\n", i, target, texture, textureId, error);
        }
        if (texture) {
#if defined(COD2_X64)
            CDirect3DDevice_ApplySamplerState(dev, i, target);
#else
            CDirect3DDevice_ApplySamplerState(i, target);
#endif
            MacShader_CheckGL("sampler state");
        }
    }
#if defined(COD2_X64)
    }
#endif
    glActiveTextureARB(GL_TEXTURE0_ARB);
    MacShader_CheckGL("textures");
    glEnable(GL_VERTEX_PROGRAM_ARB);
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, program);
    pixelVtable = *(void ***)dev->pixelShader;
    ((void (*)(const void *))pixelVtable[7])(dev->pixelShader);
    MacShader_CheckGL("programs");

#if defined(COD2_X64)
    if (dev->zEnable && !dev->depthStencilDetached) {
#else
    if (dev->zEnable) {
#endif
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(CDirect3DDevice_MapCompareFunc(dev->zFunc));
    } else {
        glDisable(GL_DEPTH_TEST);
    }
#if defined(COD2_X64)
    glDepthMask(dev->zWriteEnable && !dev->depthStencilDetached);
#else
    glDepthMask(dev->zWriteEnable != 0);
#endif
    glColorMask((dev->colorWriteEnable & 1) != 0, (dev->colorWriteEnable & 2) != 0,
                (dev->colorWriteEnable & 4) != 0, (dev->colorWriteEnable & 8) != 0);
    if (g_alphaTestEnable) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(CDirect3DDevice_MapCompareFunc(dev->alphaFuncVal), dev->alphaRef);
    } else {
        glDisable(GL_ALPHA_TEST);
    }
    if (dev->alphaBlendEnable) {
        glEnable(GL_BLEND);
        glBlendFuncSeparateEXT(CDirect3DDevice_MapBlendFunc(dev->srcBlend), CDirect3DDevice_MapBlendFunc(dev->destBlend),
                                CDirect3DDevice_MapBlendFunc(dev->separateAlphaBlendEnable ? dev->srcBlendAlpha : dev->srcBlend),
                                CDirect3DDevice_MapBlendFunc(dev->separateAlphaBlendEnable ? dev->destBlendAlpha : dev->destBlend));
    } else {
        glDisable(GL_BLEND);
    }
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    MacShader_ApplyRasterEquations(dev->cullMode, dev->blendOp,
                                  dev->separateAlphaBlendEnable ? dev->alphaSrcBlend : dev->blendOp);
    MacShader_CheckGL("render state");
#if defined(COD2_X64)
    if (nativePackets) {
        DeviceStreamsNative *state = dev->streamState;
        UINT count = primitiveCount * 3;
        const byte *index = indices->data + startIndex * 2;
        while (count && *(Bool *)imp__ZN7COpenGL9sDrawFlagE) {
            UINT batch = count > 65534 ? 65535 : count;
            glDrawRangeElements(GL_TRIANGLES, state->low, state->high, batch, GL_UNSIGNED_SHORT, index);
            index += batch * 2;
            count -= batch;
        }
    } else {
#endif
#if defined(COD2_X64)
    CDirect3DDevice_SubmitIndexedNative(dev, indices->data + startIndex * 2, primitiveCount * 3);
#else
    glDrawElements(GL_TRIANGLES, primitiveCount * 3, GL_UNSIGNED_SHORT,
                    indices->data + startIndex * 2);
#endif
#if defined(COD2_X64)
    }
#endif
    if (MacShader_Diagnostics()) {
        GLenum error = glGetError();
        static int reports;
        if (error && reports++ < 16)
            fprintf(stderr, "Native ARB draw GL error: 0x%x, vertex program %u\n", error, program);
    }
#if defined(COD2_X64)
    if (!nativePackets) {
#endif
    for (UINT i = 0; i < 16; ++i)
        glDisableVertexAttribArrayARB(i);
    glDisable(GL_VERTEX_PROGRAM_ARB);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
#if defined(COD2_X64)
    }
#endif
    glActiveTextureARB(GL_TEXTURE0_ARB);
    return 0;
}
#endif
