#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static dvar_t language, variable;
static const dvar_t *loc_language=&language;
static int found=1,g_editingField,variant_calls,draw_calls,cursor_calls;
static editFieldDef_t edit;
static Font_s font;
static char drawn[1024],extent[1024];
static const char *UI_TraceScriptItemParentName(itemDef_t *item) { return NULL; }
editFieldDef_t *Item_GetEditFieldDef(itemDef_t *item) { return &edit; }
void Item_Text_Paint(displayContextDef_t *dc,itemDef_t *item) {}
const char *Dvar_GetVariantString(const char *name) { ++variant_calls;return "raw-value"; }
struct dvar_s *Dvar_FindVar(const char *name) { return found?&variable:NULL; }
void I_strncpyz(char *out,const char *text,int size) { snprintf(out,size,"%s",text); }
void Item_SetTextExtents(itemDef_t *item,int *width,int *height,const char *text) { strcpy(extent,text);*width=20;*height=16; }
void Item_TextColor(displayContextDef_t *dc,itemDef_t *item,vec4_t *out) { (*out)[0]=1;(*out)[1]=1;(*out)[2]=1;(*out)[3]=1; }
FontHandle UI_GetFontHandle(int fontEnum,float scale) { return &font; }
int Key_GetOverstrikeMode(void) { return 1; }
void UI_DrawText(const char *text,int max,FontHandle handle,float x,float y,int ha,int va,float scale,const float *color,int style)
{ assert(handle==&font);strcpy(drawn,text);++draw_calls; }
void UI_DrawTextWithCursor(const char *text,int max,FontHandle handle,float x,float y,int ha,int va,float scale,const float *color,int style,int cursor,int cursorChar)
{ assert(cursorChar=='_');strcpy(drawn,text);++cursor_calls; }
#include "numeric_fields_source.h"
int main(void)
{
 itemDef_t item={0};displayContextDef_t dc={0};int failures=0;
 item.type=17;item.dvar="number";item.textscale=1;edit.maxPaintChars=100;variable.type=1;variable.current.value=12.375f;
 for(int locale=0;locale<15;++locale) {
  language.current.integer=locale;draw_calls=cursor_calls=variant_calls=0;
  Item_TextField_Paint(&dc,&item);
  const char *expected=(locale>=1&&locale<=4)||locale==6||locale==7?"12,38":"12.38";
  if(strcmp(drawn,expected)||strcmp(extent,expected)||variant_calls||draw_calls!=1) ++failures;
 }
 found=0;Item_TextField_Paint(&dc,&item);if(*drawn) ++failures;
 found=1;variable.type=5;Item_TextField_Paint(&dc,&item);if(*drawn) ++failures;
 variable.type=1;item.type=4;variant_calls=0;Item_TextField_Paint(&dc,&item);
 assert(!strcmp(drawn,"raw-value")&&variant_calls==1);
 item.type=17;language.current.integer=1;g_editingField=1;item.window.staticFlags=6;item.cursorPos[0]=2;
 cursor_calls=0;Item_TextField_Paint(&dc,&item);if(strcmp(drawn,"12,38")||cursor_calls!=1) ++failures;
 edit.paintOffset=3;Item_TextField_Paint(&dc,&item);if(strcmp(drawn,"38")) ++failures;
 printf("numeric field localization: %d failures\n",failures);return failures!=0;
}
