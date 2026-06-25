#include "common_types.h"
extern dvar_t *r_rendererInUse;
extern r_globals_t rg;
#include "imports.h"
extern int alwaysfails;
#include <string.h>
extern DxGlobals dx;
extern refimport_t ri;

extern void Com_Printf(const char *fmt, ...);
extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern const char *R_ErrorDescription(int hr);

extern const D3DVERTEXELEMENT9 declEnd;
extern unsigned char materialGlobals[];
extern const stream_source_info_t s_streamSourceInfo[];
extern const stream_dest_info_t s_streamDestInfo[];
extern r_global_permanent_t rgp;

#define ARRAY_COUNT(x) ((int)(sizeof(x) / sizeof((x)[0])))

static BuiltInMaterialTable s_fillTestMaterials[] = {
    { "filltest_normal", (_ValueType *)&rgp.normalFillMaterial },
    { "filltest_alphablend", (_ValueType *)&rgp.alphaBlendFillMaterial },
    { "filltest_alphatest_pass", (_ValueType *)&rgp.alphaTestPassFillMaterial },
    { "filltest_alphatest_fail", (_ValueType *)&rgp.alphaTestFailFillMaterial },
    { "filltest_additive", (_ValueType *)&rgp.additiveFillMaterial },
    { "filltest_depthonly", (_ValueType *)&rgp.depthOnlyFillMaterial },
    { "filltest_stencil_oneside_keep", (_ValueType *)&rgp.stencilOneSideKeepFillMaterial },
    { "filltest_stencil_oneside_reject", (_ValueType *)&rgp.stencilOneSideRejectFillMaterial },
    { "filltest_stencil_twoside_keep", (_ValueType *)&rgp.stencilTwoSideKeepFillMaterial },
    { "filltest_stencil_twoside_reject", (_ValueType *)&rgp.stencilTwoSideRejectFillMaterial },
    { "filltest_phong_bump", (_ValueType *)&rgp.phongBumpFillMaterial },
    { "filltest_phong_bumpspec", (_ValueType *)&rgp.phongBumpSpecFillMaterial },
};

BuiltInMaterialTable s_builtInMaterials[] = {
    { "$default", (_ValueType *)&rgp.defaultMaterial },
    { "stencil_plane", (_ValueType *)&rgp.stencilPlaneMaterial },
    { "white", (_ValueType *)&rgp.whiteMaterial },
    { "$additive", (_ValueType *)&rgp.additiveMaterial },
    { "$glare_blind", (_ValueType *)&rgp.glareBlindMaterial },
    { "$point", (_ValueType *)&rgp.pointMaterial },
    { "$line", (_ValueType *)&rgp.lineMaterial },
    { "clear_alpha_stencil", (_ValueType *)&rgp.clearAlphaStencilMaterial },
    { "shadowclear", (_ValueType *)&rgp.shadowClearMaterial },
    { "shadowcookieoverlay", (_ValueType *)&rgp.shadowCookieOverlayMaterial },
    { "shadowcookieblur", (_ValueType *)&rgp.shadowCookieBlurMaterial },
    { "shellshock", (_ValueType *)&rgp.shellShockMaterial },
    { "color_channel_mixer", (_ValueType *)&rgp.colorChannelMixerMaterial },
    { "frame_color_debug", (_ValueType *)&rgp.frameColorDebugMaterial },
    { "frame_alpha_debug", (_ValueType *)&rgp.frameAlphaDebugMaterial },
    { "feedbackblend", (_ValueType *)&rgp.feedbackBlendMaterial },
    { "feedbackreplace", (_ValueType *)&rgp.feedbackReplaceMaterial },
    { "glow_setup", (_ValueType *)&rgp.glowSetupMaterial },
    { "glow_apply_bloom", (_ValueType *)&rgp.glowApplyBloomMaterial },
    { "glow_apply_sky_bleed", (_ValueType *)&rgp.glowApplySkyBleedMaterial },
    { "filter_symmetric_1", (_ValueType *)&rgp.symmetricFilterMaterial[0] },
    { "filter_symmetric_2", (_ValueType *)&rgp.symmetricFilterMaterial[1] },
    { "filter_symmetric_3", (_ValueType *)&rgp.symmetricFilterMaterial[2] },
    { "filter_symmetric_4", (_ValueType *)&rgp.symmetricFilterMaterial[3] },
    { "filter_symmetric_5", (_ValueType *)&rgp.symmetricFilterMaterial[4] },
    { "filter_symmetric_6", (_ValueType *)&rgp.symmetricFilterMaterial[5] },
    { "filter_symmetric_7", (_ValueType *)&rgp.symmetricFilterMaterial[6] },
    { "filter_symmetric_8", (_ValueType *)&rgp.symmetricFilterMaterial[7] },
};

extern int R_HashAssetName(const char *name);
extern int stricmp(const char *s1, const char *s2);
extern void R_SetPicmip(void);
extern void Image_UpdatePicmip(GfxImage *image);
extern void Image_Release(GfxImage *image);
extern void Image_Reload(GfxImage *image);
extern Bool Image_LoadRaw(GfxImage *image, const char *filepath, int imageTrack);
extern void R_Error(int errorLevel, const char *msg, ...);

