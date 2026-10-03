#if COD2_APPLE_SDK
/* Native cache generated locally by tools/macos-port/extract_shaders.py. */
static void *MacShader_ReadCache(const char *name, UINT32 *size)
{
    const char *root = getenv("COD2_MAC_SHADER_CACHE");
    char path[1024];
    FILE *file;
    long length;
    void *data;

    if (!root || !name || strspn(name, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") != strlen(name) ||
        snprintf(path, sizeof(path), "%s/%s", root, name) >= sizeof(path))
        return NULL;
    file = fopen(path, "rb");
    if (!file)
        return NULL;
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 || length > 1024 * 1024 || fseek(file, 0, SEEK_SET)) {
        fclose(file);
        return NULL;
    }
    data = calloc(1, (size_t)length + 1);
    if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *size = (UINT32)length;
    return data;
}

HRESULT MacD3DXCompileShader(const char *name, const char *source, UINT length, const char *profile,
                             void **shader, void **messages)
{
    char cacheName[256];
    char constantName[256];
    char marker[280];
    const char *extension = strrchr(name, '.');
    UINT32 size;
    char *code;
    char *program;
    int markerSize;
    int vertex = profile && profile[0] == 'v';

    if (!getenv("COD2_MAC_SHADER_CACHE")) {
        static int reported;
        if (!reported) {
            fputs("Using generic shaders: set COD2_MAC_SHADER_CACHE to a locally extracted Mac 1.3 shader cache.\n", stderr);
            reported = 1;
        }
        return D3DXCompileShader(source, length, NULL, NULL, NULL, profile, 0, shader, messages, NULL);
    }
    *shader = NULL;
    if (messages)
        *messages = NULL;
    if (!extension || (size_t)(extension - name) >= sizeof(cacheName) - 4)
        return (HRESULT)0x88760b59;
    snprintf(cacheName, sizeof(cacheName), "%.*s.%s", (int)(extension - name), name, vertex ? "vsa" : "pse");
    snprintf(constantName, sizeof(constantName), "%.*s.%s", (int)(extension - name), name, vertex ? "vc" : "pc");
    code = MacShader_ReadCache(cacheName, &size);
    if (!code || strncmp(code, vertex ? "!!ARBvp1.0" : "!!ARBfp1.0", 10)) {
        fprintf(stderr, "Missing or invalid native shader cache entry: %s\n", cacheName);
        free(code);
        return (HRESULT)0x88760b59;
    }
    markerSize = snprintf(marker, sizeof(marker), "\n#COD2CTAB:%s\n", constantName);
    program = malloc(size + markerSize + 1);
    if (!program) {
        free(code);
        return (HRESULT)0x8007000e;
    }
    memcpy(program, code, size);
    memcpy(program + size, marker, markerSize + 1);
    *shader = CD3DXBuffer_Create(program, size + markerSize + 1);
    free(program);
    free(code);
    return 0;
}

HRESULT D3DXGetShaderConstantTable(const void *function, void **constantTable)
{
    const char *marker = function ? strstr(function, "\n#COD2CTAB:") : NULL;
    CD3DXConstantTableImpl *table = calloc(1, sizeof(*table));
    UINT32 size = 32;
    byte *data = NULL;

    *constantTable = NULL;
    if (!table)
        return (HRESULT)0x8007000e;
    if (marker) {
        char name[256];
        size_t length;
        marker += strlen("\n#COD2CTAB:");
        length = strcspn(marker, "\n\r");
        if (length >= sizeof(name))
            goto invalid;
        memcpy(name, marker, length);
        name[length] = 0;
        data = MacShader_ReadCache(name, &size);
        if (!data || size < 28)
            goto invalid;
        /* CTAB offsets remain 32-bit; validate every record before engine use. */
        unsigned int count, offset;
        memcpy(&count, data + 12, 4);
        memcpy(&offset, data + 16, 4);
        if (count > 256 || offset > size || count > (size - offset) / 20)
            goto invalid;
        for (unsigned int i = 0; i < count; ++i) {
            unsigned int nameOffset, typeOffset;
            memcpy(&nameOffset, data + offset + i * 20, 4);
            memcpy(&typeOffset, data + offset + i * 20 + 12, 4);
            if (nameOffset >= size || !memchr(data + nameOffset, 0, size - nameOffset) ||
                typeOffset > size || size - typeOffset < 16)
                goto invalid;
        }
    } else {
        data = calloc(1, size);
        if (!data)
            goto invalid;
    }
    table->vtable = vtbl_CD3DXConstantTable;
    table->refCount = 1;
    table->data = data;
    table->size = size;
    *constantTable = table;
    return 0;
invalid:
    free(data);
    free(table);
    return (HRESULT)0x88760b59;
}
#endif
