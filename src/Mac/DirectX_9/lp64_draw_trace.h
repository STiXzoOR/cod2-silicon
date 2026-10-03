#include <float.h>
#if COD2_APPLE_SDK
/* Request exactly one frame by writing its output filename into the external
 * COD2_MAC_DRAW_TRACE request file. Numeric state only; no shader/game payloads. */
static FILE *macDrawTrace;
static unsigned macDrawNumber;
static float macPixelConstants[256][4];
static struct { const void *shader; char name[128]; } macShaderLabels[1024];
static unsigned macShaderLabelCount;
static void MacTrace_Label(const void *shader, const char *source)
{
    const char *name = source ? strstr(source, "#COD2CTAB:") : NULL;
    if (!name || macShaderLabelCount == 1024) return;
    name += 10;
    unsigned i = macShaderLabelCount++;
    macShaderLabels[i].shader = shader;
    snprintf(macShaderLabels[i].name, sizeof(macShaderLabels[i].name), "%.*s", (int)strcspn(name, "\r\n"), name);
}
static const char *MacTrace_ShaderName(const void *shader)
{
    for (unsigned i = macShaderLabelCount; i-- > 0; )
        if (macShaderLabels[i].shader == shader) return macShaderLabels[i].name;
    return "generic";
}

static void MacTrace_BeginFrame(void)
{
    static const char *request;
    static int initialized;
    char output[1024];
    FILE *file;
    if (!initialized) {
        request = getenv("COD2_MAC_DRAW_TRACE");
        initialized = 1;
    }
    if (!request || macDrawTrace || !(file = fopen(request, "r")))
        return;
    if (!fgets(output, sizeof(output), file)) output[0] = 0;
    fclose(file);
    remove(request);
    output[strcspn(output, "\r\n")] = 0;
    macDrawTrace = output[0] ? fopen(output, "w") : NULL;
    macDrawNumber = 0;
}

static void MacTrace_Floats(const float *values, unsigned count)
{
    fputc('[', macDrawTrace);
    for (unsigned i = 0; i < count; ++i)
        {
            if (i) fputc(',', macDrawTrace);
            if ((values[i] == values[i] && values[i] >= -FLT_MAX && values[i] <= FLT_MAX)) fprintf(macDrawTrace, "%.9g", values[i]);
            else fputs("null", macDrawTrace);
        }
    fputc(']', macDrawTrace);
}

static void MacTrace_Draw(DeviceImpl *dev, UINT type, INT base, UINT min, UINT vertices, UINT first, UINT primitives)
{
    if (!macDrawTrace) return;
    const r_backEndGlobals_t *backend = imp_backEnd;
    const materialCommands_t *commands = imp_tess;
    const Material *material = commands->material;
    const char *name = material ? material->info.name : "";
    fprintf(macDrawTrace, "{\"draw\":%u,\"material\":\"%s\",\"type\":%u,\"base\":%d,\"min\":%u,\"vertices\":%u,\"first\":%u,\"primitives\":%u,\"2d\":%u,\"vs\":\"%p\",\"ps\":\"%p\",\"vs_name\":\"%s\",\"ps_name\":\"%s\",\"lmap\":%d,\"origin\":",
            macDrawNumber++, name, type, base, min, vertices, first, primitives, backend->projection2D,
            g_activeVertexShader, dev->pixelShader, MacTrace_ShaderName(g_activeVertexShader), MacTrace_ShaderName(dev->pixelShader), commands->lmapIndex);
    if (backend->viewParms) MacTrace_Floats(backend->viewParms->origin, 3);
    else fputs("null", macDrawTrace);
    fputs(",\"vs_constants\":", macDrawTrace); MacTrace_Floats(g_vsConst, 256 * 4);
    fputs(",\"ps_constants\":", macDrawTrace); MacTrace_Floats(&macPixelConstants[0][0], 256 * 4);
    fprintf(macDrawTrace, ",\"states\":{\"7\":%u,\"14\":%u,\"23\":%u,\"27\":%u,\"19\":%u,\"20\":%u,\"22\":%u,\"168\":%u,\"15\":%u,\"24\":%u,\"25\":%u,\"171\":%u,\"206\":%u},\"samplers\":[",
            dev->zEnable, dev->zWriteEnable, dev->zFunc, dev->alphaBlendEnable, dev->srcBlend, dev->destBlend,
            dev->cullMode, dev->colorWriteEnable, g_alphaTestEnable, dev->alphaRefVal, dev->alphaFuncVal,
            dev->blendOp, dev->separateAlphaBlendEnable);
    for (unsigned s = 0; s < 16; ++s) {
        fprintf(macDrawTrace, "%s[", s ? "," : "");
        for (unsigned state = 0; state < 14; ++state)
            fprintf(macDrawTrace, "%s%u", state ? "," : "", g_samplerState[s][state]);
        fputc(']', macDrawTrace);
    }
    fputs("],\"textures\":[", macDrawTrace);
    for (unsigned s = 0; s < 16; ++s) {
        IDirect3DBaseTexture9 *texture = g_boundTextures[s];
        fprintf(macDrawTrace, "%s{\"object\":\"%p\",\"id\":%u,\"target\":%u}", s ? "," : "", texture,
                CDirect3DDevice_GetTextureGLId(texture), CDirect3DDevice_GetTextureTarget(texture));
    }
    fputs("],\"stages\":[", macDrawTrace);
    for (unsigned s = 0; s < 8; ++s) {
        fprintf(macDrawTrace, "%s[", s ? "," : "");
        for (unsigned state = 0; state < 33; ++state)
            fprintf(macDrawTrace, "%s%u", state ? "," : "", g_textureStageState[s][state]);
        fputc(']', macDrawTrace);
    }
    fputs("]}\n", macDrawTrace);
}
static void MacTrace_EndFrame(void)
{
    if (macDrawTrace) fclose(macDrawTrace);
    macDrawTrace = NULL;
}
#endif
