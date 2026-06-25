#include "common_types.h"
#include "imports.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern keywordHash_t itemParseKeywords[67];
extern keywordHash_t menuParseKeywords[35];

extern char *va(const char *format, ...);

extern keywordHash_t *itemParseKeywordHash[512];
static char string_00f3b9c0[4096];
static unsigned char g_load[0x640];
static char menuBuf[32768];
static keywordHash_t *menuParseKeywordHash[512];
static char menuBuf1[4096];
void UI_MapLoadInfo(const char *filename);
void PC_SourceError(int handle, char *format, ...);
qboolean PC_Script_Parse(int handle, const char **out);
void Window_SetStaticFlags(void *item, int flags);
void Window_SetDynamicFlags(Window *w, const int flags);
qboolean PC_ReadTokenHandle(int handle, void *token);
int PC_ReadLineHandle(int handle, pc_token_t *pc_token);
const char *String_Alloc(const char *str);
void I_strncpyz(char *dest, const char *src, int destsize);
char *I_strlwr(char *str);
void I_strncat(char *dest, int destsize, const char *src);
void *CL_RegisterMaterialNoMip(const char *name, int imageTrack);
snd_alias_list_t *Com_FindSoundAlias(const char *name);
void *UI_Alloc(int size, int align);
int PC_LoadSourceHandle(const char *filename);
int PC_FreeSourceHandle(int handle);
FontHandle CL_RegisterFont(const char *fontName, int imageTrack);
void CL_DuplicateFont(FontHandle fontCopy, const char *name);
void Com_DuplicateSoundAlias(snd_alias_list_t *aliasCopy, const char *name);
int PC_SourceFileAndLine(int handle, char *filename, int *line);
listBoxDef_t *Item_GetListBoxDef(itemDef_t *item);
multiDef_t *Item_GetMultiDef(itemDef_t *item);
void Item_SetCursorPos(itemDef_t *item, int cursorPos);
void ListBox_SetCursorPos(listBoxDef_t *listBox, int cursorPos);
void ListBox_SetStartPos(listBoxDef_t *listBox, int startPos);
void ListBox_SetEndPos(listBoxDef_t *listBox, int endPos);
void Window_SetRectClient(Window *w, const UiRectangle *rectClient);
extern int FS_ReadFile(const char *filename, void **buffer);
extern void FS_FreeFile(void *buffer);
extern void Com_SetCSV(int csv);
extern const char *Com_ParseOnLine(const char **data_p);
extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern void CL_Material_Duplicate(int material, const char *name);
void Window_SetRect(menuDef_t *menu, rectDef_t *rect);
void Menu_UpdatePosition(menuDef_t *menu);
void Item_InitControls(const char (*item)[4]);
qboolean MenuParse_onOpen(const char (*item)[4], int handle);
qboolean MenuParse_onClose(const char (*item)[4], int handle);
qboolean MenuParse_onESC(const char (*item)[4], int handle);
qboolean MenuParse_soundLoop(const char (*item)[4], int handle);
void __attribute_regparm__(2) Item_ValidateTypeData(itemDef_t *item, int handle);
qboolean Item_IsEditFieldDef(itemDef_t *item);
editFieldDef_t *Item_GetEditFieldDef(itemDef_t *item);
qboolean ItemParse_textsavegame(const char (*item)[4], int handle);
qboolean ItemParse_notselectable(const char (*item)[4], int handle);
qboolean ItemParse_noScrollBars(const char (*item)[4], int handle);
qboolean ItemParse_horizontalscroll(const char (*item)[4], int handle);
qboolean ItemParse_doubleClick(const char (*item)[4], int handle);
qboolean ItemParse_onFocus(const char (*item)[4], int handle);
qboolean ItemParse_leaveFocus(const char (*item)[4], int handle);
qboolean ItemParse_mouseEnter(const char (*item)[4], int handle);
qboolean ItemParse_mouseExit(const char (*item)[4], int handle);
qboolean ItemParse_mouseEnterText(const char (*item)[4], int handle);
qboolean ItemParse_mouseExitText(const char (*item)[4], int handle);
qboolean ItemParse_action(itemDef_t *item, int handle);
qboolean ItemParse_accept(itemDef_t *item, int handle);
qboolean Item_Parse_maxCharsGotoNext(const char (*item)[4], int handle);
qboolean ItemParse_dvarStrList(const char (*item)[4], int handle);
qboolean ItemParse_enableDvar(const char (*item)[4], int handle);
qboolean ItemParse_disableDvar(const char (*item)[4], int handle);
qboolean ItemParse_showDvar(const char (*item)[4], int handle);
qboolean ItemParse_hideDvar(const char (*item)[4], int handle);
qboolean ItemParse_focusDvar(const char (*item)[4], int handle);
void Menu_PostParse(menuDef_t *menu);
qboolean MenuParse_popup(const char (*item)[4], int handle);
qboolean MenuParse_outOfBounds(const char (*item)[4], int handle);
qboolean MenuParse_legacySplitScreenScale(const char (*item)[4], int handle);
qboolean ItemParse_decoration(const char (*item)[4], int handle);
qboolean ItemParse_wrapped(const char (*item)[4], int handle);
qboolean ItemParse_autowrapped(const char (*item)[4], int handle);
qboolean MenuParse_execKey(const char (*item)[4], int handle);
qboolean ItemParse_execKey(const char (*item)[4], int handle);
qboolean ItemParse_textfile(const char (*item)[4], int handle);
int FS_FOpenFileByMode(const char *qpath, fileHandle_t *f, fsMode_t mode);
int FS_Read(void *buffer, int len, fileHandle_t h);
void FS_FCloseFile(fileHandle_t h);
int Com_Compress(char *data_p);
void Com_BeginParseSession(const char *filename);
void Com_EndParseSession(void);
const char *Com_Parse(const char **data_p);
int I_stricmp(const char *s1, const char *s2);
Bool I_isupper(int c);
void Com_Printf(const char *fmt, ...);
void Com_Error(errorParm_t code, const char *fmt, ...);
void Menu_SetCursorItem(menuDef_t *menu, int cursorItem);
static Bool __attribute_regparm__(2) Menu_New(int handle, int imageTrack);
qboolean Item_Parse(int handle, const char (*item)[4]);
qboolean MenuParse_itemDef(const char (*item)[4], int handle);
void Item_SetupKeywordHash(void);
void Menu_SetupKeywordHash(void);
qboolean MenuParse_name(menuDef_t *menu, int handle);
qboolean MenuParse_background(const char (*item)[4], int handle);
qboolean MenuParse_cinematic(menuDef_t *menu, int handle);
qboolean ItemParse_name(itemDef_t *item, int handle);
qboolean ItemParse_focusSound(const char (*item)[4], int handle);
qboolean ItemParse_text(itemDef_t *item, int handle);
qboolean ItemParse_group(itemDef_t *item, int handle);
qboolean ItemParse_background(const char (*item)[4], int handle);
qboolean ItemParse_cinematic(itemDef_t *item, int handle);
qboolean ItemParse_dvarTest(itemDef_t *item, int handle);
qboolean ItemParse_dvar(const char (*item)[4], int handle);
qboolean ItemParse_dvarEnumList(const char (*item)[4], int handle);
qboolean ItemParse_outlinecolor(const char (*item)[4], int handle);
qboolean MenuParse_outlinecolor(const char (*item)[4], int handle);
qboolean PC_Rect_Parse(int handle, rectDef_t *r);
qboolean ItemParse_rect(const char (*item)[4], int handle);
qboolean MenuParse_rect(const char (*item)[4], int handle);
qboolean MenuParse_borderSize(const char (*item)[4], int handle);
qboolean MenuParse_backcolor(const char (*item)[4], int handle);
qboolean MenuParse_forecolor(const char (*item)[4], int handle);
qboolean MenuParse_bordercolor(const char (*item)[4], int handle);
qboolean MenuParse_focuscolor(const char (*item)[4], int handle);
qboolean MenuParse_disablecolor(const char (*item)[4], int handle);
qboolean MenuParse_fadeClamp(const char (*item)[4], int handle);
qboolean MenuParse_fadeAmount(const char (*item)[4], int handle);
qboolean MenuParse_fadeInAmount(const char (*item)[4], int handle);
qboolean MenuParse_blurWorld(const char (*item)[4], int handle);
qboolean ItemParse_elementwidth(const char (*item)[4], int handle);
qboolean ItemParse_elementheight(const char (*item)[4], int handle);
qboolean ItemParse_feeder(const char (*item)[4], int handle);
qboolean ItemParse_bordersize(const char (*item)[4], int handle);
qboolean ItemParse_textalignx(const char (*item)[4], int handle);
qboolean ItemParse_textaligny(const char (*item)[4], int handle);
qboolean ItemParse_textscale(const char (*item)[4], int handle);
qboolean ItemParse_backcolor(const char (*item)[4], int handle);
qboolean ItemParse_forecolor(const char (*item)[4], int handle);
qboolean ItemParse_bordercolor(const char (*item)[4], int handle);
qboolean ItemParse_special(const char (*item)[4], int handle);
qboolean ItemParse_dvarFloat(const char (*item)[4], int handle);
qboolean ItemParse_dvarFloatList(const char (*item)[4], int handle);
qboolean MenuParse_fullscreen(const char (*item)[4], int handle);
qboolean MenuParse_style(const char (*item)[4], int handle);
qboolean MenuParse_visible(const char (*item)[4], int handle);
qboolean MenuParse_border(const char (*item)[4], int handle);
qboolean MenuParse_ownerdrawFlag(const char (*item)[4], int handle);
qboolean MenuParse_ownerdraw(const char (*item)[4], int handle);
qboolean MenuParse_fadeCycle(const char (*item)[4], int handle);
qboolean MenuParse_execKeyInt(const char (*item)[4], int handle);
qboolean ItemParse_origin(const char (*item)[4], int handle);
qboolean ItemParse_style(const char (*item)[4], int handle);
qboolean ItemParse_type(const char (*item)[4], int handle);
qboolean ItemParse_elementtype(const char (*item)[4], int handle);
qboolean ItemParse_columns(const char (*item)[4], int handle);
qboolean ItemParse_border(const char (*item)[4], int handle);
qboolean ItemParse_visible(const char (*item)[4], int handle);
qboolean ItemParse_ownerdraw(const char (*item)[4], int handle);
qboolean ItemParse_align(const char (*item)[4], int handle);
qboolean ItemParse_textalign(const char (*item)[4], int handle);
qboolean ItemParse_textstyle(const char (*item)[4], int handle);
qboolean ItemParse_textfont(const char (*item)[4], int handle);
qboolean ItemParse_maxChars(const char (*item)[4], int handle);
qboolean ItemParse_maxPaintChars(const char (*item)[4], int handle);
qboolean ItemParse_ownerdrawFlag(const char (*item)[4], int handle);
qboolean ItemParse_execKeyInt(const char (*item)[4], int handle);
static Bool __attribute_regparm__(2) UI_ParseMenuInternal(const char *menuFile, int imageTrack);
MenuList *UI_LoadMenus(const char *menuFile, int imageTrack);
MenuList *UI_LoadMenu(const char *menuFile, int imageTrack);

