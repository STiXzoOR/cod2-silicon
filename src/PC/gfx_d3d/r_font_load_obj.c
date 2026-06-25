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

    font = (Font *)Hunk_AllocInternal(len + 4);

    FS_Read(font, 0x10, fileHandle);

    FS_Read((char *)font + 0x14, len - 0x10, fileHandle);
    FS_FCloseFile(fileHandle);

    font->glyphs = (Glyph *)((char *)font + 0x14);

    *(int *)&font->name += (int)((char *)font + 4);

    font->material = Material_RegisterHandle((const char *)((int)font->material + (int)((char *)font + 4)), 0, imageTrack);

    return font;
}
