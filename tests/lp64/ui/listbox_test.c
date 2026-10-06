/* Drives the production Item_ListBox_HandleKey (extracted from the preprocessed
   COD2_X64 source) with a UI_FeederCount that dispatches like ui_main_mp.c:4479. */
#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define NUM_SERVERS 25
static int lastListBoxClickTime;
static float lastFeederId;
static int selections, lastSelection, scriptsRun;
static listBoxDef_t lb;

int UI_FeederCount(float feederID) { lastFeederId = feederID; return (feederID == 2.0f || feederID == 4.0f || feederID == 8.0f) ? NUM_SERVERS : 0; }
void UI_FeederSelection(float feederID, int index) { assert(feederID == 2.0f || feederID == 4.0f || feederID == 8.0f); selections++; lastSelection = index; }
listBoxDef_t *Item_GetListBoxDef(itemDef_t *item) { (void)item; return &lb; }
void CalcScreenX(float *v, int a) { (void)v; (void)a; }
void CalcScreenY(float *v, int a) { (void)v; (void)a; }
void CalcScreenPlacement(float *x, float *y, float *w, float *h, int ha, int va)
{ (void)x; (void)y; (void)w; (void)h; (void)ha; (void)va; }
void UI_OverrideCursorPos(itemDef_t *item) { (void)item; }
int Item_ListBox_MaxScroll(itemDef_t *item) { (void)item; return NUM_SERVERS - 22; }
void Item_SetCursorPos(itemDef_t *item, int pos) { item->cursorPos[0] = pos; }
void ListBox_SetCursorPos(itemDef_t *l, int pos) { ((listBoxDef_t *)l)->cursorPos[0] = pos; }
void ListBox_SetStartPos(itemDef_t *l, int pos) { ((listBoxDef_t *)l)->startPos[0] = pos; }
qboolean ListBox_HasValidCursorPos(itemDef_t *l)
{
    listBoxDef_t *b = (listBoxDef_t *)l;
    return b->cursorPos[0] < b->endPos[0] && b->cursorPos[0] >= b->startPos[0];
}
void Item_RunScript(displayContextDef_t *dc, itemDef_t *item, const char *s)
{ (void)dc; (void)item; assert(!strcmp(s, "uiScript JoinServer")); scriptsRun++; }

#include "listbox_production.h"

enum { K_MOUSE1 = 0xc8, K_MWHEELDOWN = 0xcd, K_MWHEELUP = 0xce };

static int check_feeder(float feeder)
{
    static displayContextDef_t dc;
    static itemDef_t item;
    int i, wheel, click, dbl, ok;

    item.type = 6;
    memset(&lb, 0, sizeof(lb)); selections = scriptsRun = lastListBoxClickTime = 0;
    item.special = feeder; /* feeder FEEDER_SERVERS */
    item.window.rect[0].x = 9; item.window.rect[0].y = 80; item.window.rect[0].w = 620; item.window.rect[0].h = 332;
    item.window.dynamicFlags[0] = 2; /* WINDOW_HASFOCUS */
    lb.elementHeight = 15; lb.elementWidth = 120; lb.endPos[0] = 22;
    lb.doubleClick = "uiScript JoinServer";
    dc.cursorx = 300; dc.cursory = 200; dc.realTime = 10000;
    item.cursorPos[0] = 0; /* row 0 currently selected */

    for (i = 0; i < 5; i++)
        Item_ListBox_HandleKey(&dc, &item, K_MWHEELDOWN, 1, 0);
    wheel = item.cursorPos[0];
    printf("5x K_MWHEELDOWN: cursor=%d startPos=%d (UI_FeederCount saw %g)\n", wheel, lb.startPos[0], lastFeederId);
    Item_ListBox_HandleKey(&dc, &item, K_MWHEELUP, 1, 0);
    printf("1x K_MWHEELUP:   cursor=%d\n", item.cursorPos[0]);
    assert(item.cursorPos[0] == 4);

    lb.cursorPos[0] = 7; /* Item_ListBox_MouseEnter: pointer over row 7 */
    dc.realTime = 20000;
    Item_ListBox_HandleKey(&dc, &item, K_MOUSE1, 1, 0);
    click = item.cursorPos[0];
    printf("K_MOUSE1 on row 7: cursor=%d, UI_FeederSelection index=%d\n", click, lastSelection);
    dc.realTime = 20150;
    Item_ListBox_HandleKey(&dc, &item, K_MOUSE1, 1, 0);
    dbl = scriptsRun;
    printf("second K_MOUSE1 150 ms later: doubleClick scripts run=%d\n", dbl);

    ok = wheel == 5 && click == 7 && dbl == 1;
    printf("%s\n", ok ? "PASS" : "FAIL");
    return !ok;
}

int main(void)
{
    return check_feeder(2.0f) | check_feeder(4.0f) | check_feeder(8.0f);
}