static inline __attribute__((always_inline)) loadAssets_t *UI_LoadAssets(void)
{
    return (loadAssets_t *)g_load;
}

static inline __attribute__((always_inline)) MenuList *UI_MenuList(void)
{
    return (MenuList *)(g_load + sizeof(loadAssets_t));
}

static inline __attribute__((always_inline)) itemDef_t **UI_MenuItemStorage(void)
{
    return (itemDef_t **)(g_load + 64);
}

static inline __attribute__((always_inline)) menuDef_t **UI_MenuStorage(void)
{
    return (menuDef_t **)(g_load + 1088);
}

static inline __attribute__((always_inline)) keywordHash_t *UI_KeywordHashNext(keywordHash_t *hash)
{
    return (keywordHash_t *)(uintptr_t)hash->next;
}

static inline __attribute__((always_inline)) unsigned int UI_KeywordHashKey(const char *keyword)
{
    int hash;
    int weight;
    unsigned char c;

    hash = 0;
    weight = 0x77;
    c = (unsigned char)*keyword;

    while (c != '\0') {
        int ch = (char)c;

        if (I_isupper(ch))
            ch += 0x20;

        hash += ch * weight;
        weight += 1;
        c = (unsigned char)*++keyword;
    }

    return (unsigned int)((hash ^ (hash >> 10) ^ (hash >> 20)) & 0x1ff);
}

static inline __attribute__((always_inline)) qboolean UI_ParseFloatToken(int handle, float *out)
{
    pc_token_t token;
    qboolean negative;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    negative = 0;
    if (token.string[0] == '-') {
        if (!PC_ReadTokenHandle(handle, &token))
            return 0;
        negative = 1;
    }

    if (token.type != 3) {
        PC_SourceError(handle, "expected float but found %s\n", token.string);
        return 0;
    }

    if (!negative)
        *out = token.floatvalue;
    else
        *out = -token.floatvalue;
    return 1;
}

static inline __attribute__((always_inline)) qboolean UI_ParseIntToken(int handle, int *out)
{
    pc_token_t token;
    qboolean negative = 0;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    if (token.string[0] == '-') {
        negative = 1;
        if (!PC_ReadTokenHandle(handle, &token))
            return 0;
    }

    if (token.type != 3) {
        PC_SourceError(handle, "expected integer but found %s\n", token.string);
        return 0;
    }

    *out = token.intvalue;
    if (negative)
        *out = -token.intvalue;
    return 1;
}

static inline __attribute__((always_inline)) void UI_ParseOptionalIntLine(int handle, int *out)
{
    pc_token_t token;
    qboolean negative;

    if (!PC_ReadLineHandle(handle, &token)) {
        *out = 0;
        return;
    }

    negative = 0;
    if (token.string[0] == '-') {
        if (!PC_ReadLineHandle(handle, &token)) {
            *out = 0;
            return;
        }
        negative = 1;
    }

    if (token.type != 3) {
        PC_SourceError(handle, "expected integer but found %s\n", token.string);
        *out = 0;
        return;
    }

    *out = negative ? -token.intvalue : token.intvalue;
}

static inline __attribute__((always_inline)) qboolean UI_ParseFloatArray(int handle, float *out, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (!UI_ParseFloatToken(handle, &out[i]))
            return 0;
    }

    return 1;
}

void UI_MapLoadInfo(const char *filename)
{
    void *loadfile;
    const char *parse;
    const char *token;
    char key[256];
    char name[64];
    int tokenLen;
    int material;
    const char *value;

    if (!filename[0])
        return;

    if (FS_ReadFile(filename, &loadfile) < 0) {
        Com_Printf((const char *)"^3WARNING: Could not find '%s'.\n", filename);
        return;
    }

    parse = (const char *)loadfile;
    Com_BeginParseSession(filename);
    Com_SetCSV(1);

    for (;;) {
        token = Com_Parse(&parse);
        if (!token[0])
            break;

        tokenLen = strlen(token);
        if (tokenLen + 1 > 255) {
            Com_EndParseSession();
            Com_Error(1, (const char *)"key '%s' is %i > %i characters long", key, tokenLen, 255);
        }

        memcpy(key, token, tokenLen + 1);

        value = Com_ParseOnLine(&parse);
        if (!value[0]) {
            Com_EndParseSession();
            Com_Error(1, (const char *)"key '%s' missing value in '%s'\n", key, filename);
        }

        material = (int)CL_RegisterMaterialNoMip(value, 3);
        Com_sprintf(name, 64, (const char *)"$%s", key);
        I_strlwr(name);
        CL_Material_Duplicate(material, name);
    }

    Com_EndParseSession();
    FS_FreeFile(loadfile);
}

void PC_SourceError(int handle, char *format, ...)
{
    int line;
    va_list argptr;
    char filename[128];

    va_start(argptr, format);
    vsnprintf(string_00f3b9c0, sizeof(string_00f3b9c0), format, argptr);
    va_end(argptr);

    filename[0] = '\0';
    line = 0;
    PC_SourceFileAndLine(handle, filename, &line);
    Com_Printf("^1Menu load error: %s, line %d: %s\n", filename, line, string_00f3b9c0);
}

qboolean PC_Script_Parse(int handle, const char **out)
{
    char script[1024];
    pc_token_t token;

    memset(script, 0, sizeof(script));

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;
    if (I_stricmp(token.string, "{"))
        return 0;

    for (;;) {
        if (!PC_ReadTokenHandle(handle, &token))
            return 0;
        if (!I_stricmp(token.string, "}")) {
            *out = String_Alloc(script);
            return 1;
        }

        if (token.string[0] == '\0' || token.string[1] != '\0')
            I_strncat(script, sizeof(script), va("\"%s\"", token.string));
        else
            I_strncat(script, sizeof(script), token.string);
        I_strncat(script, sizeof(script), " ");
    }
}

void Item_InitControls(const char (*item)[4])
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    if (!itemDef || itemDef->type != 6)
        return;

    listPtr = Item_GetListBoxDef(itemDef);
    Item_SetCursorPos(itemDef, 0);
    if (!listPtr)
        return;

    ListBox_SetCursorPos(listPtr, 0);
    ListBox_SetStartPos(listPtr, 0);
    ListBox_SetEndPos(listPtr, 0);
}

qboolean MenuParse_onOpen(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x244)) != 0;
}

qboolean MenuParse_onClose(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x248)) != 0;
}

qboolean MenuParse_onESC(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x24c)) != 0;
}

qboolean MenuParse_soundLoop(const char (*item)[4], int handle)
{
    pc_token_t token;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;
    if (token.string[0])
        (*(const char **)&((itemDef_t *)item)->textRect[2].vertAlign) = String_Alloc(token.string);
    return 1;
}

