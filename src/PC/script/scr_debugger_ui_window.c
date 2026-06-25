#define UI_Component ct_UI_Component
#define UI_LinesComponent ct_UI_LinesComponent
#define UI_ScrollPane ct_UI_ScrollPane
#define UI_VerticalDivider ct_UI_VerticalDivider
#define VariableUnion ct_VariableUnion
#include "common_types.h"
#undef UI_Component
#undef UI_LinesComponent
#undef UI_ScrollPane
#undef UI_VerticalDivider
#undef VariableUnion

#include "cod2_feature_config.h"
#include "scr_debugger_ui.h"

#if COD2_FEATURE_SCRIPT_DEBUGGER

#    include <string.h>

extern void Com_Printf(const char *fmt, ...);
extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *p);
extern void I_strncpyz(char *dst, const char *src, int len);
extern char *CopyStringInternal(const char *src);

extern void *imp_re;
extern void *imp_cls;

extern struct scrParserPub_t scrParserPub;

#    define SCR_TAB_WIDTH 4

extern int Sys_IsRemoteDebugServer(void);

extern char *Sys_SetClipboardData(const char *text);
extern int Key_IsDown(int key);

extern char *Scr_GetOpcodePosOfType(int fileIndex, int sourceStart, int sourceEnd,
                                    int type, int *outSourcePos);
extern char *Scr_GetSourcePosOfType(int fileIndex, int codePos, int type, int *outSourcePos);

extern void Scr_SetTempBreakpoint(const char *codePos, int remote);
extern void Scr_AddManualBreakpoint(struct Scr_WatchElement_s *elem);

extern void Scr_SelectScriptLine(int fileIndex, int sourcePos);

extern Scr_WatchElement_s *Scr_ScriptWatch_DeleteElementInternal(Scr_ScriptWatch *self,
                                                                 Scr_WatchElement_s *e);

extern Scr_ScriptWatch *scr_debuggerWatch;

extern void UI_Component_DrawPic(float x, float y, float w, float h,
                                 const float *color, struct Material *mat);
extern void UI_Component_DrawText(float x, float y, int font,
                                  const float *color, const char *text);
extern void UI_LinesComponent_Init(UI_LinesComponent *self);
extern void UI_LinesComponent_UpdateHeight(UI_LinesComponent *self);
extern int UI_LinesComponent_KeyEvent(UI_LinesComponent *self, float *pos, int key);
extern void UI_Component_dtor(UI_Component *self);

static float ScriptWindow_LineHeight(void)
{
    refexport_t *re = (refexport_t *)imp_re;
    clientStatic_t *cls = (clientStatic_t *)imp_cls;
    return (float)re->TextHeight(cls->consoleFont);
}

static float ScriptWindow_CharWidth(void)
{
    refexport_t *re = (refexport_t *)imp_re;
    clientStatic_t *cls = (clientStatic_t *)imp_cls;

    return (float)re->TextWidth("M", 1, cls->consoleFont);
}

extern void CL_LookupColor(int index, unsigned char *outRGBA);

static void ScriptWindow_LookupColorF(int index, float out[4])
{
    unsigned char b[4];
    CL_LookupColor(index, b);
    out[0] = (float)b[0] / 255.0f;
    out[1] = (float)b[1] / 255.0f;
    out[2] = (float)b[2] / 255.0f;
    out[3] = (float)b[3] / 255.0f;
}

static void ScriptWindow_DrawBox(float x, float y, float w, float h, const float *color)
{
    refexport_t *re = (refexport_t *)imp_re;
    clientStatic_t *cls = (clientStatic_t *)imp_cls;
    re->DrawStretchPic(x, y, w, h, 0.0f, 0.0f, 0.0f, 0.0f, color, cls->whiteMaterial);
}

