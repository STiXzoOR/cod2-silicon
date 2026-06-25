#include "cod2_feature_config.h"

#if COD2_FEATURE_SCRIPT_DEBUGGER

#    define UI_Component CT_UI_Component
#    define UI_LinesComponent CT_UI_LinesComponent
#    define VariableUnion CT_VariableUnion
#    define VariableValue_s CT_VariableValue_s
#    include "common_types.h"
#    undef UI_Component
#    undef UI_LinesComponent
#    undef VariableUnion
#    undef VariableValue_s

#    include "scr_debugger_ui.h"

#    include <math.h>
#    include <stdlib.h>

extern const UI_Component_vtbl UI_Component_vtable;
extern const UI_LinesComponent_vtbl UI_LinesComponent_vtable;
extern const UI_ScrollPane_vtbl UI_ScrollPane_vtable;
extern const UI_VerticalDivider_vtbl UI_VerticalDivider_vtable;

extern void Com_Printf(const char *fmt, ...);
extern void Com_Error(int code, const char *fmt, ...);
extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern void I_strncpyz(char *dest, const char *src, int destsize);

extern void *imp_re;
extern void *imp_cls;

extern int Sys_IsRemoteDebugServer(void);

extern int Key_IsDown(int key);

extern void Con_ToggleConsole_f(void);

#    define DBG_K_ALT 158
#    define DBG_K_CTRL 159
#    define DBG_K_SHIFT 160
#    define DBG_K_MOUSE1 200
#    define DBG_K_ENTER 13
#    define DBG_K_SPACE 32
#    define DBG_K_UPARROW 152
#    define DBG_K_DOWNARROW 153
#    define DBG_K_MWHEELUP 154
#    define DBG_K_MWHEELDOWN 155
#    define DBG_K_GKEY 103

typedef struct UI_Component_data_s {
    float screenWidth;
    float screenHeight;
    float charWidth;
    float charHeight;
    float scrollBarSize;
    Material *cursor;
    float cursorPos[2];
    int hideCursor;
    Material *filledCircle;
    int consoleReason;
    char findText[128];
} UI_Component_data_s;

UI_Component_data_s UI_Component_g;
UI_Component *UI_Component_selectionComp;

typedef struct UI_ScrollPane_layout {
     const UI_ScrollPane_vtbl *vtbl;
     float size[2];
     UI_Component *selectionParent;
     UI_LinesComponent *comp;
     unsigned char forceHorScoll;
     unsigned char _pad15[3];
     float mouseHeldScale[2];
     float mouseHeldPos[2];
     float mouseHeldCompPos[2];
     unsigned char mouseWasDown[2];
     unsigned char _pad32[2];
} UI_ScrollPane_layout;

typedef struct UI_VerticalDivider_layout {
     const UI_VerticalDivider_vtbl *vtbl;
     float size[2];
     UI_Component *selectionParent;
     UI_ScrollPane *topComp;
     UI_ScrollPane *bottomComp;
     float posY;
} UI_VerticalDivider_layout;

typedef struct UI_Component_layout {
    const UI_Component_vtbl *vtbl;
    float size[2];
    UI_Component *selectionParent;
} UI_Component_layout;

#    define LC_SIZE0(p) ((p)->w)
#    define LC_SIZE1(p) ((p)->h)
#    define LC_SELLINE(p) ((p)->selectedLine)
#    define LC_FOCUS(p) ((p)->hasFocus)
#    define LC_FOCUSUSER(p) ((p)->focusFromMouse)
#    define LC_NUMLINES(p) ((p)->lineCount)
#    define LC_POS0(p) ((p)->scrollTop)
#    define LC_POS1(p) ((p)->_f20)

void UI_Component_DrawPic(float x, float y, float w, float h,
                          const float *color, struct Material *mat)
{
    refexport_t *re = (refexport_t *)imp_re;
    float fx, fy;

    if (!re || !re->DrawStretchPic)
        return;
    fy = (float)floor(y);
    fx = (float)floor(x);
    re->DrawStretchPic(fx, fy, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color,
                       (MaterialHandle)mat);
}