void __attribute_regparm__(2) Item_ValidateTypeData(itemDef_t *item, int handle)
{
    editFieldDef_t *editDef;

    if (item->typeData.data) {
        if (item->dataType != item->type) {
            PC_SourceError(handle,
                           "Attempting to change type from %d to %d.\nMove the type definition higher up in the itemDef.\n",
                           item->dataType, item->type);
        }
        return;
    }

    item->dataType = item->type;

    switch (item->type) {
    case 6:
        item->typeData.listBox = UI_Alloc(sizeof(listBoxDef_t), 4);
        return;

    case 4:
    case 9:
    case 0x10:
    case 0x12:
    case 0xb:
    case 0xe:
    case 0xa:
    case 0:
    case 0x11:
        item->typeData.editField = UI_Alloc(sizeof(editFieldDef_t), 4);
        if (item->type != 4 && item->type != 0x10 && item->type != 9 &&
            item->type != 0x12 && item->type != 0x11) {
            return;
        }

        editDef = Item_GetEditFieldDef(item);
        if (editDef && !editDef->maxPaintChars) {
            editDef->maxPaintChars = 0x100;
        }
        return;

    case 0xc:
        item->typeData.multi = UI_Alloc(sizeof(multiDef_t), 4);
        return;
    }
}

static inline __attribute__((always_inline)) void Item_InitDvarEditField(itemDef_t *item)
{
    editFieldDef_t *editDef;

    if (!item->dvar || !item->typeData.data || !Item_IsEditFieldDef(item)) {
        return;
    }

    editDef = Item_GetEditFieldDef(item);
    if (!editDef) {
        return;
    }

    editDef->minVal = -1.0f;
    editDef->maxVal = -1.0f;
    editDef->defVal = -1.0f;
}

qboolean ItemParse_textsavegame(const char (*item)[4], int handle)
{
    ((itemDef_t *)item)->text = (const char *)"savegameinfo";
    ((itemDef_t *)item)->textSavegameInfo = 1;
    return 1;
}

qboolean ItemParse_notselectable(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    Item_ValidateTypeData(itemDef, handle);
    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;
    if (itemDef->type == 6)
        listPtr->notselectable = 1;
    return 1;
}

qboolean ItemParse_noScrollBars(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    Item_ValidateTypeData(itemDef, handle);
    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;
    if (itemDef->type == 6)
        listPtr->noScrollBars = 1;
    return 1;
}

qboolean ItemParse_horizontalscroll(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x200000);
    return 1;
}

qboolean ItemParse_doubleClick(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;

    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;

    return PC_Script_Parse(handle, &listPtr->doubleClick) != 0;
}

qboolean ItemParse_onFocus(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x2b8)) != 0;
}

qboolean ItemParse_leaveFocus(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x2bc)) != 0;
}

qboolean ItemParse_mouseEnter(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x2a8)) != 0;
}

qboolean ItemParse_mouseExit(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x2ac)) != 0;
}

qboolean ItemParse_mouseEnterText(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x2a0)) != 0;
}

qboolean ItemParse_mouseExitText(const char (*item)[4], int handle)
{
    return PC_Script_Parse(handle, (const char **)((char *)item + 0x2a4)) != 0;
}

qboolean ItemParse_action(itemDef_t *item, int handle)
{
    return PC_Script_Parse(handle, &item->action) != 0;
}

qboolean ItemParse_accept(itemDef_t *item, int handle)
{
    return PC_Script_Parse(handle, &item->onAccept) != 0;
}

qboolean Item_Parse_maxCharsGotoNext(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    editFieldDef_t *editDef;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;

    editDef = Item_GetEditFieldDef(itemDef);
    if (!editDef)
        return 0;

    editDef->maxCharsGotoNext = 1;
    return 1;
}

qboolean ItemParse_dvarStrList(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    multiDef_t *multiPtr;
    pc_token_t token;
    qboolean haveValue;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data || itemDef->type != 12)
        return 0;

    multiPtr = Item_GetMultiDef(itemDef);
    multiPtr->count = 0;
    multiPtr->strDef = 1;

    if (!PC_ReadTokenHandle(handle, &token) || token.string[0] != '{')
        return 0;

    for (;;) {
        haveValue = 0;

        for (;;) {
            if (!PC_ReadTokenHandle(handle, &token)) {
                PC_SourceError(handle, "end of file inside menu item\n");
                return 0;
            }

            if (token.string[0] == '}')
                return 1;
            if (token.string[0] == ',' || token.string[0] == ';')
                continue;

            if (!haveValue) {
                multiPtr->dvarList[multiPtr->count] = String_Alloc(token.string);
                haveValue = 1;
                continue;
            }

            multiPtr->dvarStr[multiPtr->count] = String_Alloc(token.string);
            multiPtr->count++;
            if (multiPtr->count > 31)
                return 0;
            break;
        }
    }
}

qboolean ItemParse_enableDvar(const char (*item)[4], int handle)
{
    if (!PC_Script_Parse(handle, (const char **)((char *)item + 0x2cc)))
        return 0;
    ((itemDef_t *)item)->dvarFlags |= 1;
    return 1;
}

qboolean ItemParse_disableDvar(const char (*item)[4], int handle)
{
    if (!PC_Script_Parse(handle, (const char **)((char *)item + 0x2cc)))
        return 0;
    ((itemDef_t *)item)->dvarFlags |= 2;
    return 1;
}

qboolean ItemParse_showDvar(const char (*item)[4], int handle)
{
    if (!PC_Script_Parse(handle, (const char **)((char *)item + 0x2cc)))
        return 0;
    ((itemDef_t *)item)->dvarFlags |= 4;
    return 1;
}

qboolean ItemParse_hideDvar(const char (*item)[4], int handle)
{
    if (!PC_Script_Parse(handle, (const char **)((char *)item + 0x2cc)))
        return 0;
    ((itemDef_t *)item)->dvarFlags |= 8;
    return 1;
}

qboolean ItemParse_focusDvar(const char (*item)[4], int handle)
{
    if (!PC_Script_Parse(handle, (const char **)((char *)item + 0x2cc)))
        return 0;
    ((itemDef_t *)item)->dvarFlags |= 0x10;
    return 1;
}

void Menu_PostParse(menuDef_t *menu)
{
    int size = ((menuDef_t *)menu)->itemCount * 4;
    void *items = UI_Alloc(size, 4);
    (*(void **)&((menuDef_t *)menu)->items) = items;
    memcpy(items, (void *)(g_load + 64), size);
    if (((menuDef_t *)menu)->fullScreen) {
        rectDef_t rect;
        rect.x = 0.0f;
        rect.y = 0.0f;
        rect.w = 640.0f;
        rect.h = 480.0f;
        Window_SetRect(menu, &rect);
    }
    Menu_UpdatePosition(menu);
}

qboolean MenuParse_popup(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x1000000);
    return 1;
}

qboolean MenuParse_outOfBounds(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x2000000);
    return 1;
}

qboolean MenuParse_legacySplitScreenScale(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x4000000);
    return 1;
}

qboolean ItemParse_decoration(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x100000);
    return 1;
}

qboolean ItemParse_wrapped(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x400000);
    return 1;
}

qboolean ItemParse_autowrapped(const char (*item)[4], int handle)
{
    Window_SetStaticFlags((void *)item, ((itemDef_t *)item)->window.staticFlags | 0x800000);
    return 1;
}

qboolean MenuParse_execKey(const char (*item)[4], int handle)
{
    menuDef_t *menu = (menuDef_t *)item;
    pc_token_t token;
    const char *action;
    ItemKeyHandler *handler;
    short keyindex;

    if (__builtin_expect(PC_ReadTokenHandle(handle, &token) != 0, 1)) {
        keyindex = (short)(unsigned char)token.string[0];
        if (!PC_Script_Parse(handle, &action))
            return 0;

        handler = (ItemKeyHandler *)UI_Alloc(sizeof(ItemKeyHandler), 4);
        handler->key = keyindex;
        handler->action = action;
        handler->next = (int)menu->onKey;
        menu->onKey = handler;
        return 1;
    }
    return 0;
}

qboolean ItemParse_execKey(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    pc_token_t token;
    const char *action;
    ItemKeyHandler *handler;
    short keyindex;

    if (__builtin_expect(PC_ReadTokenHandle(handle, &token) != 0, 1)) {
        keyindex = (short)(unsigned char)token.string[0];
        if (!PC_Script_Parse(handle, &action))
            return 0;

        handler = (ItemKeyHandler *)UI_Alloc(sizeof(ItemKeyHandler), 4);
        handler->key = keyindex;
        handler->action = action;
        handler->next = (int)itemDef->onKey;
        itemDef->onKey = handler;
        return 1;
    }
    return 0;
}