void *Material_Alloc(int size);
const float *Material_RegisterLiteral(const vec_t *literal);
static Bool Material_Compare(const Material *mtl0, const Material *mtl1);
void Material_SetTechnique(const char *name, MaterialTechnique *technique);
void Material_SetTechniqueSet(const char *name, MaterialTechniqueSet *techniqueSet);
void Material_SetStateMap(const char *name, MaterialStateMap *stateMap);
void Material_SetShader(const char *shaderName, MaterialShaderType shaderType, int shaderVersion, MaterialShader *mtlShader);
Bool Material_IsDefault(const Material *material);
Bool R_IsMaterialRefractive(MaterialHandle handle);
void Material_FinishLoading(void);
void Material_ReleaseAll(void);
void Material_UpdatePicmipAll(void);
int Material_LoadFile(const char *filename, fileHandle_t *file);
const char *R_GetMaterialName(_ValueType handle);
int R_GetMaterialSubimageCount(MaterialHandle handle);
void Material_Sort(void);
const char *Material_RegisterString(const char *string);
MaterialVertexDeclaration *Material_AllocVertexDecl(MaterialStreamRouting *routingData, int streamCount, Bool *existing);
MaterialStateMap *Material_FindStateMap(const char *name);
MaterialTechniqueSet *Material_FindTechniqueSet(const char *name);
MaterialTechnique *Material_FindTechnique(const char *name);
MaterialShader *Material_FindShader(const char *shaderName, MaterialShaderType shaderType, int shaderVersion);
void Material_Shutdown(void);
MaterialHandle R_RegisterRawImage(const char *name, int baseImageFlags, int imageTrack);
void Material_ReloadAll(void);
void Load_BuildVertexDecl(MaterialVertexDeclaration **mtlVertDecl);
void R_Cmd_ReloadMaterialTextures(void);
MaterialHandle Material_Duplicate(MaterialHandle mtlCopy, const char *name);
MaterialHandle Material_Register(const char *name, int imageTrack);
MaterialHandle Material_RegisterHandle(const char *name, int baseImageFlags, int imageTrack);
void Material_Init(void);
typedef unsigned char (*MaterialCompFunc)(const Material *, const Material *);
void ZSt13__adjust_heapIPP8MaterialiS1_PFhPKS0_S4_EEvT_T0_S8_T1_T2_(Material **first, int holeIndex, int len, Material *value, MaterialCompFunc comp);
void ZSt16__insertion_sortIPP8MaterialPFhPKS0_S4_EEvT_S7_T0_(Material **first, Material **last, MaterialCompFunc comp);
void ZSt16__introsort_loopIPP8MaterialiPFhPKS0_S4_EEvT_S7_T0_T1_(Material **first, Material **last, int depth_limit, MaterialCompFunc comp);
extern void FS_FCloseFile(fileHandle_t f);

static qboolean Material_IsUiLikeName(const char *name)
{
    return strncmp(name, "ui/", 3) == 0 ||
           strncmp(name, "ui_", 3) == 0 ||
           strncmp(name, "menu/", 5) == 0 ||
           strncmp(name, "levelshots/", 11) == 0 ||
           stricmp(name, "$levelbriefing") == 0;
}

static qboolean Material_HasImageExtension(const char *name)
{
    int len = (int)strlen(name);

    if (len <= 4 || name[len - 4] != '.')
        return 0;

    return stricmp(name + len - 4, ".tga") == 0 ||
           stricmp(name + len - 4, ".jpg") == 0 ||
           stricmp(name + len - 4, ".iwi") == 0;
}

static MaterialHandle Material_TryAliasWithoutExtension(const char *name, int imageTrack)
{
    char aliasName[64];
    fileHandle_t fileHandle;
    int fileSize;
    int len;

    if (!Material_HasImageExtension(name))
        return NULL;

    len = (int)strlen(name);
    if (len >= (int)sizeof(aliasName))
        return NULL;

    memcpy(aliasName, name, len - 4);
    aliasName[len - 4] = '\0';

    fileSize = Material_LoadFile(aliasName, &fileHandle);
    if (fileSize < 0)
        return NULL;

    FS_FCloseFile(fileHandle);
    return Material_Duplicate(Material_Register(aliasName, imageTrack), name);
}

void *Material_Alloc(int size)
{
    return ri.Hunk_AllocInternal(size);
}

const float *Material_RegisterLiteral(const vec_t *literal)
{
    int literalCount = *(int *)(materialGlobals + 0x230c);
    float *literals = (float *)(materialGlobals + 0x2310);
    int i;
    float *dest;

    if (literalCount > 0) {
        for (i = 0; i < literalCount; i++) {
            float *entry = literals + i * 4;
            if (entry[0] == literal[0] && entry[1] == literal[1] &&
                entry[2] == literal[2] && entry[3] == literal[3]) {
                return entry;
            }
        }
        if (literalCount == 16) {
            R_Error(1, "more than %i shader literals used", 16);
        }
    } else {
        literalCount = 0;
    }

    dest = literals + literalCount * 4;
    dest[0] = literal[0];
    dest[1] = literal[1];
    dest[2] = literal[2];
    dest[3] = literal[3];
    return dest;
}

static Bool Material_Compare(const Material *mtl0, const Material *mtl1)
{
    int diff = (int)mtl0->info.sortKey - (int)mtl1->info.sortKey;
    if (diff != 0)
        return (unsigned int)diff >> 31;
    diff = (int)((unsigned int)mtl0->techniqueSet - (unsigned int)mtl1->techniqueSet);
    if (diff == 0)
        return 0;
    return (unsigned int)diff >> 31;
}

void Material_SetTechnique(const char *name, MaterialTechnique *technique)
{
    if (*(int *)(materialGlobals + 0x1308) == 0x3ff) {
        R_Error(1, "More than %i techniques in use", 0x3ff);
    }

    int hash = R_HashAssetName(name) & 0x3ff;
    MaterialTechnique *entry;

    char *techniqueTable = (char *)(materialGlobals + 0x1300);

    entry = ((MaterialTechnique **)(materialGlobals + 0x130C))[hash];
    while (entry != NULL) {
        if (stricmp(entry->name, name) == 0)
            break;
        hash = (hash + 1) & 0x3ff;
        entry = ((MaterialTechnique **)(materialGlobals + 0x130C))[hash];
    }

    (*(int *)(materialGlobals + 0x1308))++;
    *(MaterialTechnique **)(techniqueTable + 0xc + hash * 4) = technique;
}

void Material_SetTechniqueSet(const char *name, MaterialTechniqueSet *techniqueSet)
{
    int hash = R_HashAssetName(name) & 0x3ff;
    MaterialTechniqueSet *entry;

    char *techSetTable = (char *)(materialGlobals + 0x300);

    entry = ((MaterialTechniqueSet **)(materialGlobals + 0x308))[hash];
    while (entry != NULL) {
        if (stricmp(entry->name, name) == 0)
            break;
        hash = (hash + 1) & 0x3ff;
        entry = ((MaterialTechniqueSet **)(materialGlobals + 0x308))[hash];
    }

    *(MaterialTechniqueSet **)(techSetTable + 8 + hash * 4) = techniqueSet;
}