void UI_Component_DrawPicRotate(float x, float y, float w, float h,
                                const float *color, struct Material *mat)
{
    refexport_t *re = (refexport_t *)imp_re;
    float fx, fy;

    if (!re || !re->DrawStretchPicRotate)
        return;
    fx = (float)floor(x);
    fy = (float)floor(y);
    re->DrawStretchPicRotate(fx, fy, w, h, 0.0f, 0.0f, 1.0f, 1.0f, 90.0f,
                             color, (MaterialHandle)mat);
}

void UI_Component_DrawText(float x, float y, int font, const float *color,
                           const char *text)
{
    refexport_t *re = (refexport_t *)imp_re;
    clientStatic_t *cls = (clientStatic_t *)imp_cls;
    float fx, fy;
    int maxChars;

    (void)font;
    if (!re || !cls || !re->DrawText || !text)
        return;

    maxChars = 0x7fffffff;
    fx = (float)floor(x);
    fy = (float)floor(UI_Component_g.charHeight + y);
    re->DrawText(text, maxChars, cls->consoleFont, fx, fy, 1.0f, 1.0f, color, 0);
}

static void UI_PureCall(void)
{
    Com_Error(1, "pure virtual call");
}

void UI_Component_Init(UI_Component *self)
{
    UI_Component_layout *c = (UI_Component_layout *)self;
    c->size[0] = 0.0f;
    c->size[1] = 0.0f;
    c->selectionParent = 0;
}

void UI_Component_Shutdown(UI_Component *self)
{
    (void)self;
}

void UI_Component_dtor(UI_Component *self)
{
    UI_Component_layout *c = (UI_Component_layout *)self;
    c->vtbl = (const UI_Component_vtbl *)&UI_Component_vtable;
}

static void UI_Component_DeletingDtor(UI_Component *self, unsigned int flags)
{
    self->vtbl->Shutdown(self);
    UI_Component_dtor(self);
    if (flags & 1u)
        Z_FreeInternal(self);
}

UI_Component *UI_Component_GetCompAtLocation(UI_Component *self, float *pos)
{
    UI_Component_layout *c = (UI_Component_layout *)self;
    if (pos[0] < 0.0f || c->size[0] <= pos[0] ||
        pos[1] < 0.0f || c->size[1] <= pos[1])
        return 0;
    return self;
}

void UI_Component_AddText(UI_Component *self, const char *text)
{
    (void)self;
    (void)text;
}

void UI_LinesComponent_Init(UI_LinesComponent *self)
{
    UI_Component_Init((UI_Component *)self);
    self->selectedLine = -1;
    self->lineCount = 0;
    self->hasFocus = 0;
    self->scrollTop = 0.0f;
    self->_f20 = 0.0f;
}

int UI_LinesComponent_SetSelectedLineFocus(UI_LinesComponent *self,
                                           int line, int fromMouse)
{
    int low;

    if (Sys_IsRemoteDebugServer()) {

    }
    low = fromMouse ? 0 : -1;
    if (line < low || line >= self->lineCount) {
        self->selectedLine = -1;
        return 0;
    }
    self->selectedLine = line;
    self->focusFromMouse = (unsigned char)fromMouse;
    self->hasFocus = 1;
    return 1;
}

void UI_LinesComponent_IncSelectedLineFocus(UI_LinesComponent *self,
                                            int wrap)
{
    if (self->selectedLine < 0 ||
        (wrap && self->selectedLine >= self->lineCount - 1))
        self->vtbl->SetSelectedLineFocus(self, 0, 0);
    else
        self->vtbl->SetSelectedLineFocus(self, self->selectedLine + 1, 0);
}

void UI_LinesComponent_DecSelectedLineFocus(UI_LinesComponent *self,
                                            int wrap)
{
    if (self->selectedLine < 0 || (wrap && self->selectedLine <= 0))
        self->vtbl->SetSelectedLineFocus(self, self->lineCount - 1, 0);
    else
        self->vtbl->SetSelectedLineFocus(self, self->selectedLine - 1, 0);
}

void UI_LinesComponent_UpdateHeight(UI_LinesComponent *self)
{
    self->h = (float)self->lineCount * UI_Component_g.charHeight;
}

void UI_LinesComponent_ClearFocus(UI_LinesComponent *self)
{
    self->hasFocus = 0;
}