static void ScriptWindow_DrawText(const char *text, float x, float y,
                                  const float *color, int style)
{
    refexport_t *re = (refexport_t *)imp_re;
    clientStatic_t *cls = (clientStatic_t *)imp_cls;
    re->DrawText(text, 0x7FFFFFFF, cls->consoleFont, x, y, 1.0f, 1.0f, color, style);
}

void *Scr_ScriptWindow_operator_new(unsigned int size)
{
    (void)size;
    return Z_MallocInternal((int)size);
}

void Scr_ScriptWindow_operator_delete(void *p)
{
    Z_FreeInternal(p);
}

extern const Scr_ScriptWindow_vtbl Scr_ScriptWindow_vtable;

Scr_ScriptWindow *Scr_ScriptWindow_ctor(Scr_ScriptWindow *self)
{
    self->vtbl = &Scr_ScriptWindow_vtable;
    return self;
}

void Scr_ScriptWindow_dtor(Scr_ScriptWindow *self)
{
    UI_Component_dtor((UI_Component *)self);
}

void Scr_ScriptWindow_scalar_deleting_dtor(Scr_ScriptWindow *self, unsigned int flags)
{
    Scr_ScriptWindow_dtor(self);
    if (flags & 1)
        Z_FreeInternal(self);
}

void Scr_ScriptWindow_SetScriptFile(Scr_ScriptWindow *self, const char *filename)
{
    unsigned int count = scrParserPub.sourceBufferLookupLen;
    SourceBufferInfo *table = scrParserPub.sourceBufferLookup;
    unsigned int i;

    if (count == 0) {
        self->scriptFileIndex = -1;
        self->vtbl->Init(self);
        return;
    }

    for (i = 0; i < count; i++) {
        const char *buf = table[i].buf;
        if (buf != 0 && stricmp(buf, filename) == 0) {
            self->scriptFileIndex = (int)i;
            self->vtbl->Init(self);
            return;
        }
    }

    self->scriptFileIndex = -1;
    self->vtbl->Init(self);
}

void Scr_ScriptWindow_SetCurrentLine(Scr_ScriptWindow *self, int line)
{
    SourceBufferInfo *entry;
    const char *sourceStart;
    const char *sourceEnd;

    if (self->scriptFileIndex < 0)
        return;

    entry = &scrParserPub.sourceBufferLookup[self->scriptFileIndex];
    sourceStart = entry->sourceBuf;
    sourceEnd = entry->sourceBuf + entry->len;

    if (line == self->currentLine)
        return;

    if (line > self->currentLine) {

        for (;;) {

            if (self->currentLinePtr[0] != 0) {
                do {
                    self->currentLinePtr++;
                } while (self->currentLinePtr[0] != 0);
            }

            self->currentLinePtr++;
            self->currentLine++;
            if (self->currentLinePtr >= sourceEnd) {

                line = self->currentLine - 1;
                Scr_ScriptWindow_SetCurrentLine(self, line);
                return;
            }
            if (line <= self->currentLine)
                return;
        }
    } else {

        for (;;) {
            if (self->currentLinePtr == sourceStart) {
                self->currentLine = 0;
                return;
            }
            self->currentLinePtr--;

            do {
                self->currentLinePtr--;
            } while (self->currentLinePtr[0] != 0);
            self->currentLine--;
            self->currentLinePtr++;
            if (line >= self->currentLine)
                return;
        }
    }
}

const char *Scr_ScriptWindow_GetFilename(Scr_ScriptWindow *self)
{
    if (self->scriptFileIndex >= 0)
        return scrParserPub.sourceBufferLookup[self->scriptFileIndex].buf;
    return "";
}

