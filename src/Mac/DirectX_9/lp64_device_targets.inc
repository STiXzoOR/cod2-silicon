/* Native implementations of the Mac pixel/copy/target methods. Included before
 * the Device GL cache wrappers: all temporary GL overrides here are raw calls. */
HRESULT CDirect3DDevice_SetViewport(const CDirect3DDevice *, const D3DVIEWPORT9 *);
bool CDirect3DSurface_IsDirty(const CDirect3DSurface *);
void CDirect3DSurface_SetDirty(void *, int);

static void DeviceTargets_DrawBuffer(GLenum buffer)
{
#if COD2_APPLE_SDK
    MacGL_DrawBuffer(buffer);
#else
    glDrawBuffer(buffer);
#endif
}

static void DeviceTargets_ReadBuffer(GLenum buffer)
{
#if COD2_APPLE_SDK
    MacGL_ReadBuffer(buffer);
#else
    glReadBuffer(buffer);
#endif
}

static void DeviceTargets_ColorMask(DWORD mask)
{
    glColorMask(mask & 1, (mask >> 1) & 1, (mask >> 2) & 1, (mask >> 3) & 1);
}

static void DeviceTargets_Capability(GLenum capability, int enabled)
{
    if (enabled) glEnable(capability); else glDisable(capability);
}

static HRESULT DeviceTargets_GetDesc(const void *surface, D3DSURFACE_DESC *desc)
{
    memset(desc, 0, sizeof(*desc));
    return ((HRESULT (*)(const void *, D3DSURFACE_DESC *))(*(void ***)surface)[12])(surface, desc);
}

static void DeviceTargets_Release(void *surface)
{
    if (surface)
        ((ULONG (*)(void *))(*(void ***)surface)[2])(surface);
}

static void DeviceTargets_AddRef(void *surface)
{
    if (surface)
        ((ULONG (*)(void *))(*(void ***)surface)[1])(surface);
}

static HRESULT DeviceTargets_Clear(DeviceImpl *dev, DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    GLbitfield mask = 0;
    int depth = !dev->depthStencilDetached;
    if (flags & 1) {
        if (dev->colorWriteEnable != 15)
            DeviceTargets_ColorMask(15);
        if (CDirect3DDevice_GL()->ditherEnabled)
            glDisable(0x0bd0);
        glClearColor((float)((color >> 16) & 255) / 255.0f,
                     (float)((color >> 8) & 255) / 255.0f,
                     (float)(color & 255) / 255.0f,
                     (float)(color >> 24) / 255.0f);
        mask = 0x4000;
    }
    if (depth && (flags & 2)) {
        if (!dev->zWriteEnable) glDepthMask(1);
        if (!dev->zEnable) glEnable(0x0b71);
        if (dev->zFunc != 8) glDepthFunc(0x0207);
        mask |= 0x0100;
        glClearDepth((double)z);
    }
    if (depth && (flags & 4)) {
        mask |= 0x0400;
        if (dev->stencilWriteMask != 255) glStencilMask(255);
        glClearStencil(stencil);
    }
    D3DSURFACE_DESC desc;
    DeviceTargets_GetDesc(dev->renderTarget, &desc);
    glEnable(0x0c11);
    glScissor(dev->viewportX, desc.Height - (dev->viewportY + dev->viewportH),
              dev->viewportW, dev->viewportH);
    glClear(mask);
    glDisable(0x0c11);
    if (flags & 1) {
        if (dev->colorWriteEnable != 15) DeviceTargets_ColorMask(dev->colorWriteEnable);
        if (CDirect3DDevice_GL()->ditherEnabled) glEnable(0x0bd0);
    }
    if (depth && (flags & 2)) {
        if (!dev->zWriteEnable) glDepthMask(0);
        if (!dev->zEnable) glDisable(0x0b71);
        if (dev->zFunc != 8)
            glDepthFunc(dev->zFunc >= 1 && dev->zFunc <= 8 ? 0x01ff + dev->zFunc : 0);
    }
    if (depth && (flags & 4) && dev->stencilWriteMask != 255)
        glStencilMask(dev->stencilWriteMask);
    return 0;
}