int UI_LinesComponent_KeyEvent(UI_LinesComponent *self, float *pos, int key)
{
    int result;

    if (Key_IsDown(DBG_K_ALT) || Key_IsDown(DBG_K_CTRL) ||
        Key_IsDown(DBG_K_SHIFT)) {

        if (Key_IsDown(DBG_K_ALT) || !Key_IsDown(DBG_K_CTRL) ||
            Key_IsDown(DBG_K_SHIFT))
            return 0;

        if (key == DBG_K_GKEY) {
            UI_Component_g.consoleReason = 0;
            Con_ToggleConsole_f();
            return 1;
        }
        if (key == DBG_K_MOUSE1) {
            if (self->selectedLine ==
                (int)(pos[1] / UI_Component_g.charHeight))
                self->selectedLine = -1;
            return 0;
        }
        return 0;
    }

    switch (key) {
    case DBG_K_SPACE:
    case DBG_K_ENTER:
        self->selectedLine = -1;
        result = 1;
        break;
    case DBG_K_UPARROW:
        UI_LinesComponent_DecSelectedLineFocus(self, 0);
        result = 1;
        break;
    case DBG_K_DOWNARROW:
        UI_LinesComponent_IncSelectedLineFocus(self, 0);
        result = 1;
        break;
    case DBG_K_MOUSE1:
        self->vtbl->SetSelectedLineFocus(
            self, (int)(pos[1] / UI_Component_g.charHeight), 1);
        result = 0;
        break;
    default:
        return 0;
    }
    return result;
}

void UI_LinesComponent_AddText(UI_LinesComponent *self, const char *text)
{
    int lineIndex;

    if (UI_Component_g.consoleReason == 0) {
        lineIndex = atoi(text);
        self->vtbl->SetSelectedLineFocus(self, lineIndex - 1, 0);
    }
}

int UI_ScrollPane_GetInnerSize(UI_ScrollPane *self, float *innerSize)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    int horScroll;

    innerSize[0] = sp->size[0] - UI_Component_g.scrollBarSize;
    horScroll = sp->forceHorScoll ||
                LC_POS0(sp->comp) != 0.0f ||
                innerSize[0] < LC_SIZE0(sp->comp);
    if (innerSize[0] > LC_SIZE0(sp->comp))
        LC_SIZE0(sp->comp) = innerSize[0];
    innerSize[1] = sp->size[1];
    if (horScroll)
        innerSize[1] = innerSize[1] - UI_Component_g.scrollBarSize;
    return horScroll;
}

int UI_ScrollPane_GetInnerLinesCount(UI_ScrollPane *self)
{
    float innerSize[2];
    UI_ScrollPane_GetInnerSize(self, innerSize);
    return (int)(innerSize[1] / UI_Component_g.charHeight);
}

int UI_ScrollPane_GetFirstDisplayedLine(UI_ScrollPane *self)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    float frac;

    frac = LC_POS1(sp->comp) / UI_Component_g.charHeight;

    if (frac - (float)(int)frac < 0.0f)
        return (int)frac + 1;
    return (int)frac;
}

int UI_ScrollPane_GetLastDisplayedLine(UI_ScrollPane *self)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    float innerSize[2];
    int lastLine;

    UI_ScrollPane_GetInnerSize(self, innerSize);
    lastLine = (int)((LC_POS1(sp->comp) + innerSize[1]) /
                     UI_Component_g.charHeight);
    if (innerSize[1] < (float)(lastLine + 1) * UI_Component_g.charHeight)
        --lastLine;
    if (lastLine >= LC_NUMLINES(sp->comp))
        return LC_NUMLINES(sp->comp) - 1;
    return lastLine;
}

void UI_ScrollPane_DisplaySelectedLine(UI_ScrollPane *self)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    int firstLine, lastLine;

    firstLine = UI_ScrollPane_GetFirstDisplayedLine(self);
    if (LC_SELLINE(sp->comp) >= firstLine) {
        lastLine = UI_ScrollPane_GetLastDisplayedLine(self);
        if (LC_SELLINE(sp->comp) > lastLine)
            LC_SELLINE(sp->comp) = lastLine;
    } else {
        LC_SELLINE(sp->comp) = firstLine;
    }
}