qboolean ItemParse_textfile(const char (*item)[4], int handle)
{
    pc_token_t token;
    fileHandle_t f;
    int len;
    const char *text;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    len = FS_FOpenFileByMode(token.string, &f, FS_READ);
    text = NULL;
    if (f) {
        FS_Read(menuBuf1, len, f);
        menuBuf1[len] = '\0';
        FS_FCloseFile(f);
        text = menuBuf1;
    }

    ((itemDef_t *)item)->text = String_Alloc(text);
    return 1;
}

static Bool __attribute_regparm__(2) Menu_New(int handle, int imageTrack)
{
    loadAssets_t *load;
    MenuList *menuList;
    menuDef_t *menu;
    keywordHash_t *hash;
    pc_token_t token;
    byte *menuBytes;

    load = UI_LoadAssets();
    menuList = UI_MenuList();

    menu = (menuDef_t *)UI_Alloc(0x280, 4);
    memset(menu, 0, sizeof(menuDef_t));
    Menu_SetCursorItem(menu, -1);

    menuBytes = (byte *)menu;
    ((menuDef_t *)menuBytes)->fadeAmount = load->fadeAmount;
    ((menuDef_t *)menuBytes)->fadeInAmount = load->fadeInAmount;
    ((menuDef_t *)menuBytes)->fadeClamp = load->fadeClamp;
    ((menuDef_t *)menuBytes)->fadeCycle = load->fadeCycle;
    ((menuDef_t *)menuBytes)->imageTrack = imageTrack;
    ((menuDef_t *)menuBytes)->items = UI_MenuItemStorage();

    memset(menu, 0, 0x210);
    ((menuDef_t *)menuBytes)->window.borderSize = 1.0f;
    ((menuDef_t *)menuBytes)->window.foreColor[3] = 1.0f;
    ((menuDef_t *)menuBytes)->window.foreColor[2] = 1.0f;
    ((menuDef_t *)menuBytes)->window.foreColor[1] = 1.0f;
    ((menuDef_t *)menuBytes)->window.foreColor[0] = 1.0f;
    ((menuDef_t *)menuBytes)->window.cinematic = -1;

    if (!PC_ReadTokenHandle(handle, &token) || token.string[0] != '{')
        return 0;

    for (;;) {
        memset(&token, 0, sizeof(token));

        if (!PC_ReadTokenHandle(handle, &token)) {
            PC_SourceError(handle, "end of file inside menu");
            return 0;
        }

        if (token.string[0] == '}')
            break;

        hash = menuParseKeywordHash[UI_KeywordHashKey(token.string)];
        while (hash != NULL && I_stricmp(hash->keyword, token.string) != 0)
            hash = UI_KeywordHashNext(hash);

        if (hash == NULL) {
            PC_SourceError(handle, "unknown menu keyword %s", token.string);
            continue;
        }

        if (!((qboolean (*)(menuDef_t *, int))hash->func)(menu, handle)) {
            PC_SourceError(handle, "couldn't parse menu keyword %s", token.string);
            return 0;
        }
    }

    if (((menuDef_t *)menuBytes)->window.name == NULL) {
        PC_SourceError(handle, "menu has no name");
        return 0;
    }

    Menu_PostParse(menu);

    if (menuList->menuCount > 0x7f)
        Com_Error(ERR_DROP, "too many menus");

    menuList->menus[menuList->menuCount] = menu;
    menuList->menuCount += 1;
    return 1;
}

qboolean Item_Parse(int handle, const char (*item)[4])
{
    keywordHash_t *hash;
    pc_token_t token;

    if (!PC_ReadTokenHandle(handle, &token) || token.string[0] != '{')
        return 0;

    for (;;) {
        memset(&token, 0, sizeof(token));

        if (!PC_ReadTokenHandle(handle, &token)) {
            PC_SourceError(handle, "end of file inside menu item");
            return 0;
        }

        if (token.string[0] == '}')
            return 1;

        hash = itemParseKeywordHash[UI_KeywordHashKey(token.string)];
        while (hash != NULL && I_stricmp(hash->keyword, token.string) != 0)
            hash = UI_KeywordHashNext(hash);

        if (hash == NULL) {
            PC_SourceError(handle, "unknown menu item keyword %s", token.string);
            continue;
        }

        if (!((qboolean (*)(const char (*)[4], int))hash->func)(item, handle)) {
            PC_SourceError(handle, "couldn't parse menu item keyword %s", token.string);
            return 0;
        }
    }
}

qboolean MenuParse_itemDef(const char (*item)[4], int handle)
{
    itemDef_t *itemDef;
    itemDef_t **items;
    byte *menuBytes;
    byte *itemBytes;
    int itemCount;
    int imageTrack;

    menuBytes = (byte *)item;
    itemCount = ((menuDef_t *)menuBytes)->itemCount;
    if (itemCount > 0xff)
        return 1;

    items = ((menuDef_t *)menuBytes)->items;
    itemDef = (itemDef_t *)UI_Alloc(0x2f4, 4);
    items[itemCount] = itemDef;

    imageTrack = ((menuDef_t *)menuBytes)->imageTrack;
    itemBytes = (byte *)itemDef;
    memset(itemDef, 0, sizeof(itemDef_t));
    ((itemDef_t *)itemBytes)->textscale = 0.55f;
    ((itemDef_t *)itemBytes)->imageTrack = imageTrack;

    memset(itemDef, 0, 0x210);
    ((itemDef_t *)itemBytes)->window.borderSize = 1.0f;
    ((itemDef_t *)itemBytes)->window.foreColor[3] = 1.0f;
    ((itemDef_t *)itemBytes)->window.foreColor[2] = 1.0f;
    ((itemDef_t *)itemBytes)->window.foreColor[1] = 1.0f;
    ((itemDef_t *)itemBytes)->window.foreColor[0] = 1.0f;
    ((itemDef_t *)itemBytes)->window.cinematic = -1;

    if (!Item_Parse(handle, (const char (*)[4])itemDef))
        return 0;

    Item_InitControls((const char (*)[4])itemDef);

    *(const char (**)[4])(itemBytes + 0x29c) = item;
    ((menuDef_t *)menuBytes)->itemCount = itemCount + 1;
    return 1;
}

void Item_SetupKeywordHash(void)
{
    keywordHash_t *key;
    unsigned int hash;
    int i;

    memset(itemParseKeywordHash, 0, sizeof(itemParseKeywordHash));
    for (i = 0; itemParseKeywords[i].keyword; i++) {
        key = &itemParseKeywords[i];
        hash = UI_KeywordHashKey(key->keyword);
        key->next = (int)itemParseKeywordHash[hash];
        itemParseKeywordHash[hash] = key;
    }
}

void Menu_SetupKeywordHash(void)
{
    keywordHash_t *key;
    unsigned int hash;
    int i;

    memset(menuParseKeywordHash, 0, sizeof(menuParseKeywordHash));
    for (i = 0; menuParseKeywords[i].keyword; i++) {
        key = &menuParseKeywords[i];
        hash = UI_KeywordHashKey(key->keyword);
        key->next = (int)menuParseKeywordHash[hash];
        menuParseKeywordHash[hash] = key;
    }
}

qboolean MenuParse_name(menuDef_t *menu, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    menu->window.name = String_Alloc(token + 0x10);
    return 1;
}

qboolean MenuParse_background(const char (*item)[4], int handle)
{
    char token[0x410];
    char name[0x40];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    I_strncpyz(name, String_Alloc(token + 0x10), 0x40);
    I_strlwr(name);
    (*(void **)&((itemDef_t *)item)->window.background) = CL_RegisterMaterialNoMip(name, (*(int *)&((itemDef_t *)item)->textRect[3].x));
    return 1;
}

qboolean MenuParse_cinematic(menuDef_t *menu, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    menu->window.cinematicName = String_Alloc(token + 0x10);
    return 1;
}

qboolean ItemParse_name(itemDef_t *item, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    item->window.name = String_Alloc(token + 0x10);
    return 1;
}

qboolean ItemParse_focusSound(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    pc_token_t token;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    itemDef->focusSound = Com_FindSoundAlias(String_Alloc(token.string));
    return 1;
}

qboolean ItemParse_text(itemDef_t *item, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    item->text = String_Alloc(token + 0x10);
    return 1;
}

qboolean ItemParse_group(itemDef_t *item, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    item->window.group = String_Alloc(token + 0x10);
    return 1;
}

qboolean ItemParse_background(const char (*item)[4], int handle)
{
    char token[0x410];
    char name[0x40];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    I_strncpyz(name, String_Alloc(token + 0x10), 0x40);
    I_strlwr(name);
    (*(void **)&((itemDef_t *)item)->window.background) = CL_RegisterMaterialNoMip(name, ((itemDef_t *)item)->imageTrack);
    return 1;
}

qboolean ItemParse_cinematic(itemDef_t *item, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    item->window.cinematicName = String_Alloc(token + 0x10);
    return 1;
}

qboolean ItemParse_dvarTest(itemDef_t *item, int handle)
{
    char token[0x410];
    if (!PC_ReadTokenHandle(handle, token))
        return 0;
    item->dvarTest = String_Alloc(token + 0x10);
    return 1;
}