void Scr_ScriptWindow_Init(Scr_ScriptWindow *self)
{
    SourceBufferInfo *entry;
    const char *p;
    int col;

    self->breakpoints = 0;
    UI_LinesComponent_Init((UI_LinesComponent *)self);
    self->currentLine = 0;

    if (self->scriptFileIndex < 0) {

        if (Sys_IsRemoteDebugServer())
            return;
        self->currentLinePtr = 0;
        self->w = 0.0f;
        self->h = 0.0f;
        return;
    }

    entry = &scrParserPub.sourceBufferLookup[self->scriptFileIndex];
    self->maxLineWidth = 0;

    p = entry->sourceBuf;
    col = 0;
    {
        int remaining = entry->len;
        while (remaining > 0) {
            char c = *p;
            if (c != 0) {
                if (c == '\t') {

                    col = col + (SCR_TAB_WIDTH - (col & (SCR_TAB_WIDTH - 1)));
                } else {
                    col += 1;
                }
            } else {

                if (col > self->maxLineWidth)
                    self->maxLineWidth = col;
                self->lineCount++;
                col = 0;
            }
            p++;
            remaining--;
        }

        if (remaining == 0 && p != entry->sourceBuf && p[-1] != 0)
            self->lineCount++;
    }

    self->w = (float)self->maxLineWidth * ScriptWindow_CharWidth();
    UI_LinesComponent_UpdateHeight((UI_LinesComponent *)self);
    self->currentLinePtr = entry->sourceBuf;
}

void Scr_ScriptWindow_Draw(Scr_ScriptWindow *self,
                           float x, float y, float w, float h,
                           float a, float b)
{
    SourceBufferInfo *entry;
    const char *p, *sourceEnd;
    float lineH, charW, leftMargin;
    float yLine;
    int line;
    float colSel[4], colCur[4], colBp[4];
    Scr_Breakpoint *bp;
    int isCurrentFile;

    ScriptWindow_DrawBox(x, y, w, h, 0 );

    if (self->scriptFileIndex < 0)
        return;

    entry = &scrParserPub.sourceBufferLookup[self->scriptFileIndex];

    ScriptWindow_LookupColorF(55, colSel);
    ScriptWindow_LookupColorF(49, colCur);
    ScriptWindow_LookupColorF(51, colBp);

    lineH = ScriptWindow_LineHeight();
    charW = ScriptWindow_CharWidth();
    leftMargin = charW;

    {
        int first = (int)(self->scrollTop - b / lineH);
        line = first < 0 ? 0 : first;
    }

    Scr_ScriptWindow_SetCurrentLine(self, line);

    p = self->currentLinePtr;
    sourceEnd = entry->sourceBuf + entry->len;

    bp = self->breakpoints;
    if (bp != 0) {
        while ((const char *)(intptr_t)bp->codePos < p && bp->next != 0)
            bp = bp->next;
    }

    isCurrentFile = (self->scriptFileIndex ==
                     (scr_debuggerWatch ? scr_debuggerWatch->selectedLine : -1));

    yLine = y;
    while (p < sourceEnd && yLine <= (y + h)) {
        char expanded[1024];
        int col = 0;
        const char *q = p;
        int outLen = 0;

        if (line == self->selectedLine) {
            float v60[4];
            v60[0] = colSel[0];
            v60[1] = colSel[1];
            v60[2] = colSel[2];
            v60[3] = colSel[3];
            ScriptWindow_DrawBox(x + leftMargin, yLine, w - leftMargin, lineH, v60);
        }

        if (bp != 0 && (const char *)(intptr_t)bp->codePos == p) {
            Scr_WatchElement_s *elem = bp->watchElement;
            int kind = elem ? *((unsigned char *)elem + 0x193) : 0;
            if (kind != 5) {

                ScriptWindow_DrawText("\x95" , x, yLine, colCur, 5);
            } else {

                ScriptWindow_DrawBox(x, yLine, charW, lineH, colCur);
            }

            while (bp->next != 0 && (const char *)(intptr_t)bp->next->codePos == p)
                bp = bp->next;
        }

        if (isCurrentFile && line == (scr_debuggerWatch ? scr_debuggerWatch->lineCount : -1)) {
            ScriptWindow_DrawText("\x96" , x + leftMargin, yLine, colBp, 5);
        }

        while (*q != 0 && outLen < (int)sizeof(expanded) - SCR_TAB_WIDTH - 1) {
            if (*q == '\t') {
                int stop = SCR_TAB_WIDTH - (col & (SCR_TAB_WIDTH - 1));
                while (stop-- > 0) {
                    expanded[outLen++] = ' ';
                    col++;
                }
            } else {
                expanded[outLen++] = *q;
                col++;
            }
            q++;
        }
        expanded[outLen] = 0;

        ScriptWindow_DrawText(expanded, x + leftMargin, yLine, colSel, 5);

        while (*q != 0)
            q++;
        p = q + 1;
        line++;
        yLine += lineH;
    }
}