void Material_SetStateMap(const char *name, MaterialStateMap *stateMap)
{
    int hash = R_HashAssetName(name) & 0x1f;
    MaterialStateMap *entry;

    char *stateMapTable = (char *)(materialGlobals + 0x2410);

    entry = ((MaterialStateMap **)(materialGlobals + 0x2414))[hash];
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0)
            break;
        hash = (hash + 1) & 0x1f;
        entry = ((MaterialStateMap **)(materialGlobals + 0x2414))[hash];
    }

    *(MaterialStateMap **)(stateMapTable + 4 + hash * 4) = stateMap;
}

void Material_SetShader(const char *shaderName, MaterialShaderType shaderType, int shaderVersion, MaterialShader *mtlShader)
{
    (*(int *)(materialGlobals + 0x2598))++;
    if (*(int *)(materialGlobals + 0x2598) == 0x100) {
        R_Error(1, "More than %i unique pixel and vertex shaders", 0xff);
    }

    int hash = R_HashAssetName(shaderName);
    hash = (hash + (int)shaderType * 97 + shaderVersion) & 0xff;

    char *shaderTable = (char *)(materialGlobals + 0x2590);

    MaterialShader *entry = ((MaterialShader **)(materialGlobals + 0x259C))[hash];
    while (entry != NULL) {
        if ((int)entry->shaderType == (int)shaderType &&
            (int)entry->shaderVersion == shaderVersion &&
            strcmp(entry->name, shaderName) == 0)
            break;
        hash = (hash + 1) & 0xff;
        entry = ((MaterialShader **)(materialGlobals + 0x259C))[hash];
    }

    *(MaterialShader **)(shaderTable + 0xc + hash * 4) = mtlShader;
}

Bool Material_IsDefault(const Material *material)
{
    const Material *defaultMtl = rgp.defaultMaterial;
    if (material->textures != defaultMtl->textures)
        return 0;
    if (material->constants != defaultMtl->constants)
        return 0;
    if (material->techniqueSet != defaultMtl->techniqueSet)
        return 0;
    return 1;
}

Bool R_IsMaterialRefractive(MaterialHandle handle)
{
    if (r_rendererInUse->current.integer != 2) {
        MaterialTechniqueSet *ts = handle->techniqueSet;
        MaterialTechnique *tech;
        if (!ts)
            return 0;
        tech = ts->techniques[21];
        if (tech && (tech->flags & 1))
            return 1;
    }
    return 0;
}

void Material_FinishLoading(void)
{
}

extern void RB_ReleaseVertexDecl(void);

void Material_ReleaseAll(void)
{
    byte *outer;
    int j;

    RB_ReleaseVertexDecl();

    for (outer = materialGlobals + 4; outer != materialGlobals + 0x304; outer += 0x18) {
        byte *slot = outer;
        for (j = 4; j != 0; j--) {
            void **pObj = (void **)(slot + 8);
            if (*pObj) {
                do {
                    void *obj = *pObj;
                    void **vtable = *(void ***)obj;
                    ((void (*)(void *))vtable[2])(obj);
                    *pObj = NULL;
                } while (*(int *)&alwaysfails);
            }
            slot += 4;
        }
    }

    {
        byte *sh;
        for (sh = materialGlobals; sh != materialGlobals + 0x400; sh += 4) {
            void *shader = *(void **)(sh + 0x259c);
            if (shader) {
                void *obj = *(void **)((char *)shader + 0xc);
                void **vtable = *(void ***)obj;
                ((void (*)(void *))vtable[2])(obj);
            }
        }
    }
}

void Material_UpdatePicmipAll(void)
{
    byte *rg;
    byte *slot;
    int textureIndex, textureCount;
    byte *material;
    byte *texdef;
    GfxImage *image;
    int offset;

    R_SetPicmip();

    rg = (byte *)imp_rg;

    for (slot = rg; slot != rg + sizeof(((r_globals_t *)0)->materialHashTable); slot += 4) {
        material = *(byte **)(slot + 0x28);
        if (!material)
            continue;

        textureCount = ((Material *)material)->textureCount;
        if (textureCount <= 0)
            continue;

        textureIndex = 0;
        offset = 0;
        do {
            texdef = (byte *)((Material *)material)->textures + offset;

            if (((MaterialTextureDef *)texdef)->semantic != 5) {
                image = ((MaterialTextureDef *)texdef)->u.image;
                if (image) {
                    Image_UpdatePicmip(image);

                    textureCount = ((Material *)material)->textureCount;
                }
            }
            textureIndex++;
            offset += 0xc;
        } while (textureIndex < textureCount);
    }
}

extern int FS_FOpenFileRead(const char *filename, int *file, int uniqueFILE);
int Material_LoadFile(const char *filename, int *file)
{
    char fullFilename[64];
    Com_sprintf(fullFilename, 64, "materials/%s", filename);
    return ri.FS_FOpenFileRead(fullFilename, file, 1);
}

const char *R_GetMaterialName(_ValueType handle)
{
    return *(const char **)(*(int *)&handle);
}

int R_GetMaterialSubimageCount(MaterialHandle handle)
{
    return handle->info.textureAtlasColumnCount * handle->info.textureAtlasRowCount;
}

