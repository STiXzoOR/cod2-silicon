#include "common_types.h"
#include <assert.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static int allocations;
char *CopyStringInternal(const char *s) { ++allocations; return strdup(s); }
void Z_FreeInternal(void *p) { if (p) --allocations; free(p); }
void *Z_MallocInternal(int n) { ++allocations; return malloc(n); }
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

#include "PC/universal/dvar.c"
int main(void)
{
    /* Startup commands create strings before the network registers typed dvars. */
    Dvar_SetCommand("net_port", "29237");
    const dvar_t *port = Dvar_RegisterInt("net_port", 28960, 0, 65535, 0x1021);
    assert(port->current.integer == 29237);
    assert(allocations == 0);
    Dvar_Shutdown();

    /* Every alias partition must release each owned string exactly once. */
    for (int pattern = 0; pattern < 5; ++pattern) {
        dvar_t var = {0};
        var.type = DVAR_TYPE_STRING;
        var.current.string = CopyStringInternal("12345");
        var.latched.string = pattern < 2 ? var.current.string : CopyStringInternal("23456");
        var.reset.string = pattern == 0 || pattern == 2 ? var.current.string :
                           pattern == 1 || pattern == 3 ? var.latched.string : CopyStringInternal("34567");
        DvarValue reset = {.integer = 28960};
        DvarLimits domain = {.integer = {.min = 0, .max = 65535}};
        Dvar_MakeExplicitType(&var, "port", DVAR_TYPE_INT, 0, reset, domain);
        assert(var.current.integer == 12345 && allocations == 0);
    }
    puts("dvar lifetime: startup type promotion frees all string alias partitions");
}
