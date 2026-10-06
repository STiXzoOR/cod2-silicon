#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static int labels, values, extents, translated;
static float drawX, drawY, drawColor[4];
const char *UI_SafeTranslateString(const char *text) { assert(!strcmp(text, "VALUE")); translated++; return "value"; }
FontHandle UI_GetFontHandle(int font, float scale) { (void)font; (void)scale; return NULL; }
void UI_DrawText(const char *text, int max, FontHandle font, float x, float y, int ha, int va, float scale, const float *color, int style)
{ (void)max; (void)font; (void)ha; (void)va; (void)scale; (void)style; assert(!strcmp(text, "value") || !*text); values++; drawX = x; drawY = y; memcpy(drawColor, color, sizeof(drawColor)); }
void Item_Text_Paint(displayContextDef_t *dc, itemDef_t *item)
{ (void)dc; assert(translated); labels++; item->textRect[0].w = 80; }
void Item_SetTextExtents(itemDef_t *item, int *w, int *h, const char *text)
{ (void)text; extents++; item->textRect[0].w = 12; *w = 12; *h = 10; }
void Item_TextColor(displayContextDef_t *dc, itemDef_t *item, vec4_t *out)
{ (void)dc; memcpy(out, item->window.foreColor, sizeof(vec4_t)); }
void Item_Text_Wrapped_Paint(itemDef_t *item, const char *text, const float *color) { (void)item; (void)text; (void)color; assert(0); }
void Item_Text_AutoWrapped_Paint(itemDef_t *item, const char *text, const float *color) { (void)item; (void)text; (void)color; assert(0); }
#include "listpaint_production.h"
int main(void)
{
    itemDef_t item = {0}; menuDef_t menu = {0}; displayContextDef_t dc = {0};
    item.parent = &menu; item.text = "Connection:"; item.textRect[0].x = 10; item.textRect[0].y = 25;
    item.window.foreColor[0] = .3f; item.window.foreColor[3] = 1;
    Item_List_Paint(&dc, &item, "@VALUE");
    assert(labels == 1 && values == 1 && drawX == 98 && drawY == 25);
    assert(extents == 0 && item.textRect[0].w == 80 && drawColor[0] == .3f);
    item.text = NULL; Item_List_Paint(&dc, &item, "value");
    assert(labels == 1 && values == 2 && drawX == 10);
    item.window.dynamicFlags[0] = 6; menu.focusColor[0] = .5f; menu.focusColor[3] = 1;
    Item_List_Paint(&dc, &item, "value");
    assert(fabsf(drawColor[0] - .45f) < 1e-6f);
    /* Fade and text-style effects belong to label painting, not the value. */
    item.window.dynamicFlags[0] = 0x10; item.textStyle = 1; dc.realTime = 300;
    Item_List_Paint(&dc, &item, "value"); assert(drawColor[0] == .3f);
    item.text = " "; Item_List_Paint(&dc, &item, ""); assert(labels == 2);
    puts("PASS multi label/value positions, translation order, cached extents and reference colours");
    return 0;
}
