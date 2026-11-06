/* Caller-dependency contract; nested loaders and VM services are external seams. */
#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static scr_data_t g_scr_data;
static dvar_t mapDvar;
const dvar_t *sv_mapname = &mapDvar;
static int events[16], eventCount, loadCount, postCount, hasMap;
enum { DELETE_SCRIPT = 1, INIT_STRUCTS, CREATE_STRUCT, GAME_TYPE, MAP_LOAD,
    MAP_HANDLE, POST_COMPILE, CLASS_0, CLASS_1, CLASS_2, CLASS_3,
    ENTITY_FIELDS, HUD_FIELDS, RADIANT_FIELDS };
static void Record(int event)
{
    assert(eventCount < 16);
    events[eventCount++] = event;
}
static scr_func_t GScr_LoadScriptFunction(const char *script, const char *label, int required)
{
    const char *scripts[] = { "codescripts/delete", "codescripts/struct", "codescripts/struct" };
    const char *labels[] = { "main", "initstructs", "createstruct" };
    assert(loadCount < 3 && !postCount && required == 1);
    assert(!strcmp(script, scripts[loadCount]) && !strcmp(label, labels[loadCount]));
    Record(DELETE_SCRIPT + loadCount);
    return (scr_func_t)(101 + loadCount++);
}
static void GScr_LoadGameTypeScript(void)
{
    assert(loadCount == 3 && !postCount);
    assert(g_scr_data.delete_ == 101 && g_scr_data.initstructs == 102 && g_scr_data.createstruct == 103);
    Record(GAME_TYPE);
}
unsigned int Scr_LoadScript(const char *filename)
{
    assert(!postCount && hasMap && !strcmp(filename, "maps/mp/synthetic_map"));
    assert(events[eventCount - 1] == GAME_TYPE);
    Record(MAP_LOAD);
    return 1;
}
scr_func_t Scr_GetFunctionHandle(const char *file, const char *label)
{
    assert(!postCount && !strcmp(file, "maps/mp/synthetic_map") && !strcmp(label, "main"));
    assert(events[eventCount - 1] == MAP_LOAD);
    Record(MAP_HANDLE);
    return 104;
}
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    assert(size == 64 && !strcmp(format, "maps/mp/%s"));
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, size, format, args);
    va_end(args);
    return result;
}
void Scr_PostCompileScripts(void)
{
    assert(!postCount && loadCount == 3);
    assert(events[eventCount - 1] == (hasMap ? MAP_HANDLE : GAME_TYPE));
    assert(g_scr_data.levelscript == (hasMap ? 104 : 99));
    ++postCount;
    Record(POST_COMPILE);
}
void Scr_SetClassMap(int classnum)
{
    assert(postCount == 1 && classnum >= 0 && classnum <= 3);
    assert(events[eventCount - 1] == POST_COMPILE + classnum);
    Record(CLASS_0 + classnum);
}
void GScr_AddFieldsForEntity(void)
{
    assert(events[eventCount - 1] == CLASS_3);
    Record(ENTITY_FIELDS);
}
void GScr_AddFieldsForHudElems(void)
{
    assert(events[eventCount - 1] == ENTITY_FIELDS);
    Record(HUD_FIELDS);
}
void GScr_AddFieldsForRadiant(void)
{
    assert(events[eventCount - 1] == HUD_FIELDS);
    Record(RADIANT_FIELDS);
}
#include "script_load_body.inc"
int main(int argc, char **argv)
{
    assert(argc == 2);
    g_scr_data.levelscript = 99;
    if (!strcmp(argv[1], "map")) {
        hasMap = 1;
        mapDvar.current.string = "synthetic_map";
    } else if (!strcmp(argv[1], "empty-map")) {
        mapDvar.current.string = "";
    } else if (!strcmp(argv[1], "null-map-string")) {
        mapDvar.current.string = NULL;
    } else if (!strcmp(argv[1], "null-map-dvar")) {
        sv_mapname = NULL;
    } else {
        abort();
    }
    GScr_LoadScripts(1);
    int expected[16], count = 0;
    for (int event = DELETE_SCRIPT; event <= RADIANT_FIELDS; ++event) {
        if (!hasMap && (event == MAP_LOAD || event == MAP_HANDLE))
            continue;
        expected[count++] = event;
    }
    assert(eventCount == count && postCount == 1);
    for (int i = 0; i < count; ++i)
        assert(events[i] == expected[i]);
    return 0;
}
