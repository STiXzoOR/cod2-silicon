#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static unsigned int runeData[269];
void *__DefaultRuneLocale = runeData;
char *va(const char *format, ...)
{
    static char text[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}
#include "ui_conversion_source.h"
int main(void)
{
    char count[64];
    const char *plain = "Connecting";
    runeData[13 + '1'] = 0x400;
    snprintf(count, sizeof(count), "%d", 123);
    assert(UI_ReplaceConversionString(plain, count) == plain);
    assert(!strcmp(UI_ReplaceConversionString("Awaiting connection...&&1", count),
                   "Awaiting connection...123"));
    assert(!strcmp(UI_ReplaceConversionString("&&1 servers (&&1)", count),
                   "123 servers (123)"));
    puts("online: LP64 connection text conversions passed");
    return 0;
}