void UI_ScrollPane_SetPos(UI_ScrollPane *self)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    float innerSize[2];
    float maxPos;
    float *compPos;
    int i;

    compPos = &LC_POS0(sp->comp);
    for (i = 0; i < 2; ++i) {
        if (compPos[i] >= 0.0f) {
            UI_ScrollPane_GetInnerSize(self, innerSize);
            maxPos = ((&LC_SIZE0(sp->comp))[i]) - innerSize[i];
            if (maxPos < 0.0f)
                maxPos = 0.0f;
            if (maxPos < compPos[i])
                compPos[i] = maxPos;
        } else {
            compPos[i] = 0.0f;
        }
    }
    UI_LinesComponent_ClearFocus(sp->comp);
}

static int UI_ScrollPane_InRect(const float *point, const float *pos,
                                const float *size)
{
    int i;
    for (i = 0; i < 2; ++i) {
        if (pos[i] > point[i])
            return 0;
        if (point[i] >= pos[i] + size[i])
            return 0;
    }
    return 1;
}

void UI_ScrollPane_CheckMouseScroll(UI_ScrollPane *self, int index,
                                    float *thumbPos, float *thumbSize,
                                    float thumbMaxSize)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    float *compPos = &LC_POS0(sp->comp);

    if (Key_IsDown(DBG_K_MOUSE1)) {
        if (sp->mouseHeldScale[index] == 0.0f) {
            if (!UI_Component_g.hideCursor &&
                UI_ScrollPane_InRect(UI_Component_g.cursorPos, thumbPos,
                                     thumbSize)) {
                if (!sp->mouseWasDown[index]) {
                    sp->mouseHeldScale[index] =
                        ((&LC_SIZE0(sp->comp))[index]) / thumbMaxSize;
                    sp->mouseHeldPos[0] = UI_Component_g.cursorPos[0];
                    sp->mouseHeldPos[1] = UI_Component_g.cursorPos[1];
                    sp->mouseHeldCompPos[0] = compPos[0];
                    sp->mouseHeldCompPos[1] = compPos[1];
                }
            } else {
                sp->mouseWasDown[index] = 1;
            }
        } else {
            compPos[index] =
                (UI_Component_g.cursorPos[index] - sp->mouseHeldPos[index]) *
                    sp->mouseHeldScale[index] +
                sp->mouseHeldCompPos[index];
            UI_ScrollPane_SetPos(self);
        }
    } else {
        sp->mouseWasDown[index] = 0;
        sp->mouseHeldScale[0] = 0.0f;
        sp->mouseHeldScale[1] = 0.0f;
    }
}

void UI_ScrollPane_Init(UI_ScrollPane *self)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    UI_Component_Init((UI_Component *)self);
    sp->comp = 0;
    sp->forceHorScoll = 0;
    sp->mouseHeldScale[0] = 0.0f;
    sp->mouseHeldScale[1] = 0.0f;
    sp->mouseWasDown[0] = 0;
    sp->mouseWasDown[1] = 0;
}

UI_Component *UI_ScrollPane_GetCompAtLocation(UI_ScrollPane *self, float *pos)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    float innerSize[2];

    if (!sp->comp)
        return UI_Component_GetCompAtLocation((UI_Component *)self, pos);

    UI_ScrollPane_GetInnerSize(self, innerSize);
    if (innerSize[0] <= pos[0] || innerSize[1] <= pos[1])
        return UI_Component_GetCompAtLocation((UI_Component *)self, pos);

    pos[0] = pos[0] + LC_POS0(sp->comp);
    pos[1] = pos[1] + LC_POS1(sp->comp);
    return sp->comp->vtbl->GetCompAtLocation(sp->comp, pos);
}

void UI_ScrollPane_AddText(UI_ScrollPane *self, const char *text)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    if (sp->comp)
        sp->comp->vtbl->AddText(sp->comp, text);
}

