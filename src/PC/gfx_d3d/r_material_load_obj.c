#include "common_types.h"
#include "imports.h"
#include "bytematch.h"

#define MATERIAL_REGPARM2_ABI COD2_REGPARM(2)
#define MATERIAL_REGPARM3_ABI COD2_REGPARM(3)

extern unsigned char mtlLoadGlob[];
extern dvar_t *r_rendererInUse;

#if defined(__x86_64__)
#    define MTLGLOB_COUNT (*(int *)mtlLoadGlob)
#    define MTLGLOB_PTR(T) (*(T **)(mtlLoadGlob + 8))
#    define MTLGLOB_SET_PTR(p) (*(void **)(mtlLoadGlob + 8) = (void *)(p))
#else
#    define MTLGLOB_COUNT (*(int *)mtlLoadGlob)
#    define MTLGLOB_PTR(T) (*(T **)(mtlLoadGlob + 4))
#    define MTLGLOB_SET_PTR(p) (*(int *)(mtlLoadGlob + 4) = (int)(p))
#endif
extern const unsigned char g_useTechnique[];

#include "r_material_load_obj_data.h"
extern DxGlobals dx;
extern const CodeSamplerSource s_lightSamplers[];
extern const CodeSamplerSource s_lightmapSamplers[];
extern const CodeSamplerSource s_lightGridSamplers[];
extern const CodeSamplerSource s_codeSamplers[];
extern const CodeSamplerSource s_defaultCodeSamplers[];
extern const CodeConstantSource s_lightConsts[];
extern const CodeConstantSource s_cameraConsts[];
extern const CodeConstantSource s_nearPlaneConsts[];
extern const CodeConstantSource s_lightGridConsts[];
extern const CodeConstantSource s_codeConsts[];
extern const CodeConstantSource s_defaultCodeConsts[];
extern const MtlStateMapBitName s_alphaTestBitNames[];
extern const MtlStateMapBitName s_blendOpRgbBitNames[];
extern const MtlStateMapBitName s_srcBlendRgbBitNames[];
extern const MtlStateMapBitName s_dstBlendRgbBitNames[];
extern const MtlStateMapBitName s_blendOpAlphaBitNames[];
extern const MtlStateMapBitName s_srcBlendAlphaBitNames[];
extern const MtlStateMapBitName s_dstBlendAlphaBitNames[];
extern const MtlStateMapBitName s_cullFaceBitNames[];
extern const MtlStateMapBitName s_depthTestBitNames[];
extern const MtlStateMapBitName s_depthWriteBitNames[];
extern const MtlStateMapBitName s_colorWriteRgbBitNames[];
extern const MtlStateMapBitName s_colorWriteAlphaBitNames[];
extern const MtlStateMapBitName s_fogBitNames[];
extern const MtlStateMapBitName s_polygonOffsetBitNames[];
extern const MtlStateMapBitName s_wireframeBitNames[];
extern const MtlStateMapBitName s_stencilBitNames[];
extern const MtlStateMapBitName s_stencilOpFrontPassBitNames[];
extern const MtlStateMapBitName s_stencilOpFrontFailBitNames[];
extern const MtlStateMapBitName s_stencilOpFrontZFailBitNames[];
extern const MtlStateMapBitName s_stencilFuncFrontBitNames[];
extern const MtlStateMapBitName s_stencilOpBackPassBitNames[];
extern const MtlStateMapBitName s_stencilOpBackFailBitNames[];
extern const MtlStateMapBitName s_stencilOpBackZFailBitNames[];
extern const MtlStateMapBitName s_stencilFuncBackBitNames[];
extern const MtlStateMapBitGroup s_stateMapSrcBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstAlphaTestBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstBlendFuncRgbBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstBlendFuncAlphaBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstCullFaceBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstDepthTestBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstDepthWriteBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstColorWriteBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstFogBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstPolygonOffsetBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstWireframeBitGroup[];
extern const MtlStateMapBitGroup s_stateMapDstStencilBitGroup[];
static const MtlTextureFunctionDx7 s_textureFuncsDx7[21];
static const PassOptionDx7 s_passOptionsDx7[5];

HRESULT IncludeClass_Close(const IncludeClass *_this, LPCVOID data);
static Bool MATERIAL_REGPARM3_ABI Material_ValidatePassArguments_impl(const Material *material, const char *techniqueSetName, const char *techniqueName, int argCount, const MaterialShaderArgument *args);
static void MATERIAL_REGPARM3_ABI Material_PreLoadSingleShaderText(const char *filename, const char *subdir, GfxCachedShaderText *cached);
static void MATERIAL_REGPARM3_ABI Material_PreLoadSingleShaderText_impl(const char *filename, const char *subdir, GfxCachedShaderText *cached);
extern int Com_MatchToken(const char **text, const char *match, int allowLineBreaks);
extern float Com_ParseFloat(const char **text);
extern void Com_Printf(const char *fmt, ...);
extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern int FS_FOpenFileRead(const char *filename, int *fileHandle, int uniqueFILE);
extern int FS_Read(void *buf, int len, int fileHandle);
extern void FS_FCloseFile(fileHandle_t fileHandle);
extern void *Hunk_AllocAlignInternal(int size, int align);
static Bool Material_CachedShaderTextLess(const GfxCachedShaderText *cached0, const GfxCachedShaderText *cached1);
HRESULT IncludeClass_Open(const IncludeClass *_this, D3DXINCLUDE_TYPE IncludeType, LPCSTR filename, LPCVOID parentData, LPCVOID *data, MaterialTechnique *(*byteCount)[4][34]);
void Material_PreLoadAllShaderText(void);
static Bool MATERIAL_REGPARM3_ABI Material_ParseCodeConstantSource_r_impl(const char **text, const byte *routing, int offset, const CodeConstantSource *sourceTable, byte *arg);
extern void Com_UngetToken(void);
extern void *Material_Alloc(int size);
extern const char *R_ErrorDescription(HRESULT hr);
extern HRESULT D3DXGetShaderConstantTable(const void *function, void **constantTable);
extern void *Material_RegisterLiteral(float *literal);
extern void Com_SkipRestOfLine(const char **text);
extern void Com_SetScriptWarningPrefix(const char *prefix);
static Bool MATERIAL_REGPARM3_ABI Material_ParseVector_impl(const char **text, int elemCount, float *vector);

static Bool MATERIAL_REGPARM3_ABI Material_LoadPassTextureStateDx7_impl(const char **text, int samplerIndex, const char *texStateName, int validTest, int *texStageBits);
static Bool MATERIAL_REGPARM3_ABI Material_SetPassShaderArguments_impl(const char **text, const byte *mtlShader, short unsigned int *techFlags, short unsigned int *argCount, MaterialShaderArgument **args);
static Bool MATERIAL_REGPARM3_ABI Material_ParseRuleSet_impl(const char **text, const char *ruleSetName, const MtlStateMapBitGroup *stateSet, const MaterialStateMapRuleSet **ruleSet);
static Bool MATERIAL_REGPARM2_ABI Material_LoadPassStateMap_impl(const char **text, MaterialStateMap **stateMap);
static MaterialShader *MATERIAL_REGPARM2_ABI Material_LoadPassShader_impl(const char **text, int shaderType);
static Bool MATERIAL_REGPARM2_ABI Material_FinishLoadingInstance_impl(MaterialObj *material, int imageTrack);

static Bool MATERIAL_REGPARM3_ABI Material_CodeSamplerSource_r(const char **text, int offset, const CodeSamplerSource *sourceTable, MaterialShaderArgument *arg);
static Bool MATERIAL_REGPARM3_ABI Material_CodeSamplerSource_r_impl(const char **text, int offset, const CodeSamplerSource *sourceTable, MaterialShaderArgument *arg);
static Bool MATERIAL_REGPARM2_ABI Material_ParseSamplerSource(const char **text, MaterialShaderArgument *arg);
static Bool MATERIAL_REGPARM2_ABI Material_ParseSamplerSource_impl(const char **text, MaterialShaderArgument *arg);
extern void Com_ScriptWarning(const char *fmt, ...);
extern const char *Com_Parse(const char **text);
extern int Com_ParseInt(const char **text);
extern const char *Material_RegisterString(const char *string);
static MtlParseSuccess MATERIAL_REGPARM3_ABI Material_ParseRuleSetConditionTest_impl(const char **text, const char *token, MaterialStateMapRule *rule);
static Bool MATERIAL_REGPARM3_ABI Material_ParseRuleSet(const char **text, const char *ruleSetName, const MtlStateMapBitGroup *stateSet, const MaterialStateMapRuleSet **ruleSet);
static Bool MATERIAL_REGPARM2_ABI Material_FinishLoadingInstance(MaterialObj *material, int imageTrack);
Material *Material_Load(const char *name, int imageTrack);
typedef unsigned char (*GfxCachedShaderTextCompFunc)(const GfxCachedShaderText *, const GfxCachedShaderText *);
void ZSt13__adjust_heapIP19GfxCachedShaderTextiS0_PFhRKS0_S3_EEvT_T0_S7_T1_T2_(GfxCachedShaderText *first, int holeIndex, int len, GfxCachedShaderText value, GfxCachedShaderTextCompFunc comp);
void ZSt16__introsort_loopIP19GfxCachedShaderTextiPFhRKS0_S3_EEvT_S6_T0_T1_(GfxCachedShaderText *first, GfxCachedShaderText *last, int depth_limit, GfxCachedShaderTextCompFunc comp);

HRESULT IncludeClass_Close(const IncludeClass *_this, LPCVOID data)
{
    return 0;
}

static Bool MATERIAL_REGPARM3_ABI Material_ValidatePassArguments_impl(const Material *material, const char *techniqueSetName, const char *techniqueName, int argCount, const MaterialShaderArgument *args)
{
    int i, j;
    for (i = 0; i < argCount; i++) {
        const MaterialShaderArgument *arg = &args[i];

        if (arg->type == 2) {

            const char *argName = arg->u.name;
            byte *consts = (byte *)material->constants;
            int found = 0;
            for (j = 0; j < material->constantCount; j++) {
                if (*(const char **)(consts + j * 0x14) == argName) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                Com_Printf("material '%s' using technique '%s' from techniqueSet '%s' doesn't have constant '%s'\n",
                           material->info.name, techniqueName, techniqueSetName, argName);
                return 0;
            }
        } else if (arg->type == 4) {

            const char *argName = arg->u.name;
            byte *texs = (byte *)material->textures;
            int found = 0;
            for (j = 0; j < material->textureCount; j++) {
                if (*(const char **)(texs + j * 0x0c) == argName) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                Com_Printf("material '%s' using technique '%s' from techniqueSet '%s' doesn't have texture '%s'\n",
                           material->info.name, techniqueName, techniqueSetName, argName);
                return 0;
            }
        }
    }
    return 1;
}

static Bool MATERIAL_REGPARM3_ABI Material_ValidatePassArguments(const Material *material, const char *techniqueSetName, const char *techniqueName, int argCount, const MaterialShaderArgument *args)
{
    return Material_ValidatePassArguments_impl(material, techniqueSetName, techniqueName, argCount, args);
}

static void MATERIAL_REGPARM3_ABI Material_PreLoadSingleShaderText_impl(const char *filename, const char *subdir, GfxCachedShaderText *cached)
{
    char filepath[64];
    int fileHandle;

    Com_sprintf(filepath, 64, "materials/shaders/%s%s", subdir, filename);
    int fileSize = FS_FOpenFileRead(filepath, &fileHandle, 1);

    int allocSize = strlen(subdir) + strlen(filename) + fileSize + 2;
    char *buf = (char *)Hunk_AllocAlignInternal(allocSize, 1);

    cached->name = buf;
    int nameLen = sprintf(buf, "%s%s", subdir, filename);
    cached->text = buf + nameLen + 1;
    FS_Read((void *)cached->text, fileSize, fileHandle);
    FS_FCloseFile(fileHandle);
    ((char *)cached->text)[fileSize] = '\0';
    cached->textSize = fileSize;
}

static void MATERIAL_REGPARM3_ABI Material_PreLoadSingleShaderText(const char *filename, const char *subdir, GfxCachedShaderText *cached)
{
    Material_PreLoadSingleShaderText_impl(filename, subdir, cached);
}

static Bool Material_CachedShaderTextLess(const GfxCachedShaderText *cached0, const GfxCachedShaderText *cached1)
{
    return strcmp(*(const char **)cached0, *(const char **)cached1) < 0;
}

extern const char *va(const char *fmt, ...);
extern int stricmp(const char *s1, const char *s2);