void Material_Sort(void)
{
    r_global_permanent_t *rgpPtr = (r_global_permanent_t *)imp_rgp;
    int count = rgpPtr->materialCount;
    Material **first = rgpPtr->sortedMaterials;
    Material **last = first + count;
    int i;

    if (first != last) {
        int byteRange = (int)((byte *)last - (byte *)first);
        int n = byteRange >> 2;
        int depthLimit = 0;

        if (n != 1) {
            int tmp = n;
            do {
                depthLimit++;
                tmp >>= 1;
            } while (tmp != 1);
            depthLimit *= 2;
        }

        ZSt16__introsort_loopIPP8MaterialiPFhPKS0_S4_EEvT_S7_T0_T1_(
            first, last, depthLimit, Material_Compare);

        if (byteRange <= 0x43) {

            ZSt16__insertion_sortIPP8MaterialPFhPKS0_S4_EEvT_S7_T0_(
                first, last, Material_Compare);
        } else {

            Material **mid = first + 16;
            ZSt16__insertion_sortIPP8MaterialPFhPKS0_S4_EEvT_S7_T0_(
                first, mid, Material_Compare);

            while (mid != last) {
                Material *val = *mid;
                Material **hole = mid;
                Material **prev = mid - 1;
                while (Material_Compare(val, *prev)) {
                    *hole = *prev;
                    hole = prev;
                    prev--;
                }
                *hole = val;
                mid++;
            }
        }
    }

    rgpPtr = (r_global_permanent_t *)imp_rgp;
    for (i = 0; i < rgpPtr->materialCount; i++) {
        rgpPtr->sortedMaterials[i]->info.sortedIndex = (unsigned short)i;
    }
}

extern int R_HashString(const char *string);
extern void R_Error(int level, const char *msg, ...);

const char *Material_RegisterString(const char *string)
{
    int hash = R_HashString(string) & 0x3f;
    const char *existing;
    int count;
    int nameLen;
    void *(*hunkAlloc)(int);
    char *copy;

    existing = ((const char **)(materialGlobals + 0x2498))[hash];
    while (existing) {
        if (strcmp(existing, string) == 0)
            return existing;
        hash = (hash + 1) & 0x3f;
        existing = ((const char **)(materialGlobals + 0x2498))[hash];
    }

    count = *(int *)(materialGlobals + 0x2494) + 1;
    *(int *)(materialGlobals + 0x2494) = count;
    if (count == 64) {
        R_Error(1, "More than %i string identifiers used by shaders", 63);
    }

    nameLen = strlen(string) + 1;
    hunkAlloc = ri.Hunk_AllocInternal;
    copy = (char *)hunkAlloc(nameLen);
    memcpy(copy, string, nameLen);

    {
        char *stringIdentTable = (char *)(materialGlobals + 0x2490);
        *(const char **)(stringIdentTable + 8 + hash * 4) = copy;
    }

    return copy;
}

MaterialVertexDeclaration *Material_AllocVertexDecl(MaterialStreamRouting *routingData, int streamCount, Bool *existing)
{
    int dataSize = streamCount * 2;
    int hash = 0;
    int i;
    byte *routingBytes = (byte *)routingData;
    byte *mvd;
    byte *data;
    void *(*hunkAlloc)(int);

    for (i = 0; i < dataSize; i++) {
        hash += (i + 0x77) * routingBytes[i];
    }
    hash &= 0x1f;

    for (;;) {
        mvd = (byte *)&materialGlobals[hash * 24 + 4];
        data = *(byte **)mvd;

        if (!data)
            break;

        if (*(int *)(mvd + 4) == streamCount &&
            memcmp(data, routingData, dataSize) == 0) {
            *existing = 1;
            return (MaterialVertexDeclaration *)mvd;
        }

        hash = (hash + 1) & 0x1f;
    }

    if (*(int *)materialGlobals == 0x1f) {
        R_Error(1, "More than %i vertex declarations in use", 31);
    }
    (*(int *)materialGlobals)++;

    hunkAlloc = ri.Hunk_AllocInternal;
    data = (byte *)hunkAlloc(dataSize);
    memcpy(data, routingData, dataSize);

    memset(mvd, 0, 24);
    *(byte **)(mvd + 0) = data;
    *(int *)(mvd + 4) = streamCount;

    *existing = 0;
    return (MaterialVertexDeclaration *)mvd;
}

MaterialStateMap *Material_FindStateMap(const char *name)
{
    int hash = R_HashAssetName(name) & 0x1f;
    while (((MaterialStateMap **)(materialGlobals + 0x2414))[hash]) {
        if (strcmp(((MaterialStateMap **)(materialGlobals + 0x2414))[hash]->name, name) == 0)
            return ((MaterialStateMap **)(materialGlobals + 0x2414))[hash];
        hash = (hash + 1) & 0x1f;
    }
    return NULL;
}

MaterialTechniqueSet *Material_FindTechniqueSet(const char *name)
{
    int hash = R_HashAssetName(name) & 0x3ff;
    while (((MaterialTechniqueSet **)(materialGlobals + 0x308))[hash]) {
        if (stricmp(((MaterialTechniqueSet **)(materialGlobals + 0x308))[hash]->name, name) == 0)
            return ((MaterialTechniqueSet **)(materialGlobals + 0x308))[hash];
        hash = (hash + 1) & 0x3ff;
    }
    return NULL;
}

MaterialTechnique *Material_FindTechnique(const char *name)
{
    int hash = R_HashAssetName(name) & 0x3ff;
    while (((MaterialTechnique **)(materialGlobals + 0x130C))[hash]) {
        if (stricmp(((MaterialTechnique **)(materialGlobals + 0x130C))[hash]->name, name) == 0)
            return ((MaterialTechnique **)(materialGlobals + 0x130C))[hash];
        hash = (hash + 1) & 0x3ff;
    }
    return NULL;
}

MaterialShader *Material_FindShader(const char *shaderName, MaterialShaderType shaderType, int shaderVersion)
{
    int hash = (R_HashAssetName(shaderName) + shaderType * 97 + shaderVersion) & 0xff;
    while (((MaterialShader **)(materialGlobals + 0x259C))[hash]) {
        MaterialShader *entry = ((MaterialShader **)(materialGlobals + 0x259C))[hash];
        if (entry->shaderType == shaderType && entry->shaderVersion == shaderVersion && strcmp(entry->name, shaderName) == 0)
            return ((MaterialShader **)(materialGlobals + 0x259C))[hash];
        hash = (hash + 1) & 0xff;
    }
    return NULL;
}

