#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"

static byte fileData[128];
static int fileSize, cursor, shortRead;
static void *allocations[8];
static int allocationCount;
static Material material;
int FS_FOpenFileRead(const char *name, int *handle, int unique)
{ (void)name; (void)unique; *handle = 1; cursor = 0; return fileSize; }
int FS_Read(void *buffer, int len, int handle)
{ (void)handle; if (shortRead) return 0; memcpy(buffer, fileData + cursor, len); cursor += len; return len; }
void FS_FCloseFile(fileHandle_t handle) { (void)handle; }
void *Hunk_AllocInternal(int size)
{ void *p = malloc(size); allocations[allocationCount++] = p; return p; }
MaterialHandle Material_RegisterHandle(const char *name, int index, int track)
{ assert(!strcmp(name, "material") && index == 0 && track == 6); return &material; }
void Com_Printf(const char *format, ...) { (void)format; }
#include "PC/gfx_d3d/r_font_load_obj.c"
static void release(void) { while (allocationCount) free(allocations[--allocationCount]); }
static void write32(int offset, unsigned int value) { memcpy(fileData + offset, &value, 4); }

int main(void)
{
    fileSize = 16 + sizeof(Glyph) + 14;
    assert(fileSize <= sizeof(fileData));
    write32(0, 16 + sizeof(Glyph)); write32(4, 18); write32(8, 1);
    write32(12, 16 + sizeof(Glyph) + 5);
    Glyph glyph = {0}; glyph.letter = 'A';
    memcpy(fileData + 16, &glyph, sizeof(glyph));
    memcpy(fileData + 16 + sizeof(Glyph), "font\0material\0", 14);
    Font *font = R_LoadFont("test", 6);
    assert(font && font->pixelHeight == 18 && font->glyphCount == 1);
    assert(!strcmp(font->name, "font") && font->material == &material);
    assert(font->glyphs[0].letter == 'A' && (uintptr_t)font->glyphs > UINT32_MAX);
    release();
    write32(0, UINT32_MAX); assert(!R_LoadFont("test", 6)); release();
    write32(0, 16 + sizeof(Glyph)); write32(8, INT_MAX);
    assert(!R_LoadFont("test", 6)); release();
    write32(8, 1); shortRead = 1; assert(!R_LoadFont("test", 6)); release();
    puts("renderer font disk layout, strings and short reads: passed");
}
