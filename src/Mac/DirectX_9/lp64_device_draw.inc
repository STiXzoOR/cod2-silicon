/* Native indexed-draw orchestration. Kept separate from the preserved i386
 * source and from the explicit source-shader approximation below. */
#if defined(COD2_X64)
extern UINT32 MacOpenGLUtils_GetElementCount(GLenum, UINT32);
extern void GetHighAndLowIndices(UINT32 *, UINT32 *, const UINT16 *, UINT32);
extern unsigned int COpenGL_SetVertexProgram(const COpenGL *, const COpenGLVertexProgram *);
extern GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *);
extern const COpenGLVertexProgram *CDirect3DVertexShader_GetProgramInfo(const CDirect3DVertexShader *);
extern Bool CDirect3DVertexShader_GetDrawPointFlag(const CDirect3DVertexShader *);
extern void CDirect3DVertexBuffer_SetFence(const CDirect3DVertexBuffer *, const void *, UINT32,
                                          const CDirect3DVertexBuffer *);
extern intptr_t g_Low, g_High;

static void CDirect3DDevice_SubmitIndexedNative(DeviceImpl *dev, const byte *indices, UINT32 count)
{
    DeviceStreamsNative *state = dev->streamState;
    /* Mac 0x1233c-0x12397: raw indices and base-adjusted range hints are
     * deliberately separate; a 65535-element chunk is exactly 21845 triangles. */
    while (count) {
        UINT32 batch = count > 65534 ? 65535 : count;
        glDrawRangeElements(0x4, state->low, state->high, batch, 0x1403, indices);
        indices += batch * 2;
        count -= batch;
    }
}

static HRESULT CDirect3DDevice_DrawApproximateNative(const CDirect3DDevice *_this,
    D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex,
    UINT NumVertices, UINT startIndex, UINT primCount)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    byte *vbData, *ibData, *vertBase;
    UINT stride, offset;
    int indexCount;
    GLenum glMode;
    GLenum stage0Target;
    r_backEndGlobals_t *backEndState;
    int is2D;
    int usesLightmap;
    int positionOffset;
    int positionComponents;
    int colorOffset;
    int colorByteOrder;
    int texOffset;
    int lightmapOffset;
    int normalOffset;
    const D3DVERTEXELEMENT9 *element;

    (void)PrimitiveType;

    if (!dev->streams[0] || !dev->indexBuffer)
        return 0;
#if defined(COD2_X64)
    /* The fallback writes client arrays directly. Keep those writes in VAO0,
     * so a later packet hit can still rely on its retained GL array state. */
    glBindVertexArrayAPPLE(0);
    *(GLint *)imp_g_VAOID = -1;
#endif

#if defined(COD2_X64)
    vbData = ((const CDirect3DVertexBufferClean *)dev->streams[0])->data;
#else
    vbData = *(byte **)((byte *)dev->streams[0] + sizeof(void *) + 8);   /* VB.data: vtable+refCount+lengthBytes (x86 was +12) */
#endif
    if (!vbData)
        return 0;
    offset = dev->streamOffsets[0];
    stride = dev->streamStrides[0];
    backEndState = (r_backEndGlobals_t *)imp_backEnd;
    is2D = backEndState && backEndState->projection2D;
    usesLightmap = !is2D && stride == 0x44;
    stage0Target = 0x0DE1;

    positionOffset = 0;
    positionComponents = 3;
    colorOffset = stride == 0x44                       ? 0x18
                  : stride == 0x40                     ? 0x1c
                  : stride == 0x24                     ? 0x18
                  : (stride == 0x18 || stride == 0x20) ? 0x0c
                                                       : -1;
    texOffset = stride == 0x44   ? 0x1c
                : stride == 0x40 ? 0x20
                : stride == 0x24 ? 0x1c
                                 : -1;
    lightmapOffset = stride == 0x44 ? 0x24 : -1;
    normalOffset = stride == 0x44   ? 0x0c
                   : stride == 0x40 ? 0x10
                   : stride == 0x24 ? 0x0c
                                    : -1;

    element = CDirect3DDevice_FindVertexElement(0, 0, 0);
    if (!element)
        element = CDirect3DDevice_FindVertexElement(0, 9, 0);
    if (element) {
        positionOffset = element->Offset;
        positionComponents = element->Type == 3 ? 4 : 3;
    }
    element = CDirect3DDevice_FindVertexElement(0, 10, 0);
    if (element)
        colorOffset = element->Offset;
    element = CDirect3DDevice_FindVertexElement(0, 5, 0);
    if (element)
        texOffset = element->Offset;
    element = CDirect3DDevice_FindVertexElement(0, 5, 1);
    if (element)
        lightmapOffset = element->Offset;
    element = CDirect3DDevice_FindVertexElement(0, 3, 0);
    if (element)
        normalOffset = element->Offset;