void Material_Shutdown(void)
{
    byte *outer;
    int j;
    byte *p;

    RB_ReleaseVertexDecl();

    for (outer = materialGlobals + 4; outer != materialGlobals + 0x304; outer += 0x18) {
        byte *slot = outer;
        for (j = 0; j < 4; j++) {
            void **pObj = (void **)(slot + 8);
#ifdef GFX_REAL_D3D9

            *pObj = NULL;
            slot += 4;
            continue;
#endif
            if (*pObj) {
                do {
                    void *obj = *pObj;
                    void **vtable = *(void ***)obj;
                    ((void (*)(void *))vtable[2])(obj);
                    *pObj = NULL;
                } while (*(int *)&alwaysfails);
            }
            slot += 4;
        }
    }

    memset(materialGlobals + 4, 0, 0x300);
    *(int *)materialGlobals = 0;

    for (p = materialGlobals; p != materialGlobals + 0x400; p += 4) {
        void *shader = *(void **)(p + 0x259c);
#ifdef GFX_REAL_D3D9

        (void)shader;
        continue;
#else
        if (shader) {
            void *obj = *(void **)((char *)shader + 0xc);
            void **vtable = *(void ***)obj;
            ((void (*)(void *))vtable[2])(obj);
        }
#endif
    }

    for (p = materialGlobals; p != materialGlobals + 0x400; p += 4)
        *(void **)(p + 0x259c) = NULL;

    *(int *)(materialGlobals + 0x230C) = 0;

    for (p = materialGlobals; p != materialGlobals + 0x100; p += 4) {
        if (*(void **)(p + 0x2498))
            *(void **)(p + 0x2498) = NULL;
    }

    for (p = materialGlobals; p != materialGlobals + 0x80; p += 4) {
        if (*(void **)(p + 0x2414))
            *(void **)(p + 0x2414) = NULL;
    }

    for (p = materialGlobals; p != materialGlobals + 0x1000; p += 4) {
        if (*(void **)(p + 0x130c))
            *(void **)(p + 0x130c) = NULL;
    }

    for (p = materialGlobals; p != materialGlobals + 0x1000; p += 4) {
        if (*(void **)(p + 0x308))
            *(void **)(p + 0x308) = NULL;
    }

    memset(materialGlobals, 0, 0x299c);
    memset(rg.materialHashTable, 0, sizeof(rg.materialHashTable));
    rgp.materialCount = 0;
}

MaterialHandle R_RegisterRawImage(const char *name, int baseImageFlags, int imageTrack)
{
    r_global_permanent_t *rgp;
    Material *rawMaterial;
    Material *defaultMaterial;

    if (*name == '\0')
        return rgp->defaultMaterial;

    rgp = (r_global_permanent_t *)imp_rgp;
    rawMaterial = rgp->rawMaterial;
    defaultMaterial = rgp->defaultMaterial;

    if (rawMaterial->textures == defaultMaterial->textures &&
        rawMaterial->constants == defaultMaterial->constants &&
        rawMaterial->techniqueSet == defaultMaterial->techniqueSet) {
        return rawMaterial;
    }

    Image_Release(rgp->rawImage);
    if (!Image_LoadRaw(rgp->rawImage, name, imageTrack))
        return rgp->defaultMaterial;
    return rgp->rawMaterial;
}

void Material_ReloadAll(void)
{
    char *dx = (char *)imp_dx;

    byte *vertDeclPtr = materialGlobals + 4;
    byte *routingPtr = materialGlobals + 8;
    int i;

    while (vertDeclPtr < materialGlobals + 772) {
        int routingCount = *(int *)(routingPtr - 4);
        if (routingCount != 0) {

            const byte *sourceInfoBase = (const byte *)s_streamSourceInfo;
            int vertDeclType;

            for (vertDeclType = 0; vertDeclType < 4; vertDeclType++) {
                const byte *sourceInfo = sourceInfoBase;
                int elemCount = *(int *)(routingPtr);
                const byte *routingData = *(const byte **)(routingPtr - 4);
                D3DVERTEXELEMENT9 elemTable[256];
                void *decl = NULL;
                int numElems = 0;
                int routingIdx;
                const byte *rp = routingData;

                if (elemCount == 0)
                    goto writeEnd2;

                for (routingIdx = 0; routingIdx < elemCount; routingIdx++) {
                    byte sourceIdx = rp[0];
                    byte destIdx = rp[1];
                    const byte *src = sourceInfo + sourceIdx * 3;
                    const byte *dest = (const byte *)s_streamDestInfo + destIdx * 2;
                    int insertPos;

                    rp += 2;

                    if (src[0] == 0xFF) {
                        decl = NULL;
                        goto storeDecl2;
                    }

                    insertPos = numElems;
                    if (numElems > 0 && (unsigned)elemTable[numElems - 1].Stream > (unsigned)src[0]) {
                        int j = numElems - 1;
                        while (j > 0) {
                            elemTable[j] = elemTable[j - 1];
                            if ((unsigned)elemTable[j - 1].Stream <= (unsigned)src[0])
                                break;
                            j--;
                        }
                        insertPos = j;
                    }

                    *(unsigned short *)&elemTable[insertPos].Stream = (unsigned short)src[0];
                    *(unsigned short *)&elemTable[insertPos].Offset = (unsigned short)src[1];
                    ((byte *)&elemTable[insertPos])[4] = src[2];
                    ((byte *)&elemTable[insertPos])[5] = 0;
                    ((byte *)&elemTable[insertPos])[6] = dest[0];
                    ((byte *)&elemTable[insertPos])[7] = dest[1];
                    numElems++;
                }

            writeEnd2:
                *(int *)&elemTable[numElems] = *(int *)&declEnd;

                memcpy((byte *)&elemTable[numElems] + 4, (byte *)&declEnd + 4, 4);

                do {
                    void *device = (void *)((DxGlobals *)dx)->device;
                    void **vtable = *(void ***)device;
                    ((int (*)(void *, const void *, void **))vtable[0x158 / 4])(
                        device, elemTable, &decl);
                } while (*(int *)&alwaysfails);

            storeDecl2:
                *(void **)(vertDeclPtr + 8 + vertDeclType * 4) = decl;
                sourceInfoBase += 21;
            }
        }

        vertDeclPtr += 0x18;
        routingPtr += 0x18;
    }

    {
        byte *matSlot = materialGlobals;
        byte *matEnd = materialGlobals + 1020;
        void (*ri_Printf)(int, const char *, ...) = (void (*)(int, const char *, ...))ri.Error;

        for (; matSlot < matEnd; matSlot += 4) {

            byte *shader = *(byte **)(matSlot + 0x259c);
            int hr;

            if (!shader)
                continue;

            if (((MaterialShader *)shader)->shaderType == 0) {

                void *device = (void *)((DxGlobals *)dx)->device;
                void **vtable = *(void ***)device;
                hr = ((int (*)(void *, const void *, void **))vtable[0x16c / 4])(
                    device, ((MaterialShader *)shader)->program, (void **)&((MaterialShader *)shader)->u);
            } else {

                void *device = (void *)((DxGlobals *)dx)->device;
                void **vtable = *(void ***)device;
                hr = ((int (*)(void *, const void *, void **))vtable[0x1a8 / 4])(
                    device, ((MaterialShader *)shader)->program, (void **)&((MaterialShader *)shader)->u);
            }

            if (hr < 0) {
                ri_Printf(0, "Couldn't reload shader '%s' when recovering from a lost devi",
                          *(const char **)shader, R_ErrorDescription(hr), hr);
            }
        }
    }
}

