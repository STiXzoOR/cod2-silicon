/* Mac SetRenderState 0x15e18–0x174eb. Raw D3D caches remain available to
 * native fallback consumers; mapped driver values have separate typed caches. */
static void DeviceRenderState_Init(DeviceImpl *dev)
{
    dev->glSrcBlend = dev->glSrcBlendAlpha = 1;
    dev->glDestBlend = dev->glDestBlendAlpha = 0;
    dev->glStencilFunc = 0x207;
    dev->alphaBlendValue = dev->ditherValue = 0;
    dev->ambientMaterialSource = 0;
    dev->diffuseMaterialSource = 1;
    dev->blendStateChanged = 1;
    memset(dev->lightModelAmbient, 0, sizeof(dev->lightModelAmbient));
    dev->slopeScaleDepthBias = dev->scaledDepthBias = 0.0f;

    /* Mac Reset 0x19d85/0x19e50 recreates the context before default caches.
     * SDL retains it, so restore the fresh-context state not set by COpenGL_Init. */
    glStencilFunc(dev->glStencilFunc, 0, 0xffffffffu);
    glStencilOp(0x1e00, 0x1e00, 0x1e00);
    glStencilMask(0xffffffffu);
    const GLfloat fogColor[4] = {0, 0, 0, 0};
    glFogfv(0xb66, fogColor);
    glFogf(0xb63, 0.0f);
    glFogf(0xb64, 1.0f);
    glFogf(0xb62, 1.0f);
    glColorMaterial(0x408, 0x1602);
    GLfloat smoothRange[2], aliasedRange[2];
    glGetFloatv(0xb12, smoothRange);
    glGetFloatv(0x846d, aliasedRange);
    glPointParameterfARB(0x8126, 0.0f);
    glPointParameterfARB(0x8127, smoothRange[1] > aliasedRange[1] ? smoothRange[1] : aliasedRange[1]);
    const GLfloat attenuation[3] = {1, 0, 0};
    /* sPointScale is process-wide in the original, independent of this GL default. */
    glPointParameterfvARB(0x8129, attenuation);
    glDisable(0x8861);
    for (UINT i = 0; i < dev->maxTextureUnits; ++i) {
        COpenGL_SetActiveTexUnit((const COpenGL *)CDirect3DDevice_GL(), i);
        glTexEnvi(0x8861, 0x8862, 0);
    }
    COpenGL_SetActiveTexUnit((const COpenGL *)CDirect3DDevice_GL(), 0);
}

static GLfloat DeviceRenderState_Float(DWORD value)
{
    GLfloat result;
    memcpy(&result, &value, sizeof(result));
    return result;
}

static void DeviceRenderState_Color(DWORD value, GLfloat color[4])
{
    const GLfloat scale = 0.003921568859368563f;
    color[0] = (GLfloat)((value >> 16) & 255) * scale;
    color[1] = (GLfloat)((value >> 8) & 255) * scale;
    color[2] = (GLfloat)(value & 255) * scale;
    color[3] = (GLfloat)(value >> 24) * scale;
}

static void DeviceRenderState_Enable(GLenum state, DWORD value)
{
    if (value) glEnable(state); else glDisable(state);
}

static GLenum DeviceRenderState_Blend(DWORD value, GLenum previous, int destination, int alpha)
{
    /* Four decoded tables at 0x326da0/df4/e48/e78. Unhandled entries retain
     * the previous GL factor even though the raw D3D value has been stored. */
    switch (value) {
    case 1: return 0;
    case 2: return 1;
    case 3: return destination ? 0x300 : previous;
    case 4: return destination && !alpha ? 0x301 : previous;
    case 5: return 0x302;
    case 6: return 0x303;
    case 7: return 0x304;
    case 8: return 0x305;
    case 9: return 0x306;
    case 10: return !destination || alpha ? 0x307 : previous;
    case 11: return 0x308;
    default: return previous;
    }
}

static GLenum DeviceRenderState_StencilOp(DWORD value)
{
    static const GLenum operations[9] = { 0, 0x1e00, 0, 0x1e01, 0x1e02, 0x1e03, 0x150a, 0x8507, 0x8508 };
    return value <= 8 ? operations[value] : 0;
}