int UI_ScrollPane_KeyEvent(UI_ScrollPane *self, float *pos, int key)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    int result;
    float innerSize[2];
    float innerPoint[2];
    float vec2_origin[2];
    int linesCount;

    if (!sp->comp)
        return 0;

    if (Key_IsDown(DBG_K_ALT) || Key_IsDown(DBG_K_CTRL) ||
        Key_IsDown(DBG_K_SHIFT)) {
        if (Key_IsDown(DBG_K_ALT) || !Key_IsDown(DBG_K_CTRL) ||
            Key_IsDown(DBG_K_SHIFT))
            goto forward;

        switch (key) {
        case DBG_K_MWHEELUP:
            LC_POS1(sp->comp) -= UI_Component_g.charHeight;
            UI_ScrollPane_SetPos(self);
            result = 1;
            break;
        case DBG_K_MWHEELDOWN:
            LC_POS1(sp->comp) += UI_Component_g.charHeight;
            UI_ScrollPane_SetPos(self);
            result = 1;
            break;
        case 156:
            LC_POS0(sp->comp) -= UI_Component_g.charWidth;
            UI_ScrollPane_SetPos(self);
            result = 1;
            break;
        case 157:
            LC_POS0(sp->comp) += UI_Component_g.charWidth;
            UI_ScrollPane_SetPos(self);
            result = 1;
            break;
        case 163:
            LC_SELLINE(sp->comp) = 0x7FFFFFFF;
            UI_ScrollPane_DisplaySelectedLine(self);
            result = 1;
            break;
        case 164:
            LC_SELLINE(sp->comp) = 0;
            UI_ScrollPane_DisplaySelectedLine(self);
            result = 1;
            break;
        case 165:
            LC_POS1(sp->comp) = 0.0f;
            LC_SELLINE(sp->comp) = 0;
            UI_ScrollPane_SetPos(self);
            UI_ScrollPane_DisplaySelectedLine(self);
            result = 1;
            break;
        case 166:
            LC_POS1(sp->comp) = 3.402823466e+38f;
            LC_SELLINE(sp->comp) = 0x7FFFFFFF;
            UI_ScrollPane_SetPos(self);
            UI_ScrollPane_DisplaySelectedLine(self);
            result = 1;
            break;
        default:
            goto forward;
        }
        return result;
    }

    switch (key) {
    case 163:
        linesCount = UI_ScrollPane_GetInnerLinesCount(self);
        LC_POS1(sp->comp) += (float)linesCount * UI_Component_g.charHeight;
        LC_SELLINE(sp->comp) += linesCount;
        UI_ScrollPane_SetPos(self);
        UI_ScrollPane_DisplaySelectedLine(self);
        result = 1;
        break;
    case 164:
        linesCount = UI_ScrollPane_GetInnerLinesCount(self);
        LC_POS1(sp->comp) -= (float)linesCount * UI_Component_g.charHeight;
        LC_SELLINE(sp->comp) -= linesCount;
        UI_ScrollPane_SetPos(self);
        UI_ScrollPane_DisplaySelectedLine(self);
        result = 1;
        break;
    case 205:
        LC_POS1(sp->comp) += UI_Component_g.charHeight * 3.0f;
        UI_ScrollPane_SetPos(self);
        result = 1;
        break;
    case 206:
        LC_POS1(sp->comp) -= UI_Component_g.charHeight * 3.0f;
        UI_ScrollPane_SetPos(self);
        result = 1;
        break;
    default:
        goto forward;
    }
    return result;

forward:
    if (key != DBG_K_MOUSE1) {
        vec2_origin[0] = 0.0f;
        vec2_origin[1] = 0.0f;
        UI_ScrollPane_GetInnerSize(self, innerSize);
        if (!UI_ScrollPane_InRect(pos, vec2_origin, innerSize))
            return 0;
    }
    innerPoint[0] = pos[0] + LC_POS0(sp->comp);
    innerPoint[1] = pos[1] + LC_POS1(sp->comp);
    return sp->comp->vtbl->KeyEvent((UI_LinesComponent *)sp->comp,
                                    innerPoint, key);
}