void Load_BuildVertexDecl(MaterialVertexDeclaration **mtlVertDecl)
{
    const byte *sourceInfoBase = (const byte *)s_streamSourceInfo;
    int vertDeclType;

    for (vertDeclType = 0; vertDeclType < 4; vertDeclType++) {
        MaterialVertexDeclaration *vd = *mtlVertDecl;
        int elemCount = vd->streamCount;
        const byte *routingData = *(const byte **)vd;
        D3DVERTEXELEMENT9 elemTable[256];
        void *decl = NULL;
        int numElems = 0;
        int routingIdx;
        const byte *routingPtr;
        const byte *sourceInfo;

        sourceInfo = sourceInfoBase;
        routingPtr = routingData;

        if (elemCount == 0)
            goto writeEnd;

        for (routingIdx = 0; routingIdx < elemCount; routingIdx++) {
            byte sourceIdx = routingPtr[0];
            byte destIdx = routingPtr[1];
            const byte *src = sourceInfo + sourceIdx * 3;
            const byte *dest = (const byte *)s_streamDestInfo + destIdx * 2;
            int insertPos;

            routingPtr += 2;

            if (src[0] == 0xFF) {
                decl = NULL;
                goto storeDecl;
            }

            insertPos = numElems;
            if (numElems > 0 && (unsigned)elemTable[numElems - 1].Stream > (unsigned)src[0]) {

                int j = numElems - 1;
                while (j > 0) {
                    int prev = j - 1;
                    elemTable[j] = elemTable[j - 1];
                    if ((unsigned)elemTable[prev].Stream <= (unsigned)src[0])
                        break;
                    j = prev;
                }
                insertPos = j;
            }

            *(unsigned short *)&elemTable[insertPos].Stream = (unsigned short)src[0];
            *(unsigned short *)&elemTable[insertPos].Offset = (unsigned short)src[1];
            ((byte *)&elemTable[insertPos])[4] = src[2];
            ((byte *)&elemTable[insertPos])[5] = 0;
            ((byte *)&elemTable[insertPos])[6] = dest[0];
            ((byte *)&elemTable[insertPos])[7] = dest[1];
            numElems++;
        }

    writeEnd:

        *(int *)&elemTable[numElems] = *(int *)&declEnd;

        memcpy((byte *)&elemTable[numElems] + 4, (byte *)&declEnd + 4, 4);

        do {
            void *device = dx.device;
            void **vtable = *(void ***)device;
            ((int (*)(void *, const void *, void **))vtable[0x158 / 4])(
                device, elemTable, &decl);
        } while (*(int *)&alwaysfails);

    storeDecl:

        vd->decl[vertDeclType] = (void (*)())decl;

        sourceInfoBase += 21;
    }
}

extern void RB_BindDefaultImages(void);
extern const char *va(const char *fmt, ...);

void R_Cmd_ReloadMaterialTextures(void)
{
    r_globals_t *rgg = (r_globals_t *)imp_rg;
    const char *name;
    byte *rg;
    int hash;
    byte *existing;
    Material *material;
    int textureCount, i;
    byte *texdefs;
    GfxImage *best;
    GfxImage *lastReloaded;

    if (ri.Cmd_Argc() != 2) {
        ri.Printf(0, "Usage: reloadMaterialTextures <materialName>\n");
        return;
    }

    name = ri.Cmd_Argv(1);

    hash = R_HashAssetName(name);
    hash &= 0x3ff;
    existing = (byte *)rgg->materialHashTable[hash];
    while (existing) {
        if (strcmp(*(const char **)existing, name) == 0)
            break;
        hash = (hash + 1) & 0x3ff;
        existing = (byte *)rgg->materialHashTable[hash];
    }

    material = existing ? rgg->materialHashTable[hash] : NULL;

    if (!material) {
        ri.Printf(0, "%s", va("ReloadMaterialTextures: Material '%s' is not currently loaded\n", name));
        return;
    }

    RB_BindDefaultImages();

    textureCount = material->textureCount;
    if (textureCount == 0)
        return;

    texdefs = (byte *)material->textures;
    lastReloaded = NULL;

    for (;;) {
        best = NULL;
        for (i = 0; i < textureCount; i++) {
            byte *texdef = texdefs + i * 0xc;
            GfxImage *img;

            if (((MaterialTextureDef *)texdef)->semantic == 5)
                continue;

            img = ((MaterialTextureDef *)texdef)->u.image;
            if (!lastReloaded) {

                if ((unsigned int)img > (unsigned int)best)
                    best = img;
            } else {

                if ((unsigned int)img > (unsigned int)best &&
                    (unsigned int)img < (unsigned int)lastReloaded)
                    best = img;
            }
        }

        if (!best)
            return;

        Image_Reload(best);
        lastReloaded = best;
    }
}