static void DeviceRenderState_Stencil(DeviceImpl *dev)
{
    glStencilOp(DeviceRenderState_StencilOp(dev->stencilFail),
                DeviceRenderState_StencilOp(dev->stencilZFail),
                DeviceRenderState_StencilOp(dev->stencilPass));
}

static void DeviceRenderState_StencilFunc(DeviceImpl *dev)
{
    glStencilFunc(dev->glStencilFunc, dev->stencilRef, dev->stencilMask);
}

static void DeviceRenderState_MaterialSource(DWORD value, DWORD other, GLenum mode)
{
    if (value == 1 || value == 0) {
        if (value == 1 || other == 1) {
            glColorMaterial(0x408, value == 1 && other == 1 ? 0x1602 : mode);
            COpenGL_SetColorMaterialEnable((const COpenGL *)CDirect3DDevice_GL(), 1);
        } else {
            COpenGL_SetColorMaterialEnable((const COpenGL *)CDirect3DDevice_GL(), 0);
        }
    }
}

static void DeviceRenderState_BlendOp(DeviceImpl *dev, DWORD value)
{
    if (dev->blendOp == value)
        return;
    dev->blendOp = value;
    COpenGLNative *gl = CDirect3DDevice_GL();
    if (value == 1) {
        if (gl->supportsBlendSubtract || gl->supportsBlendMinMax)
            glBlendEquationEXT(0x8006);
    } else if (value == 2 || value == 3) {
        if (gl->supportsBlendSubtract)
            glBlendEquationEXT(value == 2 ? 0x800a : 0x800b);
    } else if (value == 4 || value == 5) {
        if (gl->supportsBlendMinMax)
            glBlendEquationEXT(value == 4 ? 0x8007 : 0x8008);
    }
}