void UI_ScrollPane_Draw(UI_ScrollPane *self, float x, float y, float w,
                        float h, float compX, float compY)
{
    UI_ScrollPane_layout *sp = (UI_ScrollPane_layout *)self;
    clientStatic_t *cls = (clientStatic_t *)imp_cls;
    float innerSize[3];
    float thumbColor[4];
    float thumbPos[2];
    float thumbSize[2];
    float scrollbarX, scrollbarY, scrollBarSize, thumbFiller, thumbMaxSize;
    float thumbStart, thumbEnd;
    int horScroll;
    int selectedLine;
    float focusRegionStart, focusRegionStart2, focusRegionEnd;

    (void)compX;
    (void)compY;

    sp->size[0] = w;
    sp->size[1] = h;

    if (!sp->comp || LC_SIZE0(sp->comp) == 0.0f || LC_SIZE1(sp->comp) == 0.0f) {
        UI_Component_DrawPic(x, y, w, h, 0, cls ? (Material *)cls->consoleMaterial : 0);
        return;
    }

    horScroll = UI_ScrollPane_GetInnerSize(self, innerSize);

    if (LC_FOCUS(sp->comp)) {
        LC_FOCUS(sp->comp) = 0;
        if (LC_SELLINE(sp->comp) < 0)
            selectedLine = 0;
        else
            selectedLine = LC_SELLINE(sp->comp);
        focusRegionStart2 = (float)selectedLine * UI_Component_g.charHeight;
        if (LC_FOCUSUSER(sp->comp)) {
            focusRegionStart = (float)selectedLine * UI_Component_g.charHeight;
            focusRegionEnd = focusRegionStart2 + UI_Component_g.charHeight;
        } else {
            focusRegionStart = focusRegionStart2 - UI_Component_g.charHeight * 4.0f;
            focusRegionEnd = UI_Component_g.charHeight * 5.0f + focusRegionStart2;
        }
        if (LC_POS1(sp->comp) <= focusRegionStart) {
            if (focusRegionEnd > LC_POS1(sp->comp) + innerSize[1]) {
                if (focusRegionStart > LC_POS1(sp->comp) + innerSize[1]) {
                    LC_POS1(sp->comp) = focusRegionStart2 - innerSize[1] / 2.0f;
                } else {
                    LC_POS1(sp->comp) = focusRegionEnd - innerSize[1];
                    if (LC_POS1(sp->comp) > focusRegionStart2 - innerSize[1] / 2.0f)
                        LC_POS1(sp->comp) = focusRegionStart2 - innerSize[1] / 2.0f;
                }
            }
        } else if (LC_POS1(sp->comp) > focusRegionEnd) {
            LC_POS1(sp->comp) = focusRegionStart2 - innerSize[1] / 2.0f;
        } else {
            LC_POS1(sp->comp) = focusRegionStart;
            if (LC_POS1(sp->comp) < focusRegionStart2 - innerSize[1] / 2.0f)
                LC_POS1(sp->comp) = focusRegionStart2 - innerSize[1] / 2.0f;
        }
    }

    UI_ScrollPane_SetPos(self);

    thumbColor[0] = 0.5f;
    thumbColor[1] = 0.5f;
    thumbColor[2] = 0.5f;
    thumbColor[3] = 1.0f;

    sp->comp->vtbl->Draw(sp->comp, x, y, innerSize[0], innerSize[1],
                         LC_POS0(sp->comp), LC_POS1(sp->comp));

    scrollbarX = x + innerSize[0];
    UI_Component_DrawPic(scrollbarX, y, UI_Component_g.scrollBarSize, h, 0,
                         cls ? (Material *)cls->consoleMaterial : 0);

    scrollbarY = y + UI_Component_g.scrollBarSize;
    scrollBarSize = innerSize[1] -
                    (UI_Component_g.scrollBarSize + UI_Component_g.scrollBarSize);
    if (UI_Component_g.scrollBarSize >= scrollBarSize)
        thumbFiller = scrollBarSize;
    else
        thumbFiller = UI_Component_g.scrollBarSize;
    UI_Component_DrawPic(scrollbarX, scrollbarY, UI_Component_g.scrollBarSize,
                         scrollBarSize, 0,
                         cls ? (Material *)cls->consoleMaterial : 0);

    thumbMaxSize = scrollBarSize - thumbFiller;
    thumbStart = LC_POS1(sp->comp) / LC_SIZE1(sp->comp) * thumbMaxSize;
    thumbEnd = (LC_POS1(sp->comp) + innerSize[1]) / LC_SIZE1(sp->comp) *
                   thumbMaxSize +
               thumbFiller;
    if (scrollBarSize < thumbEnd)
        thumbEnd = scrollBarSize;
    thumbPos[0] = scrollbarX;
    thumbPos[1] = y + UI_Component_g.scrollBarSize + thumbStart;
    thumbSize[0] = UI_Component_g.scrollBarSize;
    thumbSize[1] = thumbEnd - thumbStart;
    UI_Component_DrawPic(scrollbarX, thumbPos[1], UI_Component_g.scrollBarSize,
                         thumbSize[1], thumbColor,
                         cls ? (Material *)cls->whiteMaterial : 0);
    UI_ScrollPane_CheckMouseScroll(self, 1, thumbPos, thumbSize, thumbMaxSize);

    if (horScroll) {
        float scrollbarYa = y + innerSize[1];
        UI_Component_DrawPic(x, scrollbarYa, innerSize[0],
                             UI_Component_g.scrollBarSize, 0,
                             cls ? (Material *)cls->consoleMaterial : 0);
        scrollbarX = x;
        scrollbarX = scrollbarX + UI_Component_g.scrollBarSize;
        scrollBarSize = innerSize[0] -
                        (UI_Component_g.scrollBarSize + UI_Component_g.scrollBarSize);
        if (UI_Component_g.scrollBarSize >= scrollBarSize)
            thumbFiller = scrollBarSize;
        else
            thumbFiller = UI_Component_g.scrollBarSize;
        UI_Component_DrawPicRotate(scrollbarX, scrollbarYa, scrollBarSize,
                                   UI_Component_g.scrollBarSize, 0,
                                   cls ? (Material *)cls->consoleMaterial : 0);
        scrollbarX = scrollbarX + scrollBarSize;
        thumbMaxSize = scrollBarSize - thumbFiller;
        thumbStart = LC_POS0(sp->comp) / LC_SIZE0(sp->comp) * thumbMaxSize;
        thumbEnd = (LC_POS0(sp->comp) + innerSize[0]) / LC_SIZE0(sp->comp) *
                       thumbMaxSize +
                   thumbFiller;
        if (scrollBarSize < thumbEnd)
            thumbEnd = scrollBarSize;
        thumbPos[0] = x + UI_Component_g.scrollBarSize + thumbStart;
        thumbPos[1] = scrollbarYa;
        thumbSize[0] = thumbEnd - thumbStart;
        thumbSize[1] = UI_Component_g.scrollBarSize;
        UI_Component_DrawPicRotate(thumbPos[0], scrollbarYa, thumbSize[0],
                                   UI_Component_g.scrollBarSize, thumbColor,
                                   cls ? (Material *)cls->whiteMaterial : 0);
        UI_ScrollPane_CheckMouseScroll(self, 0, thumbPos, thumbSize, thumbMaxSize);
    }
}