static int Material_FindCachedShaderText(const char *searchName)
{
    int count = MTLGLOB_COUNT;
    GfxCachedShaderText *cached = MTLGLOB_PTR(GfxCachedShaderText);
    int bot = 0, top = count - 1;

    while (bot <= top) {
        int mid = (bot + top) / 2;
        int cmp = stricmp(searchName, cached[mid].name);
        if (cmp == 0)
            return mid;
        if (cmp > 0)
            bot = mid + 1;
        else
            top = mid - 1;
    }
    return -1;
}

HRESULT IncludeClass_Open(const IncludeClass *_this, D3DXINCLUDE_TYPE IncludeType, LPCSTR filename, LPCVOID parentData, LPCVOID *data, MaterialTechnique *(*byteCount)[4][34])
{
    GfxCachedShaderText *cached = MTLGLOB_PTR(GfxCachedShaderText);
    int idx;

    idx = Material_FindCachedShaderText(filename);
    if (idx < 0) {

        idx = Material_FindCachedShaderText(va("lib/%s", filename));
        if (idx < 0)
            return 1;
    }

    *(const char **)data = cached[idx].text;
    *(int *)byteCount = cached[idx].textSize;
    return 0;
}

extern char **FS_ListFiles(const char *dir, const char *ext, int flags, int *count, int);
extern void FS_FreeFileList(char **list, int allocTrackType);
extern int FS_ReadFile(const char *path, void **buffer);
extern void FS_FreeFile(void *buffer);
extern void *Hunk_AllocInternal(int size);
extern unsigned char Material_CachedShaderTextLess(const GfxCachedShaderText *, const GfxCachedShaderText *);

