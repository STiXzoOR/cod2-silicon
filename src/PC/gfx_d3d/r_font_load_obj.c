#include "common_types.h"
#include "imports.h"

extern int FS_FOpenFileRead(const char *filename, int *fileHandle, int uniqueFILE);
extern int FS_Read(void *buffer, int len, int fileHandle);
extern void FS_FCloseFile(fileHandle_t fileHandle);
extern void *Hunk_AllocInternal(int size);
extern MaterialHandle Material_RegisterHandle(const char *name, int lightmapIndex, int imageTrack);
extern void Com_Printf(const char *fmt, ...);

Font *R_LoadFont(const char *fontName, int imageTrack)
{
    Font *font;
    int fileHandle;
    int len;

    len = FS_FOpenFileRead(fontName, &fileHandle, 1);
    if (len < 0) {
        if (len == -2) {
            Com_Printf("^1ERROR: Couldn't find font in iwd files or localized direct"
                       "ory: '%s'\n",
                       fontName);
        } else {
            Com_Printf("^1ERROR: Couldn't find font '%s'\n", fontName);
        }
        font = 0;
        return font;
    }

    if (len <= 15) {
        FS_FCloseFile(fileHandle);
        Com_Printf("^1ERROR: Font file '%s' too small\n", fontName);
        font = 0;
        return font;
    }

#if defined(COD2_X64)
    /* The .font file is a 32-bit image: a 16-byte header {name_off, pixelHeight,
     * glyphCount, material_off} + glyph data (arch-neutral Glyph), with name/
     * material as 4-byte offsets relative to (base+4). On x64 the in-place relocate
     * truncates pointers, so load the file into a blob (x86 layout) and marshal into
     * a real x64 Font. */
    {
        byte *blob = (byte *)Hunk_AllocInternal(len + 4);
        Font *fx;
        FS_Read(blob, 0x10, fileHandle);
        FS_Read(blob + 0x14, len - 0x10, fileHandle);
        FS_FCloseFile(fileHandle);
        fx = (Font *)Hunk_AllocInternal((int)sizeof(Font));
        fx->name = (const char *)(blob + 4 + *(unsigned int *)(blob + 0));
        fx->pixelHeight = *(int *)(blob + 4);
        fx->glyphCount = *(int *)(blob + 8);
        fx->glyphs = (Glyph *)(blob + 0x14);
        fx->material = Material_RegisterHandle((const char *)(blob + 4 + *(unsigned int *)(blob + 12)), 0, imageTrack);
        return fx;
    }
#else
    font = (Font *)Hunk_AllocInternal(len + 4);

    FS_Read(font, 0x10, fileHandle);

    FS_Read((char *)font + 0x14, len - 0x10, fileHandle);
    FS_FCloseFile(fileHandle);

    font->glyphs = (Glyph *)((char *)font + 0x14);

    *(int *)&font->name += (int)((char *)font + 4);

    font->material = Material_RegisterHandle((const char *)((int)font->material + (int)((char *)font + 4)), 0, imageTrack);

    return font;
#endif
}
