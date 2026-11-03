/* Hit-location table contracts through the complete production combat TU. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern void G_ParseHitLocDmgTable(void);
extern scr_string_t G_GetHitLocationString(hitLocation_t);
extern hitLocation_t G_GetHitLocationIndexFromString(int);
extern int G_IndexForMeansOfDeath(const char *);
float g_fHitLocDamageMult[19];
static const char *names[] = {
    "none", "helmet", "head", "neck", "torso_upper", "torso_lower",
    "right_arm_upper", "left_arm_upper", "right_arm_lower", "left_arm_lower",
    "right_hand", "left_hand", "right_leg_upper", "left_leg_upper",
    "right_leg_lower", "left_leg_lower", "right_foot", "left_foot", "gun"
};
static const char payload[] = "\\helmet\\2.5";
static const char *scenario;
static int allocations, reads, closes, parsed;
static char diagnostic[256];
static jmp_buf errorJump;

unsigned int Scr_AllocString(const char *value, int flag)
{
    assert(flag == 1 && allocations < 19);
    assert(!strcmp(value, names[allocations]));
    return 101 + allocations++;
}
int FS_FOpenFileByMode(const char *name, int *handle, int mode)
{
    assert(!strcmp(name, "info/mp_lochit_dmgtable") && mode == 0);
    *handle = 42;
    if (!strcmp(scenario, "missing")) return -1;
    if (!strcmp(scenario, "long")) return 11 + 8192;
    return 11 + sizeof(payload) - 1;
}
int FS_Read(void *buffer, int length, int handle)
{
    assert(handle == 42);
    if (reads++ == 0) {
        assert(length == 11);
        memcpy(buffer, !strcmp(scenario, "header") ? "BADDMGTABL" : "LOCDMGTABLE", 11);
    } else {
        assert(reads == 2 && length == sizeof(payload) - 1);
        memcpy(buffer, payload, length);
    }
    return length;
}
void FS_FCloseFile(fileHandle_t handle) { assert(handle == 42); ++closes; }
int Info_Validate(const char *text)
{
    assert(reads == 2 && closes == 1 && !strcmp(text, payload));
    return strcmp(scenario, "invalid") != 0;
}
qboolean ParseConfigStringToStruct(byte *data, const cspField_t *fields, int count,
                                  const char *text, int maxTypes,
                                  qboolean (*special)(byte *, const char *, int),
                                  void (*copy)(byte *, const char *))
{
    assert(data == (byte *)g_fHitLocDamageMult && count == 19);
    assert(!strcmp(text, payload) && maxTypes == 0 && special == NULL);
    for (int i = 0; i < 19; ++i) {
        assert(!strcmp(fields[i].szName, names[i]));
        assert(fields[i].iOffset == i * sizeof(float) && fields[i].iFieldType == 6);
        assert(g_fHitLocDamageMult[i] == (i == 18 ? 0.0f : 1.0f));
    }
    char copied[16] = {0};
    copy((byte *)copied, "head");
    assert(!strcmp(copied, "head"));
    g_fHitLocDamageMult[1] = 2.5f;
    ++parsed;
    return strcmp(scenario, "parse") != 0;
}
void Com_Error(int code, const char *format, ...)
{
    assert(code == 1);
    va_list args;
    va_start(args, format);
    vsnprintf(diagnostic, sizeof(diagnostic), format, args);
    va_end(args);
    longjmp(errorJump, 1);
}
int I_stricmp(const char *left, const char *right) { return strcasecmp(left, right); }
void Com_Printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(diagnostic, sizeof(diagnostic), format, args);
    va_end(args);
}
static void Success(void)
{
    G_ParseHitLocDmgTable();
    assert(allocations == 19 && reads == 2 && closes == 1 && parsed == 1);
    assert(g_fHitLocDamageMult[1] == 2.5f && g_fHitLocDamageMult[18] == 0);
    for (int i = 0; i < 19; ++i) {
        assert(G_GetHitLocationString((hitLocation_t)i) == 101 + i);
        assert(G_GetHitLocationIndexFromString(101 + i) == i);
    }
    assert(G_GetHitLocationIndexFromString(999) == 0);
    assert(G_IndexForMeansOfDeath("mod_rifle_bullet") == 2);
    assert(G_IndexForMeansOfDeath("MOD_EXPLOSIVE") == 14);
    assert(G_IndexForMeansOfDeath("missing") == 0);
    assert(!strcmp(diagnostic, "Unknown means of death string 'missing'\n"));
}
static void Error(void)
{
    const char *expected;
    if (!strcmp(scenario, "missing"))
        expected = "\x15" "Could not load hitloc damage table info/mp_lochit_dmgtable\n";
    else if (!strcmp(scenario, "header"))
        expected = "\x15" "\"info/mp_lochit_dmgtable\" does not appear to be a hitloc damage table\n";
    else if (!strcmp(scenario, "long"))
        expected = "\x15" "\"info/mp_lochit_dmgtable\" Is too long of a hitloc damage table to parse\n";
    else if (!strcmp(scenario, "invalid"))
        expected = "\x15" "\"info/mp_lochit_dmgtable\" is not a valid hitloc damage table\n";
    else {
        assert(!strcmp(scenario, "parse"));
        expected = "\x15" "Error parsing hitloc damage table info/mp_lochit_dmgtable\n";
    }
    if (!setjmp(errorJump)) { G_ParseHitLocDmgTable(); abort(); }
    assert(!strcmp(diagnostic, expected));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    scenario = argv[1];
    if (!strcmp(scenario, "success")) Success(); else Error();
    printf("combat table %s: pass\n", scenario);
    return 0;
}
