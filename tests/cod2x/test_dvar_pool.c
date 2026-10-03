#include "common_types.h"
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_Shutdown(void);
int dvarCount, dvar_modifiedFlags;
dvar_t *sortedDvars;
const char str_00219524[] = "off", str_00219528[] = "on";
static jmp_buf exhausted;
static char error[512];

void Com_Error(int code, const char *format, ...)
{
    va_list args;
    (void)code;
    va_start(args, format);
    vsnprintf(error, sizeof(error), format, args);
    va_end(args);
    longjmp(exhausted, 1);
}
void Com_Printf(const char *format, ...) { (void)format; }
void Com_PrintMessage(int channel, const char *message) { (void)channel; (void)message; }
char *CopyStringInternal(const char *s) { return strdup(s); }
void Z_FreeInternal(void *p) { free(p); }
void *Z_MallocInternal(int n) { return malloc(n); }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
int ___tolower(int c) { return tolower(c); }
void I_strncpyz(char *out, const char *s, int n) { snprintf(out, n, "%s", s); }
int Com_sprintf(char *out, int n, const char *format, ...)
{
    int result;
    va_list args;
    va_start(args, format); result = vsnprintf(out, n, format, args); va_end(args);
    return result;
}
char *va(const char *format, ...)
{
    static char out[4096];
    va_list args;
    va_start(args, format); vsnprintf(out, sizeof(out), format, args); va_end(args);
    return out;
}

int main(void)
{
    static char names[4097][20];
    int i;
    if (!setjmp(exhausted)) {
        for (i = 0; i < 4096; ++i) {
            const dvar_t *var;
            snprintf(names[i], sizeof(names[i]), "ws10_%04d", i);
            var = Dvar_RegisterInt(names[i], i, 0, 5000, 0);
            assert(var && var->current.integer == i);
            assert(Dvar_FindVar(names[i]) == var);
        }
        assert(dvarCount == 4096);
        Dvar_RegisterInt("overflow", 0, 0, 1, 0);
        assert(!"expected exhaustion");
    }
    assert(dvarCount == 4096);
    assert(strstr(error, "4096") && strstr(error, "overflow"));
    Dvar_Shutdown();
    assert(dvarCount == 0 && !Dvar_FindVar("ws10_0000"));
    assert(Dvar_RegisterInt("reuse", 7, 0, 10, 0)->current.integer == 7);
    puts("CoD2x actual dvar pool: 4096 registrations, exhaustion, reuse passed");
    return 0;
}