void Scr_ScriptWindow_AddBreakpoint(Scr_ScriptWindow *self, Scr_Breakpoint **list,
                                    char *codePos, Scr_WatchElement_s *elem, int enabled)
{
    Scr_Breakpoint *old = *list;
    Scr_Breakpoint *node = (Scr_Breakpoint *)Z_MallocInternal(24);

    if (node == 0)
        return;
    memset(node, 0, 24);

    *list = node;
    node->_pad14 = (int)(intptr_t)list;
    node->next = old;
    if (old != 0)
        old->_pad14 = (int)(intptr_t)&node->next;

    node->watchElement = elem;
    node->codePos = (int)(intptr_t)self->selectedLine;
    node->enabled = enabled;
    node->scriptFile = self->scriptFileIndex;
    (void)codePos;

    if (elem != 0) {
        *((Scr_Breakpoint **)((char *)elem + 0x1B0)) = node;
        *((unsigned char *)elem + 0x193) = enabled ? 5 : 4;
        Scr_AddManualBreakpoint(elem);
    }
}

const char *Scr_ScriptWindow_GetBreakpointCodePos(Scr_ScriptWindow *self)
{
    SourceBufferInfo *entry;
    const char *lineStart;
    const char *sourceBase;
    int line;
    int target = self->selectedLine;

    if (self->scriptFileIndex < 0)
        return 0;

    entry = &scrParserPub.sourceBufferLookup[self->scriptFileIndex];
    sourceBase = entry->sourceBuf;
    lineStart = sourceBase;
    line = 0;

    for (;;) {
        const char *lineEnd = lineStart;
        int sourcePos;
        char *cp;

        if (*lineEnd != 0) {
            do {
                lineEnd++;
            } while (*lineEnd != 0);
        }

        if (line == target) {
            cp = Scr_GetOpcodePosOfType(self->scriptFileIndex,
                                        (int)(intptr_t)(lineStart - sourceBase),
                                        (int)(intptr_t)(lineEnd - sourceBase),
                                        1, &sourcePos);
            if (cp != 0)
                return cp;

            if (self->selectedLine >= self->lineCount - 1)
                return 0;
            self->selectedLine++;
            target = self->selectedLine;
        }

        lineStart = lineEnd + 1;
        line++;
    }
}

void Scr_ScriptWindow_RunToCursor(Scr_ScriptWindow *self)
{
    if (Sys_IsRemoteDebugServer() == 0) {

        extern int *scr_debuggerVmState;
        if (scr_debuggerVmState)
            scr_debuggerVmState[1] &= ~2;
    } else {

        extern int scr_debuggerStepping;
        extern int scr_debuggerStepLine;
        extern Scr_WatchElement_s *scr_debuggerWatchRoot;
        if (scr_debuggerStepping) {
            Scr_WatchElement_s *e;
            scr_debuggerStepping = 0;
            scr_debuggerStepLine = -1;
            for (e = scr_debuggerWatchRoot; e != 0;
                 e = *(Scr_WatchElement_s **)((char *)e + 0x1D0)) {
                *((unsigned char *)e + 0x195) = 0;
                *((unsigned char *)e + 0x194) = 0;
            }
        }
    }

    {
        extern int scr_debuggerConnected;
        if (scr_debuggerConnected) {
            const char *cp = Scr_ScriptWindow_GetBreakpointCodePos(self);
            Scr_SetTempBreakpoint(cp, 0);
        }
    }
}