static void DeviceTargets_StartPixel(DeviceImpl *dev)
{
    DeviceTargets_ColorMask(15);
    glStencilMask(0);
    glDisable(0x0bc0);
    glDisable(0x0b71);
    glDisable(0x0be2);
    glDisable(0x0bd0);
    glDisable(0x0b50);
    glDisable(0x0b60);
    for (int unit = (int)dev->maxTextureUnits - 1; unit >= 0; --unit) {
        glActiveTextureARB(0x84c0 + unit);
        glDisable(0x0de1);
        glDisable(0x806f);
        glDisable(0x8513);
    }
    glDisable(0x0b90);
    glDepthMask(0);
    glDepthFunc(0x0207);
    glShadeModel(0x1d00);
    glDisable(0x0b44);
}

static void DeviceTargets_EndPixel(DeviceImpl *dev)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    int depth = !dev->depthStencilDetached;
    if (depth && dev->zWriteEnable) glDepthMask((GLboolean)dev->zWriteEnable);
    if (dev->colorWriteEnable != 15) DeviceTargets_ColorMask(dev->colorWriteEnable);
    if (depth && dev->stencilWriteMask) glStencilMask(dev->stencilWriteMask & 255);
    if (dev->zFunc != 8)
        glDepthFunc(dev->zFunc >= 1 && dev->zFunc <= 8 ? 0x01ff + dev->zFunc : 0);
    if (dev->fogEnable) glEnable(0x0b60);
    if (dev->lighting) glEnable(0x0b50);
    if (gl->ditherEnabled) glEnable(0x0bd0);
    if (dev->alphaBlendEnable) glEnable(0x0be2);
    if (depth && dev->zEnable) glEnable(0x0b71);
    if (g_alphaTestEnable) glEnable(0x0bc0);
    for (int unit = (int)dev->maxTextureUnits - 1; unit >= 0; --unit) {
        COpenGL_SetActiveTexUnit((const COpenGL *)gl, unit);
        if (gl->texUnits[unit].mTargetEnabled[0]) glEnable(0x0de1);
        if (gl->texUnits[unit].mTargetEnabled[1]) glEnable(0x806f);
        if (gl->texUnits[unit].mTargetEnabled[2]) glEnable(0x8513);
    }
    if (dev->stencilEnable) glEnable(0x0b90);
    if (dev->shadeMode != 1) glShadeModel(0x1d01);
    if (dev->cullMode != 1) glEnable(0x0b44);
}

static HRESULT DeviceTargets_SetDepth(DeviceImpl *dev, IDirect3DSurface9 *surface)
{
    int detached = surface == NULL;
    if (dev->depthStencilDetached != detached) {
        if (dev->zWriteEnable) glDepthMask(!detached);
        if (dev->zEnable) DeviceTargets_Capability(0x0b71, !detached);
        if (dev->stencilWriteMask) glStencilMask(detached ? 0 : dev->stencilWriteMask & 255);
        if (dev->stencilEnable) {
            COpenGL_SetActiveTexUnit((const COpenGL *)CDirect3DDevice_GL(), 0);
            DeviceTargets_Capability(0x0b90, !detached);
        }
    }
    DeviceTargets_Release(dev->depthStencil);
    dev->depthStencil = surface;
    DeviceTargets_AddRef(surface);
    dev->depthStencilDetached = detached;
    return 0;
}