void UI_VerticalDivider_Init(UI_VerticalDivider *self)
{
    UI_VerticalDivider_layout *vd = (UI_VerticalDivider_layout *)self;
    UI_Component_Init((UI_Component *)self);
    vd->topComp = 0;
    vd->bottomComp = 0;
}

void UI_VerticalDivider_DrawTop(UI_VerticalDivider *self, float x, float y,
                                float w, float h)
{
    UI_VerticalDivider_layout *vd = (UI_VerticalDivider_layout *)self;
    UI_ScrollPane_layout *top = (UI_ScrollPane_layout *)vd->topComp;

    if (!vd->topComp)
        return;
    top->vtbl->Draw((UI_ScrollPane *)top, x, y, w, h, 0.0f, 0.0f);
    if (top->mouseHeldScale[0] != 0.0f)
        vd->posY = UI_Component_g.cursorPos[1] + UI_Component_g.charHeight;
}

void UI_VerticalDivider_Draw(UI_VerticalDivider *self, float x, float y,
                             float w, float h, float compX, float compY)
{
    UI_VerticalDivider_layout *vd = (UI_VerticalDivider_layout *)self;
    refexport_t *re = (refexport_t *)imp_re;
    UI_ScrollPane_layout *bottom;
    float topHeight, v7, v8;

    (void)compX;

    vd->size[0] = w;
    vd->size[1] = h;
    vd->posY = (float)(int)(vd->posY / UI_Component_g.charHeight) *
               UI_Component_g.charHeight;
    if (UI_Component_g.charHeight <= vd->posY) {
        if (h < vd->posY)
            vd->posY = h;
    } else {
        vd->posY = UI_Component_g.charHeight;
    }

    (void)re;

    bottom = (UI_ScrollPane_layout *)vd->bottomComp;

    if (!vd->topComp) {
        if (!vd->bottomComp)
            return;
        bottom->vtbl->Draw((UI_ScrollPane *)bottom, x, y, w, h, 0.0f, 0.0f);
        return;
    }
    if (!vd->bottomComp) {
        UI_VerticalDivider_DrawTop(self, x, y, w, h);
        return;
    }
    if (compY >= vd->posY) {
        bottom->vtbl->Draw((UI_ScrollPane *)bottom, x, y, w, h, 0.0f, 0.0f);
        return;
    }
    topHeight = vd->posY - compY;
    if (h > topHeight) {
        v8 = h - topHeight;
        v7 = y + vd->posY;
        bottom->vtbl->Draw((UI_ScrollPane *)bottom, x, v7, w, v8, 0.0f, 0.0f);
        UI_VerticalDivider_DrawTop(self, x, y, w, topHeight);
    } else {
        UI_VerticalDivider_DrawTop(self, x, y, w, h);
    }
}