void Material_PreLoadAllShaderText(void)
{
    int fileCountRoot, fileCountLib;
    char **shaderListRoot, **shaderListLib;
    GfxCachedShaderText *cache;
    int totalCount, i;

    shaderListRoot = FS_ListFiles("materials/shaders/", "hlsl", 0, &fileCountRoot, 0x14);
    shaderListLib = FS_ListFiles("materials/shaders/lib/", "hlsl", 0, &fileCountLib, 0x14);

    totalCount = fileCountRoot + fileCountLib;
    MTLGLOB_COUNT = totalCount;
#if defined(__x86_64__)
    cache = (GfxCachedShaderText *)Hunk_AllocInternal(totalCount * sizeof(GfxCachedShaderText));
#else
    cache = (GfxCachedShaderText *)Hunk_AllocInternal(totalCount * 12);
#endif
    MTLGLOB_SET_PTR(cache);

    {
        GfxCachedShaderText *entry = cache;
        for (i = 0; i < fileCountRoot; i++) {
            Material_PreLoadSingleShaderText_impl(shaderListRoot[i], "", entry);
            entry++;
        }

        for (i = 0; i < fileCountLib; i++) {
            Material_PreLoadSingleShaderText_impl(shaderListLib[i], "lib/", entry);
            entry++;
        }
    }

    {
        GfxCachedShaderText *first = MTLGLOB_PTR(GfxCachedShaderText);
        GfxCachedShaderText *last = first + totalCount;
        if (first != last && totalCount > 1) {
            int n = totalCount, depth = 0;
            while (n > 1) {
                depth++;
                n >>= 1;
            }
            depth *= 2;
            ZSt16__introsort_loopIP19GfxCachedShaderTextiPFhRKS0_S3_EEvT_S6_T0_T1_(
                first, last, depth, Material_CachedShaderTextLess);

#if defined(__x86_64__)
            if ((char *)last - (char *)first > (long)(sizeof(GfxCachedShaderText) * 16)) {
#else
            if ((char *)last - (char *)first > 12 * 16) {
#endif
                GfxCachedShaderText *threshold = first + 16;

                GfxCachedShaderText *ii;
                for (ii = first + 1; ii != threshold && ii != last; ii++) {
                    GfxCachedShaderText val = *ii;
                    if (Material_CachedShaderTextLess(&val, first)) {
                        memmove(first + 1, first, (char *)ii - (char *)first);
                        *first = val;
                    } else {
                        GfxCachedShaderText *prev = ii - 1;
                        GfxCachedShaderText *hole = ii;
                        while (Material_CachedShaderTextLess(&val, prev)) {
                            *hole = *prev;
                            hole = prev;
                            prev--;
                        }
                        *hole = val;
                    }
                }

                for (; ii != last; ii++) {
                    GfxCachedShaderText val = *ii;
                    GfxCachedShaderText *prev = ii - 1;
                    GfxCachedShaderText *hole = ii;
                    while (Material_CachedShaderTextLess(&val, prev)) {
                        *hole = *prev;
                        hole = prev;
                        prev--;
                    }
                    *hole = val;
                }
            } else {

                GfxCachedShaderText *ii;
                for (ii = first + 1; ii != last; ii++) {
                    GfxCachedShaderText val = *ii;
                    if (Material_CachedShaderTextLess(&val, first)) {
                        memmove(first + 1, first, (char *)ii - (char *)first);
                        *first = val;
                    } else {
                        GfxCachedShaderText *prev = ii - 1;
                        GfxCachedShaderText *hole = ii;
                        while (Material_CachedShaderTextLess(&val, prev)) {
                            *hole = *prev;
                            hole = prev;
                            prev--;
                        }
                        *hole = val;
                    }
                }
            }
        }
    }

    FS_FreeFileList(shaderListRoot, 0x14);
    FS_FreeFileList(shaderListLib, 0x14);
}

static Bool MATERIAL_REGPARM3_ABI Material_ParseCodeConstantSource_r_impl(const char **text, const byte *routing, int offset, const CodeConstantSource *sourceTable, byte *arg)
{
    if (!Com_MatchToken(text, ".", 1))
        return 0;

    const char *token = Com_Parse(text);

    int sourceIndex;
    for (sourceIndex = 0; sourceTable[sourceIndex].name; sourceIndex++) {
        if (strcmp(token, sourceTable[sourceIndex].name) == 0)
            goto found;
    }
    Com_ScriptWarning("unknown constant source '%s'\n", token);
    return 0;

found:;
    const CodeConstantSource *entry = &sourceTable[sourceIndex];
    int arrayCount = entry->arrayCount;
    int arrayIndex = 0;

    if (arrayCount != 0) {
        byte componentCount = *(routing + 5);
        if (entry->subtable != 0 || componentCount <= 1) {

            int arrayStride = entry->arrayStride;
            if (!Com_MatchToken(text, "[", 1))
                return 0;
            arrayIndex = Com_ParseInt(text);
            if (arrayIndex < 0 || arrayIndex >= arrayCount) {
                Com_ScriptWarning("array index must be in range [0, %i]\n", arrayCount - 1);
                return 0;
            }
            if (!Com_MatchToken(text, "]", 1))
                return 0;
            offset += arrayIndex * arrayStride;
        } else {

            const char *peek = Com_Parse(text);
            if (*peek == '[') {

                arrayIndex = Com_ParseInt(text);
                if (arrayIndex < 0 || arrayIndex >= entry->arrayCount) {
                    Com_ScriptWarning("index %i is not in the range [0, %i]\n", arrayIndex, entry->arrayCount - 1);
                    return 0;
                }

                if (componentCount > 1) {
                    if (!Com_MatchToken(text, ",", 1))
                        return 0;
                    int endIndex = Com_ParseInt(text);
                    int expectedEnd = arrayIndex + componentCount - 1;
                    if (endIndex != expectedEnd) {
                        Com_ScriptWarning("ending index %i should be %i instead\n", endIndex, expectedEnd);
                        return 0;
                    }
                }
                if (!Com_MatchToken(text, "]", 1))
                    return 0;
                offset += arrayIndex;
            } else {
                Com_UngetToken();

                if ((int)componentCount > entry->arrayCount) {
                    Com_ScriptWarning("code constant '%s' has only %i members, but %i were requested\n",
                                      entry->name, entry->arrayCount, (int)componentCount);
                    return 0;
                }

            }
        }
    }

    if (entry->subtable) {
        return Material_ParseCodeConstantSource_r_impl(text, routing, offset,
                                                       (const CodeConstantSource *)(intptr_t)entry->subtable, arg);
    }

    int source = offset + (unsigned char)entry->source;

    if (source <= 0xBA) {

        *(unsigned short *)(arg + 4) = (unsigned short)source;
        *(arg + 6) = 0;
        *(arg + 7) = *(routing + 5);
        return 1;
    }

    short *routingShaderType = *(short **)(routing + 0xc);
    if (*routingShaderType == 3)
        source ^= 2;

    const char *nextToken = Com_Parse(text);
    if (*nextToken == ';') {

        Com_UngetToken();
        *(unsigned short *)(arg + 4) = (unsigned short)source;
        *(arg + 6) = 0;
        byte *routingShader = *(byte **)(routing + 8);
        *(arg + 7) = (byte) * (unsigned short *)(routingShader + 8);
        return 1;
    }

    if (*nextToken == '[') {

        *(unsigned short *)(arg + 4) = (unsigned short)source;
        int rowIndex = Com_ParseInt(text);
        if ((unsigned)rowIndex > 3) {
            Com_ScriptWarning("row index %i should be in the range [0, 3]\n", rowIndex);
            return 0;
        }
        *(arg + 6) = (byte)rowIndex;
        *(arg + 7) = 1;
        return Com_MatchToken(text, "]", 1) ? 1 : 0;
    }

    Com_ScriptWarning("expected ';' or '[', found '%s' instead\n", nextToken);
    return 0;
}

static Bool MATERIAL_REGPARM3_ABI Material_ParseCodeConstantSource_r(const char **text, const byte *routing, int offset, const CodeConstantSource *sourceTable, byte *arg)
{
    return Material_ParseCodeConstantSource_r_impl(text, routing, offset, sourceTable, arg);
}

static Bool MATERIAL_REGPARM3_ABI Material_ParseVector_impl(const char **text, int elemCount, float *vector)
{
    int i;

    if (!Com_MatchToken(text, "(", 1))
        return 0;

    i = 0;
    goto parse_elem;
    do {
        if (!Com_MatchToken(text, ",", 1))
            return 0;
    parse_elem:
        vector[i] = Com_ParseFloat(text);
        i++;
    } while (i != elemCount);

    return Com_MatchToken(text, ")", 1) ? 1 : 0;
}

static Bool MATERIAL_REGPARM3_ABI Material_ParseVector(const char **text, int elemCount, float *vector)
{
    return Material_ParseVector_impl(text, elemCount, vector);
}

static Bool MATERIAL_REGPARM3_ABI Material_LoadPassTextureStateDx7_impl(const char **text, int samplerIndex,
                                                                        const char *texStateName, int validTest, int *texStageBits)
{
    const char *token;
    int fnIndex, argCount, argIndex, texArg, shiftBits;

    if (!Com_MatchToken(text, "stage", 1))
        return 0;
    if (Com_MatchToken(text, "[", 1)) {
        int idx = Com_ParseInt(text);
        if (idx != samplerIndex) {
            Com_ScriptWarning("expected %i, found %i instead\n", samplerIndex, idx);
            return 0;
        }
        if (!Com_MatchToken(text, "]", 1))
            return 0;
    }
    if (!Com_MatchToken(text, ".", 1))
        return 0;
    if (!Com_MatchToken(text, texStateName, 1))
        return 0;
    if (!Com_MatchToken(text, "=", 1))
        return 0;

    token = Com_Parse(text);
    for (fnIndex = 0; fnIndex < 21; fnIndex++) {
        const byte *entry = (const byte *)s_textureFuncsDx7 + fnIndex * 16;
        if (strcmp(token, *(const char **)entry) == 0)
            break;
    }
    if (fnIndex >= 21) {
        Com_ScriptWarning("expected a texture function, found '%s' instead.\n", token);
        Com_Printf("Valid texture functions:.\n");
        {
            int i;
            for (i = 0; i < 21; i++) {
                const byte *e = (const byte *)s_textureFuncsDx7 + i * 16;
                Com_Printf("  %s\n", *(const char **)e);
            }
        }
        return 0;
    }

    {
        const byte *entry = (const byte *)s_textureFuncsDx7 + fnIndex * 16;
        int funcValidMask = *(int *)(entry + 12);
        if (!(validTest & funcValidMask)) {
            const char *desc = (validTest == 1) ? "alpha" : "color";
            Com_ScriptWarning("%s is only valid for %s\n", token, desc);
            return 0;
        }
        *texStageBits = *(int *)(entry + 4);
        argCount = *(int *)(entry + 8);
    }

    if (!Com_MatchToken(text, "(", 1))
        return 0;

    shiftBits = 0xa;
    for (argIndex = 0; argIndex < argCount; argIndex++) {
        if (argIndex > 0) {
            if (!Com_MatchToken(text, ",", 1))
                return 0;
        }
        texArg = 0;

        for (;;) {
            token = Com_Parse(text);
            while (strcmp(token, "complement") == 0) {
                texArg |= 8;
                token = Com_Parse(text);
            }
            if (strcmp(token, "alphaReplicate") == 0) {
                texArg |= 0x10;
                continue;
            }
            break;
        }

        if (strcmp(token, "vertex") == 0) {
            if (!Com_MatchToken(text, ".", 1))
                return 0;
            if (!Com_MatchToken(text, "color", 1))
                return 0;
            texArg |= 2;
        } else if (strcmp(token, "texture") == 0) {
            if (!Com_MatchToken(text, "[", 1))
                return 0;
            int idx = Com_ParseInt(text);
            if (idx != samplerIndex) {
                Com_ScriptWarning("expected %i, found %i instead\n", samplerIndex, idx);
                return 0;
            }
            if (!Com_MatchToken(text, "]", 1))
                return 0;
            texArg |= 5;
        } else if (strcmp(token, "stage") == 0) {
            if (samplerIndex == 0) {
                Com_ScriptWarning("first stage cannot reference the previous stage\n");
                return 0;
            }
            if (!Com_MatchToken(text, "[", 1))
                return 0;
            int idx = Com_ParseInt(text);
            if (idx != samplerIndex - 1) {
                Com_ScriptWarning("expected %i, found %i instead\n", samplerIndex - 1, idx);
                return 0;
            }
            if (!Com_MatchToken(text, "]", 1))
                return 0;
            texArg |= 1;
        } else if (strcmp(token, "constant") == 0) {
            texArg |= 6;
        } else {
            Com_ScriptWarning("unknown texture function argument '%s'\n", token);
            return 0;
        }

        texArg <<= shiftBits;
        *texStageBits |= texArg;
        shiftBits += 5;
    }

    if (!Com_MatchToken(text, ")", 1))
        return 0;

    return Com_MatchToken(text, ";", 1) ? 1 : 0;
}

static Bool MATERIAL_REGPARM3_ABI Material_LoadPassTextureStateDx7(const char **text, int samplerIndex, const char *texStateName, int validTest, int *texStageBits)
{
    return Material_LoadPassTextureStateDx7_impl(text, samplerIndex, texStateName, validTest, texStageBits);
}

static Bool MATERIAL_REGPARM3_ABI Material_CodeSamplerSource_r_impl(const char **text, int offset, const CodeSamplerSource *sourceTable, MaterialShaderArgument *arg)
{
    if (!Com_MatchToken(text, ".", 1))
        return 0;

    const char *token = Com_Parse(text);

    int sourceIndex;
    for (sourceIndex = 0; sourceTable[sourceIndex].name; sourceIndex++) {
        if (strcmp(token, sourceTable[sourceIndex].name) == 0)
            goto found;
    }
    Com_ScriptWarning("unknown sampler source '%s'\n", token);
    return 0;

found:;
    const CodeSamplerSource *entry = &sourceTable[sourceIndex];
    int arrayIndex = 0;

    if (entry->arrayCount != 0) {

        int arrayStride = entry->arrayStride;
        if (!Com_MatchToken(text, "[", 1))
            return 0;
        arrayIndex = Com_ParseInt(text);
        if (arrayIndex < 0 || arrayIndex >= entry->arrayCount) {
            Com_ScriptWarning("array index must be in range [0, %i]\n", entry->arrayCount - 1);
            return 0;
        }
        if (!Com_MatchToken(text, "]", 1))
            return 0;
        offset += arrayIndex * arrayStride;
    }

    if (entry->subtable) {
        return Material_CodeSamplerSource_r_impl(text, offset, (const CodeSamplerSource *)(intptr_t)entry->subtable, arg);
    }

    offset += entry->source;
    arg->u.codeSampler = offset;
    return 1;
}

static Bool MATERIAL_REGPARM3_ABI Material_CodeSamplerSource_r(const char **text, int offset, const CodeSamplerSource *sourceTable, MaterialShaderArgument *arg)
{
    return Material_CodeSamplerSource_r_impl(text, offset, sourceTable, arg);
}

static Bool MATERIAL_REGPARM2_ABI Material_ParseSamplerSource_impl(const char **text, MaterialShaderArgument *arg)
{
    const char *token = Com_Parse(text);

    if (memcmp(token, "sampler", 8) == 0) {

        arg->type = 3;
        return Material_CodeSamplerSource_r_impl(text, 0, s_codeSamplers, arg);
    }

    if (memcmp(token, "material", 9) == 0) {

        if (!Com_MatchToken(text, ".", 1))
            return 0;
        const char *texName = Com_Parse(text);
        arg->type = 4;
        arg->u.name = Material_RegisterString(texName);
        return arg->u.name != NULL;
    }

    Com_ScriptWarning("expected 'sampler' or 'material', found '%s' instead\n", token);
    return 0;
}

static Bool MATERIAL_REGPARM2_ABI Material_ParseSamplerSource(const char **text, MaterialShaderArgument *arg)
{
    return Material_ParseSamplerSource_impl(text, arg);
}

extern HRESULT D3DXGetShaderConstantTable(const void *function, void **constantTable);
extern void *Material_RegisterLiteral(float *literal);
extern void Com_SkipRestOfLine(const char **text);
extern int printf(const char *fmt, ...);

static Bool MATERIAL_REGPARM3_ABI Material_SetPassShaderArguments_impl(const char **text, const byte *mtlShader,
                                                                       short unsigned int *techFlags, short unsigned int *argCount, MaterialShaderArgument **args)
{
    void *constants;
    int hr;
    const byte *constantTable;
    unsigned int constantCount;
    const char *shaderName;
    const byte *constantInfo;
    byte usedConstant[256];
    int usedCount;
    MaterialShaderArgument *allocatedArgs;
    Bool success;
    unsigned short constType;
    byte routing[16];
    float literal[4];

    hr = D3DXGetShaderConstantTable(*(const void **)(mtlShader + 4), &constants);
    if (hr < 0) {
        Com_ScriptWarning("Couldn't get the constant table: %s (%08x)\n",
                          R_ErrorDescription(hr), hr);
        return 0;
    }

#ifdef GFX_REAL_D3D9
    constantTable = (const byte *)((void *(D3DVTCC *)(void *))((*(void ***)constants)[3]))(constants);
#else
    constantTable = (const byte *)((int (*)(void *))((*(int **)constants)[3]))(constants);
#endif
    constantCount = *(const unsigned int *)(constantTable + 0xC);
    *argCount = (unsigned short)constantCount;

    if (constantCount == 0) {
        *args = NULL;

        if (!Com_MatchToken(text, "{", 1))
            goto fail;
        {
            const char *tok;
            for (;;) {
                tok = Com_Parse(text);
                if (tok[0] == '\0' || tok[0] == '}')
                    break;
            }
        }
        goto succeed;
    }

    allocatedArgs = (MaterialShaderArgument *)Material_Alloc(constantCount * 8);
    *args = allocatedArgs;
    shaderName = *(const char **)mtlShader;
    constantInfo = constantTable + *(const unsigned int *)(constantTable + 0x10);
    memset(usedConstant, 0, constantCount);

    if (!Com_MatchToken(text, "{", 1))
        goto fail;

    usedCount = 0;
    for (;;) {
        const char *token;
        unsigned int i;
        const byte *constEntry = NULL;
        const byte *typeInfo;
        MaterialShaderArgument *arg;
        byte firstRow, rowCount;
        int found;

        token = Com_Parse(text);
        if (token[0] == '\0') {
            Com_ScriptWarning("unexpected end-of-file\n");
            goto fail;
        }
        if (token[0] == '}')
            break;

        found = -1;
        {
            const byte *se = constantInfo;
            for (i = 0; i < constantCount; i++, se += 0x14) {
                const char *cn = (const char *)(constantTable + *(const unsigned int *)se);
                if (strcmp(cn, token) == 0) {
                    found = (int)i;
                    constEntry = se;
                    break;
                }
            }
        }

        if (found == -1) {
            printf("*WARNING*: constant '%s' is not used by shader '%s'\n", token, shaderName);
            Com_SetScriptWarningPrefix("^3WARNING: ");
            Com_ScriptWarning("'%s' is not defined by %s\n", token, shaderName);
            Com_SetScriptWarningPrefix("^1ERROR: ");
            if (!Com_MatchToken(text, "=", 1))
                goto fail;
            Com_SkipRestOfLine(text);
            continue;
        }

        if (usedConstant[found]) {
            Com_ScriptWarning("shader constant '%s' defined more than once for shader '%s'\n", token, shaderName);
            goto fail;
        }
        usedConstant[found] = 1;

        typeInfo = constantTable + *(const unsigned int *)(constEntry + 0xC);
        arg = &allocatedArgs[usedCount];
        arg->dest = *(const unsigned short *)(constEntry + 6);

        if (*(const unsigned short *)typeInfo == 1) {
            const char *peek = Com_Parse(text);
            if (peek[0] == '[') {
                int rowIdx = Com_ParseInt(text);
                firstRow = (byte)rowIdx;
                if ((unsigned)firstRow >= *(const unsigned short *)(typeInfo + 8)) {
                    Com_ScriptWarning("row index '%i' is not in the range [0, %i]\n",
                                      (int)firstRow, (int)*(const unsigned short *)(typeInfo + 8) - 1);
                    goto fail;
                }
                const char *next = Com_Parse(text);
                if (next[0] == ']') {
                    firstRow = 0;
                    rowCount = 1;
                } else if (next[0] == ',') {
                    int endRow = Com_ParseInt(text);
                    if (endRow < firstRow || endRow >= (int)*(const unsigned short *)(typeInfo + 8)) {
                        Com_ScriptWarning("end row index '%i' is not in the range [%i, %i]\n",
                                          endRow, (int)firstRow, (int)*(const unsigned short *)(typeInfo + 8) - 1);
                        goto fail;
                    }
                    rowCount = (byte)(endRow - firstRow + 1);
                    if (!Com_MatchToken(text, "]", 1))
                        goto fail;
                } else {
                    Com_ScriptWarning("expected ',' or ']', found '%s' instead\n", next);
                    goto fail;
                }
            } else {
                Com_UngetToken();
                firstRow = 0;
                rowCount = (byte) * (const unsigned short *)(typeInfo + 8);
            }
        } else {
            firstRow = 0;
            rowCount = 1;
        }

        routing[4] = firstRow;
        routing[5] = rowCount;

        if (!Com_MatchToken(text, "=", 1))
            goto fail;

        constType = *(const unsigned short *)(typeInfo + 2);

        if (constType == 0xA || constType == 0xC || constType == 0xD || constType == 0xE) {
            success = Material_ParseSamplerSource_impl(text, (MaterialShaderArgument *)arg);
        } else if (constType == 3) {
            const char *vt = Com_Parse(text);
            literal[0] = 0;
            literal[1] = 0;
            literal[2] = 0;
            literal[3] = 1.0f;
            if (memcmp(vt, "float1", 7) == 0) {
                Material_ParseVector_impl(text, 1, literal);
                goto chk_lit;
            } else if (memcmp(vt, "float2", 7) == 0) {
                Material_ParseVector_impl(text, 2, literal);
                goto chk_lit;
            } else if (memcmp(vt, "float3", 7) == 0) {
                Material_ParseVector_impl(text, 3, literal);
                goto chk_lit;
            } else if (memcmp(vt, "float4", 7) == 0) {
                Material_ParseVector_impl(text, 4, literal);
                goto chk_lit;
            } else if (memcmp(vt, "constant", 9) == 0) {
                arg->type = 1;
                success = Material_ParseCodeConstantSource_r_impl(text, routing, 0,
                                                                  (const CodeConstantSource *)s_codeConsts, (byte *)arg);
                goto chk_succ;
            } else if (memcmp(vt, "material", 9) == 0) {
                if (rowCount > 1) {
                    Com_ScriptWarning("Each element of the array must be set to a material constant individually.\n");
                    success = 0;
                    goto chk_succ;
                }
                if (!Com_MatchToken(text, ".", 1)) {
                    success = 0;
                    goto chk_succ;
                }
                const char *mn = Com_Parse(text);
                arg->type = 2;
                arg->u.name = (const char *)Material_RegisterString(mn);
                success = (arg->u.name != NULL);
                goto chk_succ;
            } else {
                Com_ScriptWarning("expected 'sampler' or 'material', found '%s' instead\n", vt);
                success = 0;
                goto chk_succ;
            }
        chk_lit:
            if (rowCount > 1) {
                Com_ScriptWarning("Each element of the array must be set to a float individually.\n");
                success = 0;
                goto chk_succ;
            }
            arg->type = 0;
            arg->u.literalConst = (const float16 *)Material_RegisterLiteral(literal);
            success = (arg->u.literalConst != NULL);
            goto chk_succ;
        } else {
            Com_ScriptWarning("unknown constant type '%i'\n", (int)constType);
            goto fail;
        }

    chk_succ:
        if (!success)
            goto fail;
        if (!Com_MatchToken(text, ";", 1))
            goto fail;
        if (arg->type == 3) {
            if (arg->u.codeSampler == 0xE)
                *techFlags |= 1;
            else if (arg->u.codeSampler == 0xF)
                *techFlags |= 2;
        }
        usedCount++;
    }

    if (usedCount == (int)constantCount)
        goto succeed;

    {
        unsigned int idx;
        const byte *entry = constantInfo;
        for (idx = 0; idx < constantCount; idx++, entry += 0x14) {
            const byte *dti;
            MaterialShaderArgument *da;
            const char *cn;
            byte drw;
            Bool df;
            if (usedConstant[idx])
                continue;
            dti = constantTable + *(const unsigned int *)(entry + 0xC);
            da = &allocatedArgs[usedCount];
            da->dest = *(const unsigned short *)(entry + 6);
            cn = (const char *)(constantTable + *(const unsigned int *)entry);
            drw = (*(const unsigned short *)dti == 1) ? (byte) * (const unsigned short *)(dti + 8) : 1;
            constType = *(const unsigned short *)(dti + 2);
            if (constType == 0xA || constType == 0xC || constType == 0xD || constType == 0xE) {
                da->type = 3;
                df = 0;
                {
                    int si = 0;
                    const CodeSamplerSource *se = &s_defaultCodeSamplers[0];
                    while (se->name) {
                        if (!se->subtable && !se->arrayCount && strcmp(cn, se->name) == 0) {
                            da->u.codeSampler = s_defaultCodeSamplers[si].source;
                            df = 1;
                            break;
                        }
                        si++;
                        se = &s_defaultCodeSamplers[si];
                    }
                }
                if (!df)
                    continue;
            } else if (constType == 3) {
                da->type = 1;
                df = 0;
                {
                    int ci = 0;
                    const CodeConstantSource *ce = &s_codeConsts[0];
                    while (ce->name) {
                        if (!ce->subtable && strcmp(cn, ce->name) == 0) {
                            int src = (unsigned char)ce->source;
                            if (src > 0xBA) {
                                int adj = src ^ 2;
                                if (*(const unsigned short *)dti == 3)
                                    src = adj;
                                da->u.codeConst.index = (unsigned short)src;
                                da->u.codeConst.firstRow = 0;
                                da->u.codeConst.rowCount = (byte) * (const unsigned short *)(entry + 8);
                            } else {
                                da->u.codeConst.index = (unsigned short)src;
                                da->u.codeConst.firstRow = 0;
                                da->u.codeConst.rowCount = drw;
                            }
                            df = 1;
                            break;
                        }
                        ci++;
                        ce = &s_codeConsts[ci];
                    }
                }
                if (!df) {
                    int di = 0;
                    const CodeConstantSource *de = &s_defaultCodeConsts[0];
                    while (de->name) {
                        if (!de->subtable && strcmp(cn, de->name) == 0) {
                            int src = (unsigned char)de->source;
                            if (src > 0xBA) {
                                int adj = src ^ 2;
                                if (*(const unsigned short *)dti == 3)
                                    src = adj;
                                da->u.codeConst.index = (unsigned short)src;
                                da->u.codeConst.firstRow = 0;
                                da->u.codeConst.rowCount = (byte) * (const unsigned short *)(entry + 8);
                            } else {
                                da->u.codeConst.index = (unsigned short)src;
                                da->u.codeConst.firstRow = 0;
                                da->u.codeConst.rowCount = drw;
                            }
                            df = 1;
                            break;
                        }
                        di++;
                        de = &s_defaultCodeConsts[di];
                    }
                }
                if (!df)
                    continue;
            } else
                continue;
            usedConstant[idx] = 1;
            usedCount++;
        }
    }

    if (usedCount == (int)constantCount)
        goto succeed;
    Com_ScriptWarning("Undefined shader constant(s) in %s\n", shaderName);
    {
        const byte *e = constantInfo;
        unsigned int idx;
        for (idx = 0; idx < constantCount; idx++, e += 0x14)
            if (!usedConstant[idx])
                Com_Printf("  %s\n", (const char *)(constantTable + *(const unsigned int *)e));
    }
    Com_Printf("%i constant(s) were undefined\n", (int)constantCount - usedCount);
    goto fail;

succeed:
    success = 1;
    goto cleanup;
fail:
    success = 0;
cleanup:
    ((void (*)(void *))((*(int **)constants)[2]))(constants);
    return success;
}

static Bool MATERIAL_REGPARM3_ABI Material_SetPassShaderArguments(const char **text, const byte *mtlShader, short unsigned int *techFlags, short unsigned int *argCount, MaterialShaderArgument **args)
{
    return Material_SetPassShaderArguments_impl(text, mtlShader, techFlags, argCount, args);
}

static MtlParseSuccess MATERIAL_REGPARM3_ABI Material_ParseRuleSetConditionTest_impl(const char **text, const char *token, MaterialStateMapRule *rule)
{
    const MtlStateMapBitGroup *srcGroup = s_stateMapSrcBitGroup;

    int sourceIndex;
    for (sourceIndex = 0; srcGroup[sourceIndex].name; sourceIndex++) {
        if (strcmp(token, srcGroup[sourceIndex].name) == 0)
            goto foundSource;
    }
    return 1;

foundSource:

    if (!Com_MatchToken(text, "==", 1))
        return 2;

    const MtlStateMapBitName *bitNames = srcGroup[sourceIndex].bitNames;

    const char *valueToken = Com_Parse(text);
    int valueIndex;
    for (valueIndex = 0; bitNames[valueIndex].name; valueIndex++) {
        if (strcmp(valueToken, bitNames[valueIndex].name) == 0)
            goto foundValue;
    }
    Com_ScriptWarning("%s is not a valid state value\n", valueToken);
    return 2;

foundValue:;
    int destWordIndex = srcGroup[sourceIndex].stateBitsMask[0] ? 0 : 1;

    rule->stateBitsMask[destWordIndex] |= srcGroup[sourceIndex].stateBitsMask[destWordIndex];
    rule->stateBitsValue[destWordIndex] |= bitNames[valueIndex].bits;

    return 0;
}

static MtlParseSuccess MATERIAL_REGPARM3_ABI Material_ParseRuleSetConditionTest(const char **text, const char *token, MaterialStateMapRule *rule)
{
    return Material_ParseRuleSetConditionTest_impl(text, token, rule);
}

extern void Com_Memcpy(void *dest, const void *src, int count);

static Bool MATERIAL_REGPARM3_ABI Material_ParseRuleSet_impl(const char **text, const char *ruleSetName,
                                                             const MtlStateMapBitGroup *stateSet, const MaterialStateMapRuleSet **ruleSet)
{
    byte rules[0x2020];
    int firstRule, ruleCount, ruleOffset;
    const char *token;
    MtlParseSuccess condResult;

    if (!Com_MatchToken(text, ruleSetName, 1))
        return 0;
    if (!Com_MatchToken(text, "{", 1))
        return 0;

    memset(rules, 0, 0x2020);
    firstRule = 0;
    ruleCount = 0;
    ruleOffset = 0;

    for (;;) {
        if (ruleCount > 256) {
            Com_ScriptWarning("state %s has more than %i rules\n", ruleSetName, 256);
            return 0;
        }

        token = Com_Parse(text);
        if (token[0] == '}')
            break;

        byte *rule = rules + ruleOffset;

        if (strcmp(token, "default") == 0) {
            if (!Com_MatchToken(text, ":", 1))
                return 0;
            ruleCount++;
            ruleOffset += 0x20;
            continue;
        }

        condResult = Material_ParseRuleSetConditionTest_impl(text, token, (MaterialStateMapRule *)rule);
        if (condResult == MTL_PARSE_SUCCESS) {

        parse_operator:;
            const char *opToken = Com_Parse(text);
            if (strcmp(opToken, ":") == 0) {
                ruleCount++;
                ruleOffset += 0x20;
                continue;
            } else if (strcmp(opToken, "&&") == 0) {
                const char *nextToken = Com_Parse(text);
                MtlParseSuccess r2 = Material_ParseRuleSetConditionTest_impl(text, nextToken, (MaterialStateMapRule *)rule);
                if (r2 == MTL_PARSE_SUCCESS)
                    goto parse_operator;
                if (r2 == MTL_PARSE_NO_MATCH) {
                    Com_ScriptWarning("can't use '==' for multiple conditions\n");
                    return 0;
                }
                return 0;
            } else {
                Com_ScriptWarning("expected ':' or '&&', found '%s'\n", opToken);
                return 0;
            }
        }

        if (condResult == MTL_PARSE_ERROR)
            return 0;

        if (firstRule == ruleCount) {
            Com_ScriptWarning("missing rule condition for state %s\n", ruleSetName);
            return 0;
        }

        MaterialStateMapRule *ruleAtFirstRule = (MaterialStateMapRule *)(rules + firstRule * 0x20);

        if (strcmp(token, "passthrough") == 0) {
            if (!Com_MatchToken(text, ";", 1))
                return 0;
            goto copy_values;
        }

        Com_UngetToken();
        {
            const MtlStateMapBitGroup *bgPtr = stateSet;
            const MtlStateMapBitGroup *bgNext = stateSet + 1;
            for (;;) {
                const MtlStateMapBitName *bitNames = bgPtr->bitNames;
                const char *valueName = Com_Parse(text);
                const MtlStateMapBitName *bitName = NULL;
                int vi;
                for (vi = 0; bitNames[vi].name != NULL; vi++) {
                    if (strcmp(valueName, bitNames[vi].name) == 0) {
                        bitName = &bitNames[vi];
                        break;
                    }
                }
                if (!bitName) {
                    Com_ScriptWarning("%s is not a valid state value\n", valueName);
                    return 0;
                }

                int colIndex;
                if (bgPtr->stateBitsMask[0] != 0) {
                    colIndex = 0;
                } else {
                    colIndex = 1;
                    while (bgPtr->stateBitsMask[colIndex] == 0)
                        colIndex++;
                }

                ((unsigned int *)ruleAtFirstRule)[4 + colIndex] |= (unsigned int)bitName->bits;
                ((unsigned int *)ruleAtFirstRule)[6 + colIndex] |= (unsigned int)bgPtr->stateBitsMask[colIndex];

                if (bgNext->name == NULL) {
                    if (!Com_MatchToken(text, ";", 1))
                        return 0;
                    goto copy_values;
                }
                if (!Com_MatchToken(text, ",", 1))
                    return 0;
                bgPtr++;
                bgNext++;
            }
        }

    copy_values:

        if (firstRule + 1 < ruleCount) {
            int i;
            for (i = firstRule + 1; i < ruleCount; i++) {
                MaterialStateMapRule *dst = (MaterialStateMapRule *)(rules + i * 0x20);
                dst->stateBitsSet[0] = ruleAtFirstRule->stateBitsSet[0];
                dst->stateBitsSet[1] = ruleAtFirstRule->stateBitsSet[1];
                dst->stateBitsClear[0] = ruleAtFirstRule->stateBitsClear[0];
                dst->stateBitsClear[1] = ruleAtFirstRule->stateBitsClear[1];
            }
        }
        firstRule = ruleCount;
    }

    if (ruleCount == 0) {
        Com_ScriptWarning("no entries for state %s: you may want to do 'default: passthrough;'\n", ruleSetName);
        return 0;
    }
    if (firstRule != ruleCount) {
        Com_ScriptWarning("missing value for state %s\n", ruleSetName);
        return 0;
    }

    {
        int totalBytes = ruleOffset + 4;
        MaterialStateMapRuleSet *rs = (MaterialStateMapRuleSet *)Material_Alloc(totalBytes);
        rs->ruleCount = ruleCount;
        Com_Memcpy(&rs->rules[0], rules, ruleOffset);

        {
            int i;
            for (i = 0; i < ruleCount; i++) {
                MaterialStateMapRule *r = (MaterialStateMapRule *)((byte *)&rs->rules[0] + i * 0x20);
                r->stateBitsClear[0] = ~r->stateBitsClear[0];
                r->stateBitsClear[1] = ~r->stateBitsClear[1];
            }
        }
        *ruleSet = rs;
    }
    return 1;
}

static Bool MATERIAL_REGPARM3_ABI Material_ParseRuleSet(const char **text, const char *ruleSetName, const MtlStateMapBitGroup *stateSet, const MaterialStateMapRuleSet **ruleSet)
{
    return Material_ParseRuleSet_impl(text, ruleSetName, stateSet, ruleSet);
}

extern void *Material_FindStateMap(const char *name);
extern void Material_SetStateMap(const char *name, void *stateMap);
#ifndef MATERIAL_ALLOC_DECLARED
#    define MATERIAL_ALLOC_DECLARED
extern void *Material_Alloc(int size);
#endif
extern void Com_BeginParseSession(const char *name);
extern void Com_SetScriptWarningPrefix(const char *prefix);
extern void Com_SetSpaceDelimited(int value);
extern void Com_EndParseSession(void);
extern int FS_ReadFile(const char *path, void **data);
extern void FS_FreeFile(void *data);

static Bool MATERIAL_REGPARM2_ABI Material_LoadPassStateMap_impl(const char **text, MaterialStateMap **stateMapOut)
{
    const char *token;
    void *existing;

    if (!Com_MatchToken(text, "stateMap", 1))
        return 0;

    token = Com_Parse(text);
    if (token[0] == '\0' || token[0] == ';') {
        Com_ScriptWarning("missing stateMap name\n");
        return 0;
    }

    existing = Material_FindStateMap(token);
    if (!existing) {
        char filename[64];
        void *fileData;
        Com_sprintf(filename, 64, "materials/statemaps/%s.sm", token);

        if (FS_ReadFile(filename, &fileData) < 0) {
            Com_ScriptWarning("Couldn't open statemap '%s'\n", filename);
        } else {
            const char *smText = (const char *)fileData;
            int nameLen = (int)strlen(token) + 1;
            byte *sm = (byte *)Material_Alloc(0x30 + nameLen);
            *(const char **)sm = (char *)sm + 0x30;
            memcpy(sm + 0x30, token, nameLen);

            Com_BeginParseSession(filename);
            Com_SetScriptWarningPrefix("");
            Com_SetSpaceDelimited(0);

            if (!Material_ParseRuleSet_impl(&smText, "alphaTest",
                                            s_stateMapDstAlphaTestBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x04)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "blendFunc",
                                            s_stateMapDstBlendFuncRgbBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x08)))
                goto sm_fail;

            if (((byte *)imp_dx)[0x2d7c] == 0) {
                byte *rs = (*(byte **)&((Material *)sm)->info.hashIndex);
                int rc = *(int *)rs;
                byte *rule = rs;
                int i;
                for (i = 0; i < rc; i++, rule += 0x20) {
                    unsigned int v1c = ((MaterialStateMapRule *)rule)->stateBitsClear[1];
                    if (((v1c >> 8) & 7) != 0) {
                        unsigned int v14 = ((MaterialStateMapRule *)rule)->stateBitsValue[1];
                        if ((v14 & 0x700) > 0x100) {
                            ((MaterialStateMapRule *)rule)->stateBitsClear[1] = v1c | 0x7ff;
                            ((MaterialStateMapRule *)rule)->stateBitsValue[1] = (v14 & 0xfffff800) | 0x111;
                        }
                    }
                }
            }

            if (!Material_ParseRuleSet_impl(&smText, "separateAlphaBlendFunc",
                                            s_stateMapDstBlendFuncAlphaBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x0C)))
                goto sm_fail;

            {
                byte *alphaRS = *(byte **)(sm + 0x0C);
                if (((byte *)imp_dx)[0x2d7d] == 0) {

                    *(int *)alphaRS = 1;
                    ((MaterialStateMapRule *)alphaRS)->stateBitsMask[1] = 0;
                    ((MaterialStateMapRule *)alphaRS)->stateBitsValue[0] = 0;
                    ((MaterialStateMapRule *)alphaRS)->stateBitsValue[1] = 0;
                    ((MaterialStateMapRule *)alphaRS)->stateBitsSet[0] = 0;
                    ((MaterialStateMapRule *)alphaRS)->stateBitsClear[1] |= 0x7ff0000;
                    {
                        unsigned int v14 = *(unsigned int *)(alphaRS + 0x14);
                        *(unsigned int *)(alphaRS + 0x14) = (v14 & 0xf800ffff) | 0x120000;
                    }
                } else if (((byte *)imp_dx)[0x2d7c] == 0) {
                    int rc = *(int *)alphaRS;
                    byte *rule = alphaRS;
                    int i;
                    for (i = 0; i < rc; i++, rule += 0x20) {
                        unsigned int v1c = ((MaterialStateMapRule *)rule)->stateBitsClear[1];
                        if ((v1c & 0x7000000) != 0) {
                            unsigned int v14 = ((MaterialStateMapRule *)rule)->stateBitsValue[1];
                            if ((v14 & 0x7000000) > 0x1000000) {
                                ((MaterialStateMapRule *)rule)->stateBitsClear[1] = v1c | 0x7ff0000;
                                ((MaterialStateMapRule *)rule)->stateBitsValue[1] = (v14 & 0xf800ffff) | 0x01110000;
                            }
                        }
                    }
                }
            }

            if (!Material_ParseRuleSet_impl(&smText, "cullFace",
                                            s_stateMapDstCullFaceBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x10)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "depthTest",
                                            s_stateMapDstDepthTestBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x14)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "depthWrite",
                                            s_stateMapDstDepthWriteBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x18)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "colorWrite",
                                            s_stateMapDstColorWriteBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x1C)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "fog",
                                            s_stateMapDstFogBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x20)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "polygonOffset",
                                            s_stateMapDstPolygonOffsetBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x24)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "stencil",
                                            s_stateMapDstStencilBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x28)))
                goto sm_fail;
            if (!Material_ParseRuleSet_impl(&smText, "wireframe",
                                            s_stateMapDstWireframeBitGroup,
                                            (const MaterialStateMapRuleSet **)(sm + 0x2C)))
                goto sm_fail;

            goto sm_done;
        sm_fail:
            sm = NULL;
        sm_done:
            Com_EndParseSession();
            FS_FreeFile(fileData);
            if (sm)
                Material_SetStateMap(token, sm);
            existing = sm;
        }
    }

    *stateMapOut = (MaterialStateMap *)existing;
    if (!existing)
        return 0;
    return Com_MatchToken(text, ";", 1) ? 1 : 0;
}

