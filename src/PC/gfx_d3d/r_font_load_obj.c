#include "common_types.h"
#include "imports.h"
#if defined(COD2_X64)
#include <limits.h>
#endif

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
        /* The old loader inserts a four-byte glyph-pointer slot after the
         * disk header. Disk string offsets are relative to this expanded image. */
        struct { unsigned int nameOffset; int pixelHeight, glyphCount;
                 unsigned int materialOffset; } disk;
        if (len > INT_MAX - 4) {
            FS_FCloseFile(fileHandle);
            return NULL;
        }
        byte *blob = (byte *)Hunk_AllocInternal(len + 4);
        if (FS_Read(blob, 16, fileHandle) != 16 ||
            FS_Read(blob + 20, len - 16, fileHandle) != len - 16) {
            FS_FCloseFile(fileHandle);
            return NULL;
        }
        FS_FCloseFile(fileHandle);
        memcpy(&disk, blob, sizeof(disk));
        unsigned int imageSize = (unsigned int)len + 4;
        if (disk.glyphCount < 0 || (unsigned int)disk.glyphCount >
                (unsigned int)(len - 16) / sizeof(Glyph) ||
            disk.nameOffset < 16 || disk.nameOffset >= imageSize - 4 ||
            disk.materialOffset < 16 || disk.materialOffset >= imageSize - 4 ||
            !memchr(blob + 4 + disk.nameOffset, 0, imageSize - 4 - disk.nameOffset) ||
            !memchr(blob + 4 + disk.materialOffset, 0, imageSize - 4 - disk.materialOffset))
            return NULL;
        Font *fx = (Font *)Hunk_AllocInternal((int)sizeof(Font));
        fx->name = (const char *)(blob + 4 + disk.nameOffset);
        fx->pixelHeight = disk.pixelHeight;
        fx->glyphCount = disk.glyphCount;
        fx->glyphs = (Glyph *)(blob + 20);
        fx->material = Material_RegisterHandle((const char *)(blob + 4 + disk.materialOffset), 0, imageTrack);
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