static void DeviceTargets_SetCombiner(const CTexUnitNative *unit)
{
    glTexEnvi(0x2300, 0x8571, unit->mCombinerColorOp);
    glTexEnvi(0x2300, 0x8572, unit->mCombinerAlphaOp);
    glTexEnvi(0x2300, 0x8580, unit->mCombinerColorSource0);
    glTexEnvi(0x2300, 0x8590, unit->mCombinerColorOperand0);
    glTexEnvi(0x2300, 0x8588, unit->mCombinerAlphaSource0);
    glTexEnvi(0x2300, 0x8598, unit->mCombinerAlphaOperand0);
    glTexEnvf(0x2300, 0x8573, unit->mCombinerRGBScale);
    glTexEnvf(0x2300, 0x0d1c, unit->mCombinerAlphaScale);
}

static void DeviceTargets_CopyColor(DeviceImpl *dev)
{
    CDirect3DSurfaceNativeInfo info;
    if (!CDirect3DSurface_GetNativeInfo(dev->renderTarget, &info)) return;
    GLint active, binding;
    glGetIntegerv(0x84e0, &active);
    glActiveTextureARB(0x84c0);
    glGetIntegerv(0x8069, &binding);
    glBindTexture(0x0de1, info.name);
    D3DSURFACE_DESC back;
    DeviceTargets_GetDesc(dev->backBuffer, &back);
    glPixelStorei(0x0cf2, back.Width);
    /* Native uploads allocate logical extents, unlike Mac client-storage POT
     * backing. No original padded extent is safe to copy into these textures. */
    glPixelStorei(0x0d02, info.owner->width);
    glCopyTexSubImage2D(0x0de1, 0, 0, 0, 0, 0, info.owner->width, info.owner->height);
    if (g_RenderToShadowCookie) {
        COpenGLNative *gl = CDirect3DDevice_GL();
        GLfloat textureMatrix[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        if (g_activeVertexShader) glDisable(0x8620);
        if (dev->pixelShader) glDisable(0x8804);
        glMatrixMode(0x1700); glPushMatrix(); glLoadIdentity();
        glMatrixMode(0x1701); glPushMatrix(); glLoadIdentity();
        glOrtho(0.0, 1.0, 1.0, 0.0, -1.0, 1.0);
        if (dev->lighting) glDisable(0x0b50);
        if (!dev->alphaBlendEnable) glEnable(0x0be2);
        if (g_alphaTestEnable) glDisable(0x0bc0);
        if (dev->zEnable) glDisable(0x0b71);
        if (gl->ditherEnabled) glDisable(0x0bd0);
        if (dev->stencilWriteMask) glStencilMask(0);
        if (dev->colorWriteEnable != 15) DeviceTargets_ColorMask(15);
        if (dev->zWriteEnable) glDepthMask(0);
        if (dev->cullMode != 1) glDisable(0x0b44);
        if (dev->fogEnable) glDisable(0x0b60);
        if (dev->stencilEnable) glDisable(0x0b90);
        for (int unit = (int)dev->maxTextureUnits - 1; unit >= 0; --unit) {
            glActiveTextureARB(0x84c0 + unit);
            if (!unit) {
                for (int coord = 0; coord < 4; ++coord) glDisable(0x0c60 + coord);
                glMatrixMode(0x1702);
                glGetFloatv(0x0ba8, textureMatrix);
                glLoadIdentity();
                glEnable(0x0de1); glDisable(0x806f); glDisable(0x8513);
                CTexUnitNative combiner = { 0 };
                combiner.mCombinerColorOp = combiner.mCombinerAlphaOp = 0x2100;
                combiner.mCombinerColorSource0 = combiner.mCombinerAlphaSource0 = 0x1702;
                combiner.mCombinerColorOperand0 = 0x0300;
                combiner.mCombinerAlphaOperand0 = 0x0302;
                combiner.mCombinerRGBScale = combiner.mCombinerAlphaScale = 1.0f;
                DeviceTargets_SetCombiner(&combiner);
            } else {
                glDisable(0x0de1); glDisable(0x806f); glDisable(0x8513);
            }
        }
        static const float weights[5] = { 1.5f, 1.125f, 1.125f, 1.125f, 1.125f };
        static const float offsets[5][2] = { {0,0}, {-1,-1}, {1,-1}, {-1,1}, {1,1} };
        for (UINT32 pass = 0; pass < g_NumBlurShadowPasses * 2u; ++pass) {
            glBlendFunc(1, 0);
            glBegin(7);
            for (UINT32 tap = 0; tap < 5; ++tap) {
                if (tap == 1) {
                    glEnd(); glBlendFunc(1, 1); glBegin(7);
                }
                float weight = weights[tap] / 6.0f;
                float x = offsets[tap][0] * 0.0078125f;
                float y = offsets[tap][1] * 0.0078125f;
                glColor4f(weight, weight, weight, weight);
                glTexCoord2f(0,1); glVertex3f(x,y,0);
                glTexCoord2f(1,1); glVertex3f(1.0f+x,y,0);
                glTexCoord2f(1,0); glVertex3f(1.0f+x,1.0f+y,0);
                glTexCoord2f(0,0); glVertex3f(x,1.0f+y,0);
            }
            glEnd();
            glCopyTexSubImage2D(0x0de1, 0, 0, 0, 0, 0, info.owner->width, info.owner->height);
        }
        glLoadMatrixf(textureMatrix);
        glMatrixMode(0x1701); glPopMatrix();
        glMatrixMode(0x1700); glPopMatrix();
        if (dev->separateAlphaBlendEnable)
            glBlendFuncSeparateEXT(gl->blendSrcRGB, gl->blendDstRGB, gl->blendSrcAlpha, gl->blendDstAlpha);
        else glBlendFunc(gl->blendSrcRGB, gl->blendDstRGB);
        if (dev->lighting) glEnable(0x0b50);
        if (!dev->alphaBlendEnable) glDisable(0x0be2);
        if (g_alphaTestEnable) glEnable(0x0bc0);
        if (dev->zEnable) glEnable(0x0b71);
        if (gl->ditherEnabled) glEnable(0x0bd0);
        if (dev->stencilWriteMask) glStencilMask(dev->stencilWriteMask);
        if (dev->colorWriteEnable != 15) DeviceTargets_ColorMask(dev->colorWriteEnable);
        if (dev->zWriteEnable) glDepthMask(1);
        if (dev->cullMode != 1) glEnable(0x0b44);
        if (dev->fogEnable) glEnable(0x0b60);
        if (dev->stencilEnable) glEnable(0x0b90);
        if (g_activeVertexShader) glEnable(0x8620);
        if (dev->pixelShader) glEnable(0x8804);
        for (int unit = (int)dev->maxTextureUnits - 1; unit >= 0; --unit) {
            CTexUnitNative *texture = &gl->texUnits[unit];
            glActiveTextureARB(0x84c0 + unit);
            if (!unit) {
                /* Original tests the S flag for all four texgen enables and
                 * leaves the texture matrix at identity. Preserve those quirks. */
                if (texture->mTexGenEnable[0])
                    for (int coord = 0; coord < 4; ++coord) glEnable(0x0c60 + coord);
                glMatrixMode(0x1702); glGetFloatv(0x0ba8, textureMatrix); glLoadIdentity();
                DeviceTargets_SetCombiner(texture);
            }
            if (texture->mTargetEnabled[0]) glEnable(0x0de1);
            if (texture->mTargetEnabled[1]) glEnable(0x806f);
            if (texture->mTargetEnabled[2]) glEnable(0x8513);
        }
        /* Native wrappers cache the actual GL matrix mode. The raw Mac path
         * ends in TEXTURE; synchronize that bookkeeping so the next wrapper
         * cannot skip a needed mode change. Keep mTexMatrix's Mac quirk. */
        gl->matrixMode = dev->maxTextureUnits ? 0x1702 : 0x1700;
    }
    glPixelStorei(0x0cf2, 0); glPixelStorei(0x0d02, 0);
    glBindTexture(0x0de1, binding);
    glActiveTextureARB(active);
    CDirect3DSurface_MarkGPUWritten(dev->renderTarget);
}

static HRESULT DeviceTargets_SetTarget(DeviceImpl *dev, IDirect3DSurface9 *target)
{
    if (target) {
        if (dev->renderTarget && dev->renderTarget != dev->backBuffer && !g_InhibitCopy)
            DeviceTargets_CopyColor(dev);
        DeviceTargets_Release(dev->renderTarget);
        dev->renderTarget = target;
        DeviceTargets_AddRef(target);
    }
    D3DSURFACE_DESC desc;
    DeviceTargets_GetDesc(dev->renderTarget, &desc);
    D3DVIEWPORT9 viewport = { 0, 0, desc.Width, desc.Height, 0.0f, 1.0f };
    CDirect3DDevice_SetViewport((const CDirect3DDevice *)dev, &viewport);
    return 0;
}

static void DeviceTargets_FilterTexture(D3DTEXTUREFILTERTYPE filter)
{
    GLint glFilter = filter == D3DTEXF_LINEAR ? 0x2601 : 0x2600;
    glTexParameteri(0x0de1, 0x2801, glFilter);
    glTexParameteri(0x0de1, 0x2800, glFilter);
    glTexParameteri(0x0de1, 0x2802, 0x2900);
    glTexParameteri(0x0de1, 0x2803, 0x2900);
    glTexParameteri(0x0de1, 0x813d, 0);
}

static const GLenum DeviceTargets_SamplerNames[5] = { 0x2801, 0x2800, 0x2802, 0x2803, 0x813d };

static void DeviceTargets_SaveSampler(GLint values[5])
{
    for (UINT parameter = 0; parameter < 5; ++parameter)
        glGetTexParameteriv(0x0de1, DeviceTargets_SamplerNames[parameter], &values[parameter]);
}

static void DeviceTargets_RestoreSampler(const GLint values[5])
{
    for (UINT parameter = 0; parameter < 5; ++parameter)
        glTexParameteri(0x0de1, DeviceTargets_SamplerNames[parameter], values[parameter]);
}

static int DeviceTargets_IsColor(D3DFORMAT format)
{
    switch (format) {
    case D3DFMT_D16_LOCKABLE: case D3DFMT_D32: case D3DFMT_D15S1:
    case D3DFMT_D24S8: case D3DFMT_D24X8: case D3DFMT_D24X4S4:
    case D3DFMT_D16: case D3DFMT_D32F_LOCKABLE: case D3DFMT_D24FS8:
    case D3DFMT_DXT1: case D3DFMT_DXT2: case D3DFMT_DXT3:
    case D3DFMT_DXT4: case D3DFMT_DXT5:
        return 0;
    default:
        return 1;
    }
}

static HRESULT DeviceTargets_Stretch(DeviceImpl *dev, void *source, const RECT *sourceRect,
                                     void *destination, const RECT *destinationRect, D3DTEXTUREFILTERTYPE filter)
{
    CDirect3DSurfaceNativeInfo src, dst;
    D3DSURFACE_DESC sourceDesc, destDesc;
    HRESULT result = DeviceTargets_GetDesc(source, &sourceDesc);
    if (result) return result;
    result = DeviceTargets_GetDesc(destination, &destDesc);
    if (result) return result;
    /* The Windows cinematic caller passes a destination rectangle to D3D9.
     * Preserve that platform boundary even though the Mac shim ignores it. */
    RECT sourceRegion = { 0, 0, sourceDesc.Width, sourceDesc.Height };
    RECT destRegion = { 0, 0, destDesc.Width, destDesc.Height };
    if (sourceRect) sourceRegion = *sourceRect;
    if (destinationRect) destRegion = *destinationRect;
    if (filter < D3DTEXF_NONE || filter > D3DTEXF_LINEAR || source == destination ||
        !DeviceTargets_IsColor(sourceDesc.Format) || !DeviceTargets_IsColor(destDesc.Format) ||
        sourceDesc.MultiSampleType || destDesc.MultiSampleType ||
        sourceRegion.left < 0 || sourceRegion.top < 0 ||
        sourceRegion.right <= sourceRegion.left || sourceRegion.bottom <= sourceRegion.top ||
        sourceRegion.right > sourceDesc.Width || sourceRegion.bottom > sourceDesc.Height ||
        destRegion.left < 0 || destRegion.top < 0 ||
        destRegion.right <= destRegion.left || destRegion.bottom <= destRegion.top ||
        destRegion.right > destDesc.Width || destRegion.bottom > destDesc.Height)
        return (HRESULT)0x8876086c;
    int fromBack = source == dev->backBuffer;
    int toBack = destination == dev->backBuffer;
    int fromCurrentAux = !fromBack && source == dev->renderTarget;
    int toCurrentAux = !toBack && destination == dev->renderTarget;
    if (fromCurrentAux) {
        D3DSURFACE_DESC backDesc;
        result = DeviceTargets_GetDesc(dev->backBuffer, &backDesc);
        if (result) return result;
        if (sourceDesc.Width > backDesc.Width || sourceDesc.Height > backDesc.Height)
            return (HRESULT)0x8876086c;
    }
    if (!toBack) {
        D3DSURFACE_DESC backDesc;
        result = DeviceTargets_GetDesc(dev->backBuffer, &backDesc);
        if (result) return result;
        if (!CDirect3DSurface_GetNativeInfo(destination, &dst) || dst.target != 0x0de1 || dst.level ||
            destDesc.Width > backDesc.Width || destDesc.Height > backDesc.Height)
            return (HRESULT)0x8876086c;
        int fullDestination = destRegion.left == 0 && destRegion.top == 0 &&
            destRegion.right == destDesc.Width && destRegion.bottom == destDesc.Height;
        /* A partial copy cannot mix framebuffer rows with untouched top-down
         * CPU rows. Current targets resolve their whole authoritative buffer. */
        if (!toCurrentAux && !fullDestination &&
            (!dst.pixelsBottomUp || CDirect3DSurface_IsDirty(destination)))
            return (HRESULT)0x8876086c;
    }
    GLint active, binding;
    if (fromBack && !toCurrentAux && !sourceRect && !destinationRect &&
        sourceDesc.Width == destDesc.Width && sourceDesc.Height == destDesc.Height) {
        GLint readBuffer;
        glGetIntegerv(0x0c02, &readBuffer);
        glGetIntegerv(0x84e0, &active);
        glActiveTextureARB(0x84c0);
        glGetIntegerv(0x8069, &binding);
        glBindTexture(0x0de1, dst.name);
        DeviceTargets_ReadBuffer(0x0405);
        glPixelStorei(0x0cf2, sourceDesc.Width); glPixelStorei(0x0d02, sourceDesc.Width);
        glCopyTexSubImage2D(0x0de1, 0, 0, 0, 0, 0, destDesc.Width, destDesc.Height);
        DeviceTargets_ReadBuffer(readBuffer);
        glPixelStorei(0x0cf2, 0); glPixelStorei(0x0d02, 0);
        glBindTexture(0x0de1, binding); glActiveTextureARB(active);
        CDirect3DSurface_MarkGPUWritten(destination);
        return 0;
    }
    if (!CDirect3DSurface_GetNativeInfo(source, &src) || src.target != 0x0de1 || src.level)
        return (HRESULT)0x8876086c;
    int sourceBottomUp = src.pixelsBottomUp;
    GLint currentDrawBuffer;
    glGetIntegerv(0x0c01, &currentDrawBuffer);
    GLenum staging = 0;
    if (currentDrawBuffer == 0x0409 || currentDrawBuffer == 0x8ce1)
        staging = 0x040a;
    else if (currentDrawBuffer == 0x040a || currentDrawBuffer == 0x8ce2)
        staging = 0x0409;
    else if (currentDrawBuffer == 0x0405 || currentDrawBuffer == 0x8ce0)
        staging = 0x040a;
    else if (!toBack || fromCurrentAux)
        return (HRESULT)0x8876086c;
    if ((fromCurrentAux || toCurrentAux) &&
        currentDrawBuffer != 0x0409 && currentDrawBuffer != 0x8ce1 &&
        currentDrawBuffer != 0x040a && currentDrawBuffer != 0x8ce2)
        return (HRESULT)0x8876086c;
    GLenum destinationBuffer = toBack ? 0x0405 : toCurrentAux ? (GLenum)currentDrawBuffer : staging;
    glPushAttrib(0x000fffff);
    glPushClientAttrib(0xffffffffu);
    /* Previous auxiliary targets were resolved by RB_SetRenderTarget. Their
     * inactive attachment is native scratch; preserve the current surface's
     * pixels while drawing a different texture destination. Source identity
     * does not override the current AUX attachment's preservation priority. */
    glDisable(0x8620);
    int card = MacDisplay_GetCardType();
    if (card == 2) { glDisable(0x86de); glDisable(0x8522); }
    else if (card == 1) glDisable(0x8200);
    if (MacOpenGLUtils_GetPCPixelShaderVersion() > 0xffff01ffu) glDisable(0x8804);
    DeviceTargets_StartPixel(dev);
    glActiveTextureARB(0x84c0);
    glBindTexture(0x0de1, src.name);
    GLint sourceSampler[5];
    DeviceTargets_SaveSampler(sourceSampler);
    /* Lock/Unlock eagerly uploads native CPU pixels; AddDirtyRect and pending
     * locks still need uploading. GPU-written targets have no coherent Apple
     * client-storage mirror, so reuploading their stale CPU allocation loses it. */
    if (!fromBack && !fromCurrentAux && CDirect3DSurface_IsDirty(source)) {
        CDirect3DSurface_UpdateOpenGLSurfaceObject(source, 0);
        CDirect3DSurface_SetDirty(source, 0);
        sourceBottomUp = 0;
    }
    DeviceTargets_FilterTexture(filter);
    if (fromBack || fromCurrentAux) {
        /* RB_SetRenderTarget selects matching draw/read BACK/AUX0/AUX1. Read
         * a still-current auxiliary source before selecting the blit staging
         * buffer; native CPU backing is not Mac's coherent client storage. */
        DeviceTargets_ReadBuffer(fromBack ? 0x0405 : currentDrawBuffer);
        glPixelStorei(0x0cf2, sourceDesc.Width); glPixelStorei(0x0d02, sourceDesc.Width);
        glCopyTexSubImage2D(0x0de1, 0, 0, 0, 0, 0, sourceDesc.Width, sourceDesc.Height);
        CDirect3DSurface_MarkGPUWritten(source);
        sourceBottomUp = 1;
    }
    if (!fromBack) {
        DeviceTargets_DrawBuffer(destinationBuffer);
        DeviceTargets_ReadBuffer(destinationBuffer);
    }
    GLfloat matrices[16][16];
    glMatrixMode(0x1702);
    for (int unit = (int)dev->maxTextureUnits - 1; unit >= 0; --unit) {
        glActiveTextureARB(0x84c0 + unit);
        glGetFloatv(0x0ba8, matrices[unit]); glLoadIdentity();
        for (int coord = 0; coord < 4; ++coord) glDisable(0x0c60 + coord);
        if (unit) {
            glDisable(0x0de1); glDisable(0x806f); glDisable(0x8513);
        } else {
            glEnable(0x0de1); glDisable(0x8513);
            CTexUnitNative combiner = { 0 };
            combiner.mCombinerColorOp = combiner.mCombinerAlphaOp = 0x1e01;
            combiner.mCombinerColorSource0 = combiner.mCombinerAlphaSource0 = 0x1702;
            combiner.mCombinerColorOperand0 = 0x0300;
            combiner.mCombinerAlphaOperand0 = 0x0302;
            combiner.mCombinerRGBScale = combiner.mCombinerAlphaScale = 1.0f;
            DeviceTargets_SetCombiner(&combiner);
        }
    }
    glMatrixMode(0x1700); glPushMatrix(); glLoadIdentity();
    glMatrixMode(0x1701); glPushMatrix(); glLoadIdentity();
    /* D3D StretchRect coordinates are surface coordinates, independent of the
     * current drawing viewport/scissor. The attribute stack restores both. */
    glViewport(0, 0, destDesc.Width, destDesc.Height);
    glDisable(0x0c11);
    glOrtho(0.0, (double)destDesc.Width, (double)destDesc.Height, 0.0, -1.0, 1.0);
    if (fromBack) DeviceTargets_DrawBuffer(destinationBuffer);
    float u0 = (float)sourceRegion.left / (float)sourceDesc.Width;
    float u1 = (float)sourceRegion.right / (float)sourceDesc.Width;
    float v0 = (float)sourceRegion.top / (float)sourceDesc.Height;
    float v1 = (float)sourceRegion.bottom / (float)sourceDesc.Height;
    if (sourceBottomUp) { v0 = 1.0f - v0; v1 = 1.0f - v1; }
    glBegin(7);
    glColor4f(1,1,1,1);
    glTexCoord2f(u0,v0); glVertex2f(destRegion.left,destRegion.top);
    glTexCoord2f(u1,v0); glVertex2f(destRegion.right,destRegion.top);
    glTexCoord2f(u1,v1); glVertex2f(destRegion.right,destRegion.bottom);
    glTexCoord2f(u0,v1); glVertex2f(destRegion.left,destRegion.bottom);
    glEnd();
    glMatrixMode(0x1701); glPopMatrix();
    glMatrixMode(0x1700); glPopMatrix();
    glMatrixMode(0x1702);
    for (UINT unit = 0; unit < dev->maxTextureUnits; ++unit) {
        glActiveTextureARB(0x84c0 + unit); glLoadMatrixf(matrices[unit]);
    }
    if (!toBack) {
        /* The Mac copies into the destination on the currently active unit. */
        glBindTexture(0x0de1, dst.name);
        GLint destinationSampler[5];
        DeviceTargets_SaveSampler(destinationSampler);
        DeviceTargets_ReadBuffer(destinationBuffer);
        DeviceTargets_FilterTexture(filter);
        if (toCurrentAux) {
            /* The current framebuffer remains authoritative after this call.
             * Resolve all its pixels so the next target switch agrees too. */
            glCopyTexSubImage2D(0x0de1, 0, 0, 0, 0, 0, destDesc.Width, destDesc.Height);
        } else {
            GLint bottom = destDesc.Height - destRegion.bottom;
            glCopyTexSubImage2D(0x0de1, 0, destRegion.left, bottom, destRegion.left, bottom,
                                destRegion.right - destRegion.left, destRegion.bottom - destRegion.top);
        }
        CDirect3DSurface_MarkGPUWritten(destination);
        DeviceTargets_RestoreSampler(destinationSampler);
        if (!fromBack) { DeviceTargets_ReadBuffer(0x0405); DeviceTargets_DrawBuffer(0x0405); }
    }
    if (fromBack || fromCurrentAux) { glPixelStorei(0x0cf2, 0); glPixelStorei(0x0d02, 0); }
    /* Texture object parameters can outlive the attribute-stack bindings. Keep
     * the Windows-style blit from invalidating the typed persistent samplers. */
    glActiveTextureARB(0x84c0);
    glBindTexture(0x0de1, src.name);
    DeviceTargets_RestoreSampler(sourceSampler);
    glPopClientAttrib(); glPopAttrib();
    return 0;
}
