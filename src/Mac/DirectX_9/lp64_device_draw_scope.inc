/* Raw GL calls: included before the Device cache-writing wrappers.
 * This scope owns only its sampler journal. Texture pointers are borrowed;
 * names, uploads, binding lists, COM references and packet storage stay live. */
typedef struct {
    GLenum wrapS, wrapT, wrapR, minFilter, magFilter;
    GLuint border;
    GLint lastLevel;
    GLfloat anisotropy;
} DeviceDrawSamplerCache;

typedef struct {
    GLuint name;
    GLenum target;
    GLint wrapS, wrapT, wrapR, minFilter, magFilter, lastLevel;
    GLfloat border[4], anisotropy;
    COpenGLTextureStateNative *info;
    DeviceDrawSamplerCache infoCache;
} DeviceDrawSamplerRecord;

typedef struct DeviceDrawScope {
    struct DeviceDrawScope *previous;
    GLint activeTexture, clientTexture, matrixMode, vaoCache, program[2];
    UINT32 activeTextureCache, clientTextureCache;
    GLenum matrixModeCache;
    GLuint programCache;
    const GLuint *textureNameCache[16][3];
    GLuint textureNames[16][3];
    unsigned char targetCache[2][3];
    GLdouble modelView[16], projection[16], textureMatrix[2][16];
    GLfloat modelViewCache[16], lodBias[2];
    DeviceDrawSamplerCache samplerCache[2][3];
    UINT32 units, samplerCount, samplerCapacity;
    DeviceDrawSamplerRecord *samplers;
} DeviceDrawScope;

static DeviceDrawScope *sDeviceDrawScope;

static DeviceDrawSamplerCache DeviceDraw_SaveSamplerCache(const COpenGLTextureStateNative *info)
{
    return (DeviceDrawSamplerCache){ info->wrapS, info->wrapT, info->wrapR,
        info->minFilter, info->magFilter, info->border, info->lastLevel, info->anisotropy };
}

static void DeviceDraw_RestoreSamplerCache(COpenGLTextureStateNative *info,
                                          const DeviceDrawSamplerCache *cache)
{
    info->wrapS = cache->wrapS; info->wrapT = cache->wrapT; info->wrapR = cache->wrapR;
    info->minFilter = cache->minFilter; info->magFilter = cache->magFilter;
    info->border = cache->border; info->lastLevel = cache->lastLevel;
    info->anisotropy = cache->anisotropy;
}

/* ApplySamplerState is called after the selected texture's pending uploads.
 * Save each sampled object once, including substituted textures that were not
 * bound when GL_TEXTURE_BIT was pushed. At most two are selected today. */
static void DeviceDraw_SaveSampler(GLenum target, COpenGLTextureStateNative *info)
{
    DeviceDrawScope *scope = sDeviceDrawScope;
    if (!scope) return;
    GLint name;
    glGetIntegerv(target == 0x8513 ? 0x8514 : target == 0x806f ? 0x806a : 0x8069, &name);
    DeviceDrawSamplerRecord *record = NULL;
    for (UINT32 i = 0; i < scope->samplerCount; ++i)
        if (scope->samplers[i].target == target && scope->samplers[i].name == (GLuint)name) {
            record = &scope->samplers[i];
            break;
        }
    if (!record) {
        if (scope->samplerCount == scope->samplerCapacity) {
            UINT32 capacity = scope->samplerCapacity ? scope->samplerCapacity * 2 : 2;
            void *allocation = realloc(scope->samplers, capacity * sizeof(*scope->samplers));
            if (!allocation) abort();
            scope->samplers = allocation; scope->samplerCapacity = capacity;
        }
        record = &scope->samplers[scope->samplerCount++];
        memset(record, 0, sizeof(*record));
        record->target = target; record->name = name;
        glGetTexParameteriv(target, 0x2802, &record->wrapS);
        glGetTexParameteriv(target, 0x2803, &record->wrapT);
        glGetTexParameteriv(target, 0x8072, &record->wrapR);
        glGetTexParameteriv(target, 0x2801, &record->minFilter);
        glGetTexParameteriv(target, 0x2800, &record->magFilter);
        glGetTexParameteriv(target, 0x813d, &record->lastLevel);
        glGetTexParameterfv(target, 0x1004, record->border);
        if (CDirect3DDevice_GL()->supportsAnisotropy)
            glGetTexParameterfv(target, 0x84fe, &record->anisotropy);
    }
    if (info && !record->info) {
        record->info = info;
        record->infoCache = DeviceDraw_SaveSamplerCache(info);
    }
}