qboolean ItemParse_dvar(const char (*item)[4], int handle)
{
    itemDef_t *itemDef;
    pc_token_t token;

    itemDef = (itemDef_t *)item;
    if (!PC_ReadTokenHandle(handle, &token)) {
        return 0;
    }

    itemDef->dvar = String_Alloc(token.string);
    Item_InitDvarEditField(itemDef);
    return 1;
}

qboolean ItemParse_dvarEnumList(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    pc_token_t token;

    Item_ValidateTypeData(itemDef, handle);
    if (itemDef->type != 13)
        return 0;

    if (itemDef->typeData.data) {
        PC_SourceError(handle, "enumDvarList already given");
        return 0;
    }

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    itemDef->typeData.data = (void *)String_Alloc(token.string);
    return 1;
}

qboolean ItemParse_outlinecolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->window.outlineColor[0], 4);
}

qboolean MenuParse_outlinecolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->window.outlineColor[0], 4);
}

qboolean PC_Rect_Parse(int handle, rectDef_t *r)
{
    if (!UI_ParseFloatToken(handle, &r->x))
        return 0;
    if (!UI_ParseFloatToken(handle, &r->y))
        return 0;
    if (!UI_ParseFloatToken(handle, &r->w))
        return 0;
    if (!UI_ParseFloatToken(handle, &r->h))
        return 0;

    UI_ParseOptionalIntLine(handle, &r->horzAlign);
    UI_ParseOptionalIntLine(handle, &r->vertAlign);
    return 1;
}

qboolean ItemParse_rect(const char (*item)[4], int handle)
{
    rectDef_t rectClient;

    if (!PC_Rect_Parse(handle, &rectClient))
        return 0;

    Window_SetRectClient((Window *)item, &rectClient);
    return 1;
}

qboolean MenuParse_rect(const char (*item)[4], int handle)
{
    rectDef_t rect;

    if (!PC_Rect_Parse(handle, &rect))
        return 0;

    Window_SetRect((menuDef_t *)item, &rect);
    return 1;
}

qboolean MenuParse_borderSize(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->window.borderSize);
}

qboolean MenuParse_backcolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->window.backColor[0], 4);
}

qboolean MenuParse_forecolor(const char (*item)[4], int handle)
{
    float *color = (float *)&((itemDef_t *)item)->window.foreColor[0];
    int i;

    for (i = 0; i < 4; i++) {
        if (!UI_ParseFloatToken(handle, &color[i]))
            return 0;
        Window_SetDynamicFlags((Window *)item, ((itemDef_t *)item)->window.dynamicFlags[0] | 0x10000);
    }

    return 1;
}

qboolean MenuParse_bordercolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->window.borderColor[0], 4);
}

qboolean MenuParse_focuscolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->textRect[3].y, 4);
}

qboolean MenuParse_disablecolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->textRect[3].vertAlign, 4);
}

qboolean MenuParse_fadeClamp(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->textRect[1].h);
}

qboolean MenuParse_fadeAmount(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->textRect[1].horzAlign);
}

qboolean MenuParse_fadeInAmount(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->textRect[1].vertAlign);
}

qboolean MenuParse_blurWorld(const char (*item)[4], int handle)
{
    float *blurRadius = (float *)&((itemDef_t *)item)->textRect[2].x;

    if (!UI_ParseFloatToken(handle, blurRadius))
        return 0;

    if (*blurRadius < 0.0f) {
        PC_SourceError(handle, "blur must be >= 0; %g is invalid", *blurRadius);
        return 0;
    }

    return 1;
}

qboolean ItemParse_elementwidth(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    Item_ValidateTypeData(itemDef, handle);
    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;

    return UI_ParseFloatToken(handle, &listPtr->elementWidth);
}

qboolean ItemParse_elementheight(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    Item_ValidateTypeData(itemDef, handle);
    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;

    return UI_ParseFloatToken(handle, &listPtr->elementHeight);
}

qboolean ItemParse_feeder(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->special);
}

qboolean ItemParse_bordersize(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->window.borderSize);
}

qboolean ItemParse_textalignx(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->textalignx);
}

qboolean ItemParse_textaligny(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->textaligny);
}

qboolean ItemParse_textscale(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->textscale);
}

qboolean ItemParse_backcolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->window.backColor[0], 4);
}

qboolean ItemParse_forecolor(const char (*item)[4], int handle)
{
    float *color = (float *)&((itemDef_t *)item)->window.foreColor[0];
    int i;

    for (i = 0; i < 4; i++) {
        if (!UI_ParseFloatToken(handle, &color[i]))
            return 0;
        Window_SetDynamicFlags((Window *)item, ((itemDef_t *)item)->window.dynamicFlags[0] | 0x10000);
    }

    return 1;
}

qboolean ItemParse_bordercolor(const char (*item)[4], int handle)
{
    return UI_ParseFloatArray(handle, (float *)&((itemDef_t *)item)->window.borderColor[0], 4);
}

qboolean ItemParse_special(const char (*item)[4], int handle)
{
    return UI_ParseFloatToken(handle, (float *)&((itemDef_t *)item)->special);
}

qboolean ItemParse_dvarFloat(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    editFieldDef_t *editPtr;
    pc_token_t token;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;

    editPtr = Item_GetEditFieldDef(itemDef);
    if (!editPtr)
        return 0;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    itemDef->dvar = String_Alloc(token.string);

    if (!UI_ParseFloatToken(handle, &editPtr->defVal))
        return 0;
    if (!UI_ParseFloatToken(handle, &editPtr->minVal))
        return 0;
    if (!UI_ParseFloatToken(handle, &editPtr->maxVal))
        return 0;

    return 1;
}

qboolean ItemParse_dvarFloatList(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    multiDef_t *multiPtr;
    pc_token_t token;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data || itemDef->type != 12)
        return 0;

    multiPtr = Item_GetMultiDef(itemDef);
    multiPtr->count = 0;
    multiPtr->strDef = 0;

    if (!PC_ReadTokenHandle(handle, &token) || token.string[0] != '{')
        return 0;

    for (;;) {
        if (!PC_ReadTokenHandle(handle, &token)) {
            PC_SourceError(handle, "end of file inside menu item\n");
            return 0;
        }

        if (token.string[0] == '}')
            return 1;
        if (token.string[0] == ',' || token.string[0] == ';')
            continue;

        multiPtr->dvarList[multiPtr->count] = String_Alloc(token.string);
        if (!UI_ParseFloatToken(handle, &multiPtr->dvarValue[multiPtr->count]))
            return 0;

        multiPtr->count++;
        if (multiPtr->count > 31)
            return 0;
    }
}

qboolean MenuParse_fullscreen(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->textRect[0].y);
}

qboolean MenuParse_style(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->window.style);
}

qboolean MenuParse_visible(const char (*item)[4], int handle)
{
    int visible;

    if (!UI_ParseIntToken(handle, &visible))
        return 0;

    if (visible)
        Window_SetDynamicFlags((Window *)item, ((itemDef_t *)item)->window.dynamicFlags[0] | 4);

    return 1;
}

qboolean MenuParse_border(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->window.border);
}

qboolean MenuParse_ownerdrawFlag(const char (*item)[4], int handle)
{
    int flags;

    if (!UI_ParseIntToken(handle, &flags))
        return 0;

    ((itemDef_t *)item)->window.ownerDrawFlags |= flags;
    return 1;
}

qboolean MenuParse_ownerdraw(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->window.ownerDraw);
}

qboolean MenuParse_fadeCycle(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->textRect[1].w);
}

qboolean MenuParse_execKeyInt(const char (*item)[4], int handle)
{
    menuDef_t *menu = (menuDef_t *)item;
    const char *action;
    ItemKeyHandler *handler;
    int keyname;

    if (!UI_ParseIntToken(handle, &keyname))
        return 0;
    if (!PC_Script_Parse(handle, &action))
        return 0;

    handler = (ItemKeyHandler *)UI_Alloc(sizeof(ItemKeyHandler), 4);
    handler->key = keyname;
    handler->action = action;
    handler->next = (int)menu->onKey;
    menu->onKey = handler;
    return 1;
}

qboolean ItemParse_origin(const char (*item)[4], int handle)
{
    UiRectangle newRect;
    int x;
    int y;

    if (!UI_ParseIntToken(handle, &x))
        return 0;
    if (!UI_ParseIntToken(handle, &y))
        return 0;

    newRect = ((Window *)item)->rectClient[0];
    newRect.x += (float)x;
    newRect.y += (float)y;
    Window_SetRectClient((Window *)item, &newRect);
    return 1;
}

qboolean ItemParse_style(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->window.style);
}

qboolean ItemParse_type(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;

    if (!UI_ParseIntToken(handle, &itemDef->type))
        return 0;

    Item_ValidateTypeData(itemDef, handle);
    Item_InitDvarEditField(itemDef);
    return 1;
}

qboolean ItemParse_elementtype(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;

    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;

    return UI_ParseIntToken(handle, &listPtr->elementStyle);
}

