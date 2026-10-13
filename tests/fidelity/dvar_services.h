#include "common_types.h"
#include "check.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
int dvarCount, dvar_modifiedFlags;
dvar_t *sortedDvars;
const char str_00219524[] = "off", str_00219528[] = "on";
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
void Com_Printf(const char *format, ...) { (void)format; }
void Com_PrintMessage(int channel, const char *text) { (void)channel; (void)text; }
void Dvar_AddCommands(void) {}
char *CopyStringInternal(const char *s) { return strdup(s); }
void Z_FreeInternal(void *p) { free(p); }
void *Z_MallocInternal(int n) { return malloc(n); }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
int ___tolower(int c) { return tolower(c); }
void I_strncpyz(char *out, const char *s, int n) { snprintf(out, n, "%s", s); }
void I_strncat(char *out, int n, const char *s) { size_t len = strlen(out); if (len < (size_t)n) strncat(out, s, n - len - 1); }
int Com_sprintf(char *out, int n, const char *format, ...)
{
    va_list args; va_start(args, format); int result = vsnprintf(out, n, format, args); va_end(args); return result;
}
char *va(const char *format, ...)
{
    static char out[4096]; va_list args; va_start(args, format); vsnprintf(out, sizeof(out), format, args); va_end(args); return out;
}
extern const dvar_t *Dvar_FindVar(const char *);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterFloat(const char *, float, float, float, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_RegisterVec3(const char *, float, float, float, float, float, unsigned short);
extern const dvar_t *Dvar_RegisterColor(const char *, float, float, float, float, unsigned short);
extern void Dvar_SetCommand(const char *, const char *);
extern void Dvar_AddFlags(const dvar_t *, int);
extern void Dvar_SetBool(const dvar_t *, unsigned char);
extern void Dvar_Init(void);
extern void Dvar_Shutdown(void);