MaterialHandle Material_Duplicate(MaterialHandle mtlCopy, const char *name)
{
    r_global_permanent_t *rgpg = (r_global_permanent_t *)imp_rgp;
    r_globals_t *rgg = (r_globals_t *)imp_rg;
    byte *rg = (byte *)imp_rg;
    byte *rgp;
    int hash = R_HashAssetName(name) & 0x3ff;
    byte *material;
    byte *existing;
    int nameLen;
    void *(*hunkAlloc)(int);
    char *nameDst;
    int count;

    existing = (byte *)rgg->materialHashTable[hash];
    while (existing) {
        if (strcmp(*(const char **)existing, name) == 0) {

            memcpy(existing, (void *)mtlCopy, sizeof(Material));
            rgpg->materialLoaded = 1;
            return (MaterialHandle)existing;
        }
        hash = (hash + 1) & 0x3ff;
        existing = (byte *)rgg->materialHashTable[hash];
    }

    nameLen = strlen(name) + 1;
    hunkAlloc = ri.Hunk_AllocInternal;
    material = (byte *)hunkAlloc(0x44 + nameLen);

    memcpy(material, (void *)mtlCopy, sizeof(Material));

    nameDst = (char *)(material + sizeof(Material));
    ((Material *)material)->info.name = nameDst;
    memcpy(nameDst, name, nameLen);

    {
        r_global_permanent_t *rgpPtr = (r_global_permanent_t *)imp_rgp;
        rgpPtr->materialLoaded = 1;
        ((Material *)material)->info.hashIndex = (unsigned short)hash;
        count = rgpPtr->materialCount;
        ((Material *)material)->info.sortedIndex = (unsigned short)count;
        rgpPtr->sortedMaterials[count] = (Material *)material;
        rgg->materialHashTable[hash] = (Material *)material;
        count++;
        rgpPtr->materialCount = count;
    }

    if (count == 0x400)
        R_Error(0, "Too many unique materials (%i or more)\n", 0x400);

    return (MaterialHandle)material;
}

extern Material *Material_Load(const char *name, int imageTrack);

MaterialHandle Material_Register(const char *name, int imageTrack)
{
    r_globals_t *rgg = (r_globals_t *)imp_rg;
    byte *rg;
    byte *rgp;
    int hash = R_HashAssetName(name) & 0x3ff;
    byte *existing;
    Material *material;
    int count;

    existing = (byte *)rgg->materialHashTable[hash];
    while (existing) {
        if (strcmp(*(const char **)existing, name) == 0) {

            return (MaterialHandle)rgg->materialHashTable[hash];
        }
        hash = (hash + 1) & 0x3ff;
        existing = (byte *)rgg->materialHashTable[hash];
    }

    material = Material_Load(name, imageTrack);
    if (!material) {
        r_global_permanent_t *rgpPtr = (r_global_permanent_t *)imp_rgp;
        if (!rgpPtr->defaultMaterial)
            R_Error(0, "couldn't load material '$default'");
        Com_Printf("^3WARNING: Could not find material '%s'\n", name);
        return Material_Duplicate(rgpPtr->defaultMaterial, name);
    }

    r_global_permanent_t *rgpPtr = (r_global_permanent_t *)imp_rgp;
    rgpPtr->materialLoaded = 1;
    material->info.hashIndex = (unsigned short)hash;
    count = rgpPtr->materialCount;
    material->info.sortedIndex = (unsigned short)count;
    rgpPtr->sortedMaterials[count] = material;
    rgg->materialHashTable[hash] = material;
    count++;
    rgpPtr->materialCount = count;

    if (count == 0x400)
        R_Error(0, "Too many unique materials (%i or more)\n", 0x400);

    return (MaterialHandle)material;
}

MaterialHandle Material_RegisterHandle(const char *name, int baseImageFlags, int imageTrack)
{

    {
        extern int g_gfxV60DllActive;
        extern refexport_t re;
        if (g_gfxV60DllActive && re.RegisterMaterial && (void *)re.RegisterMaterial != (void *)Material_RegisterHandle)
            return re.RegisterMaterial(name, baseImageFlags, imageTrack);
    }
    (void)baseImageFlags;
    if (!name || *name == '\0')
        return rgp.defaultMaterial;
    return Material_Register(name, imageTrack);
}

extern void Material_PreLoadAllShaderText(void);
extern void Com_Error(int errorLevel, const char *msg, ...);
void Material_Init(void)
{
    byte *rgp_ptr = (byte *)imp_rgp;

    memset(materialGlobals, 0, 0x299c);
    Material_PreLoadAllShaderText();

    BuiltInMaterialTable *entry;
    for (entry = s_builtInMaterials; entry < s_builtInMaterials + ARRAY_COUNT(s_builtInMaterials); entry++) {
        *(Material **)entry->material = Material_Register(entry->name, 0);
        if (!*(Material **)entry->material)
            Com_Error(0, "Could not find material '%s'", entry->name);
    }

    if ((*(const dvar_t **)imp_r_testFillEnable)->current.enabled) {
        for (entry = s_fillTestMaterials; entry < s_fillTestMaterials + ARRAY_COUNT(s_fillTestMaterials); entry++) {
            *(Material **)entry->material = Material_Register(entry->name, 0);
            if (!*(Material **)entry->material)
                Com_Error(0, "Could not find material '%s'", entry->name);
        }
    }

    Material *rawMaterial = Material_Register("$raw", 0);
    ((r_global_permanent_t *)rgp_ptr)->rawMaterial = rawMaterial;
    Material *defaultMaterial = ((r_global_permanent_t *)rgp_ptr)->defaultMaterial;

    if (rawMaterial->textures != defaultMaterial->textures ||
        rawMaterial->constants != defaultMaterial->constants ||
        rawMaterial->techniqueSet != defaultMaterial->techniqueSet) {
        if (rawMaterial->textureCount != 1) {

            ri.Printf(3, "$raw material is not compatible with $default");
            rawMaterial = defaultMaterial;
            ((r_global_permanent_t *)rgp_ptr)->rawMaterial = defaultMaterial;
        }
    }

    rawMaterial = ((r_global_permanent_t *)rgp_ptr)->rawMaterial;
    defaultMaterial = ((r_global_permanent_t *)rgp_ptr)->defaultMaterial;

    if (rawMaterial->textures != defaultMaterial->textures ||
        rawMaterial->constants != defaultMaterial->constants ||
        rawMaterial->techniqueSet != defaultMaterial->techniqueSet) {

        *(int *)&((r_global_permanent_t *)rgp_ptr)->rawTexdef = *(int *)rawMaterial->textures;
        rawMaterial->textures = &((r_global_permanent_t *)rgp_ptr)->rawTexdef;
    }
}