qboolean ItemParse_columns(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    listBoxDef_t *listPtr;
    int num;
    int i;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;

    listPtr = Item_GetListBoxDef(itemDef);
    if (!listPtr)
        return 0;

    if (!UI_ParseIntToken(handle, &num))
        return 0;

    if (num > 16)
        num = 16;
    listPtr->numColumns = num;

    for (i = 0; i < num; i++) {
        if (!UI_ParseIntToken(handle, &listPtr->columnInfo[i].pos))
            return 0;
        if (!UI_ParseIntToken(handle, &listPtr->columnInfo[i].width))
            return 0;
        if (!UI_ParseIntToken(handle, &listPtr->columnInfo[i].maxChars))
            return 0;
    }

    return 1;
}

qboolean ItemParse_border(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->window.border);
}

qboolean ItemParse_visible(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    int visible;

    if (!UI_ParseIntToken(handle, &visible))
        return 0;

    if (!visible)
        return 1;

    if (itemDef->text && strcmp(itemDef->text, "@MENU_SHADOWS") == 0)
        return 1;
    if (itemDef->dvar && strcmp(itemDef->dvar, "ui_sc_enable") == 0)
        return 1;

    Window_SetDynamicFlags((Window *)item, ((itemDef_t *)item)->window.dynamicFlags[0] | 4);
    return 1;
}

qboolean ItemParse_ownerdraw(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;

    if (!UI_ParseIntToken(handle, &itemDef->window.ownerDraw))
        return 0;

    itemDef->type = 8;
    return 1;
}

qboolean ItemParse_align(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->alignment);
}

qboolean ItemParse_textalign(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->textalignment);
}

qboolean ItemParse_textstyle(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->textStyle);
}

qboolean ItemParse_textfont(const char (*item)[4], int handle)
{
    return UI_ParseIntToken(handle, (int *)&((itemDef_t *)item)->fontEnum);
}

qboolean ItemParse_maxChars(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    editFieldDef_t *editDef;
    int maxChars;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;
    if (!UI_ParseIntToken(handle, &maxChars))
        return 0;

    editDef = Item_GetEditFieldDef(itemDef);
    if (!editDef)
        return 0;

    editDef->maxChars = maxChars;
    return 1;
}

qboolean ItemParse_maxPaintChars(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    editFieldDef_t *editDef;
    int maxPaintChars;

    Item_ValidateTypeData(itemDef, handle);
    if (!itemDef->typeData.data)
        return 0;
    if (!UI_ParseIntToken(handle, &maxPaintChars))
        return 0;

    editDef = Item_GetEditFieldDef(itemDef);
    if (!editDef)
        return 0;

    editDef->maxPaintChars = maxPaintChars;
    return 1;
}

qboolean ItemParse_ownerdrawFlag(const char (*item)[4], int handle)
{
    int flags;

    if (!UI_ParseIntToken(handle, &flags))
        return 0;

    ((itemDef_t *)item)->window.ownerDrawFlags |= flags;
    return 1;
}

qboolean ItemParse_execKeyInt(const char (*item)[4], int handle)
{
    itemDef_t *itemDef = (itemDef_t *)item;
    const char *action;
    ItemKeyHandler *handler;
    int keyname;

    if (!UI_ParseIntToken(handle, &keyname))
        return 0;
    if (!PC_Script_Parse(handle, &action))
        return 0;

    handler = (ItemKeyHandler *)UI_Alloc(sizeof(ItemKeyHandler), 4);
    handler->key = keyname;
    handler->action = action;
    handler->next = (int)itemDef->onKey;
    itemDef->onKey = handler;
    return 1;
}

static qboolean UI_ParseFontAsset(int handle, int imageTrack, const char *duplicateName)
{
    pc_token_t token;
    int pointSize;
    const char *fontName;
    FontHandle font;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;

    fontName = String_Alloc(token.string);
    if (!UI_ParseIntToken(handle, &pointSize))
        return 0;

    (void)pointSize;
    font = CL_RegisterFont(fontName, imageTrack);
    CL_DuplicateFont(font, duplicateName);
    return 1;
}

static qboolean UI_ParseAssetGlobalDef(int handle, int imageTrack)
{
    loadAssets_t *load = UI_LoadAssets();
    pc_token_t token;
    snd_alias_list_t *alias;
    MaterialHandle material;

    if (!PC_ReadTokenHandle(handle, &token))
        return 0;
    if (I_stricmp(token.string, "{"))
        return 0;

    while (PC_ReadTokenHandle(handle, &token)) {
        if (!I_stricmp(token.string, "}"))
            return 1;

        if (!I_stricmp(token.string, "font")) {
            if (!UI_ParseFontAsset(handle, imageTrack, "$font"))
                return 0;
        } else if (!I_stricmp(token.string, "smallFont")) {
            if (!UI_ParseFontAsset(handle, imageTrack, "$smallfont"))
                return 0;
        } else if (!I_stricmp(token.string, "bigFont")) {
            if (!UI_ParseFontAsset(handle, imageTrack, "$bigfont"))
                return 0;
        } else if (!I_stricmp(token.string, "extraBigFont")) {
            if (!UI_ParseFontAsset(handle, imageTrack, "$extrabigfont"))
                return 0;
        } else if (!I_stricmp(token.string, "boldFont")) {
            if (!UI_ParseFontAsset(handle, imageTrack, "$boldfont"))
                return 0;
        } else if (!I_stricmp(token.string, "consoleFont")) {
            if (!UI_ParseFontAsset(handle, imageTrack, "$consolefont"))
                return 0;
        } else if (!I_stricmp(token.string, "gradientbar")) {
            if (!PC_ReadTokenHandle(handle, &token))
                return 0;
            material = (MaterialHandle)CL_RegisterMaterialNoMip(String_Alloc(token.string), imageTrack);
            CL_Material_Duplicate(material, "$gradientbar");
        } else if (!I_stricmp(token.string, "menuEnterSound")) {
            if (!PC_ReadTokenHandle(handle, &token))
                return 0;
            load->menuEnterSound = Com_FindSoundAlias(String_Alloc(token.string));
        } else if (!I_stricmp(token.string, "menuExitSound")) {
            if (!PC_ReadTokenHandle(handle, &token))
                return 0;
            load->menuExitSound = Com_FindSoundAlias(String_Alloc(token.string));
        } else if (!I_stricmp(token.string, "itemFocusSound")) {
            if (!PC_ReadTokenHandle(handle, &token))
                return 0;
            alias = Com_FindSoundAlias(String_Alloc(token.string));
            if (alias)
                Com_DuplicateSoundAlias(alias, "$itemfocussound");
        } else if (!I_stricmp(token.string, "menuBuzzSound")) {
            if (!PC_ReadTokenHandle(handle, &token))
                return 0;
            load->menuBuzzSound = Com_FindSoundAlias(String_Alloc(token.string));
        } else if (!I_stricmp(token.string, "cursor")) {
            if (!PC_ReadTokenHandle(handle, &token))
                return 0;
            material = (MaterialHandle)CL_RegisterMaterialNoMip(String_Alloc(token.string), imageTrack);
            CL_Material_Duplicate(material, "$cursor");
        } else if (!I_stricmp(token.string, "fadeClamp")) {
            if (!UI_ParseFloatToken(handle, &load->fadeClamp))
                return 0;
        } else if (!I_stricmp(token.string, "fadeCycle")) {
            if (!UI_ParseIntToken(handle, &load->fadeCycle))
                return 0;
        } else if (!I_stricmp(token.string, "fadeAmount")) {
            if (!UI_ParseFloatToken(handle, &load->fadeAmount))
                return 0;
        } else if (!I_stricmp(token.string, "fadeInAmount")) {
            if (!UI_ParseFloatToken(handle, &load->fadeInAmount))
                return 0;
        } else if (!I_stricmp(token.string, "shadowX")) {
            if (!UI_ParseFloatToken(handle, &load->shadowX))
                return 0;
        } else if (!I_stricmp(token.string, "shadowY")) {
            if (!UI_ParseFloatToken(handle, &load->shadowY))
                return 0;
        } else if (!I_stricmp(token.string, "shadowColor")) {
            if (!UI_ParseFloatArray(handle, load->shadowColor, 4))
                return 0;
            load->shadowFadeClamp = load->shadowColor[3];
        }
    }

    return 0;
}

static Bool __attribute_regparm__(2) UI_ParseMenuInternal(const char *menuFile, int imageTrack)
{
    int handle;
    pc_token_t token;

    Com_Printf("\tLoading '%s'...\n", menuFile);

    handle = PC_LoadSourceHandle(menuFile);
    if (!handle)
        return 0;

    while (PC_ReadTokenHandle(handle, &token) && token.string[0] != '}') {
        if (!I_stricmp(token.string, "assetGlobalDef")) {
            if (!UI_ParseAssetGlobalDef(handle, imageTrack))
                break;
        } else if (!I_stricmp(token.string, "menudef")) {
            if (!Menu_New(handle, imageTrack))
                break;
        }
    }

    PC_FreeSourceHandle(handle);
    return 1;
}

