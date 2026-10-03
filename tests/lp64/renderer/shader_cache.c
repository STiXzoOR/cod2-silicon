#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define COD2_APPLE_SDK 1
#include "Mac/DirectX_9/D3DXShader.c"

static void write_cache(const char *root, const char *name, const void *data, size_t size)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(data, 1, size, file) == size && !fclose(file));
}

int main(void)
{
    char root[] = "/tmp/cod2-shader-cache-XXXXXX";
    assert(mkdtemp(root) && !setenv("COD2_MAC_SHADER_CACHE", root, 1));
    const char program[] = "!!ARBvp1.0\nMOV result.position, vertex.position;\nEND\n";
    write_cache(root, "fixture.vsa", program, strlen(program));
    unsigned int header[7] = {28, 0, 0, 1, 28, 0, 0};
    unsigned char table[80] = {0};
    memcpy(table, header, sizeof(header));
    unsigned int name = 64, type = 48;
    unsigned short registerIndex = 23, registerCount = 4;
    unsigned short typeInfo[6] = {3, 3, 0, 0, 1, 0};
    memcpy(table + 28, &name, 4);
    memcpy(table + 34, &registerIndex, 2);
    memcpy(table + 36, &registerCount, 2);
    memcpy(table + 40, &type, 4);
    memcpy(table + 48, typeInfo, sizeof(typeInfo));
    strcpy((char *)table + name, "matrix");
    write_cache(root, "fixture.vc", table, sizeof(table));
    void *shader, *constants, *messages;
    assert(!MacD3DXCompileShader("fixture.vs", "unused", 6, "vs_2_0", &shader, &messages));
    assert(shader && !messages);
    const char *code = CD3DXBuffer_GetBufferPointer(shader);
    assert(strstr(code, "#COD2CTAB:fixture.vc"));
    assert(!D3DXGetShaderConstantTable(code, &constants));
    assert(CD3DXConstantTable_GetBufferSize(constants) == sizeof(table));
    assert(!memcmp(CD3DXConstantTable_GetBufferPointer(constants), table, sizeof(table)));
    CD3DXConstantTable_Release(constants);
    /* Invalid fixed-width offsets must fail before the material parser reads them. */
    name = UINT32_MAX;
    memcpy(table + 28, &name, 4);
    write_cache(root, "fixture.vc", table, sizeof(table));
    assert(D3DXGetShaderConstantTable(code, &constants) < 0 && !constants);
    CD3DXBuffer_Release(shader);
    assert(MacD3DXCompileShader("missing.vs", "unused", 6, "vs_2_0", &shader, &messages) < 0 && !shader);
    assert(MacD3DXCompileShader("../fixture.vs", "unused", 6, "vs_2_0", &shader, &messages) < 0 && !shader);
    char path[1024];
    snprintf(path, sizeof(path), "%s/fixture.vsa", root); unlink(path);
    snprintf(path, sizeof(path), "%s/fixture.vc", root); unlink(path);
    assert(!rmdir(root));
    puts("native ARB cache and fixed-width reflection validation: passed");
}
