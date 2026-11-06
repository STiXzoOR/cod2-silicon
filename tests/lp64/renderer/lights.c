#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "lights_source.h"

struct lightGlob_type lightGlob;
refimport_t ri;
r_global_permanent_t rgp;
static GfxLightDef dynamicLight = { .name = "light_dynamic" };
static GfxLightDef loaded[64];
static jmp_buf failure;
static int loadCount, fatalCount, printCount, rejectDynamic;
static char requested[2][32], header[128];

int stricmp(const char *a, const char *b)
{
    return strcasecmp(a, b);
}

GfxLightDef *R_LoadLightDef(const char *name)
{
    assert(loadCount < 2);
    snprintf(requested[loadCount++], sizeof(requested[0]), "%s", name);
    return !rejectDynamic && !strcmp(name, "light_dynamic") ? &dynamicLight : NULL;
}

void R_Error(int level, const char *format, ...)
{
    assert(level == 1);
    fatalCount++;
    longjmp(failure, 1);
}

static void record_message(int level, const char *format, ...)
{
    assert(level == 0);
    if (printCount++ == 0)
        snprintf(header, sizeof(header), "%s", format);
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    ri.Printf = record_message;
    if (!strcmp(argv[1], "fallback")) {
        if (setjmp(failure))
            assert(0);
        assert(R_RegisterLightDef("missing") == &dynamicLight);
        assert(loadCount == 2 && !strcmp(requested[0], "missing"));
        assert(!strcmp(requested[1], "light_dynamic"));
        assert(lightGlob.defCount == 1 && lightGlob.defs[0] == &dynamicLight);
    } else if (!strcmp(argv[1], "cached")) {
        lightGlob.defCount = 1;
        lightGlob.defs[0] = &dynamicLight;
        if (setjmp(failure))
            assert(0);
        assert(R_RegisterLightDef("missing") == &dynamicLight);
        assert(loadCount == 1 && lightGlob.defCount == 1);
    } else if (!strcmp(argv[1], "fatal")) {
        rejectDynamic = 1;
        if (!setjmp(failure)) {
            R_RegisterLightDef("light_dynamic");
            assert(0);
        }
        assert(fatalCount == 1 && loadCount == 1 && lightGlob.defCount == 0);
    } else {
        assert(!strcmp(argv[1], "limit"));
        lightGlob.defCount = 64;
        for (int i = 0; i < 64; i++) {
            loaded[i].name = "loaded";
            lightGlob.defs[i] = &loaded[i];
        }
        if (!setjmp(failure)) {
            R_RegisterLightDef("missing");
            assert(0);
        }
        assert(!strcmp(header, "Loaded light defs:\n"));
        assert(printCount == 65 && fatalCount == 1 && loadCount == 0);
    }
    puts("native light registration passed");
    return 0;
}