MenuList *UI_LoadMenus(const char *menuFile, int imageTrack)
{
    const char *p;
    const char *token;
    fileHandle_t f;
    int len;

    memset(g_load, 0, sizeof(g_load));
    UI_MenuList()->menus = UI_MenuStorage();

    len = FS_FOpenFileByMode(menuFile, &f, FS_READ);
    if (!f) {
        Com_Printf("^3WARNING: menu file not found: %s\n", menuFile);
        len = FS_FOpenFileByMode("ui/default.menu", &f, FS_READ);
        if (!f)
            Com_Error(ERR_DROP, "default menu file not found");
    }

    if (len > 0x7fff) {
        FS_FCloseFile(f);
        Com_Error(ERR_DROP, "^1menu file too large: %s is %i, max allowed is %i",
                  menuFile, len, 0x8000);
    }

    FS_Read(menuBuf, len, f);
    menuBuf[len] = '\0';
    FS_FCloseFile(f);
    Com_Compress(menuBuf);

    p = menuBuf;
    Com_BeginParseSession(menuFile);

    for (;;) {
        token = Com_Parse(&p);
        if (token == NULL || token[0] == '\0' || token[0] == '}')
            break;

        if (I_stricmp(token, "loadmenu") != 0)
            continue;

        token = Com_Parse(&p);
        if (token == NULL || token[0] != '{')
            break;

        for (;;) {
            token = Com_Parse(&p);
            if (token == NULL || token[0] == '\0')
                goto done;
            if (I_stricmp(token, "}") == 0)
                break;

            UI_ParseMenuInternal(token, imageTrack);
        }
    }

done:
    Com_EndParseSession();
    return UI_MenuList();
}

MenuList *UI_LoadMenu(const char *menuFile, int imageTrack)
{
    memset(g_load, 0, sizeof(g_load));
    UI_MenuList()->menus = UI_MenuStorage();

    if (!UI_ParseMenuInternal(menuFile, imageTrack)) {
        Com_Printf("^3WARNING: menu file not found: %s\n", menuFile);
        if (!UI_ParseMenuInternal("ui/default.menu", imageTrack))
            Com_Error(ERR_DROP, "default menu file not found");
    }

    return UI_MenuList();
}

keywordHash_t itemParseKeywords[67] = {
    { (char *)&str_002194b4, &ItemParse_name, 0x0 },
    { (char *)&str_002ace30, &ItemParse_text, 0x0 },
    { (char *)&str_002b3cb8, &ItemParse_textfile, 0x0 },
    { (char *)&str_002b3cc4, &ItemParse_textsavegame, 0x0 },
    { (char *)&str_002b3cd4, &ItemParse_group, 0x0 },
    { (char *)&str_002b3cdc, &ItemParse_rect, 0x0 },
    { (char *)&str_0021a570, &ItemParse_origin, 0x0 },
    { (char *)&str_002b3ce4, &ItemParse_style, 0x0 },
    { (char *)&str_002b3cec, &ItemParse_decoration, 0x0 },
    { (char *)&str_002b3cf8, &ItemParse_notselectable, 0x0 },
    { (char *)&str_002b3d08, &ItemParse_noScrollBars, 0x0 },
    { (char *)&str_002b3d18, &ItemParse_wrapped, 0x0 },
    { (char *)&str_002b3d20, &ItemParse_autowrapped, 0x0 },
    { (char *)&str_002b3d2c, &ItemParse_horizontalscroll, 0x0 },
    { (char *)&str_0021eb5c, &ItemParse_type, 0x0 },
    { (char *)&str_002b3d40, &ItemParse_elementwidth, 0x0 },
    { (char *)&str_002b3d50, &ItemParse_elementheight, 0x0 },
    { (char *)&str_002b3d60, &ItemParse_feeder, 0x0 },
    { (char *)&str_002b3d68, &ItemParse_elementtype, 0x0 },
    { (char *)&str_002b3d74, &ItemParse_columns, 0x0 },
    { (char *)&str_002b3d7c, &ItemParse_border, 0x0 },
    { (char *)&str_002b3d84, &ItemParse_bordersize, 0x0 },
    { (char *)&str_002b3d90, &ItemParse_visible, 0x0 },
    { (char *)&str_002b3d98, &ItemParse_ownerdraw, 0x0 },
    { (char *)&str_002b3da4, &ItemParse_align, 0x0 },
    { (char *)&str_002b3dac, &ItemParse_textalign, 0x0 },
    { (char *)&str_002b3db8, &ItemParse_textalignx, 0x0 },
    { (char *)&str_002b3dc4, &ItemParse_textaligny, 0x0 },
    { (char *)&str_002b3dd0, &ItemParse_textscale, 0x0 },
    { (char *)&str_002b3ddc, &ItemParse_textstyle, 0x0 },
    { (char *)&str_002b3de8, &ItemParse_textfont, 0x0 },
    { (char *)&str_002ac220, &ItemParse_backcolor, 0x0 },
    { (char *)&str_002ac22c, &ItemParse_forecolor, 0x0 },
    { (char *)&str_002ac238, &ItemParse_bordercolor, 0x0 },
    { (char *)&str_002b3df4, &ItemParse_outlinecolor, 0x0 },
    { (char *)&str_002b3e04, &ItemParse_background, 0x0 },
    { (char *)&str_002b3e10, &ItemParse_onFocus, 0x0 },
    { (char *)&str_002b3e18, &ItemParse_leaveFocus, 0x0 },
    { (char *)&str_002b3e24, &ItemParse_mouseEnter, 0x0 },
    { (char *)&str_002b3e30, &ItemParse_mouseExit, 0x0 },
    { (char *)&str_002b3e3c, &ItemParse_mouseEnterText, 0x0 },
    { (char *)&str_002b3e4c, &ItemParse_mouseExitText, 0x0 },
    { (char *)&str_002b3e5c, &ItemParse_action, 0x0 },
    { (char *)&str_002ae014, &ItemParse_accept, 0x0 },
    { (char *)&str_002b3e64, &ItemParse_special, 0x0 },
    { (char *)&str_002b3e6c, &ItemParse_dvar, 0x0 },
    { (char *)&str_002b3e74, &ItemParse_maxChars, 0x0 },
    { (char *)&str_002b3e80, &Item_Parse_maxCharsGotoNext, 0x0 },
    { (char *)&str_002b3e94, &ItemParse_maxPaintChars, 0x0 },
    { (char *)&str_002b3ea4, &ItemParse_focusSound, 0x0 },
    { (char *)&str_002b3eb0, &ItemParse_dvarFloat, 0x0 },
    { (char *)&str_002b3ebc, &ItemParse_dvarStrList, 0x0 },
    { (char *)&str_002b3ec8, &ItemParse_dvarFloatList, 0x0 },
    { (char *)&str_002b3ed8, &ItemParse_dvarEnumList, 0x0 },
    { (char *)&str_002b3ee8, &ItemParse_ownerdrawFlag, 0x0 },
    { (char *)&str_002b3ef8, &ItemParse_enableDvar, 0x0 },
    { (char *)&str_002b3f04, &ItemParse_dvarTest, 0x0 },
    { (char *)&str_002b3f10, &ItemParse_disableDvar, 0x0 },
    { (char *)&str_002b3f1c, &ItemParse_showDvar, 0x0 },
    { (char *)&str_002b3f28, &ItemParse_hideDvar, 0x0 },
    { (char *)&str_002b3f34, &ItemParse_focusDvar, 0x0 },
    { (char *)&str_002a90a4, &ItemParse_cinematic, 0x0 },
    { (char *)&str_002b3f40, &ItemParse_doubleClick, 0x0 },
    { (char *)&str_002b3f4c, &ItemParse_execKey, 0x0 },
    { (char *)&str_002b3f54, &ItemParse_execKeyInt, 0x0 },
    { 0, 0, 0x0 },
    { 0 }
};
keywordHash_t menuParseKeywords[35] = {
    { (char *)&str_002194b4, &MenuParse_name, 0x0 },
    { (char *)&str_002b3f60, &MenuParse_fullscreen, 0x0 },
    { (char *)&str_002b3cdc, &MenuParse_rect, 0x0 },
    { (char *)&str_002b3ce4, &MenuParse_style, 0x0 },
    { (char *)&str_002b3d90, &MenuParse_visible, 0x0 },
    { (char *)&str_002b3f6c, &MenuParse_onOpen, 0x0 },
    { (char *)&str_002b3f74, &MenuParse_onClose, 0x0 },
    { (char *)&str_002b3f7c, &MenuParse_onESC, 0x0 },
    { (char *)&str_002b3d7c, &MenuParse_border, 0x0 },
    { (char *)&str_002b3f84, &MenuParse_borderSize, 0x0 },
    { (char *)&str_002ac220, &MenuParse_backcolor, 0x0 },
    { (char *)&str_002ac22c, &MenuParse_forecolor, 0x0 },
    { (char *)&str_002ac238, &MenuParse_bordercolor, 0x0 },
    { (char *)&str_002b3f90, &MenuParse_focuscolor, 0x0 },
    { (char *)&str_002b3f9c, &MenuParse_disablecolor, 0x0 },
    { (char *)&str_002b3df4, &MenuParse_outlinecolor, 0x0 },
    { (char *)&str_002b3e04, &MenuParse_background, 0x0 },
    { (char *)&str_002b3d98, &MenuParse_ownerdraw, 0x0 },
    { (char *)&str_002b3ee8, &MenuParse_ownerdrawFlag, 0x0 },
    { (char *)&str_002b3fac, &MenuParse_outOfBounds, 0x0 },
    { (char *)&str_002b3fc0, &MenuParse_soundLoop, 0x0 },
    { (char *)&str_002b3fcc, &MenuParse_itemDef, 0x0 },
    { (char *)&str_002a90a4, &MenuParse_cinematic, 0x0 },
    { (char *)&str_002b3fd4, &MenuParse_popup, 0x0 },
    { (char *)&str_002b3fdc, &MenuParse_fadeClamp, 0x0 },
    { (char *)&str_002b3fe8, &MenuParse_fadeCycle, 0x0 },
    { (char *)&str_002b3ff4, &MenuParse_fadeAmount, 0x0 },
    { (char *)&str_002b4000, &MenuParse_fadeInAmount, 0x0 },
    { (char *)&str_002b3f4c, &MenuParse_execKey, 0x0 },
    { (char *)&str_002b3f54, &MenuParse_execKeyInt, 0x0 },
    { (char *)&str_002b4010, &MenuParse_blurWorld, 0x0 },
    { (char *)&str_002b401c, &MenuParse_legacySplitScreenScale, 0x0 },
    { 0, 0, 0x0 },
    { 0, 0, 0x0 },
    { 0 }
};

