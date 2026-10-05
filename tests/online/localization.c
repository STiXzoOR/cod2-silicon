#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static dvar_t translation = { .current.enabled = 1 };
const dvar_t *loc_translate = &translation, *loc_warnings, *loc_warningsAsErrors;
static unsigned int runeData[13 + 256];
byte *__DefaultRuneLocale = (byte *)runeData;
static int iCurrString;
static char szStrings[2][1024];

void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
const char *SE_GetString(const char *key)
{
    if (!strcmp(key, "SOURCE")) return "Source: &&1";
    if (!strcmp(key, "INTERNET")) return "Internet";
    if (!strcmp(key, "PAIR")) return "&&1 / &&2";
    return NULL;
}
const char *va(const char *format, ...)
{
    static char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(int level, const char *format, ...) { (void)level; (void)format; abort(); }
#include "localization_source.h"

int main(void)
{
    for (int i = '0'; i <= '9'; ++i) runeData[13 + i] = 0x404;
    for (int i = 'a'; i <= 'z'; ++i) runeData[13 + i] = 0x101;
    assert(SEH_IsDigit('1') && SEH_IsDigit('9') && !SEH_IsDigit('b'));
    assert(!strcmp(SEH_LocalizeTextMessage("SOURCE\x14" "INTERNET", "source", LOCMSG_NOERR), "Source: Internet"));
    assert(!strcmp(SEH_LocalizeTextMessage("PAIR\x15" "first\x15" "second", "pair", LOCMSG_NOERR), "first / second"));
    puts("online: native localized placeholder substitution passed");
}
