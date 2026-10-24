#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "light_loader_source.h"

static const byte *record;
static int recordLength, reads, frees, imageCount, allocationCount;
static void *allocations[2];
static char requestedPath[128];
static GfxImage images[2];

const char *va(const char *format, ...)
{
    static char text[128];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}

int FS_ReadFile(const char *path, void **buffer)
{
    reads++;
    strcpy(requestedPath, path);
    *buffer = (void *)record;
    return recordLength;
}

void FS_FreeFile(void *buffer)
{
    assert(buffer == record);
    frees++;
}

void *Hunk_AllocInternal(int size)
{
    assert(allocationCount < 2);
    void *result = calloc(1, size);
    assert(result);
    allocations[allocationCount++] = result;
    return result;
}

char *I_strlwr(char *text)
{
    for (char *p = text; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    return text;
}

int stricmp(const char *a, const char *b)
{
    return strcasecmp(a, b);
}

GfxImage *Image_Register(const char *name, int trackType, int filter)
{
    assert(trackType == 1 && filter == 5);
    assert(imageCount < 2);
    assert(strcmp(name, imageCount == 0 ? "cookie" : "attenuation") == 0);
    return &images[imageCount++];
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    const byte full[] = { 2, 7, 'c','o','o','k','i','e',0, 9,
                          'a','t','t','e','n','u','a','t','i','o','n',0 };
    const byte empty[] = { 1, 3, 0, 5, 0 };
    const byte truncated[] = { 1, 3, 0, 5 };
    record = full;
    recordLength = sizeof(full);
    const char *name = "MiXeD";
    if (strcmp(argv[1], "missing-default") == 0) {
        name = "DeFaUlT";
        recordLength = -1;
    } else if (strcmp(argv[1], "missing-other") == 0) {
        recordLength = -1;
    } else if (strcmp(argv[1], "empty") == 0) {
        recordLength = 0;
    } else if (strcmp(argv[1], "empty-images") == 0) {
        record = empty;
        recordLength = sizeof(empty);
    } else if (strcmp(argv[1], "truncated") == 0) {
        record = truncated;
        recordLength = sizeof(truncated);
    } else {
        assert(strcmp(argv[1], "images") == 0);
    }

    GfxLightDef *def = R_LoadLightDef(name);
    assert(reads == 1);
    assert(strcmp(requestedPath, va("lights/%s", name)) == 0);
    assert(frees == (recordLength >= 0));
    if (recordLength <= 0 || record == truncated) {
        assert(def == NULL && imageCount == 0 && allocationCount == 0);
    } else {
        assert(def && strcmp(def->name, "mixed") == 0);
        assert(allocationCount == 2 && def->type == record[0]);
        assert(def->cookie.samplerState == record[1]);
        if (record == empty) {
            assert(imageCount == 0 && !def->cookie.image && !def->attenuation.image);
            assert(def->attenuation.samplerState == 5);
        } else {
            assert(imageCount == 2 && def->cookie.image == &images[0]);
            assert(def->attenuation.image == &images[1]);
            assert(def->attenuation.samplerState == 9);
        }
    }
    for (int i = 0; i < allocationCount; i++)
        free(allocations[i]);
    return 0;
}