const char _s_str_002194b4[] __asm__("str_002194b4") = "name";
const char _s_str_002a90a4[] __asm__("str_002a90a4") = "cinematic";
const char _s_str_002ac220[] __asm__("str_002ac220") = "backcolor";
const char _s_str_002ac22c[] __asm__("str_002ac22c") = "forecolor";
const char _s_str_002ac238[] __asm__("str_002ac238") = "bordercolor";
const char _s_str_002ae014[] __asm__("str_002ae014") = "accept";
const char _s_str_002b3cb8[] __asm__("str_002b3cb8") = "textfile";
const char _s_str_002b3cc4[] __asm__("str_002b3cc4") = "textsavegame";
const char _s_str_002b3cd4[] __asm__("str_002b3cd4") = "group";
const char _s_str_002b3cdc[] __asm__("str_002b3cdc") = "rect";
const char _s_str_002b3ce4[] __asm__("str_002b3ce4") = "style";
const char _s_str_002b3cec[] __asm__("str_002b3cec") = "decoration";
const char _s_str_002b3cf8[] __asm__("str_002b3cf8") = "notselectable";
const char _s_str_002b3d08[] __asm__("str_002b3d08") = "noscrollbars";
const char _s_str_002b3d18[] __asm__("str_002b3d18") = "wrapped";
const char _s_str_002b3d20[] __asm__("str_002b3d20") = "autowrapped";
const char _s_str_002b3d2c[] __asm__("str_002b3d2c") = "horizontalscroll";
const char _s_str_002b3d40[] __asm__("str_002b3d40") = "elementwidth";
const char _s_str_002b3d50[] __asm__("str_002b3d50") = "elementheight";
const char _s_str_002b3d60[] __asm__("str_002b3d60") = "feeder";
const char _s_str_002b3d68[] __asm__("str_002b3d68") = "elementtype";
const char _s_str_002b3d74[] __asm__("str_002b3d74") = "columns";
const char _s_str_002b3d7c[] __asm__("str_002b3d7c") = "border";
const char _s_str_002b3d84[] __asm__("str_002b3d84") = "bordersize";
const char _s_str_002b3d90[] __asm__("str_002b3d90") = "visible";
const char _s_str_002b3d98[] __asm__("str_002b3d98") = "ownerdraw";
const char _s_str_002b3da4[] __asm__("str_002b3da4") = "align";
const char _s_str_002b3dac[] __asm__("str_002b3dac") = "textalign";
const char _s_str_002b3db8[] __asm__("str_002b3db8") = "textalignx";
const char _s_str_002b3dc4[] __asm__("str_002b3dc4") = "textaligny";
const char _s_str_002b3dd0[] __asm__("str_002b3dd0") = "textscale";
const char _s_str_002b3ddc[] __asm__("str_002b3ddc") = "textstyle";
const char _s_str_002b3de8[] __asm__("str_002b3de8") = "textfont";
const char _s_str_002b3df4[] __asm__("str_002b3df4") = "outlinecolor";
const char _s_str_002b3e04[] __asm__("str_002b3e04") = "background";
const char _s_str_002b3e10[] __asm__("str_002b3e10") = "onFocus";
const char _s_str_002b3e18[] __asm__("str_002b3e18") = "leaveFocus";
const char _s_str_002b3e24[] __asm__("str_002b3e24") = "mouseEnter";
const char _s_str_002b3e30[] __asm__("str_002b3e30") = "mouseExit";
const char _s_str_002b3e3c[] __asm__("str_002b3e3c") = "mouseEnterText";
const char _s_str_002b3e4c[] __asm__("str_002b3e4c") = "mouseExitText";
const char _s_str_002b3e5c[] __asm__("str_002b3e5c") = "action";
const char _s_str_002b3e64[] __asm__("str_002b3e64") = "special";
const char _s_str_002b3e6c[] __asm__("str_002b3e6c") = "dvar";
const char _s_str_002b3e74[] __asm__("str_002b3e74") = "maxChars";
const char _s_str_002b3e80[] __asm__("str_002b3e80") = "maxCharsGotoNext";
const char _s_str_002b3e94[] __asm__("str_002b3e94") = "maxPaintChars";
const char _s_str_002b3ea4[] __asm__("str_002b3ea4") = "focusSound";
const char _s_str_002b3eb0[] __asm__("str_002b3eb0") = "dvarFloat";
const char _s_str_002b3ebc[] __asm__("str_002b3ebc") = "dvarStrList";
const char _s_str_002b3ec8[] __asm__("str_002b3ec8") = "dvarFloatList";
const char _s_str_002b3ed8[] __asm__("str_002b3ed8") = "dvarEnumList";
const char _s_str_002b3ee8[] __asm__("str_002b3ee8") = "ownerdrawFlag";
const char _s_str_002b3ef8[] __asm__("str_002b3ef8") = "enableDvar";
const char _s_str_002b3f04[] __asm__("str_002b3f04") = "dvarTest";
const char _s_str_002b3f10[] __asm__("str_002b3f10") = "disableDvar";
const char _s_str_002b3f1c[] __asm__("str_002b3f1c") = "showDvar";
const char _s_str_002b3f28[] __asm__("str_002b3f28") = "hideDvar";
const char _s_str_002b3f34[] __asm__("str_002b3f34") = "focusDvar";
const char _s_str_002b3f40[] __asm__("str_002b3f40") = "doubleclick";
const char _s_str_002b3f4c[] __asm__("str_002b3f4c") = "execKey";
const char _s_str_002b3f54[] __asm__("str_002b3f54") = "execKeyInt";
const char _s_str_002b3f60[] __asm__("str_002b3f60") = "fullscreen";
const char _s_str_002b3f6c[] __asm__("str_002b3f6c") = "onOpen";
const char _s_str_002b3f74[] __asm__("str_002b3f74") = "onClose";
const char _s_str_002b3f7c[] __asm__("str_002b3f7c") = "onESC";
const char _s_str_002b3f84[] __asm__("str_002b3f84") = "borderSize";
const char _s_str_002b3f90[] __asm__("str_002b3f90") = "focuscolor";
const char _s_str_002b3f9c[] __asm__("str_002b3f9c") = "disablecolor";
const char _s_str_002b3fac[] __asm__("str_002b3fac") = "outOfBoundsClick";
const char _s_str_002b3fc0[] __asm__("str_002b3fc0") = "soundLoop";
const char _s_str_002b3fcc[] __asm__("str_002b3fcc") = "itemDef";
const char _s_str_002b3fd4[] __asm__("str_002b3fd4") = "popup";
const char _s_str_002b3fdc[] __asm__("str_002b3fdc") = "fadeClamp";
const char _s_str_002b3fe8[] __asm__("str_002b3fe8") = "fadeCycle";
const char _s_str_002b3ff4[] __asm__("str_002b3ff4") = "fadeAmount";
const char _s_str_002b4000[] __asm__("str_002b4000") = "fadeInAmount";
const char _s_str_002b4010[] __asm__("str_002b4010") = "blurWorld";
const char _s_str_002b401c[] __asm__("str_002b401c") = "legacySplitScreenScale";
