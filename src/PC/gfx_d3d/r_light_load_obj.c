#include "common_types.h"
#include "imports.h"

extern GfxImage *Image_Register(const char *name, int trackType, int filter);
extern const char *va(const char *format, ...);
extern int FS_ReadFile(const char *path, void **buffer);
extern void FS_FreeFile(void *buffer);
extern void *Hunk_AllocInternal(int size);
extern char *I_strlwr(char *s);
extern int stricmp(const char *s1, const char *s2);

GfxLightDef *R_LoadLightDef(const char *name)
{
    void *file;
    GfxLightDef *def;
    const char *loadName;

    loadName = name;
    int fileLen = FS_ReadFile(va("lights/%s", loadName), &file);
    if (fileLen < 0 && stricmp(name, "default") == 0) {
        loadName = "light_dynamic";
        fileLen = FS_ReadFile(va("lights/%s", loadName), &file);
    }
    if (fileLen < 0) {
        def = NULL;
        return def;
    }
    if (fileLen == 0) {
        FS_FreeFile(file);
        def = NULL;
        return def;
    }

    def = (GfxLightDef *)Hunk_AllocInternal(sizeof(GfxLightDef));
    def->name = (const char *)Hunk_AllocInternal(strlen(name) + 1);

    byte *data = (byte *)file;
    def->type = (GfxLightType)data[0];

    def->cookie.samplerState = (GfxSamplerState)data[1];
    const char *imageName = (const char *)&data[2];
    int imageNameLen = strlen(imageName);
    if (imageNameLen == 0) {
        def->cookie.image = NULL;
    } else {
        def->cookie.image = Image_Register(imageName, 1, 5);
    }

    const char *ptr = imageName + imageNameLen;
    def->attenuation.samplerState = (GfxSamplerState)ptr[1];
    const char *attImageName = ptr + 2;
    if (strlen(attImageName) == 0) {
        def->attenuation.image = NULL;
    } else {
        def->attenuation.image = Image_Register(attImageName, 1, 5);
    }

    strcpy((char *)def->name, name);
    I_strlwr((char *)def->name);

    FS_FreeFile(file);
    return def;
}