extern char scr_debuggerFindText[];

void Scr_ScriptWindow_FindNext(Scr_ScriptWindow *self)
{
    int findLen = (int)strlen(scr_debuggerFindText);
    int line;

    if (findLen == 0)
        return;

    line = self->selectedLine + 1;
    if (line >= self->lineCount) {
        self->selectedLine = -1;
        return;
    }

    for (;;) {
        const char *text;
        Scr_ScriptWindow_SetCurrentLine(self, line);
        text = self->currentLinePtr;
        if (strnicmp(text, scr_debuggerFindText, findLen) == 0) {
            self->vtbl->SetSelectedLineFocus(self, line, 0);
            return;
        }
        line++;
        if (line >= self->lineCount)
            break;
    }
    self->selectedLine = -1;
}

void Scr_ScriptWindow_FindPrev(Scr_ScriptWindow *self)
{
    int findLen = (int)strlen(scr_debuggerFindText);
    int line;

    if (findLen == 0)
        return;

    if (self->selectedLine == 0 || self->lineCount == 0) {
        self->selectedLine = -1;
        return;
    }

    line = (self->selectedLine > 0 ? self->selectedLine : self->lineCount) - 1;

    for (;;) {
        const char *text;
        Scr_ScriptWindow_SetCurrentLine(self, line);
        text = self->currentLinePtr;
        if (strnicmp(text, scr_debuggerFindText, findLen) == 0) {
            self->vtbl->SetSelectedLineFocus(self, line, 0);
            return;
        }
        if (line == 0) {
            self->selectedLine = -1;
            return;
        }
        line--;
    }
}

void Scr_ScriptWindow_CopySelectedText(Scr_ScriptWindow *self)
{
    SourceBufferInfo *entry;
    const char *p, *sourceEnd;
    int line;

    if (self->scriptFileIndex < 0)
        return;
    if (self->selectedLine < 0)
        return;

    entry = &scrParserPub.sourceBufferLookup[self->scriptFileIndex];
    p = entry->sourceBuf;
    sourceEnd = entry->sourceBuf + entry->len;

    line = 0;
    while (p < sourceEnd) {
        if (line == self->selectedLine) {

            while (*p == '\t')
                p++;
            Sys_SetClipboardData(p);
            return;
        }
        while (*p != 0)
            p++;
        p++;
        line++;
    }
}

void Scr_ScriptWindow_ToggleBreakpointInternal(Scr_ScriptWindow *self,
                                               Scr_WatchElement_s *elem,
                                               int add, int remote, int kind, int extra)
{
    Scr_Breakpoint **head = &self->breakpoints;
    Scr_Breakpoint *node = *head;
    int target = self->selectedLine;
    (void)extra;

    if (Sys_IsRemoteDebugServer()) {
        extern void Sys_WriteDebugSocketInt(int v);
        Sys_WriteDebugSocketInt(self->selectedLine);
    }

    if (remote && add) {

        Scr_ScriptWatch_DeleteElementInternal(scr_debuggerWatch, elem);
    }

    if (kind == 0)
        return;

    while (node != 0 && node->codePos < target)
        node = node->next;

    if (node != 0 && node->codePos == target) {

        Scr_WatchElement_s *bpElem = node->watchElement;
        if (bpElem != 0 &&
            *((unsigned char *)bpElem + 0x193) != (unsigned char)kind) {
            *((unsigned char *)bpElem + 0x193) = (unsigned char)kind;
        }
        return;
    }

    Scr_ScriptWindow_AddBreakpoint(self, head, 0, elem, kind == 5);
}

