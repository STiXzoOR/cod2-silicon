#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int language, read_ok = 1, size_ok = 1, registrations, duplicates;
static const char *input_path = "custom/my_font";
static char registered[512], duplicated[64];
static Font_s fake_font;
static const char **token_stream;
static loadAssets_t assets;
int SEH_GetCurrentLanguage(void) { return language; }
qboolean PC_ReadTokenHandle(int handle, void *value)
{
    pc_token_t *token = value;
    assert(handle == 7);
    if (!read_ok) return 0;
    if (token_stream && !*token_stream) return 0;
    strcpy(token->string, token_stream ? *token_stream++ : input_path);
    return 1;
}
const char *String_Alloc(const char *text) { assert(!strcmp(text, input_path)); return input_path; }
static qboolean UI_ParseIntToken(int handle, int *value)
{
    assert(handle == 7); *value = 23; return size_ok;
}
int Com_sprintf(char *out, int size, const char *format, ...)
{
    va_list args; va_start(args, format); int result = vsnprintf(out, size, format, args); va_end(args); return result;
}
FontHandle CL_RegisterFont(const char *name, int imageTrack)
{
    assert(imageTrack == 11); assert(strlen(name) < sizeof registered);
    strcpy(registered, name); ++registrations; return &fake_font;
}
void CL_DuplicateFont(FontHandle font, const char *name)
{
    assert(font == &fake_font); strcpy(duplicated, name); ++duplicates;
}
static loadAssets_t *UI_LoadAssets(void) { return &assets; }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void *CL_RegisterMaterialNoMip(const char *name, int track) { assert(0); return NULL; }
MaterialHandle CL_Material_Duplicate(MaterialHandle material, const char *name) { assert(0); return NULL; }
snd_alias_list_t *Com_FindSoundAlias(const char *name) { assert(0); return NULL; }
void Com_DuplicateSoundAlias(snd_alias_list_t *alias, const char *name) { assert(0); }
static qboolean UI_ParseFloatToken(int handle, float *value) { assert(0); return 0; }
static qboolean UI_ParseFloatArray(int handle, float *value, int count) { assert(0); return 0; }
#include "locale_fonts_source.h"
int main(void)
{
    const char *dirs[] = {NULL,NULL,NULL,NULL,NULL,NULL,"russian","polish","korean","taiwanese","japanese","chinese","thai",NULL,"czech"};
    const char *roles[] = {"font","smallFont","bigFont","extraBigFont","boldFont","consoleFont"};
    const char *aliases[] = {"$font","$smallfont","$bigfont","$extrabigfont","$boldfont","$consolefont"};
    int failures = 0, cases = 0;
    for (language = -1; language <= 15; ++language) {
        for (int role = 0; role < 6; ++role) {
            char expected[256];
            const char *directory = language >= 0 && language < 15 ? dirs[language] : NULL;
            if (directory) snprintf(expected, sizeof expected, "fonts/%s/%s", directory, role ? roles[role] : "normalFont");
            else strcpy(expected, input_path);
            registrations = duplicates = 0;
            assert(TEST_PARSE_FONT(roles[role], aliases[role]));
            assert(registrations == 1 && duplicates == 1 && !strcmp(duplicated, aliases[role]));
            if (strcmp(expected, registered)) {
                fprintf(stderr, "language %d role %s: got %s, expected %s\n", language, roles[role], registered, expected);
                ++failures;
            }
            ++cases;
        }
    }
    language = 6;
    for (int role = 0; role < 6; ++role) {
        const char *stream[] = {"{", roles[role], input_path, "}", NULL};
        char expected[256];
        snprintf(expected, sizeof expected, "fonts/russian/%s", role ? roles[role] : "normalFont");
        token_stream = stream; registrations = duplicates = 0;
        assert(UI_ParseAssetGlobalDef(7, 11));
        assert(registrations == 1 && duplicates == 1 && !strcmp(duplicated, aliases[role]));
        if (strcmp(expected, registered)) ++failures;
        ++cases;
    }
    token_stream = NULL;
    registrations = duplicates = 0;
    read_ok = 0; assert(!TEST_PARSE_FONT("font", "$font"));
    read_ok = 1; size_ok = 0; assert(!TEST_PARSE_FONT("font", "$font"));
    assert(!registrations && !duplicates);
    size_ok = 1;
    printf("locale font selection: %d cases, %d failures\n", cases, failures);
    return failures != 0;
}