#if defined(COD2_X64)
    colorByteOrder = MacShader_ColorByteOrder(stride, colorOffset, positionComponents);
#else
    if (stride == 0x44 || stride == 0x20 || stride == 0x18)
        colorByteOrder = COLOR_BYTES_RGBA;
    else if (stride == 0x40 && positionComponents == 3)
        colorByteOrder = COLOR_BYTES_ARGB;
    else
        colorByteOrder = COLOR_BYTES_BGRA;
#endif

#if defined(COD2_X64)
    ibData = ((const CDirect3DIndexBufferClean *)dev->indexBuffer)->data;
#else
    ibData = *(byte **)((byte *)dev->indexBuffer + sizeof(void *) + 8);   /* IB.data (x86 was +12) */
#endif
    if (!ibData)
        return 0;

    vertBase = vbData + offset + (intptr_t)BaseVertexIndex * stride;

    {
        extern void glBindVertexArray(unsigned int);
        glBindVertexArray(0);
    }
    glActiveTextureARB(GL_TEXTURE0_ARB);
    glClientActiveTextureARB(GL_TEXTURE0_ARB);
    if (!usesLightmap) {
        CDirect3DDevice_DisableExtraTextureUnits();
    }

    {

        {
#if defined(COD2_X64)
            extern GLuint CDirect3DVertexShader_GetProgramId(const CDirect3DVertexShader *shader);
            GLuint vsId = (!is2D && g_activeVertexShader)
                              ? CDirect3DVertexShader_GetProgramId((const CDirect3DVertexShader *)g_activeVertexShader)
                              : 0;
#else
            GLuint vsId = (!is2D && g_activeVertexShader)
                              ? *(GLuint *)((char *)g_activeVertexShader + 4)
                              : 0;
#endif
#if defined(COD2_X64)
            int useProg = vsId && dev->pixelShader && CDirect3DDevice_UsePrograms();
#else
            int useProg = vsId && dev->pixelShader && getenv("D3D_PROG");
#endif
            if (useProg) {
                glEnable(0x8620 );
                glBindProgramARB(0x8620, vsId);
                {
                    void **vt = *(void ***)dev->pixelShader;
                    ((void (*)(const void *))vt[7])(dev->pixelShader);
                }
            } else {

                glDisable(0x8804);
                glBindProgramARB(0x8804, 0);
                glDisable(0x8620);
                glBindProgramARB(0x8620, 0);
            }
        }
#if defined(COD2_X64)
        if (!is2D && dev->zEnable && !dev->depthStencilDetached) {
#else
        if (!is2D && dev->zEnable) {
#endif
            glEnable(0x0B71);
            glDepthFunc(CDirect3DDevice_MapCompareFunc(dev->zFunc));
            glDepthMask(dev->zWriteEnable ? 1 : 0);
        } else {
            glDisable(0x0B71);
            glDepthMask(0);
        }
        glDisable(0x0B44);

        glDisable(0x0B60);
        glDisable(0x0B50);
        if (g_alphaTestEnable) {
            glEnable(0x0BC0);
            glAlphaFunc(CDirect3DDevice_MapCompareFunc(dev->alphaFuncVal), dev->alphaRef);
        } else {
            glDisable(0x0BC0);
        }
        glColorMask(
            (dev->colorWriteEnable & 1) != 0,
            (dev->colorWriteEnable & 2) != 0,
            (dev->colorWriteEnable & 4) != 0,
            (dev->colorWriteEnable & 8) != 0);
        if (is2D) {
            glColorMask(1, 1, 1, 1);
        }
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

        if (!is2D) {

            const GfxViewParms *viewParms = backEndState ? backEndState->viewParms : NULL;
            if (viewParms) {
                float world[16];
                float view[16];
                float modelView[16];
                float proj[16];
                GfxCodeMatrices *activeMatrices = NULL;
                float aspect = (dev->viewportW && dev->viewportH)
                                   ? ((float)dev->viewportW / (float)dev->viewportH)
                                   : (640.0f / 480.0f);

                (void)aspect;
                if (backEndState && backEndState->codeMatrixStackLevel >= 0 &&
                    backEndState->codeMatrixStackLevel < 3) {
                    activeMatrices = &backEndState->codeMatrixStack[backEndState->codeMatrixStackLevel];
                }
                if (activeMatrices) {
                    memcpy(world, &activeMatrices->world.matrix[0], sizeof(world));
                    memcpy(view, &activeMatrices->view.matrix[0], sizeof(view));
                    memcpy(proj, &activeMatrices->projection.matrix[0], sizeof(proj));
                } else {
                    D3DMATRIX *activeWorld = RB_GetActiveWorldMatrix();
                    CDirect3DDevice_LoadIdentityMatrix(world);
                    if (activeWorld)
                        memcpy(world, activeWorld, sizeof(world));
                    memcpy(view, &viewParms->viewMatrix, sizeof(view));
                    memcpy(proj, &viewParms->projectionMatrix, sizeof(proj));
                }

                view[2] = -view[2];
                view[6] = -view[6];
                view[10] = -view[10];
                view[14] = -view[14];
                D3DXMatrixMultiply(modelView, world, view);

                MacOpenGLUtils_ConvertD3DProjectionMatrixToOpenGL(
                    proj,
                    dev->viewportW ? (float)dev->viewportW : 640.0f,
                    dev->viewportH ? (float)dev->viewportH : 480.0f);

                glMatrixMode(0x1701);
                glLoadMatrixf(proj);
                glMatrixMode(0x1700);
                glLoadMatrixf(modelView);
            } else {
                glMatrixMode(0x1701);
                glLoadIdentity();
                glScalef(1.0f / 4000.0f, 1.0f / 4000.0f, 1.0f / 4000.0f);
                glMatrixMode(0x1700);
                glLoadIdentity();
            }
        } else {

            float orthoW = dev->viewportW ? (float)dev->viewportW : 640.0f;
            float orthoH = dev->viewportH ? (float)dev->viewportH : 480.0f;
            float ortho[16] = {
                2.0f / orthoW, 0, 0, 0,
                0, -2.0f / orthoH, 0, 0,
                0, 0, -1.0f, 0,
                -1.0f, 1.0f, 0, 1.0f
            };
            glMatrixMode(0x1701);
            glLoadMatrixf(ortho);
            glMatrixMode(0x1700);
            glLoadIdentity();
        }
    }

    if (1)
    {
        extern void glBindTexture(unsigned int, unsigned int);
        {
            extern void glBindTexture(unsigned int, unsigned int);
            DWORD colorOp = g_textureStageState[0][D3DTSS_COLOROP];
            unsigned int glTexID;
            GfxImage *materialImage = NULL;

            if (is2D || stride != 0x44) {
                CDirect3DDevice_UpdateTextureIfNeeded(g_boundTextures[0]);
                glTexID = CDirect3DDevice_GetTextureGLId(g_boundTextures[0]);
                stage0Target = CDirect3DDevice_GetTextureTarget(g_boundTextures[0]);
                if (!glTexID) {
                    glTexID = g_prebind_texID;
                    stage0Target = g_prebind_texTarget ? g_prebind_texTarget : 0x0DE1;
                }
            } else {
                glTexID = 0;
            }

            if (!is2D && stride != 0x44) {
                extern void *imp_tess;
                const materialCommands_t *tess = (const materialCommands_t *)imp_tess;
                const Material *mmat = tess->material;
                if (mmat) {
                    GfxImage *mimg = CDirect3DDevice_SelectMaterialColorImage(mmat);
                    unsigned int mtex = CDirect3DDevice_GetImageGLId(mimg);
                    if (mtex) {
                        glTexID = mtex;
                        stage0Target = CDirect3DDevice_GetImageTextureTarget(mimg);
                    }
                }
            }
            if (!glTexID && usesLightmap) {
                extern void *imp_tess;
                const materialCommands_t *tess = (const materialCommands_t *)imp_tess;
                const Material *mat = tess->material;
                materialImage = CDirect3DDevice_SelectMaterialColorImage(mat);
                glTexID = CDirect3DDevice_GetImageGLId(materialImage);
                stage0Target = CDirect3DDevice_GetImageTextureTarget(materialImage);
            }
            if (glTexID && colorOp != D3DTOP_DISABLE) {
                CDirect3DDevice_BindTextureTarget(stage0Target, glTexID);
#if defined(COD2_X64)
                CDirect3DDevice_ApplySamplerState(dev, 0, stage0Target);
#else
                CDirect3DDevice_ApplySamplerState(0, stage0Target);
#endif
                if (usesLightmap) {

                    CDirect3DDevice_ApplyTextureStageState(0);

                    {
                        extern void *imp_tess;
                        extern GfxWorld s_world;
                        const materialCommands_t *tess = (const materialCommands_t *)imp_tess;
                        int lmapIndex = tess->lmapIndex;
                        int lmapAllowed = !dev->alphaBlendEnable || dev->destBlend == 6;
                        CDirect3DDevice_DisableExtraTextureUnits();
                        if (lmapAllowed && stage0Target != GL_TEXTURE_CUBE_MAP && lmapIndex >= 0 && lmapIndex < 31 && s_world.lightmaps) {
                            GfxImage *lmapImg = s_world.lightmaps[lmapIndex][0];
                            if (lmapImg) {
                                unsigned int lmTexID = CDirect3DDevice_GetImageGLId(lmapImg);
                                if (lmTexID) {
                                    glActiveTextureARB(GL_TEXTURE1_ARB);
                                    glDisable(GL_TEXTURE_CUBE_MAP);
                                    glEnable(0x0DE1);
                                    glBindTexture(0x0DE1, lmTexID);

                                    glMatrixMode(0x1702 );
                                    glLoadIdentity();
                                    glMatrixMode(0x1700 );
#if defined(COD2_X64)
                                    CDirect3DDevice_ApplySamplerState(dev, 1, 0x0DE1);
#else
                                    CDirect3DDevice_ApplySamplerState(1, 0x0DE1);
#endif
                                    if (g_textureStageState[1][D3DTSS_COLOROP] != D3DTOP_DISABLE) {
                                        CDirect3DDevice_ApplyTextureStageState(1);
                                    } else {

                                        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, 0x2100);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, CDirect3DDevice_LightmapScale());
                                        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, 0x1702);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, 0x0300);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PREVIOUS);
                                        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, 0x0300);
                                    }

                                    glTexParameteri(0x0DE1, 0x2801 , 0x2601 );
                                    glTexParameteri(0x0DE1, 0x2800 , 0x2601 );

                                    glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, 0x1E01 );
                                    glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
                                    glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, 0x0302 );
                                    glTexEnvi(GL_TEXTURE_ENV, 0x0D1C , 1);

                                    glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE,
                                              dev->alphaBlendEnable ? 1 : CDirect3DDevice_LightmapScale());
                                    glActiveTextureARB(GL_TEXTURE0_ARB);
                                }
                            }
                        }
                    }
                } else {

                    CDirect3DDevice_ApplyTextureStageState(0);
                }
            } else {
                glDisable(0x0DE1);
                glDisable(GL_TEXTURE_CUBE_MAP);
            }
        }
    }

    if (dev->alphaBlendEnable) {
        glEnable(0x0BE2);
        glBlendFunc(CDirect3DDevice_MapBlendFunc(dev->srcBlend),
                    CDirect3DDevice_MapBlendFunc(dev->destBlend));
    } else {
        glDisable(0x0BE2);
    }

    glEnableClientState(0x8074);
    glVertexPointer(positionComponents, 0x1406 , stride,
                    vertBase + positionOffset);

    {
        if (usesLightmap) {
            const byte *colors = CDirect3DDevice_ConvertColorArray(
                vertBase, stride, colorOffset, MinVertexIndex + NumVertices,
#if defined(COD2_X64)
                colorByteOrder, (const unsigned short *)(ibData + startIndex * 2), primCount * 3);
#else
                colorByteOrder);
#endif

            glEnableClientState(0x8076);
            if (colors)
                glColorPointer(4, 0x1401 , 0, colors);
            else
                glColorPointer(4, 0x1401 , stride,
                               vertBase + colorOffset);

            if (lightmapOffset >= 0) {
                glClientActiveTextureARB(0x84C1);
                glEnableClientState(0x8078);
                glTexCoordPointer(2, 0x1406 , stride,
                                  vertBase + lightmapOffset);
                glClientActiveTextureARB(0x84C0);
            }
        } else if (colorOffset >= 0) {
            const byte *colors = CDirect3DDevice_ConvertColorArray(
                vertBase, stride, colorOffset, MinVertexIndex + NumVertices,
#if defined(COD2_X64)
                colorByteOrder, (const unsigned short *)(ibData + startIndex * 2), primCount * 3);
#else
                colorByteOrder);