void Scr_ScriptWindow_ToggleBreakpoint(Scr_ScriptWindow *self,
                                       Scr_WatchElement_s *elem,
                                       int add, int remote, int kind, int extra)
{
    if (self->selectedLine < 0)
        return;
    Scr_ScriptWindow_ToggleBreakpointInternal(self, elem, add, remote, kind, extra);
}

void Scr_ScriptWindow_EnterCallInternal(Scr_ScriptWindow *self)
{
    SourceBufferInfo *entry;
    const char *p, *sourceEnd, *lineStart;
    int line;

    if (self->selectedLine < 0)
        return;

    entry = &scrParserPub.sourceBufferLookup[self->scriptFileIndex];
    p = entry->sourceBuf;
    sourceEnd = entry->sourceBuf + entry->len;
    lineStart = p;
    line = 0;

    while (p <= sourceEnd) {
        if (line == self->selectedLine) {
            const char *lineEnd = lineStart;
            int sourcePos = 0;
            char *cp;

            if (*lineEnd != 0) {
                do {
                    lineEnd++;
                } while (*lineEnd != 0);
            }

            cp = Scr_GetOpcodePosOfType(self->scriptFileIndex,
                                        (int)(intptr_t)(lineStart - entry->sourceBuf),
                                        (int)(intptr_t)(lineEnd - entry->sourceBuf),
                                        2, &sourcePos);
            if (cp == 0)
                return;

            if (self->breakpoints != 0)
                Scr_ScriptWindow_ToggleBreakpointInternal(self, 0, 0, 0, 0, 0);

            {
                int targetPos = 0;
                Scr_GetSourcePosOfType(self->scriptFileIndex, (int)(intptr_t)cp,
                                       2, &targetPos);
                if (Sys_IsRemoteDebugServer() == 0) {
                    Scr_SelectScriptLine(self->scriptFileIndex, sourcePos);
                    return;
                }
                return;
            }
        }

        while (*p != 0 && p < sourceEnd)
            p++;
        p++;
        lineStart = p;
        line++;
    }
}

void Scr_ScriptWindow_EnterCall(Scr_ScriptWindow *self)
{
    if (self->selectedLine < 0)
        return;
    Scr_ScriptWindow_EnterCallInternal(self);
}

int Scr_ScriptWindow_KeyEvent(Scr_ScriptWindow *self, float *pos, int key)
{
    int ctrlDown = (Key_IsDown(158) || Key_IsDown(159) || Key_IsDown(160));

    if (!ctrlDown) {

        if ((unsigned int)(key - 13) <= 210u) {
            Scr_ScriptWindow_EnterCall(self);
            return 1;
        }
        return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);
    }

    if (Key_IsDown(158) == 0 && Key_IsDown(159) != 0 && Key_IsDown(160) == 0 &&
        (unsigned int)(key - 99) <= 77u) {
        Scr_ScriptWindow_CopySelectedText(self);
        return 1;
    }
    return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);
}

extern void UI_Component_Shutdown(UI_Component *self);
extern UI_Component *UI_Component_GetCompAtLocation(UI_Component *self, float *pos);
extern void UI_LinesComponent_AddText(UI_LinesComponent *self, const char *text);
extern int UI_LinesComponent_SetSelectedLineFocus(UI_LinesComponent *self, int line, int fromMouse);

const Scr_ScriptWindow_vtbl Scr_ScriptWindow_vtable = {
     Scr_ScriptWindow_Init,
     (void (*)(Scr_ScriptWindow *))UI_Component_Shutdown,
     Scr_ScriptWindow_scalar_deleting_dtor,
     Scr_ScriptWindow_Draw,
     Scr_ScriptWindow_KeyEvent,
     (UI_Component *(*)(Scr_ScriptWindow *, float *))UI_Component_GetCompAtLocation,
     (void (*)(Scr_ScriptWindow *, const char *))UI_LinesComponent_AddText,
     (int (*)(Scr_ScriptWindow *, int, int))UI_LinesComponent_SetSelectedLineFocus,
};

#endif
