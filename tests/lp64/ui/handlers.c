#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bind_t g_bindings[56];
static int g_waitingForKey;
static itemDef_t *g_bindItem;
static dvar_t enumDvar;
static char testCurrent[64], selected[64];
static int cleared;
scrVarPub_t scrVarPub;
scrParserGlob_t scrParserGlob;
scrParserPub_t scrParserPub;
void * const imp_scrVarPub = &scrVarPub;
static size_t firstAllocation;

int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
void Key_SetBinding(int key, const char *binding) { assert(key >= 0 && !*binding); cleared++; }
void Controls_SetConfig(qboolean restart) { (void)restart; }
void CalcScreenX(float *v, int align) { (void)v; (void)align; }
void CalcScreenY(float *v, int align) { (void)v; (void)align; }
void CalcScreenPlacement(float *x, float *y, float *w, float *h, int ha, int va)
{ (void)x; (void)y; (void)w; (void)h; (void)ha; (void)va; }
static int Item_HandleKey_RectContainsPoint(byte *it, float x, float y)
{ (void)it; (void)x; (void)y; return 1; }
multiDef_t *Item_GetMultiDef(itemDef_t *item) { return item->typeData.multi; }
const char *Dvar_GetVariantString(const char *name) { (void)name; return testCurrent; }
dvar_t *Dvar_FindVar(const char *name) { (void)name; return &enumDvar; }
void Dvar_SetFromStringByName(const char *name, const char *value)
{ (void)name; snprintf(selected, sizeof(selected), "%s", value); }
char *va(const char *format, ...)
{
    static char text[64];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}
void *Z_MallocInternal(int size)
{
    if (!firstAllocation)
        firstAllocation = size;
    return malloc(size);
}

#include "production.h"

int main(void)
{
    displayContextDef_t dc = {0};
    itemDef_t item = {0};
    multiDef_t multi = {0};
    char commands[56][16];
    int i;

    for (i = 0; i < 56; i++) {
        snprintf(commands[i], sizeof(commands[i]), "command%d", i);
        g_bindings[i].command = commands[i];
        g_bindings[i].bind1 = -1;
        g_bindings[i].bind2 = -1;
    }
    item.dvar = commands[55];
    g_bindItem = &item;
    g_waitingForKey = 1;
    assert(Item_Bind_HandleKey(&dc, &item, 42, 1));
    assert(g_bindings[55].bind1 == 42);
    g_bindItem = &item;
    g_waitingForKey = 1;
    assert(Item_Bind_HandleKey(&dc, &item, 43, 1));
    assert(g_bindings[55].bind2 == 43);
    g_bindItem = &item;
    g_waitingForKey = 1;
    assert(Item_Bind_HandleKey(&dc, &item, 0x7f, 1));
    assert(cleared == 2 && g_bindings[55].bind1 == -1 && g_bindings[55].bind2 == -1);

    item.window.dynamicFlags[0] = 6;
    item.dvar = "choice";
    item.type = 0xc;
    item.typeData.multi = &multi;
    multi.count = 3;
    multi.strDef = 1;
    multi.dvarStr[0] = "zero";
    multi.dvarStr[1] = "one";
    multi.dvarStr[2] = "two";
    strcpy(testCurrent, "one");
    assert(choice_key(&dc, &item, 0xd));
    assert(!strcmp(selected, "two"));
    multi.strDef = 0;
    multi.dvarValue[0] = 10;
    multi.dvarValue[1] = 20;
    multi.dvarValue[2] = 30;
    strcpy(testCurrent, "20");
    assert(choice_key(&dc, &item, 0xd));
    assert(!strcmp(selected, "30"));

    item.type = 0xd;
    item.typeData.enumDvarName = "enumeration";
    enumDvar.type = 6;
    enumDvar.domain.enumeration.stringCount = 3;
    strcpy(testCurrent, "1");
    assert(choice_key(&dc, &item, 0xd));
    assert(!strcmp(selected, "2"));

    scrVarPub.developer = 1;
    Scr_InitOpcodeLookup();
    assert(firstAllocation == 0x10000 * sizeof(OpcodeLookup));
    assert(scrParserGlob.opcodeLookup[0x10000 - 1].profileUsage == 0);
    free(scrParserGlob.opcodeLookup);
    free(scrParserGlob.sourcePosLookup);
    free(scrParserPub.sourceBufferLookup);
    puts("PASS UI handlers and script debug table");
    return 0;
}