#endif

            glEnableClientState(0x8076);
            if (colors)
                glColorPointer(4, 0x1401 , 0, colors);
            else
                glColorPointer(4, 0x1401 , stride, vertBase + colorOffset);
        }
        if (texOffset >= 0) {
            glEnableClientState(0x8078);
            if (stage0Target == GL_TEXTURE_CUBE_MAP) {
                if (normalOffset >= 0)
                    glTexCoordPointer(3, 0x1406 , stride,
                                      vertBase + normalOffset);
                else
                    glTexCoordPointer(2, 0x1406 , stride, vertBase + texOffset);
            } else {
                glTexCoordPointer(2, 0x1406 , stride, vertBase + texOffset);
            }
        }
    }

    glActiveTextureARB(GL_TEXTURE0_ARB);
    glMatrixMode(0x1702);
    if (g_textureStageState[0][D3DTSS_TEXTURETRANSFORMFLAGS] != D3DTTFF_DISABLE)
        glLoadMatrixf(dev->texMatrix);
    else
        glLoadIdentity();
    glMatrixMode(0x1700);

    {
        extern void glDrawElements(unsigned int, int, unsigned int, const void *);
        indexCount = primCount * 3;

        CDirect3DDevice_SubmitIndexedNative(dev, ibData + startIndex * 2, indexCount);
    }

    glDisableClientState(0x8074);
    glDisableClientState(0x8076);
    glDisableClientState(0x8078);
    CDirect3DDevice_DisableExtraTextureUnits();

    return 0;
}