int UI_VerticalDivider_KeyEvent(UI_VerticalDivider *self, float *pos, int key)
{
    UI_VerticalDivider_layout *vd = (UI_VerticalDivider_layout *)self;

    (void)pos;

    if (!Key_IsDown(DBG_K_ALT) || Key_IsDown(DBG_K_CTRL) ||
        Key_IsDown(DBG_K_SHIFT))
        return 0;

    if (key == DBG_K_MWHEELUP) {
        vd->posY = vd->posY - UI_Component_g.charHeight;
        return 1;
    }
    if (key != DBG_K_MWHEELDOWN)
        return 0;
    vd->posY = vd->posY + UI_Component_g.charHeight;
    return 1;
}

UI_Component *UI_VerticalDivider_GetCompAtLocation(UI_VerticalDivider *self,
                                                   float *pos)
{
    UI_VerticalDivider_layout *vd = (UI_VerticalDivider_layout *)self;
    UI_ScrollPane_layout *top = (UI_ScrollPane_layout *)vd->topComp;
    UI_ScrollPane_layout *bottom = (UI_ScrollPane_layout *)vd->bottomComp;

    if (vd->topComp) {
        if (vd->bottomComp && vd->posY <= pos[1]) {
            pos[1] = pos[1] - vd->posY;
            return bottom->vtbl->GetCompAtLocation((UI_ScrollPane *)bottom, pos);
        }
        return top->vtbl->GetCompAtLocation((UI_ScrollPane *)top, pos);
    }
    if (vd->bottomComp)
        return bottom->vtbl->GetCompAtLocation((UI_ScrollPane *)bottom, pos);
    return (UI_Component *)self;
}

const UI_Component_vtbl UI_Component_vtable = {
    UI_Component_Init,
    UI_Component_Shutdown,
    UI_Component_DeletingDtor,
    (void (*)(UI_Component *, float, float, float, float, float, float))UI_PureCall,
    (int (*)(UI_Component *, float *, int))UI_PureCall,
    UI_Component_GetCompAtLocation,
    UI_Component_AddText,
};

const UI_LinesComponent_vtbl UI_LinesComponent_vtable = {
    UI_LinesComponent_Init,
    (void (*)(UI_LinesComponent *))UI_Component_Shutdown,
    (void (*)(UI_LinesComponent *, unsigned int))UI_Component_DeletingDtor,
    (void (*)(UI_LinesComponent *, float, float, float, float, float, float))UI_PureCall,
    UI_LinesComponent_KeyEvent,
    (UI_Component * (*)(UI_LinesComponent *, float *)) UI_Component_GetCompAtLocation,
    UI_LinesComponent_AddText,
    UI_LinesComponent_SetSelectedLineFocus,
};

const UI_ScrollPane_vtbl UI_ScrollPane_vtable = {
    UI_ScrollPane_Init,
    (void (*)(UI_ScrollPane *))UI_Component_Shutdown,
    (void (*)(UI_ScrollPane *, unsigned int))UI_Component_DeletingDtor,
    UI_ScrollPane_Draw,
    UI_ScrollPane_KeyEvent,
    UI_ScrollPane_GetCompAtLocation,
    UI_ScrollPane_AddText,
};

const UI_VerticalDivider_vtbl UI_VerticalDivider_vtable = {
    UI_VerticalDivider_Init,
    (void (*)(UI_VerticalDivider *))UI_Component_Shutdown,
    (void (*)(UI_VerticalDivider *, unsigned int))UI_Component_DeletingDtor,
    UI_VerticalDivider_Draw,
    UI_VerticalDivider_KeyEvent,
    UI_VerticalDivider_GetCompAtLocation,
    (void (*)(UI_VerticalDivider *, const char *))UI_Component_AddText,
};

#endif