static Bool MATERIAL_REGPARM2_ABI Material_LoadPassStateMap(const char **text, MaterialStateMap **stateMap)
{
    return Material_LoadPassStateMap_impl(text, stateMap);
}

extern float floorf(float x);
extern void *Material_Alloc(int size);
extern void *Material_FindShader(const char *name, int shaderType, int shaderVersion);
extern void Material_SetShader(const char *name, int shaderType, int shaderVersion, void *shader);
extern byte __ZTV12IncludeClass[];
extern int stricmp(const char *s1, const char *s2);
extern HRESULT D3DXCompileShader(const char *src, int srcLen, const void *defines, void *include,
                                 const char *entry, const char *target, int flags, void **shader, void **messages, void **constants);
extern const char *R_ErrorDescription(HRESULT hr);

static MaterialShader *MATERIAL_REGPARM2_ABI COD2_FORCE_ALIGN_ARG_POINTER Material_LoadPassShader_impl(const char **text, int shaderType)
{
    float fversion;
    int version;
    const char *filename;
    MaterialShader *mtlShader;
    char target[16];
    const char *entryPoint;
    char path[64];
    int count, lo, hi, mid, cmp;
    byte *entries, *entry;
    byte *fileData;
    int fileSize;
    HRESULT hr;
    void *shaderBlob, *messages;
    byte includeObj[8];
    const void *defines[8];
    int shaderSize, nameLen, allocSize;
    byte *shaderDataPtr;

    fversion = Com_ParseFloat(text);
    version = (int)floorf(fversion * 10.0f + 0.5f);
    if (version > 20)
        version = 20;

    filename = Com_Parse(text);

    mtlShader = (MaterialShader *)Material_FindShader(filename, shaderType, version);
    if (mtlShader)
        return mtlShader;

    {
        int compileVer = version;
#ifdef GFX_REAL_D3D9

        if (compileVer < 20)
            compileVer = 20;
#endif
        if (shaderType == 0) {
            Com_sprintf(target, 16, "vs_%i_%i", compileVer / 10, compileVer % 10);
            entryPoint = "vs_main";
        } else {
            Com_sprintf(target, 16, "ps_%i_%i", compileVer / 10, compileVer % 10);
            entryPoint = "ps_main";
        }
    }

    Com_sprintf(path, 64, "materials/shaders/%s", filename);

    count = MTLGLOB_COUNT;
    entries = MTLGLOB_PTR(byte);
    entry = NULL;
    lo = 0;
    hi = count - 1;
    while (lo <= hi) {
        mid = (lo + hi) / 2;
#if defined(__x86_64__)
        const char *entryName = *(const char **)(entries + mid * sizeof(GfxCachedShaderText));
#else
        const char *entryName = *(const char **)(entries + mid * 12);
#endif
        cmp = stricmp(filename, entryName);
        if (cmp == 0) {
#if defined(__x86_64__)
            entry = entries + mid * sizeof(GfxCachedShaderText);
#else
            entry = entries + mid * 12;
#endif
            break;
        } else if (cmp > 0) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    if (!entry) {
        Com_ScriptWarning("Shader '%s' wasn't preloaded\n", path);
        return NULL;
    }

    fileData = *(byte **)(entry + 4);
    fileSize = *(int *)(entry + 8);

    {
        char sourceName[256];
        const char *ext = (shaderType == 0) ? ".vs" : ".ps";
        int baseLen = (int)strlen(filename);

        if (baseLen >= 5) {
            const char *hlslExt = filename + baseLen - 5;
            if (hlslExt[0] == '.' && hlslExt[1] == 'h') {
                baseLen -= 5;
            }
        }
        memcpy(sourceName, filename, baseLen);
        sourceName[baseLen] = '\0';
        {
            const char *e = ext;
            int i = baseLen;
            while (*e)
                sourceName[i++] = *e++;
            sourceName[i] = '\0';
        }

        defines[0] = "IW_HLSL";
        defines[1] = "1";
        defines[2] = NULL;
        defines[3] = NULL;

#ifdef GFX_REAL_D3D9
        {
            extern void *g_realIncludeVtbl[2];
            *(void **)includeObj = (void *)g_realIncludeVtbl;
        }
#else
        *(void **)includeObj = (void *)(__ZTV12IncludeClass + 8);
#endif

        shaderBlob = NULL;
        messages = NULL;

#ifdef GFX_REAL_D3D9

        {
            static const char *const realDefs[] = {
                "IW_HLSL", "1",
                "PC", "1",
                "half", "float",
                "half2", "float2",
                "half3", "float3",
                "half4", "float4",
                "half2x2", "float2x2",
                "half3x3", "float3x3",
                "half4x4", "float4x4",
                "half4x3", "float4x3",
                "half3x4", "float3x4",
                NULL, NULL
            };

            DWORD compileFlags = 0x10;
            {
                const char *mf = getenv("REALD3D9_MATFLAG");
                if (mf)
                    compileFlags = (DWORD)strtoul(mf, NULL, 16);
            }
            hr = D3DXCompileShader((const char *)fileData, fileSize, realDefs, includeObj,
                                   entryPoint, target, compileFlags, &shaderBlob, &messages, NULL);
        }
#else
        hr = D3DXCompileShader(sourceName, fileSize, defines, includeObj,
                               entryPoint, target, 0, &shaderBlob, &messages, NULL);
#endif

#ifdef GFX_REAL_D3D9
        if (getenv("REALD3D9_SHLOG"))
            fprintf(stderr, "[SHLOG] %-44s %-8s hr=0x%08x %s\n", filename, target,
                    (unsigned)hr, (hr < 0) ? "FAIL" : "ok");
#endif

        if (messages) {
            void **msgVtable = *(void ***)messages;
            typedef const char *(D3DVTCC * GetBufPtrFn)(void *);
            typedef int(D3DVTCC * ReleaseFn)(void *);
            const char *msgText = ((GetBufPtrFn)msgVtable[3])(messages);
            Com_Printf("compiler message(s) for %s:\n%s\n", path, msgText);
            ((ReleaseFn)msgVtable[2])(messages);
        }

        if (hr < 0) {
            const char *errDesc = R_ErrorDescription(hr);
            Com_ScriptWarning("%s compilation failed - %s\n", path, errDesc);
            mtlShader = NULL;
            goto cleanup_sourceName;
        }

        if (!shaderBlob) {
            Com_ScriptWarning("%s compilation failed - NULL shader\n", path);
            mtlShader = NULL;
            goto cleanup_sourceName;
        }

        {
            void **blobVtable = *(void ***)shaderBlob;
            typedef int(D3DVTCC * GetSizeFn)(void *);
            typedef void *(D3DVTCC * GetPtrFn)(void *);
            typedef int(D3DVTCC * ReleaseFn)(void *);
            shaderSize = ((GetSizeFn)blobVtable[4])(shaderBlob);
            shaderDataPtr = (byte *)((GetPtrFn)blobVtable[3])(shaderBlob);

            nameLen = (int)strlen(filename) + 1;
            allocSize = 0x10 + shaderSize + nameLen;
            mtlShader = (MaterialShader *)Material_Alloc(allocSize);

            {
                byte *mtl = (byte *)mtlShader;

                ((MaterialShader *)mtl)->program = (void (*)(void))(mtl + 0x10);
                ((MaterialShader *)mtl)->name = (const char *)(mtl + 0x10 + shaderSize);
                memcpy(mtl + 0x10 + shaderSize, filename, nameLen);
                memcpy(mtl + 0x10, shaderDataPtr, shaderSize);
                *(unsigned short *)(mtl + 8) = (unsigned short)(shaderSize >> 2);
                ((MaterialShader *)mtl)->shaderType = (byte)shaderType;
                ((MaterialShader *)mtl)->shaderVersion = (byte)version;

                {
                    void *device = dx.device;
                    void **devVtable = *(void ***)device;
                    typedef HRESULT(D3DVTCC * CreateShaderFn)(void *, const void *, void **);
                    int vtableOffset = (shaderType == 0) ? (0x16C / 4) : (0x1A8 / 4);
                    hr = ((CreateShaderFn)devVtable[vtableOffset])(device, mtl + 0x10, (void **)(mtl + 0xC));
                }
            }

            ((ReleaseFn)blobVtable[2])(shaderBlob);

            if (hr < 0) {
                Com_ScriptWarning("shader creation failed for %s %s %s: %s\n",
                                  path, target, entryPoint, R_ErrorDescription(hr));
                mtlShader = NULL;
                goto cleanup_sourceName;
            }
        }

    cleanup_sourceName:

        (void)0;
    }

    if (mtlShader) {
        Material_SetShader(filename, shaderType, version, mtlShader);
    }
    return mtlShader;
}

static MaterialShader *MATERIAL_REGPARM2_ABI Material_LoadPassShader(const char **text, int shaderType)
{
    return Material_LoadPassShader_impl(text, shaderType);
}

extern void *Material_FindTechniqueSet(const char *name);
extern void Material_SetTechniqueSet(const char *name, void *techSet);
extern void *Material_FindTechnique(const char *name);
extern void Material_SetTechnique(const char *name, void *technique);
extern void *Material_AllocVertexDecl(byte *routing, int routingCount, byte *existing);
extern void Load_BuildVertexDecl(void *vertexDecl);
extern void *Image_Register(const char *name, int semantic, int imageTrack);
extern void *R_LoadWaterSetup(byte *setupData);
extern void Com_SetKeepStringQuotes(int value);
extern const byte s_techniqueTypeNames[];

static Bool MATERIAL_REGPARM2_ABI Material_FinishLoadingInstance_impl(MaterialObj *material, int imageTrack)
{
    byte *mtl = (byte *)material;
    int i, isDx7;

    *(int *)(mtl + 0) += (int)mtl;
    *(int *)(mtl + 4) += (int)mtl;
    ((Material *)mtl)->textures = (MaterialTextureDef *)((int)((Material *)mtl)->textures + (int)mtl);

    {
        unsigned short texCount = ((Material *)mtl)->textureCount;
        byte *tex = (byte *)((Material *)mtl)->textures;
        for (i = 0; i < texCount; i++, tex += 0x0c) {
            *(int *)tex = (int)Material_RegisterString((const char *)((int)mtl + *(int *)tex));
            byte sem = *(byte *)(tex + 5);
            if (sem == 5) {
                byte *wd = (byte *)((int)mtl + *(int *)(tex + 8));
                *(int *)(tex + 8) = (int)wd;
                byte setup[0x44];
                memset(setup, 0, sizeof(setup));
                *(int *)(setup + 0x0c) = (int)*(unsigned short *)wd;
                *(int *)(setup + 0x10) = (int)*(unsigned short *)wd;
                *(int *)(setup + 0x14) = *(int *)(wd + 4);
                *(int *)(setup + 0x18) = *(int *)(wd + 8);
                *(int *)(setup + 0x1c) = 0x44480000;
                *(int *)(setup + 0x20) = ((GfxWorld *)wd)->surfaceCount;
                *(int *)(setup + 0x24) = (*(int *)&((GfxWorld *)wd)->surfaces);
                *(int *)(setup + 0x28) = ((GfxWorld *)wd)->skySurfCount;
                *(int *)(setup + 0x2c) = (*(int *)&((GfxWorld *)wd)->nodes);
                *(int *)(setup + 0x40) = 0;
                int wres = (int)R_LoadWaterSetup(setup);
                (*(int *)&((GfxWorld *)wd)->skyStartSurfs) = wres;
                if (!wres)
                    return 0;
            } else {
                isDx7 = (r_rendererInUse->current.integer == 2);
                if (isDx7 && (unsigned)(sem - 3) <= 1) {
                    *(int *)(tex + 8) = 0;
                } else {
                    void *img = Image_Register((const char *)((int)mtl + *(int *)(tex + 8)), sem, imageTrack);
                    *(int *)(tex + 8) = (int)img;
                    if (!img)
                        return 0;
                }
            }
        }
    }

    ((Material *)mtl)->constants = (MaterialConstantDef *)((int)((Material *)mtl)->constants + (int)mtl);
    {
        unsigned short cc = ((Material *)mtl)->constantCount;
        byte *ce = (byte *)((Material *)mtl)->constants;
        for (i = 0; i < cc; i++, ce += 0x14) {
            *(int *)ce = (int)Material_RegisterString((const char *)((int)mtl + *(int *)ce));
            if (!*(int *)ce)
                return 0;
        }
    }

    {
        const char *tsName = (const char *)((int)mtl + (*(int *)&((Material *)mtl)->techniqueSet));
        MaterialTechniqueSet *techSet = (MaterialTechniqueSet *)Material_FindTechniqueSet(tsName);
#ifdef GFX_REAL_D3D9
        if (getenv("REALD3D9_MATDIAG")) {
            static int tl;
            if (tl++ < 20) {
                fprintf(stderr, "[TSLOAD] mtl='%.24s' tsOff=%d tsName='%.32s' found=%p\n",
                        *(const char **)mtl, (*(int *)&((Material *)mtl)->techniqueSet), (tsName && (unsigned)tsName > 0x10000) ? tsName : "(bad)", (void *)techSet);
                fflush(stderr);
            }
        }
#endif
        if (!techSet) {
            isDx7 = (r_rendererInUse->current.integer == 2);
            char tsFile[64];
            void *tsData;
            Com_sprintf(tsFile, 0x40, isDx7 ? "materials_dx7/techniquesets/%s.techset" : "materials/techniquesets/%s.techset", tsName);
            int frr = FS_ReadFile(tsFile, &tsData);
#ifdef GFX_REAL_D3D9
            if (getenv("REALD3D9_MATDIAG")) {
                static int fl;
                if (fl++ < 20) {
                    fprintf(stderr, "[TSFILE] '%s' FS_ReadFile=%d\n", tsFile, frr);
                    fflush(stderr);
                }
            }
#endif
            if (frr < 0) {
                Com_Printf("^1ERROR: Couldn't open techniqueSet '%s'\n", tsFile);
                techSet = NULL;
                goto storeTechSet;
            }
            int tsNLen = (int)strlen(tsName) + 1;
            techSet = (MaterialTechniqueSet *)Material_Alloc(sizeof(MaterialTechniqueSet) + tsNLen);
            techSet->name = (const char *)((byte *)techSet + sizeof(MaterialTechniqueSet));
            memcpy((byte *)techSet + sizeof(MaterialTechniqueSet), tsName, tsNLen);
            const char *tsText = (const char *)tsData;
            Com_BeginParseSession(tsFile);
            Com_SetScriptWarningPrefix("^1ERROR: ");
            Com_SetSpaceDelimited(0);
            Com_SetKeepStringQuotes(1);
            int ttCount = 0, ttSlots[34], ttUsing = 0;
            for (;;) {
                const char *tok = Com_Parse(&tsText);
                if (*(byte *)tok == 0)
                    break;
                if (*(byte *)tok == '"') {
                    if (ttCount >= 0x22) {
                        Com_ScriptWarning("Too many technique types\n");
                        techSet = NULL;
                        break;
                    }
                    const char *ttn[34];
                    memcpy(ttn, s_techniqueTypeNames, sizeof(ttn));
                    int tt;
                    for (tt = 0; tt < 0x22; tt++)
                        if (strcmp(tok, ttn[tt]) == 0)
                            break;
                    ttSlots[ttCount] = tt;
                    if (tt == 0x22) {
                        Com_ScriptWarning("Unknown technique type '%s'\n", tok);
                        techSet = NULL;
                        break;
                    }
#ifdef GFX_REAL_D3D9
                    if (getenv("REALD3D9_MATDIAG")) {
                        static int tk;
                        if (tk++ < 80) {
                            fprintf(stderr, "[TTYPE] ts='%.20s' tok='%.20s' tt=%d use=%d ttn0='%.16s' ttn1='%.16s'\n",
                                    techSet ? techSet->name : "?", tok, tt, g_useTechnique[tt], ttn[0], ttn[1]);
                            fflush(stderr);
                        }
                    }
#endif
                    if (g_useTechnique[tt] != 0)
                        ttUsing = 1;
                    ttCount++;
                    if (!Com_MatchToken(&tsText, ":", 1)) {
                        techSet = NULL;
                        break;
                    }
                    continue;
                }
                if (ttCount == 0) {
                    Com_ScriptWarning("Unknown technique type '%s'\n", tok);
                    techSet = NULL;
                    break;
                }
                const char *techName = tok;
                MaterialTechnique *technique = NULL;
                if (ttUsing) {
                    technique = (MaterialTechnique *)Material_FindTechnique(techName);
                    if (!technique) {
                        isDx7 = (r_rendererInUse->current.integer == 2);
                        if (!isDx7) {

                            char tf[64];
                            void *tfd;
                            Com_sprintf(tf, 0x40, "materials/techniques/%s.tech", techName);
                            if (FS_ReadFile(tf, &tfd) < 0) {
                                Com_ScriptWarning("Couldn't open technique '%s'\n", tf);
                                techSet = NULL;
                                goto endTsParse;
                            }
                            const char *ttext = (const char *)tfd;
                            Com_BeginParseSession(tf);
                            Com_SetScriptWarningPrefix("^1ERROR: ");
                            Com_SetSpaceDelimited(0);
                            unsigned short ltf = 0;
                            int pi2 = 0;
                            unsigned short lpc = 0;
                            byte lpd[4 * 0x1c];
                            byte *cp = lpd;
                            int terr = 0;
                            while (pi2 < 4) {
                                lpc = (unsigned short)pi2;
                                const char *pt = Com_Parse(&ttext);
                                if (*(byte *)pt == 0)
                                    break;
                                if (*(byte *)pt != '{') {
                                    Com_ScriptWarning("expected '{' but found '%s'\n", pt);
                                    terr = 1;
                                    break;
                                }
                                if (!Material_LoadPassStateMap_impl(&ttext, (MaterialStateMap **)cp)) {
                                    terr = 1;
                                    break;
                                }

                                int rc = 0;
                                byte rd[32];
                                int rok = 1;
                                for (;;) {
                                    if (rc >= 16) {
                                        Com_ScriptWarning("More than %i vertex mappings\n", 16);
                                        rok = 0;
                                        break;
                                    }
                                    const char *vt = Com_Parse(&ttext);
                                    if (strcmp(vt, "vertex") != 0) {
                                        Com_UngetToken();
                                        break;
                                    }
                                    if (!Com_MatchToken(&ttext, ".", 1)) {
                                        rok = 0;
                                        break;
                                    }
                                    const char *dn = Com_Parse(&ttext);
                                    byte di;
                                    if (strcmp(dn, "position") == 0)
                                        di = 0;
                                    else if (strcmp(dn, "normal") == 0)
                                        di = 1;
                                    else if (strcmp(dn, "color") == 0) {
                                        if (!Com_MatchToken(&ttext, "[", 1)) {
                                            rok = 0;
                                            break;
                                        }
                                        int ci = Com_ParseInt(&ttext);
                                        if (ci < 0 || ci > 1)
                                            Com_ScriptWarning("index '%i' is not in the range [0, %i]\n", ci, 1);
                                        if (!Com_MatchToken(&ttext, "]", 1)) {
                                            rok = 0;
                                            break;
                                        }
                                        di = (byte)(ci + 2);
                                    } else if (strcmp(dn, "texcoord") == 0) {
                                        if (!Com_MatchToken(&ttext, "[", 1)) {
                                            rok = 0;
                                            break;
                                        }
                                        int ti = Com_ParseInt(&ttext);
                                        if (ti < 0 || ti > 7)
                                            Com_ScriptWarning("index '%i' is not in the range [0, %i]\n", ti, 7);
                                        if (!Com_MatchToken(&ttext, "]", 1)) {
                                            rok = 0;
                                            break;
                                        }
                                        di = (byte)(ti + 4);
                                    } else {
                                        Com_ScriptWarning("unknown stream destination '%s'\n", dn);
                                        rok = 0;
                                        break;
                                    }
                                    if (!Com_MatchToken(&ttext, "=", 1) || !Com_MatchToken(&ttext, "code", 1) || !Com_MatchToken(&ttext, ".", 1)) {
                                        rok = 0;
                                        break;
                                    }
                                    const char *sn = Com_Parse(&ttext);
                                    byte si;
                                    if (strcmp(sn, "position") == 0)
                                        si = 0;
                                    else if (strcmp(sn, "normal") == 0)
                                        si = 1;
                                    else if (strcmp(sn, "color") == 0)
                                        si = 2;
                                    else if (strcmp(sn, "texcoord") == 0) {
                                        if (!Com_MatchToken(&ttext, "[", 1)) {
                                            rok = 0;
                                            break;
                                        }
                                        int sti = Com_ParseInt(&ttext);
                                        if (sti < 0 || sti > 1)
                                            Com_ScriptWarning("index '%i' is not in the range [0, %i]\n", sti, 1);
                                        if (!Com_MatchToken(&ttext, "]", 1)) {
                                            rok = 0;
                                            break;
                                        }
                                        si = (byte)(sti + 3);
                                    } else if (strcmp(sn, "tangent") == 0)
                                        si = 6;
                                    else if (strcmp(sn, "binormal") == 0)
                                        si = 5;
                                    else {
                                        Com_ScriptWarning("unknown stream source '%s'\n", sn);
                                        rok = 0;
                                        break;
                                    }
                                    if (!Com_MatchToken(&ttext, ";", 1)) {
                                        rok = 0;
                                        break;
                                    }

                                    int ip = rc;
                                    if (rc > 0 && si <= rd[(rc - 1) * 2]) {
                                        int k;
                                        for (k = rc - 1; k >= 0; k--) {
                                            if (si > rd[k * 2] || (si == rd[k * 2] && di > rd[k * 2 + 1])) {
                                                ip = k + 1;
                                                break;
                                            }
                                            rd[(k + 1) * 2] = rd[k * 2];
                                            rd[(k + 1) * 2 + 1] = rd[k * 2 + 1];
                                            ip = k;
                                        }
                                    }
                                    rd[ip * 2] = si;
                                    rd[ip * 2 + 1] = di;
                                    rc++;
                                }
                                if (!rok) {
                                    terr = 1;
                                    break;
                                }
                                {
                                    byte ef = 0;
                                    void *vd = Material_AllocVertexDecl(rd, rc, &ef);
                                    *(int *)(cp + 4) = (int)vd;
                                    if (!ef)
                                        Load_BuildVertexDecl((void *)(cp + 4));
                                }
#ifdef GFX_REAL_D3D9
#    define D9DBG(tag)                                                              \
        do {                                                                        \
            if (getenv("REALD3D9_MATDIAG")) {                                       \
                static int n;                                                       \
                if (n++ < 12) {                                                     \
                    fprintf(stderr, "[PASSFAIL] %s tech='%.24s'\n", tag, techName); \
                    fflush(stderr);                                                 \
                }                                                                   \
            }                                                                       \
        } while (0)
#else
#    define D9DBG(tag)
#endif
                                if (!Com_MatchToken(&ttext, "vertexShader", 1)) {
                                    D9DBG("vsMatch");
                                    terr = 1;
                                    break;
                                }
                                {
                                    void *vs = (void *)Material_LoadPassShader_impl(&ttext, 0);
                                    if (!vs) {
                                        D9DBG("vsLoad");
                                        terr = 1;
                                        break;
                                    }
                                    *(int *)(cp + 0x08) = (int)vs;
                                    if (!Material_SetPassShaderArguments_impl(&ttext, (const byte *)vs, &ltf,
                                                                              (unsigned short *)(cp + 0x10), (MaterialShaderArgument **)(cp + 0x14))) {
                                        D9DBG("vsArgs");
                                        terr = 1;
                                        break;
                                    }
                                }
                                if (!Com_MatchToken(&ttext, "pixelShader", 1)) {
                                    D9DBG("psMatch");
                                    terr = 1;
                                    break;
                                }
                                {
                                    void *ps = (void *)Material_LoadPassShader_impl(&ttext, 1);
                                    if (!ps) {
                                        D9DBG("psLoad");
                                        terr = 1;
                                        break;
                                    }
                                    *(int *)(cp + 0x0c) = (int)ps;
                                    if (!Material_SetPassShaderArguments_impl(&ttext, (const byte *)ps, &ltf,
                                                                              (unsigned short *)(cp + 0x12), (MaterialShaderArgument **)(cp + 0x18))) {
                                        D9DBG("psArgs");
                                        terr = 1;
                                        break;
                                    }
                                }
                                if (!Com_MatchToken(&ttext, "}", 1)) {
                                    D9DBG("braceMatch");
                                    terr = 1;
                                    break;
                                }
                                lpc = (unsigned short)(pi2 + 1);
                                pi2++;
                                cp += 0x1c;
                            }
                            Com_EndParseSession();
                            FS_FreeFile(tfd);
                            if (terr) {
                                techSet = NULL;
                                goto endTsParse;
                            }
                            if (lpc == 0) {
                                Com_ScriptWarning("Technique '%s' has no passes.  The technique should be left out of the techset\n", techName);
                                techSet = NULL;
                                goto endTsParse;
                            }
                            {
                                int nl = (int)strlen(techName) + 1;
                                int pds = (int)lpc * 0x1c;
                                technique = (MaterialTechnique *)Material_Alloc(8 + nl + pds);
                                technique->name = (const char *)((byte *)technique + 8 + pds);
                                memcpy((byte *)technique + 8 + pds, techName, nl);
                                technique->flags = ltf;
                                technique->passCount = lpc;
                                memcpy((byte *)&technique->passArray, lpd, pds);
                            }
#ifdef GFX_REAL_D3D9
                            if (getenv("REALD3D9_MATDIAG")) {
                                static int to;
                                if (to++ < 16) {
                                    fprintf(stderr, "[TECHOK] ts='%.20s' tech='%.24s' passes=%d\n", techSet ? techSet->name : "?", techName, lpc);
                                    fflush(stderr);
                                }
                            }
#endif
                            Material_SetTechnique(techName, technique);
                        } else {

                            char tf[64];
                            void *tfd;
                            Com_sprintf(tf, 0x40, "materials_dx7/techniques/%s.tech", techName);
                            if (FS_ReadFile(tf, &tfd) < 0) {
                                Com_ScriptWarning("Couldn't open technique '%s'\n", tf);
                                techSet = NULL;
                                goto endTsParse;
                            }
                            const char *dtext = (const char *)tfd;
                            Com_BeginParseSession(tf);
                            Com_SetScriptWarningPrefix("^1ERROR: ");
                            Com_SetSpaceDelimited(0);
                            int dpi = 0;
                            unsigned short dpc = 0;
                            byte dpd[4 * 0x5c];
                            byte *dcp = dpd;
                            int derr = 0;
                            while (dpi < 4) {
                                dpc = (unsigned short)dpi;
                                const char *pt = Com_Parse(&dtext);
                                if (*(byte *)pt == 0)
                                    break;
                                if (*(byte *)pt != '{') {
                                    Com_ScriptWarning("expected '{' but found '%s'\n", pt);
                                    derr = 1;
                                    break;
                                }
                                if (!Material_LoadPassStateMap_impl(&dtext, (MaterialStateMap **)dcp)) {
                                    derr = 1;
                                    break;
                                }
                                {
                                    int oi;
                                    for (oi = 0; oi < 5; oi++) {
                                        byte *sp = (byte *)&s_passOptionsDx7[oi];
                                        *(byte *)(dcp + *(int *)(sp + 4)) = 0;
                                    }
                                }
                                {
                                    int optErr = 0;
                                    for (;;) {
                                        const char *ot = Com_Parse(&dtext);
                                        int oi;
                                        for (oi = 0; oi < 5; oi++) {
                                            byte *sp = (byte *)&s_passOptionsDx7[oi];
                                            if (strcmp(*(const char **)sp, ot) == 0)
                                                break;
                                        }
                                        if (oi >= 5) {
                                            Com_UngetToken();
                                            break;
                                        }
                                        if (!Com_MatchToken(&dtext, "(", 1) || !Com_MatchToken(&dtext, ")", 1) || !Com_MatchToken(&dtext, ";", 1)) {
                                            optErr = 1;
                                            break;
                                        }
                                        {
                                            byte *sp = (byte *)&s_passOptionsDx7[oi];
                                            *(byte *)(dcp + *(int *)(sp + 4)) = 1;
                                        }
                                    }
                                    if (optErr) {
                                        derr = 1;
                                        break;
                                    }
                                }
                                {
                                    int ti = 0;
                                    int texturesParsed;

                                    for (;;) {
                                        byte *ap = dcp + 0x0c + ti * 8;
                                        if (!Com_MatchToken(&dtext, "texture", 1)) {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        if (Com_MatchToken(&dtext, "[", 1)) {
                                            int tidx = Com_ParseInt(&dtext);
                                            if (tidx != ti) {
                                                Com_ScriptWarning("expected %i, found %i instead\n", ti, tidx);
                                                derr = 1;
                                                goto endDx7Tech;
                                            }
                                            if (!Com_MatchToken(&dtext, "]", 1)) {
                                                derr = 1;
                                                goto endDx7Tech;
                                            }
                                        } else {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        if (!Com_MatchToken(&dtext, "=", 1)) {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        if (!Material_ParseSamplerSource_impl(&dtext, (MaterialShaderArgument *)ap)) {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        if (!Com_MatchToken(&dtext, ";", 1)) {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        *(byte *)(dcp + ti + 9) = 0;
                                        {
                                            const char *tct = Com_Parse(&dtext);
                                            if (strcmp(tct, "texcoord") == 0) {
                                                if (!Com_MatchToken(&dtext, "[", 1)) {
                                                    derr = 1;
                                                    goto endDx7Tech;
                                                }
                                                int tci = Com_ParseInt(&dtext);
                                                if (tci != ti) {
                                                    Com_ScriptWarning("expected %i, found %i instead\n", ti, tci);
                                                    derr = 1;
                                                    goto endDx7Tech;
                                                }
                                                if (!Com_MatchToken(&dtext, "]", 1)) {
                                                    derr = 1;
                                                    goto endDx7Tech;
                                                }
                                                if (!Com_MatchToken(&dtext, "=", 1)) {
                                                    derr = 1;
                                                    goto endDx7Tech;
                                                }
                                                const char *tcv = Com_Parse(&dtext);
                                                if (strcmp(tcv, "genEyeDirCoords") == 0)
                                                    *(byte *)(dcp + ti + 9) = 1;
                                                else if (strcmp(tcv, "texScroll") == 0)
                                                    *(byte *)(dcp + ti + 9) = 2;
                                                else {
                                                    Com_ScriptWarning("expected 'genEyeDirCoords' or 'texScroll', found '%s'\n", tcv);
                                                    derr = 1;
                                                    goto endDx7Tech;
                                                }
                                                if (!Com_MatchToken(&dtext, "(", 1) || !Com_MatchToken(&dtext, ")", 1) || !Com_MatchToken(&dtext, ";", 1)) {
                                                    derr = 1;
                                                    goto endDx7Tech;
                                                }
                                            } else
                                                Com_UngetToken();
                                        }
                                        texturesParsed = ti + 1;
                                        if (ti == 1)
                                            break;

                                        {
                                            const char *pk = Com_Parse(&dtext);
                                            int isT = strcmp(pk, "texture");
                                            Com_UngetToken();
                                            ti++;
                                            if (isT != 0)
                                                break;
                                        }
                                    }

                                    if ((unsigned short)texturesParsed <= 1) {
                                        int fi;
                                        for (fi = texturesParsed; fi < 2; fi++) {
                                            *(unsigned short *)(dcp + 0x0c + fi * 8) = 3;
                                            *(unsigned short *)(dcp + 0x0e + fi * 8) = (unsigned short)fi;
                                            *(int *)(dcp + 0x10 + fi * 8) = 1;
                                            *(byte *)(dcp + 9 + fi) = 0;
                                        }
                                    }
                                }
                                {
                                    int sti = 0;
                                    for (;;) {
                                        *(int *)(dcp + 0x1c + sti * 4) = 0;
                                        if (!Material_LoadPassTextureStateDx7_impl(&dtext, sti, "rgb", 1, (int *)(dcp + 0x1c + sti * 4))) {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        *(int *)(dcp + 0x3c + sti * 4) = 0;
                                        if (!Material_LoadPassTextureStateDx7_impl(&dtext, sti, "a", 2, (int *)(dcp + 0x3c + sti * 4))) {
                                            derr = 1;
                                            goto endDx7Tech;
                                        }
                                        sti++;
                                        if (sti > 7)
                                            break;
                                        const char *pk = Com_Parse(&dtext);
                                        Com_UngetToken();
                                        if (*(byte *)pk == '}')
                                            break;
                                    }
                                    if ((unsigned short)sti <= 7) {
                                        int ui;
                                        for (ui = sti; ui < 8; ui++) {
                                            *(int *)(dcp + 0x1c + ui * 4) = 0;
                                            *(int *)(dcp + 0x3c + ui * 4) = 0;
                                        }
                                    }
                                }
                                if (!Com_MatchToken(&dtext, "}", 1)) {
                                    derr = 1;
                                    break;
                                }
                                dpc = (unsigned short)(dpi + 1);
                                dpi++;
                                dcp += 0x5c;
                            }
                        endDx7Tech:
                            Com_EndParseSession();
                            FS_FreeFile(tfd);
                            if (derr) {
                                techSet = NULL;
                                goto endTsParse;
                            }
                            if (dpc == 0) {
                                Com_ScriptWarning("Technique '%s' has no passes.  The technique should be left out of the techset\n", techName);
                                techSet = NULL;
                                goto endTsParse;
                            }
                            {
                                int nl = (int)strlen(techName) + 1;
                                int pds = (int)dpc * 0x5c;
                                technique = (MaterialTechnique *)Material_Alloc(8 + nl + pds);
                                technique->name = (const char *)((byte *)technique + 8 + pds);
                                memcpy((byte *)technique + 8 + pds, techName, nl);
                                technique->passCount = dpc;
                                memcpy((byte *)&technique->passArray, dpd, pds);
                            }
                            Material_SetTechnique(techName, technique);
                        }
                    }
                }
                if (technique && ttCount > 0) {
                    int ti;
                    for (ti = 0; ti < ttCount; ti++)
                        techSet->techniques[ttSlots[ti]] = technique;
                }
#ifdef GFX_REAL_D3D9
                if (getenv("REALD3D9_MATDIAG")) {
                    static int pe;
                    if (pe++ < 400) {
                        fprintf(stderr, "[TSPARSE] ts='%.24s' techName='%.24s' technique=%p ttCount=%d ttUsing=%d\n",
                                techSet ? techSet->name : "?", techName, (void *)technique, ttCount, ttUsing);
                        fflush(stderr);
                    }
                }
#endif
                if (!Com_MatchToken(&tsText, ";", 1)) {
#ifdef GFX_REAL_D3D9
                    if (getenv("REALD3D9_MATDIAG")) {
                        fprintf(stderr, "[TSPARSE] ';' match FAILED -> techSet=NULL\n");
                        fflush(stderr);
                    }
#endif
                    techSet = NULL;
                    break;
                }
                ttCount = 0;
                ttUsing = 0;
            }
        endTsParse:
#ifdef GFX_REAL_D3D9
            if (getenv("REALD3D9_MATDIAG")) {
                static int es;
                if (es++ < 16) {
                    fprintf(stderr, "[ENDTS] ts='%.24s' techSet=%p\n", tsName, (void *)techSet);
                    fflush(stderr);
                }
            }
#endif
            Com_EndParseSession();
            FS_FreeFile(tsData);
            if (techSet)
                Material_SetTechniqueSet(tsName, techSet);
        }
    storeTechSet:
#ifdef GFX_REAL_D3D9
        if (getenv("REALD3D9_MATDIAG")) {
            static int ml;
            const char *mn = mtl ? *(const char **)mtl : "?";
            if (ml < 4000 && mn[0] != 0x24) {
                fprintf(stderr, "[MATLOAD] mtl='%.32s' techSet=%p tsName='%.24s'\n", mn, (void *)techSet, (techSet && tsName) ? tsName : "(none)");
                fflush(stderr);
                ml++;
            }
        }
#endif
        (*(int *)&((Material *)mtl)->techniqueSet) = (int)techSet;
        if (!techSet)
            return 0;

        {
            const char *tsNameStr = techSet->name;
            for (i = 0; i < 34; i++) {
                MaterialTechnique *tech = techSet->techniques[i];
                if (!tech)
                    continue;
                unsigned short passCount = tech->passCount;
                if (passCount == 0)
                    continue;
                isDx7 = (r_rendererInUse->current.integer == 2);
                if (isDx7) {
                    byte *pb = (byte *)tech + 8;
                    int pi;
                    for (pi = 0; pi < passCount; pi++, pb += 0x5c)
                        if (!Material_ValidatePassArguments_impl((const Material *)mtl, tsNameStr, (const char *)*(int *)tech, 2, (const MaterialShaderArgument *)&((MaterialPassDx9 *)pb)->pixelShader))
                            return 0;
                } else {
                    int pi;
                    for (pi = 0; pi < passCount; pi++) {
                        byte *passBase = (byte *)tech + 8 + pi * 0x1c;
                        if (!Material_ValidatePassArguments_impl((const Material *)mtl, tsNameStr, (const char *)*(int *)tech,
                                                                 (*(unsigned short *)&((MaterialPassDx9 *)passBase)->pixelArgCount), (const MaterialShaderArgument *)(*(int *)&((MaterialPassDx9 *)passBase)->pixelArgs)))
                            return 0;
                        if (!Material_ValidatePassArguments_impl((const Material *)mtl, tsNameStr, (const char *)*(int *)tech,
                                                                 (*(unsigned short *)&((MaterialPassDx9 *)passBase)->vertexArgCount), (const MaterialShaderArgument *)(*(int *)&((MaterialPassDx9 *)passBase)->vertexArgs)))
                            return 0;
                    }
                }
            }
        }
        return 1;
    }
}

static Bool MATERIAL_REGPARM2_ABI Material_FinishLoadingInstance(MaterialObj *material, int imageTrack)
{
    return Material_FinishLoadingInstance_impl(material, imageTrack);
}

extern int Material_LoadFile(const char *filename, int *fileHandle);
extern void *Material_Alloc(int size);

static Bool Material_IsUiLikeNameForLoad(const char *name)
{
    return strncmp(name, "ui/", 3) == 0 ||
           strncmp(name, "ui_", 3) == 0 ||
           strncmp(name, "menu/", 5) == 0 ||
           strncmp(name, "levelshots/", 11) == 0 ||
           stricmp(name, "$levelbriefing") == 0;
}

static Bool Material_HasImageExtensionForLoad(const char *name)
{
    int len = (int)strlen(name);

    if (len <= 4 || name[len - 4] != '.')
        return 0;

    return stricmp(name + len - 4, ".tga") == 0 ||
           stricmp(name + len - 4, ".jpg") == 0 ||
           stricmp(name + len - 4, ".iwi") == 0;
}

static Bool Material_HasExtensionlessAliasForLoad(const char *name)
{
    char aliasName[64];
    int aliasHandle;
    int aliasSize;
    int len;

    if (!Material_HasImageExtensionForLoad(name))
        return 0;

    len = (int)strlen(name);
    if (len >= (int)sizeof(aliasName))
        return 0;

    memcpy(aliasName, name, len - 4);
    aliasName[len - 4] = '\0';

    aliasSize = Material_LoadFile(aliasName, &aliasHandle);
    if (aliasSize < 0)
        return 0;

    FS_FCloseFile(aliasHandle);
    return 1;
}

static Bool Material_ShouldPrintMissingMaterial(const char *name)
{
    if (name[0] == '$' || Material_IsUiLikeNameForLoad(name))
        return 0;

    return !Material_HasExtensionlessAliasForLoad(name);
}

Material *Material_Load(const char *name, int imageTrack)
{
    int fileHandle;
    int fileSize = Material_LoadFile(name, &fileHandle);

    if (fileSize < 0) {
        if (Material_ShouldPrintMissingMaterial(name))
            Com_Printf("^1ERROR: Couldn't find material '%s'\n", name);
        return NULL;
    }

    if (fileSize == 0) {
        FS_FCloseFile(fileHandle);
        Com_Printf("^1ERROR: material '%s' has zero length\n", name);
        return NULL;
    }

    void *mtlData = Material_Alloc(fileSize);
    FS_Read(mtlData, fileSize, fileHandle);
    FS_FCloseFile(fileHandle);

    Bool result;
    result = Material_FinishLoadingInstance((MaterialObj *)mtlData, imageTrack);
    if (!result) {

        Com_Printf("Material_Load: '%s' FinishLoadingInstance failed, using partial data\n", name);
        return (Material *)mtlData;
    }
    return (Material *)mtlData;
}

void ZSt13__adjust_heapIP19GfxCachedShaderTextiS0_PFhRKS0_S3_EEvT_T0_S7_T1_T2_(
    GfxCachedShaderText *first, int holeIndex, int len, GfxCachedShaderText value, GfxCachedShaderTextCompFunc comp)
{
    int topIndex = holeIndex;
    int secondChild = 2 * holeIndex + 2;
    while (secondChild < len) {
        if (comp(&first[secondChild], &first[secondChild - 1]))
            secondChild--;
        first[holeIndex] = first[secondChild];
        holeIndex = secondChild;
        secondChild = 2 * secondChild + 2;
    }
    if (secondChild == len) {
        first[holeIndex] = first[len - 1];
        holeIndex = len - 1;
    }
    while (holeIndex > topIndex) {
        int parent = (holeIndex - 1) / 2;
        if (!comp(&first[parent], &value))
            break;
        first[holeIndex] = first[parent];
        holeIndex = parent;
    }
    first[holeIndex] = value;
}

void ZSt16__introsort_loopIP19GfxCachedShaderTextiPFhRKS0_S3_EEvT_S6_T0_T1_(
    GfxCachedShaderText *first, GfxCachedShaderText *last, int depth_limit, GfxCachedShaderTextCompFunc comp)
{
    while (last - first > 16) {
        if (depth_limit == 0) {

            int len = (int)(last - first);
            if (len >= 2) {

                int parent = (len - 2) / 2;
                for (;;) {
                    GfxCachedShaderText value = first[parent];
                    ZSt13__adjust_heapIP19GfxCachedShaderTextiS0_PFhRKS0_S3_EEvT_T0_S7_T1_T2_(first, parent, len, value, comp);
                    if (parent == 0)
                        break;
                    parent--;
                }

                while (last - first > 1) {
                    GfxCachedShaderText value;
                    last--;
                    value = *last;
                    *last = *first;
                    ZSt13__adjust_heapIP19GfxCachedShaderTextiS0_PFhRKS0_S3_EEvT_T0_S7_T1_T2_(first, 0, (int)(last - first), value, comp);
                }
            }
            return;
        }
        depth_limit--;
        {
            GfxCachedShaderText *midPtr = first + (last - first) / 2;
            GfxCachedShaderText pivot;
            GfxCachedShaderText *lo, *hi;

            if (comp(first, midPtr)) {
                if (comp(midPtr, last - 1))
                    pivot = *midPtr;
                else if (comp(first, last - 1))
                    pivot = *(last - 1);
                else
                    pivot = *first;
            } else {
                if (comp(first, last - 1))
                    pivot = *first;
                else if (comp(last - 1, midPtr))
                    pivot = *midPtr;
                else
                    pivot = *(last - 1);
            }

            lo = first;
            hi = last;
            for (;;) {
                while (comp(lo, &pivot))
                    lo++;
                hi--;
                while (comp(&pivot, hi))
                    hi--;
                if (!(lo < hi))
                    break;
                {
                    GfxCachedShaderText tmp = *lo;
                    *lo = *hi;
                    *hi = tmp;
                }
                lo++;
            }
            ZSt16__introsort_loopIP19GfxCachedShaderTextiPFhRKS0_S3_EEvT_S6_T0_T1_(lo, last, depth_limit, comp);
            last = lo;
        }
    }
}

const unsigned char _rd_g_useTechnique[96] __asm__("g_useTechnique") = {
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x01,
    0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