/* Mac 0x12143-0x121eb: only stream zero is AddRef/Released. The position
 * declaration may name a different buffer, but still adds stream zero's offset. */
static void CDirect3DDevice_FenceIndexedNative(const CDirect3DDevice *_this,
    INT base, UINT32 minVertex, UINT32 count)
{
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = dev->streamState;
    if (!state->position.pointer && !g_activeVertexShader)
        return;
    CDirect3DVertexBufferClean *stream0 = (void *)dev->streams[0];
    if (!stream0) return;
    ((ULONG (*)(const void *))stream0->vtable[1])(stream0);
    if (!(stream0->usage & 0x208)) {
        if (g_activeVertexShader) {
            const DeviceStreamNative *position = &state->programmable[0];
            const CDirect3DVertexBufferClean *buffer = position->used && position->stream < 16
                ? (const void *)dev->streams[position->stream] : NULL;
            if (buffer && buffer->data) {
                const byte *start = buffer->data + position->offset +
                    (intptr_t)base * position->stride + dev->streamOffsets[0] +
                    (size_t)minVertex * position->stride;
                CDirect3DVertexBuffer_SetFence((const void *)buffer, start,
                    count * position->stride, NULL);
            }
        } else if (state->position.pointer) {
            const byte *start = (const byte *)state->position.pointer +
                (size_t)minVertex * state->position.stride;
            CDirect3DVertexBuffer_SetFence((const void *)stream0, start,
                count * state->position.stride, NULL);
        }
    }
    ((ULONG (*)(const void *))stream0->vtable[2])(stream0);
}