void ZSt13__adjust_heapIPP8MaterialiS1_PFhPKS0_S4_EEvT_T0_S8_T1_T2_(
    Material **first, int holeIndex, int len, Material *value, MaterialCompFunc comp)
{
    int topIndex = holeIndex;
    int secondChild = 2 * holeIndex + 2;
    while (secondChild < len) {
        if (comp(first[secondChild], first[secondChild - 1]))
            secondChild--;
        first[holeIndex] = first[secondChild];
        holeIndex = secondChild;
        secondChild = 2 * secondChild + 2;
    }
    if (secondChild == len) {
        first[holeIndex] = first[len - 1];
        holeIndex = len - 1;
    }
    {
        int parent = (holeIndex - 1) / 2;
        while (holeIndex > topIndex && comp(first[parent], value)) {
            first[holeIndex] = first[parent];
            holeIndex = parent;
            parent = (holeIndex - 1) / 2;
        }
    }
    first[holeIndex] = value;
}

void ZSt16__insertion_sortIPP8MaterialPFhPKS0_S4_EEvT_S7_T0_(
    Material **first, Material **last, MaterialCompFunc comp)
{
    Material **i;
    if (first == last)
        return;
    for (i = first + 1; i != last; i++) {
        Material *val = *i;
        if (comp(val, *first)) {
            unsigned int n = (unsigned int)((char *)i - (char *)first) & ~3u;
            memmove(first + 1, first, n);
            *first = val;
        } else {
            Material **prev = i - 1;
            Material **hole = i;
            while (comp(val, *prev)) {
                *hole = *prev;
                hole = prev;
                prev--;
            }
            *hole = val;
        }
    }
}

void ZSt16__introsort_loopIPP8MaterialiPFhPKS0_S4_EEvT_S7_T0_T1_(
    Material **first, Material **last, int depth_limit, MaterialCompFunc comp)
{
    while (last - first > 16) {
        if (depth_limit == 0) {

            if (last - first < 2)
                return;

            {
                int len = (int)(last - first);
                int parent = (len - 2) / 2;
                for (;;) {
                    ZSt13__adjust_heapIPP8MaterialiS1_PFhPKS0_S4_EEvT_T0_S8_T1_T2_(
                        first, parent, len, first[parent], comp);
                    if (parent == 0)
                        break;
                    parent--;
                }
            }

            do {
                Material *value = *(last - 1);
                *(last - 1) = *first;
                ZSt13__adjust_heapIPP8MaterialiS1_PFhPKS0_S4_EEvT_T0_S8_T1_T2_(
                    first, 0, (int)((last - 1) - first), value, comp);
                last--;
            } while (last - first > 1);
            return;
        }
        depth_limit--;
        {
            int n = (int)(last - first);
            Material **midPtr = first + n / 2;
            Material **pivotPtr;
            Material *pivot;
            Material **lo, **hi;
            if (comp(*first, *midPtr)) {
                if (comp(*midPtr, *(last - 1)))
                    pivotPtr = midPtr;
                else if (comp(*first, *(last - 1)))
                    pivotPtr = last - 1;
                else
                    pivotPtr = first;
            } else {
                if (comp(*first, *(last - 1)))
                    pivotPtr = first;
                else if (comp(*(last - 1), *midPtr))
                    pivotPtr = midPtr;
                else
                    pivotPtr = last - 1;
            }
            pivot = *pivotPtr;
            lo = first;
            hi = last;
            for (;;) {
                while (comp(*lo, pivot))
                    lo++;
                hi--;
                while (comp(pivot, *hi))
                    hi--;
                if (lo >= hi)
                    break;
                {
                    Material *tmp = *lo;
                    *lo = *hi;
                    *hi = tmp;
                }
                lo++;
            }
            ZSt16__introsort_loopIPP8MaterialiPFhPKS0_S4_EEvT_S7_T0_T1_(
                lo, last, depth_limit, comp);
            last = lo;
        }
    }
}

const unsigned char _rd_s_streamSourceInfo[84] __asm__("s_streamSourceInfo") = {
    0x00, 0x00, 0x03, 0x00, 0x10, 0x02, 0x00, 0x1c, 0x04, 0x00, 0x20, 0x01, 0xff, 0x00, 0x00, 0x00,
    0x28, 0x02, 0x00, 0x34, 0x02, 0x00, 0x00, 0x02, 0x00, 0x0c, 0x02, 0x00, 0x18, 0x04, 0x00, 0x1c,
    0x01, 0x00, 0x24, 0x01, 0x00, 0x2c, 0x02, 0x00, 0x38, 0x02, 0x00, 0x00, 0x02, 0xff, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00, 0x0c, 0x01, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x02, 0x00, 0x0c, 0x02, 0x00, 0x18, 0x04, 0x00, 0x1c, 0x01, 0x00, 0x24, 0x06, 0x00, 0x28,
    0x02, 0x00, 0x34, 0x02
};

const unsigned char _rd_s_streamDestInfo[24] __asm__("s_streamDestInfo") = {
    0x00, 0x00, 0x03, 0x00, 0x0a, 0x00, 0x0a, 0x01, 0x05, 0x00, 0x05, 0x01, 0x05, 0x02, 0x05, 0x03,
    0x05, 0x04, 0x05, 0x05, 0x05, 0x06, 0x05, 0x07
};

const unsigned char declEnd_131795[20] __asm__("declEnd") = {
    0xff, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};
