#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static scr_data_t g_scr_data;
static dvar_t map = { .current.string = "fixture_map" };
const dvar_t *sv_mapname = &map;
static jmp_buf errorJump;
static int failAt, step, gotError, gametypeCalls, classCalls, fieldCalls;
static char message[256];
static const char *scripts[] = {"codescripts/delete", "codescripts/struct", "codescripts/struct", "maps/mp/fixture_map"};
static const char *labels[] = {"main", "initstructs", "createstruct", "main"};
unsigned int Scr_LoadScript(const char *name)
{
    assert(step % 2 == 0 && step < 8 && !strcmp(name, scripts[step / 2]));
    return ++step == failAt ? 0 : 1;
}
scr_func_t Scr_GetFunctionHandle(const char *name, const char *label)
{
    assert(step % 2 == 1 && step < 8);
    assert(!strcmp(name, scripts[step / 2]) && !strcmp(label, labels[step / 2]));
    return ++step == failAt ? 0 : 100 + step;
}
void Com_Error(int code, const char *fmt, ...)
{
    assert(code == 1);
    va_list args; va_start(args, fmt); vsnprintf(message, sizeof(message), fmt, args); va_end(args);
    gotError = 1;
    longjmp(errorJump, 1);
}
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{
    va_list args; va_start(args, fmt); int n = vsnprintf(dest, size, fmt, args); va_end(args); return n;
}
void GScr_LoadGameTypeScript(void) { assert(step == 6); ++gametypeCalls; }
void Scr_SetClassMap(int n) { assert(step == 8 && n == classCalls++); }
void GScr_AddFieldsForEntity(void) { assert(classCalls == 4); ++fieldCalls; }
void GScr_AddFieldsForHudElems(void) { assert(fieldCalls == 1); ++fieldCalls; }
void GScr_AddFieldsForRadiant(void) { assert(fieldCalls == 2); ++fieldCalls; }
#include "script_load_source.h"
int main(void)
{
    int failures = 0;
    for (failAt = 0; failAt <= 8; ++failAt) {
        memset(&g_scr_data, 0, sizeof(g_scr_data));
        step = gotError = gametypeCalls = classCalls = fieldCalls = 0;
        message[0] = 0;
        if (!setjmp(errorJump))
            GScr_LoadScripts(0);
        int requiredFailure = failAt >= 1 && failAt <= 6;
        if (requiredFailure && !gotError) {
            fprintf(stderr, "required script/label failure %d did not stop loading\n", failAt);
            ++failures;
            continue;
        }
        assert(gotError == requiredFailure);
        if (requiredFailure) {
            assert(step == failAt && !gametypeCalls && !classCalls && !fieldCalls);
            assert(strstr(message, scripts[(failAt - 1) / 2]));
            if (failAt % 2 == 0)
                assert(strstr(message, labels[(failAt - 1) / 2]));
        } else {
            assert(step == 8 && gametypeCalls == 1 && classCalls == 4 && fieldCalls == 3);
            assert(g_scr_data.delete_ == 102 && g_scr_data.initstructs == 104 && g_scr_data.createstruct == 106);
            assert(g_scr_data.levelscript == (failAt == 8 ? 0 : 108));
        }
    }
    printf("script loading: %d failures\n", failures);
    return failures != 0;
}