static int CDirect3DDevice_ExtendedDeclarationNative(void)
{
    if (g_currentFVF || !g_activeVertexDeclaration) return 0;
    const CDirect3DVertexDeclarationImpl *decl = (const void *)g_activeVertexDeclaration;
    for (UINT32 i = 0; i + 1 < decl->elementCount; ++i)
        if (decl->elements[i].Type > 7) return 1;
    return 0;
}

/* The i386 validators assume every declared buffer exists. Preserve native
 * defined handling of incomplete declarations before dereferencing providers. */
static int CDirect3DDevice_DrawStreamsValidNative(DeviceImpl *dev)
{
    if (dev->streamState->tokensSet && dev->streamState->tokens[0] == 0xffffffff)
        return 0;
    if (g_currentFVF) return (g_currentFVF & 2) != 0;
    const CDirect3DVertexDeclarationImpl *decl = (const void *)g_activeVertexDeclaration;
    if (!decl) return 0;
    int position = 0;
    for (UINT32 i = 0; i + 1 < decl->elementCount; ++i) {
        const D3DVERTEXELEMENT9 *element = &decl->elements[i];
        if (element->Stream >= 16 || !dev->streams[element->Stream] ||
            !((const CDirect3DVertexBufferClean *)dev->streams[element->Stream])->data)
            return 0;
        if ((element->Usage == 0 || element->Usage == 9) && !element->UsageIndex)
            position = 1;
    }
    return position;
}