static void DeviceDraw_BeginScope(DeviceDrawScope *scope, DeviceImpl *dev)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    memset(scope, 0, sizeof(*scope));
    scope->previous = sDeviceDrawScope;
    sDeviceDrawScope = scope;
    scope->units = dev->maxTextureUnits < 2 ? dev->maxTextureUnits : 2;
    scope->activeTextureCache = gl->activeTexUnit;
    scope->clientTextureCache = gl->clientActiveTexUnit;
    scope->matrixModeCache = gl->matrixMode;
    scope->programCache = gl->vertexProgram;
    for (UINT32 i = 0; i < 16; ++i)
        for (UINT32 j = 0; j < 3; ++j) {
            const GLuint *name = gl->texUnits[i].mTexID[j];
            scope->textureNameCache[i][j] = name;
            scope->textureNames[i][j] = name ? *name : 0;
        }
    glGetIntegerv(0x84e0, &scope->activeTexture);
    glGetIntegerv(0x84e1, &scope->clientTexture);
    glGetIntegerv(0x0ba0, &scope->matrixMode);
    scope->vaoCache = *(GLint *)imp_g_VAOID;
    glGetProgramivARB(0x8620, 0x8677, &scope->program[0]);
    glGetProgramivARB(0x8804, 0x8677, &scope->program[1]);
    glGetDoublev(0x0ba6, scope->modelView);
    glGetDoublev(0x0ba7, scope->projection);
    memcpy(scope->modelViewCache, gl->modelViewMatrix, sizeof(scope->modelViewCache));
    /* Current attributes (including ARB generic values), enables, materials,
     * raster/depth/blend and texture environments/initial bindings. Matrices
     * and ARB program bindings require the explicit storage above/below. */
    glPushAttrib(0x000471c9);
    /* APPLE_vertex_array_object restores the binding before its array data.
     * It also preserves buffer bindings and client-active texture selection. */
    glPushClientAttrib(0x00000002);
    for (UINT32 i = 0; i < scope->units; ++i) {
        CTexUnitNative *unit = &gl->texUnits[i];
        glActiveTextureARB(0x84c0 + i);
        glGetDoublev(0x0ba8, scope->textureMatrix[i]);
        scope->lodBias[i] = unit->mLodBias;
        memcpy(scope->targetCache[i], unit->mTargetEnabled, sizeof(scope->targetCache[i]));
        for (UINT32 j = 0; j < 3; ++j)
            scope->samplerCache[i][j] = (DeviceDrawSamplerCache){
                unit->mTexWrapS[j], unit->mTexWrapT[j], unit->mTexWrapR[j],
                unit->mTexMinFilter[j], unit->mTexMagFilter[j], unit->mTexBorderColor[j],
                unit->mTexLastLevel[j], unit->mTexAnisotropicFilter[j] };
    }
    glActiveTextureARB(scope->activeTexture);
}

static void DeviceDraw_EndScope(DeviceDrawScope *scope)
{
    COpenGLNative *gl = CDirect3DDevice_GL();
    glActiveTextureARB(0x84c0);
    for (UINT32 i = 0; i < scope->samplerCount; ++i) {
        const DeviceDrawSamplerRecord *record = &scope->samplers[i];
        GLenum target = record->target;
        glBindTexture(target, record->name);
        glTexParameteri(target, 0x2802, record->wrapS);
        glTexParameteri(target, 0x2803, record->wrapT);
        glTexParameteri(target, 0x8072, record->wrapR);
        glTexParameteri(target, 0x2801, record->minFilter);
        glTexParameteri(target, 0x2800, record->magFilter);
        glTexParameteri(target, 0x813d, record->lastLevel);
        glTexParameterfv(target, 0x1004, record->border);
        if (gl->supportsAnisotropy)
            glTexParameterf(target, 0x84fe, record->anisotropy);
        if (record->info) DeviceDraw_RestoreSamplerCache(record->info, &record->infoCache);
    }
    glMatrixMode(0x1700); glLoadMatrixd(scope->modelView);
    glMatrixMode(0x1701); glLoadMatrixd(scope->projection);
    for (UINT32 i = 0; i < scope->units; ++i) {
        glActiveTextureARB(0x84c0 + i);
        glMatrixMode(0x1702); glLoadMatrixd(scope->textureMatrix[i]);
    }
    glBindProgramARB(0x8620, scope->program[0]);
    glBindProgramARB(0x8804, scope->program[1]);
    glPopClientAttrib();
    glPopAttrib();
    /* Restore logical caches independently of GL state: an enclosing native
     * pixel/copy override may intentionally differ from persistent caches.
     * Name-cache pointers are borrowed and stable through uploads; restore
     * their identities only while their embedded name values are unchanged.
     * A newly generated name must be rebound by its next normal consumer. */
    gl->activeTexUnit = scope->activeTextureCache;
    gl->clientActiveTexUnit = scope->clientTextureCache;
    gl->matrixMode = scope->matrixModeCache;
    gl->vertexProgram = scope->programCache;
    for (UINT32 i = 0; i < 16; ++i)
        for (UINT32 j = 0; j < 3; ++j) {
            const GLuint *name = scope->textureNameCache[i][j];
            gl->texUnits[i].mTexID[j] = name && *name == scope->textureNames[i][j] ? name : NULL;
        }
    memcpy(gl->modelViewMatrix, scope->modelViewCache, sizeof(scope->modelViewCache));
    for (UINT32 i = 0; i < scope->units; ++i) {
        CTexUnitNative *unit = &gl->texUnits[i];
        unit->mLodBias = scope->lodBias[i];
        memcpy(unit->mTargetEnabled, scope->targetCache[i], sizeof(scope->targetCache[i]));
        for (UINT32 j = 0; j < 3; ++j) {
            const DeviceDrawSamplerCache *cache = &scope->samplerCache[i][j];
            unit->mTexWrapS[j] = cache->wrapS; unit->mTexWrapT[j] = cache->wrapT;
            unit->mTexWrapR[j] = cache->wrapR; unit->mTexMinFilter[j] = cache->minFilter;
            unit->mTexMagFilter[j] = cache->magFilter; unit->mTexBorderColor[j] = cache->border;
            unit->mTexLastLevel[j] = cache->lastLevel; unit->mTexAnisotropicFilter[j] = cache->anisotropy;
        }
    }
    glActiveTextureARB(scope->activeTexture);
    *(GLint *)imp_g_VAOID = scope->vaoCache;
    free(scope->samplers);
    sDeviceDrawScope = scope->previous;
}
