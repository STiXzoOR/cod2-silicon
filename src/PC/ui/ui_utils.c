#include "common_types.h"
#include "imports.h"
#include <string.h>
#include <stdlib.h>

extern int ___tolower(int c);
extern const char *Dvar_GetVariantString(const char *dvarName);
extern void I_strncat(char *dest, int destSize, const char *src);
extern int I_stricmp(const char *s1, const char *s2);
extern qboolean String_Parse(const char **p, char *out, int outSize);
extern const char *Com_ParseOnLine(const char **p);
extern void *Hunk_AllocAlignInternal(int size, int alignment);
extern void Com_Printf(const char *fmt, ...);

__attribute__((used, packed, aligned(4)))
const char *staticNULL[] = {
    "",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
#define staticNULL (*(const char **)&staticNULL[0])
static stringDef_t *g_strHandle[2048];

void Window_SetStaticFlags(Window *w, const int flags);
qboolean Item_EnableShowViaDvar(const itemDef_t *item, int flag);
qboolean Item_IsEditFieldDef(itemDef_t *item);
listBoxDef_t *Item_GetListBoxDef(itemDef_t *item);
multiDef_t *Item_GetMultiDef(itemDef_t *item);
void *UI_Alloc(int size, int alignment);
void String_Init(void);
qboolean Int_Parse(const char **p, int *i);
qboolean Float_Parse(const char **p, float *f);
void Window_SetOffsetTime(Window *w, int offsetTime);
void Window_SetRect(Window *w, const UiRectangle *rect);
void Window_SetRectClient(Window *w, const UiRectangle *rectClient);
void Window_SetRectEffects0(Window *w, const UiRectangle *rectEffects0);
void Window_SetRectEffects1(Window *w, const UiRectangle *rectEffects1);
void Window_SetDynamicFlags(Window *w, const int flags);
void Menu_SetCursorItem(menuDef_t *menu, int cursorItem);
void Item_SetTextRect(itemDef_t *item, const rectDef_t *textRect);
void Item_SetCursorPos(itemDef_t *item, int cursorPos);
int Item_GetCursorPosOffset(const itemDef_t *item, const char *text, int delta);
void ListBox_SetCursorPos(listBoxDef_t *listBox, int cursorPos);
void ListBox_SetStartPos(listBoxDef_t *listBox, int startPos);
void ListBox_SetEndPos(listBoxDef_t *listBox, int endPos);
Bool ListBox_HasValidCursorPos(const listBoxDef_t *listBox);
void Window_AddDynamicFlags(Window *w, const int newFlags);
void Window_RemoveDynamicFlags(Window *w, const int newFlags);
editFieldDef_t *Item_GetEditFieldDef(itemDef_t *item);
const char *String_Alloc(const char *p);
void Item_SetScreenCoords(itemDef_t *item, float x, float y, int horzAlign, int vertAlign);
void Menu_UpdatePosition(menuDef_t *menu);
qboolean Rect_Parse(const char **p, rectDef_t *r);

void Window_SetStaticFlags(Window *w, const int flags)
{
    w->staticFlags = flags;
}

qboolean Item_EnableShowViaDvar(const itemDef_t *item, int flag)
{
    char script[1024];
    char val[1024];
    const char *testValue;
    const char *p;

    memset(script, 0, 1024);

    if (!item)
        return 1;
    if (item->enableDvar == NULL)
        return 1;
    if (*item->enableDvar == '\0')
        return 1;
    if (item->dvarTest == NULL)
        return 1;
    if (*item->dvarTest == '\0')
        return 1;

    testValue = Dvar_GetVariantString(item->dvarTest);

    I_strncat(script, 1024, item->enableDvar);
    p = script;

    for (;;) {
        if (!String_Parse(&p, val, 1024)) {

            if (item->dvarFlags & flag)
                return 0;
            return 1;
        }

        if (val[0] == ';' && val[1] == '\0') {
            if (!String_Parse(&p, val, 1024)) {
                if (item->dvarFlags & flag)
                    return 0;
                return 1;
            }
        }

        if (item->dvarFlags & flag) {

            if (I_stricmp(testValue, val) == 0)
                return 1;
        } else {

            if (I_stricmp(testValue, val) == 0)
                return 0;
        }
    }
}

qboolean Item_IsEditFieldDef(itemDef_t *item)
{
    int type = item->dataType;

    if (type > 0x12)
        return 0;
    if ((1 << type) & 0x74e11)
        return 1;
    return 0;
}

listBoxDef_t *Item_GetListBoxDef(itemDef_t *item)
{
    if (item->dataType != 6) {
        Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_LISTBOX\n");
        return NULL;
    }
    return item->typeData.listBox;
}

multiDef_t *Item_GetMultiDef(itemDef_t *item)
{
    return item->typeData.multi;
}

void *UI_Alloc(int size, int alignment)
{
    return Hunk_AllocAlignInternal(size, alignment);
}

void String_Init(void)
{
    memset(g_strHandle, 0, 0x2000);
}

qboolean Int_Parse(const char **p, int *i)
{
    const char *token;

    token = Com_ParseOnLine(p);
    if (!token || *token == '\0')
        return 0;

    *i = atoi(token);
    return 1;
}

qboolean Float_Parse(const char **p, float *f)
{
    const char *token;

    token = Com_ParseOnLine(p);
    if (!token || *token == '\0')
        return 0;

    *f = (float)atof(token);
    return 1;
}

void Window_SetOffsetTime(Window *w, int offsetTime)
{
    w->offsetTime[0] = offsetTime;
}

static inline __attribute__((always_inline)) void CopyRect(byte *dst, const byte *src)
{
    *(int *)(dst) = *(int *)(src);
    *(int *)(dst + 4) = *(int *)(src + 4);
    *(int *)(dst + 8) = *(int *)(src + 8);
    *(int *)(dst + 12) = *(int *)(src + 12);
    *(int *)(dst + 16) = *(int *)(src + 16);
    *(int *)(dst + 20) = *(int *)(src + 20);
}

void Window_SetRect(Window *w, const UiRectangle *rect)
{
    CopyRect((byte *)w, (const byte *)rect);
}

void Window_SetRectClient(Window *w, const UiRectangle *rectClient)
{
    CopyRect((byte *)w->rectClient, (const byte *)rectClient);
}

void Window_SetRectEffects0(Window *w, const UiRectangle *rectEffects0)
{
    CopyRect((byte *)w->rectEffects0, (const byte *)rectEffects0);
}

void Window_SetRectEffects1(Window *w, const UiRectangle *rectEffects1)
{
    CopyRect((byte *)w->rectEffects1, (const byte *)rectEffects1);
}

void Window_SetDynamicFlags(Window *w, const int flags)
{
    w->dynamicFlags[0] = flags;
}

void Menu_SetCursorItem(menuDef_t *menu, int cursorItem)
{
    menu->cursorItem[0] = cursorItem;
}

void Item_SetTextRect(itemDef_t *item, const rectDef_t *textRect)
{
    CopyRect((byte *)item->textRect, (const byte *)textRect);
}

void Item_SetCursorPos(itemDef_t *item, int cursorPos)
{
    item->cursorPos[0] = cursorPos;
}

int Item_GetCursorPosOffset(const itemDef_t *item, const char *text, int delta)
{
    int pos;
    char c;

    pos = item->cursorPos[0];

    if (delta > 0) {

        while (1) {
            const char *cp = text + pos;

            if (cp != NULL && *cp == '^') {
                c = *(cp + 1);
                if (c != '\0' && c != '^' && c > '/' && c <= '9') {
                    pos += 2;
                    continue;
                }
            }
            if (*cp == '\0')
                break;
            if (delta == 0)
                break;
            pos += 1;
            delta -= 1;
        }
    } else {

        while (pos != 0) {
            if (delta == 0)
                break;

            if (pos > 1) {
                const char *cp = text + pos;
                if (cp != (const char *)2 && *(cp - 2) == '^') {
                    c = *(unsigned char *)(cp - 1);
                    if (c != '\0' && c != '^' && c > '/' && c <= '9') {
                        pos -= 2;
                        continue;
                    }
                }
            }
            pos -= 1;
            delta += 1;
        }
    }

    return pos;
}

void ListBox_SetCursorPos(listBoxDef_t *listBox, int cursorPos)
{
    listBox->cursorPos[0] = cursorPos;
}

void ListBox_SetStartPos(listBoxDef_t *listBox, int startPos)
{
    listBox->startPos[0] = startPos;
}

void ListBox_SetEndPos(listBoxDef_t *listBox, int endPos)
{
    listBox->endPos[0] = endPos;
}

Bool ListBox_HasValidCursorPos(const listBoxDef_t *listBox)
{

    int cursor = listBox->cursorPos[0];

    if (cursor >= listBox->endPos[0])
        return 0;
    if (cursor < listBox->startPos[0])
        return 0;
    return 1;
}

void Window_AddDynamicFlags(Window *w, const int newFlags)
{
    w->dynamicFlags[0] |= newFlags;
}

void Window_RemoveDynamicFlags(Window *w, const int newFlags)
{
    int flags = w->dynamicFlags[0];
    int mask = newFlags;

    if (mask & 4)
        mask |= 2;

    flags &= ~mask;
    w->dynamicFlags[0] = flags;
}

editFieldDef_t *Item_GetEditFieldDef(itemDef_t *item)
{

    switch (item->dataType) {
    case 0:
    case 4:
    case 9:
    case 10:
    case 11:
    case 14:
    case 16:
    case 17:
    case 18:
        return item->typeData.editField;
    }

    Com_Printf("^1Menu Error: Expecting type: ITEM_TYPE_EDITFIELD, ITEM_TYPE_NUMERICFIELD, ITEM_TYPE_DECIMALFIELD, ITEM_TYPE_VALIDFILEFIELD, ITEM_TYPE_UPREDITFIELD, ITEM_TYPE_YESNO, ITEM_TYPE_BIND, ITEM_TYPE_SLIDER, or ITEM_TYPE_TEXT\n");
    return NULL;
}

const char *String_Alloc(const char *p)
{
    int hash;
    int i;
    char *str;
    stringDef_t *s;
    stringDef_t *last;
    stringDef_t *newDef;

    if (p == NULL)
        return NULL;

    if (*p == '\0')
        return staticNULL;

    hash = 0;
    i = 0x77;
    while (p[i - 0x77] != '\0') {
        hash += ___tolower(p[i - 0x77]) * i;
        i++;
    }
    hash &= 0x7ff;

    s = g_strHandle[hash];
    while (s != NULL) {
        if (strcmp(p, *(char **)((byte *)s + 4)) == 0) {
            return *(char **)((byte *)s + 4);
        }
        s = *(stringDef_t **)s;
    }

    str = (char *)Hunk_AllocAlignInternal(strlen(p) + 1, 1);
    strcpy(str, p);

    last = g_strHandle[hash];
    if (last != NULL) {
        stringDef_t *next = *(stringDef_t **)last;
        while (next != NULL) {
            last = next;
            next = *(stringDef_t **)next;
        }
    }

    newDef = (stringDef_t *)Hunk_AllocAlignInternal(8, 4);
    *(int *)newDef = 0;
    *(char **)((byte *)newDef + 4) = str;

    if (last != NULL) {
        *(stringDef_t **)last = newDef;
    } else {
        g_strHandle[hash] = newDef;
    }

    return str;
}

void Item_SetScreenCoords(itemDef_t *item, float x, float y, int horzAlign, int vertAlign)
{
    byte *ib = (byte *)item;
    int newRect[6];

    if (item == NULL)
        return;

    if (item->window.border != 0) {
        float offset = item->window.borderSize;
        x += offset;
        y += offset;
    }

    *(float *)&newRect[0] = item->window.rectClient[0].x + x;
    *(float *)&newRect[1] = item->window.rectClient[0].y + y;
    *(float *)&newRect[2] = item->window.rectClient[0].w;
    *(float *)&newRect[3] = item->window.rectClient[0].h;

    if (item->window.rectClient[0].horzAlign != 0 || item->window.rectClient[0].vertAlign != 0) {
        newRect[4] = item->window.rectClient[0].horzAlign;
        newRect[5] = item->window.rectClient[0].vertAlign;
    } else {
        newRect[4] = horzAlign;
        newRect[5] = vertAlign;
    }

    item->window.rect[0].x = *(float *)&newRect[0];
    item->window.rect[0].y = *(float *)&newRect[1];
    item->window.rect[0].w = *(float *)&newRect[2];
    item->window.rect[0].h = *(float *)&newRect[3];
    item->window.rect[0].horzAlign = newRect[4];
    item->window.rect[0].vertAlign = newRect[5];

    *(float *)&newRect[0] = item->textRect[0].x;
    *(float *)&newRect[1] = item->textRect[0].y;
    newRect[4] = item->textRect[0].horzAlign;
    newRect[5] = item->textRect[0].vertAlign;
    newRect[2] = 0;
    newRect[3] = 0;

    item->textRect[0].x = *(float *)&newRect[0];
    item->textRect[0].y = *(float *)&newRect[1];
    item->textRect[0].w = *(float *)&newRect[2];
    item->textRect[0].h = *(float *)&newRect[3];
    item->textRect[0].horzAlign = newRect[4];
    item->textRect[0].vertAlign = newRect[5];
}

void Menu_UpdatePosition(menuDef_t *menu)
{
    float x, y;
    int i;

    if (menu == NULL)
        return;

    x = menu->window.rect[0].x;
    y = menu->window.rect[0].y;

    if (menu->window.border != 0) {
        float offset = menu->window.borderSize;
        x += offset;
        y += offset;
    }

    if (menu->itemCount <= 0)
        return;

    for (i = 0; i < menu->itemCount; i++) {
        Item_SetScreenCoords(menu->items[i], x, y,
                             menu->window.rect[0].horzAlign, menu->window.rect[0].vertAlign);
    }
}

qboolean Rect_Parse(const char **p, rectDef_t *r)
{
    const char *token;

    token = Com_ParseOnLine(p);
    if (!token || *token == '\0')
        return 0;
    r->x = (float)atof(token);

    token = Com_ParseOnLine(p);
    if (!token || *token == '\0')
        return 0;
    r->y = (float)atof(token);

    token = Com_ParseOnLine(p);
    if (!token || *token == '\0')
        return 0;
    r->w = (float)atof(token);

    token = Com_ParseOnLine(p);
    if (!token || *token == '\0')
        return 0;
    r->h = (float)atof(token);

    return 1;
}