static HRESULT CDirect3DDevice_DrawIndexedNative(const CDirect3DDevice *_this,
    D3DPRIMITIVETYPE primitive, INT base, UINT32 minVertex, UINT32 vertexCount,
    UINT32 startIndex, UINT32 primitiveCount)
{
    static const GLenum modes[7] = { 0, 0, 1, 3, 4, 5, 6 };
    DeviceImpl *dev = (DeviceImpl *)_this;
    DeviceStreamsNative *state = CDirect3DDevice_StreamsNative(_this);
    const CDirect3DIndexBufferClean *buffer = (const void *)dev->indexBuffer;
    Bool *draw = (Bool *)imp__ZN7COpenGL9sDrawFlagE;
    HRESULT result = 0;
    CDirect3DDevice_ValidateModelViewMatrix(_this);
#if COD2_APPLE_SDK
    MacTrace_Draw(dev, primitive, base, minVertex, vertexCount, startIndex, primitiveCount);
#endif
    if (dev->_pad0A8 != (UINT32)base) {
        dev->_pad0A8 = (UINT32)base;
        CDirect3DDevice_mNeedsTransformationValidation = 1;
    }
    /* Keep the defined native null/bounds behavior instead of reproducing
     * the original unchecked buffer and primitive-table reads. */
    if (!dev->streams[0] || !((const CDirect3DVertexBufferClean *)dev->streams[0])->data ||
        !buffer || !buffer->data || (UINT32)primitive >= 7)
        goto done;
    GLenum mode = modes[primitive];
    UINT32 elements = MacOpenGLUtils_GetElementCount(mode, primitiveCount);
    /* Original draw always consumes ushort indices, independently of the
     * buffer format recorded by CreateIndexBuffer. Preserve that quirk. */
    if (((uint64_t)startIndex + elements) * 2 > buffer->lengthBytes) {
        result = (HRESULT)0x8876086c;
        goto done;
    }
    const byte *indices = buffer->data + (size_t)startIndex * 2;
    state->low = (UINT32)base + minVertex;
    state->high = (UINT32)base + minVertex + vertexCount - 1;
    if (*(const Bool *)imp_g_special && elements) {
        UINT32 low, high;
        GetHighAndLowIndices(&low, &high, (const void *)indices, elements);
        state->low = (UINT32)base + low;
        state->high = (UINT32)base + high;
    }
    g_Low = state->low;
    g_High = state->high;
    UINT32 count = state->high - state->low + 1;
    /* This store is unconditional in 0x12124, independently of base changes. */
    CDirect3DDevice_mNeedsTransformationValidation = 1;
    if (mode == 4) {
        int programs = CDirect3DDevice_UsePrograms();
        int shaderReplacement = programs && g_activeVertexShader && dev->pixelShader &&
            g_activeVertexDeclaration;
        int approximate = (!programs && (g_activeVertexShader || dev->pixelShader)) ||
            (g_activeVertexShader && !(dev->pixelShader && g_activeVertexDeclaration &&
                CDirect3DVertexShader_GetProgramId((const void *)g_activeVertexShader)));
        int direct = state->directStateDirty || approximate ||
            (g_activeVertexShader && CDirect3DDevice_ExtendedDeclarationNative());
#if !COD2_APPLE_SDK
        shaderReplacement = 0;
#endif
        if (direct) approximate = !shaderReplacement;
        if (!direct) {
            if (!CDirect3DDevice_DrawStreamsValidNative(dev)) goto done;
            if (CDirect3DDevice_mNeedsVertexShaderValidation) {
                if (g_activeVertexShader) {
                    /* Native shader providers can bind outside COpenGL's
                     * legacy capability cache. Query the owning GL context. */
                    if (!glIsEnabled(0x8620)) glEnable(0x8620);
                    COpenGL_SetVertexProgram((const void *)CDirect3DDevice_GL(),
                        CDirect3DVertexShader_GetProgramInfo((const void *)g_activeVertexShader));
                } else if (glIsEnabled(0x8620)) {
                    glDisable(0x8620);
                }
                CDirect3DDevice_mNeedsVertexShaderValidation = 0;
            }
            glBindBufferARB(0x8892, 0);
            glBindBufferARB(0x8893, 0);
            if (CDirect3DDevice_mNeedsTransformationValidation)
                CDirect3DDevice_ValidateTransformation(_this, base, count, indices, elements);
            if (!*draw) goto done;
            if (CDirect3DDevice_mNeedsRasterizationValidation ||
                CDirect3DDevice_mNeedsTransformationValidation) {
                CDirect3DDevice_ValidateRasterization(_this, base, (UINT32)base + count);
                CDirect3DDevice_mNeedsTransformationValidation = 0;
                CDirect3DDevice_mNeedsRasterizationValidation = 0;
            }
            CDirect3DDevice_ValidateLighting(_this);
            if (dev->separateAlphaBlendEnable)
                COpenGL_SetBlendEXT((const void *)CDirect3DDevice_GL(), dev->blendStateChanged,
                    dev->glSrcBlend, dev->glDestBlend, dev->glSrcBlendAlpha, dev->glDestBlendAlpha);
            else
                COpenGL_SetBlend((const void *)CDirect3DDevice_GL(), dev->blendStateChanged,
                    dev->glSrcBlend, dev->glDestBlend);
            dev->blendStateChanged = 0;
        } else {
            /* The licensed-cache opt-in and source-shader approximation are
             * explicit native replacements. Preserve their state policy and
             * collect typed declaration metadata for the common fence tail. */
            if (!*draw) goto done;
            Bool canUseVAO = 0;
            if (g_activeVertexShader) {
                for (UINT32 i = 0; i < 16; ++i) state->programmable[i].used = 0;
                CDirect3DDevice_ParseShaderDeclaration(_this, 0, &canUseVAO);
            } else if (CDirect3DDevice_DrawStreamsValidNative(dev)) {
                state->position.used = state->normal.used = state->color.used = 0;
                for (UINT32 i = 0; i < 8; ++i) state->texCoord[i].used = 0;
                CDirect3DDevice_ParseShaderDeclaration(_this, 1, &canUseVAO);
                CDirect3DDevice_GetStreamPtrs(_this, base);
            } else {
                state->position.pointer = NULL;
            }
        }
        Bool point = g_activeVertexShader &&
            CDirect3DVertexShader_GetDrawPointFlag((const void *)g_activeVertexShader);
        if (point) glPointSize(2.0f);
        if (approximate)
            result = CDirect3DDevice_DrawApproximateNative(_this, primitive, base,
                minVertex, vertexCount, startIndex, primitiveCount);
#if COD2_APPLE_SDK
        else if (direct)
            result = MacShader_DrawIndexed(dev, base, minVertex, vertexCount, startIndex, primitiveCount);
#endif
        else
            CDirect3DDevice_SubmitIndexedNative(dev, indices, elements);
        if (point) glPointSize(1.0f);
        if (direct) {
            /* The direct replacement's server-state restoration is incomplete.
             * Until ResetStreamsNative, retain its established routing for
             * later draws rather than enter cache-only validators with stale
             * cull/fog/light/matrix/combiner state. */
            state->directStateDirty = 1;
            /* Programs and blend functions also bypass these native caches. */
            CDirect3DDevice_GL()->vertexProgram = 0;
            CDirect3DDevice_mNeedsVertexShaderValidation = 1;
            CDirect3DDevice_mNeedsRasterizationValidation = 1;
            dev->blendStateChanged = 1;
        }
    } else if (!*draw) {
        goto done;
    }
    CDirect3DDevice_FenceIndexedNative(_this, base, minVertex, count);
done:
    *draw = 1;
    return result;
}

#endif
