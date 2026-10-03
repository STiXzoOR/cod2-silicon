#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Info_SetValueForKey_Big(char *, const char *, const char *);
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
int main(void)
{
    struct { char text[8192]; unsigned long guard; } data = {{0}, 0xabcdef01UL};
    char value[8000], prior[8192];
    memset(value, 'x', sizeof(value) - 1); value[sizeof(value) - 1] = 0;
    Info_SetValueForKey_Big(data.text, "sv_iwdNames", value);
    assert(strlen(data.text) == strlen(value) + strlen("\\sv_iwdNames\\"));
    strcpy(prior, data.text);
    Info_SetValueForKey_Big(data.text, "extra", value);
    assert(!strcmp(prior, data.text));
    assert(data.guard == 0xabcdef01UL);
    Info_SetValueForKey_Big(data.text, "sv_iwdNames", "small");
    assert(!strcmp(data.text, "\\sv_iwdNames\\small"));
    puts("CoD2x actual big info strings: large IWD list, bound and replacement passed");
    return 0;
}