static HRESULT DeviceRenderState_Set(DeviceImpl *dev, D3DRENDERSTATETYPE state, DWORD value)
{
    switch (state) {
    case D3DRS_ZENABLE:
        if (dev->zEnable != value) {
            dev->zEnable = value;
            /* The implicit SDL depth attachment counts as the original
             * attached default surface; an explicit NULL remains detached. */
            if (!dev->depthStencilDetached)
                DeviceRenderState_Enable(0xb71, value);
        }
        break;
    case D3DRS_FILLMODE:
        if (dev->fillMode != value) {
            dev->fillMode = value;
            glPolygonMode(0x408, value == 1 ? 0x1b00 : value == 2 ? 0x1b01 : value == 3 ? 0x1b02 : 0);
        }
        break;
    case D3DRS_SHADEMODE:
        if (dev->shadeMode != value) {
            dev->shadeMode = value;
            glShadeModel(value == 1 ? 0x1d00 : value == 2 ? 0x1d01 : 0);
        }
        break;
    case D3DRS_ZWRITEENABLE:
        if (dev->zWriteEnable != value) {
            dev->zWriteEnable = value;
            if (!dev->depthStencilDetached)
                glDepthMask(value != 0);
        }
        break;
    case D3DRS_ALPHATESTENABLE:
        if (g_alphaTestEnable != value) {
            g_alphaTestEnable = value;
            DeviceRenderState_Enable(0xbc0, value);
        }
        break;
    case D3DRS_SRCBLEND:
        dev->srcBlend = value;
        dev->glSrcBlend = DeviceRenderState_Blend(value, dev->glSrcBlend, 0, 0);
        break;
    case D3DRS_DESTBLEND:
        dev->destBlend = value;
        dev->glDestBlend = DeviceRenderState_Blend(value, dev->glDestBlend, 1, 0);
        break;
    case D3DRS_CULLMODE:
        if (dev->cullMode != value) {
            dev->cullMode = value;
            if (value == 1) glDisable(0xb44);
            else if (value == 2 || value == 3) {
                glEnable(0xb44);
                glFrontFace(value == 2 ? 0x901 : 0x900);
            }
        }
        break;
    case D3DRS_ZFUNC:
        if (dev->zFunc != value) {
            dev->zFunc = value;
            glDepthFunc(value >= 1 && value <= 8 ? 0x1ff + value : 0);
        }
        break;
    case D3DRS_ALPHAREF:
        if (dev->alphaRefVal != value) {
            dev->alphaRefVal = value;
            dev->alphaRef = (GLfloat)value / 255.0f;
            glAlphaFunc(dev->alphaFunc, dev->alphaRef);
        }
        break;
    case D3DRS_ALPHAFUNC:
        if (dev->alphaFuncVal != value) {
            dev->alphaFuncVal = value;
            if (value >= 1 && value <= 8) dev->alphaFunc = 0x1ff + value;
            glAlphaFunc(dev->alphaFunc, dev->alphaRef);
        }
        break;
    case D3DRS_DITHERENABLE:
        dev->ditherValue = value;
        COpenGL_SetDither((const COpenGL *)CDirect3DDevice_GL(), value != 0);
        break;
    case D3DRS_ALPHABLENDENABLE:
        if (dev->alphaBlendValue != value) {
            dev->alphaBlendValue = value;
            dev->alphaBlendEnable = value != 0;
            dev->blendStateChanged = 1;
            DeviceRenderState_Enable(0xbe2, value);
        }
        break;
    case D3DRS_FOGENABLE:
        if (dev->fogEnable != value) {
            dev->fogEnable = value;
            DeviceRenderState_Enable(0xb60, value);
        }
        break;
    case D3DRS_FOGCOLOR: {
        GLfloat color[4];
        DeviceRenderState_Color(value, color);
        dev->fogColor = value; /* Native fallback/getter cache. */
        glFogfv(0xb66, color);
        break;
    }
    case D3DRS_FOGSTART:
    case D3DRS_FOGEND: {
        GLfloat parameter = DeviceRenderState_Float(value);
        if (parameter < 0.0f) parameter = 0.0f;
        glFogf(state == D3DRS_FOGSTART ? 0xb63 : 0xb64, parameter);
        break;
    }
    case D3DRS_FOGDENSITY:
        glFogf(0xb62, DeviceRenderState_Float(value));
        break;
    case D3DRS_STENCILENABLE:
        if (dev->stencilEnable != value) {
            dev->stencilEnable = value;
            COpenGL_SetActiveTexUnit((const COpenGL *)CDirect3DDevice_GL(), 0);
            DeviceRenderState_Enable(0xb90, value);
        }
        break;
    case D3DRS_STENCILFAIL:
        if (dev->stencilFail != value) { dev->stencilFail = value; DeviceRenderState_Stencil(dev); }
        break;
    case D3DRS_STENCILZFAIL:
        if (dev->stencilZFail != value) { dev->stencilZFail = value; DeviceRenderState_Stencil(dev); }
        break;
    case D3DRS_STENCILPASS:
        if (dev->stencilPass != value) { dev->stencilPass = value; DeviceRenderState_Stencil(dev); }
        break;
    case D3DRS_STENCILFUNC:
        if (dev->stencilFunc != value) {
            dev->stencilFunc = value;
            if (value >= 1 && value <= 8) dev->glStencilFunc = 0x1ff + value;
            DeviceRenderState_StencilFunc(dev);
        }
        break;
    case D3DRS_STENCILREF:
        if (dev->stencilRef != value) {
            dev->stencilRef = value & 255;
            DeviceRenderState_StencilFunc(dev);
        }
        break;
    case D3DRS_STENCILMASK:
        if (dev->stencilMask != value) {
            dev->stencilMask = value & 255;
            DeviceRenderState_StencilFunc(dev);
        }
        break;
    case D3DRS_STENCILWRITEMASK:
        if (dev->stencilWriteMask != value) {
            dev->stencilWriteMask = value & 255;
            if (!dev->depthStencilDetached) glStencilMask(dev->stencilWriteMask);
        }
        break;
    case D3DRS_TEXTUREFACTOR: {
        GLfloat color[4];
        g_textureFactor = value;
        DeviceRenderState_Color(value, color);
        for (UINT i = 0; i < dev->maxTextureUnits; ++i)
            memcpy(CDirect3DDevice_Stage(dev, i)->factor, color, sizeof(color));
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
        break;
    }
    case 128: case 129: case 130: case 131: case 132: case 133: case 134: case 135:
        CDirect3DDevice_mNeedsRasterizationValidation = 1;
        break;
    case D3DRS_LIGHTING:
        if (CDirect3DDevice_GL()->supportsATIArrayRevComps) dev->lighting = value;
        else if (dev->lighting != value) {
            dev->lighting = value;
            DeviceRenderState_Enable(0xb50, value);
        }
        break;
    case D3DRS_AMBIENT:
        DeviceRenderState_Color(value, dev->lightModelAmbient);
        if (!CDirect3DDevice_GL()->supportsATIArrayRevComps)
            glLightModelfv(0xb53, dev->lightModelAmbient);
        break;
    case D3DRS_FOGVERTEXMODE:
        if (value <= 3) glFogf(0xb65, value <= 1 ? 2048.0f : value == 2 ? 2049.0f : 9729.0f);
        break;
    case D3DRS_COLORVERTEX:
        dev->colorVertex = value;
        break;
    case D3DRS_NORMALIZENORMALS:
        DeviceRenderState_Enable(0xba1, value);
        break;
    case D3DRS_DIFFUSEMATERIALSOURCE:
        if (dev->diffuseMaterialSource != value) {
            dev->diffuseMaterialSource = value;
            DeviceRenderState_MaterialSource(value, dev->ambientMaterialSource, value ? 0x1201 : 0x1200);
        }
        break;
    case D3DRS_AMBIENTMATERIALSOURCE:
        if (dev->ambientMaterialSource != value) {
            dev->ambientMaterialSource = value;
            DeviceRenderState_MaterialSource(value, dev->diffuseMaterialSource, value ? 0x1200 : 0x1201);
        }
        break;
    case D3DRS_POINTSIZE_MIN:
    case D3DRS_POINTSIZE_MAX:
        glPointParameterfARB(state == D3DRS_POINTSIZE_MIN ? 0x8126 : 0x8127, DeviceRenderState_Float(value));
        break;
    case D3DRS_POINTSPRITEENABLE:
        DeviceRenderState_Enable(0x8861, value);
        for (UINT i = 0; i < dev->maxTextureUnits; ++i) {
            COpenGL_SetActiveTexUnit((const COpenGL *)CDirect3DDevice_GL(), i);
            glTexEnvi(0x8861, 0x8862, value != 0);
        }
        break;
    case D3DRS_POINTSCALE_A:
    case D3DRS_POINTSCALE_B:
    case D3DRS_POINTSCALE_C:
        sPointScale[state - D3DRS_POINTSCALE_A] = DeviceRenderState_Float(value);
        glPointParameterfvARB(0x8129, sPointScale);
        break;
    case D3DRS_COLORWRITEENABLE:
        if (dev->colorWriteEnable != value) {
            dev->colorWriteEnable = value;
            glColorMask(value & 1, (value >> 1) & 1, (value >> 2) & 1, (value >> 3) & 1);
        }
        break;
    case D3DRS_BLENDOP:
        DeviceRenderState_BlendOp(dev, value);
        break;
    case D3DRS_SLOPESCALEDEPTHBIAS:
    case D3DRS_DEPTHBIAS:
        if (state == D3DRS_SLOPESCALEDEPTHBIAS) {
            g_slopeScaleDepthBias = DeviceRenderState_Float(value);
            dev->slopeScaleDepthBias = g_slopeScaleDepthBias * g_scale1;
        } else {
            g_depthBias = DeviceRenderState_Float(value);
            dev->scaledDepthBias = g_depthBias * g_scale2;
        }
        glPolygonOffset(dev->slopeScaleDepthBias, dev->scaledDepthBias);
        DeviceRenderState_Enable(0x8037, dev->scaledDepthBias != 0.0f || dev->slopeScaleDepthBias != 0.0f);
        break;
    case D3DRS_SEPARATEALPHABLENDENABLE:
        if (dev->separateAlphaBlendEnable != value) {
            dev->separateAlphaBlendEnable = value;
            dev->blendStateChanged = 1;
        }
        break;
    case D3DRS_SRCBLENDALPHA:
        dev->srcBlendAlpha = value;
        dev->glSrcBlendAlpha = DeviceRenderState_Blend(value, dev->glSrcBlendAlpha, 0, 1);
        break;
    case D3DRS_DESTBLENDALPHA:
        dev->destBlendAlpha = value;
        dev->glDestBlendAlpha = DeviceRenderState_Blend(value, dev->glDestBlendAlpha, 1, 1);
        break;
    case D3DRS_BLENDOPALPHA:
        if (dev->separateAlphaBlendEnable) {
            dev->alphaSrcBlend = value;
            /* Mac 0x16596 jumps into the RGB blend-operation cache/driver
             * branch; no separate RGB/alpha equation call is made. */
            DeviceRenderState_BlendOp(dev, value);
        }
        break;
    default:
        break;
    }
    return 0;
}
