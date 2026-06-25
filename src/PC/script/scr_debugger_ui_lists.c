#include "cod2_feature_config.h"
#include "scr_debugger_ui.h"

#if COD2_FEATURE_SCRIPT_DEBUGGER

#    include <string.h>

typedef struct Material *MaterialHandle;
typedef struct Font *FontHandle;
typedef float vec_t;

typedef struct refexport_s {
    void *_slots0to36[37];
    void (*DrawStretchPic)(float x, float y, float w, float h,
                           float s0, float t0, float s1, float t1,
                           const vec_t *color, MaterialHandle material);
    void *_slots38to68[31];
    int (*TextWidth)(const char *text, int maxChars, FontHandle font);
    int (*TextHeight)(FontHandle font);
    void (*DrawText)(const char *text, int maxChars, FontHandle font,
                     float x, float y, float xScale, float yScale,
                     const vec_t *color, int style);
} refexport_t;

extern MaterialHandle Scr_Debugger_WhiteMaterial(void);
extern FontHandle Scr_Debugger_ConsoleFont(void);

extern void Com_Printf(const char *fmt, ...);
extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern void I_strncpyz(char *dst, const char *src, int len);
extern int I_stricmp(const char *a, const char *b);
extern char *CopyStringInternal(const char *s);
extern char *va(const char *fmt, ...);
extern void Com_sprintf(char *dst, int size, const char *fmt, ...);
extern int sscanf(const char *s, const char *fmt, ...);
extern void qsort(void *base, int n, int size,
                  int (*cmp)(const void *, const void *));

extern void *imp_re;
extern void *imp_cls;

extern int Key_IsDown(int key);

extern int FS_FOpenTextFileWrite(const char *filename);
extern void FS_Write(const void *data, int len, int handle);
extern void FS_FCloseFile(int handle);

extern char *Sys_GetClipboardData(void);
extern void Sys_SetClipboardData(const char *text);

extern int Sys_IsRemoteDebugServer(void);

extern int Scr_GetSourceBuffer(const char *codePos);
extern int Scr_GetSourcePos(int bufIndex, const char *codePos, char *out, int outLen);
extern int Scr_GetLineNum(int bufIndex, const char *codePos);
extern void Scr_SelectScriptLine(int bufIndex, int line);

extern void CL_LookupColor(int index, unsigned char *out);

extern int Scr_Debugger_GetScriptWindowCount(void);
extern Scr_ScriptWindow **Scr_Debugger_GetScriptWindows(void);

extern Scr_AbstractScriptList *Scr_Debugger_GetScriptList(void);
extern UI_LinesComponent *Scr_Debugger_GetSourceLines(void);

static void List_DrawBox(float x, float y, float w, float h, const unsigned char *rgba)
{
    refexport_t *re = (refexport_t *)imp_re;
    float fcolor[4];

    fcolor[0] = (float)rgba[0] / 255.0f;
    fcolor[1] = (float)rgba[1] / 255.0f;
    fcolor[2] = (float)rgba[2] / 255.0f;
    fcolor[3] = (float)rgba[3] / 255.0f;

    re->DrawStretchPic(x, y, w, h, 0, 0, 0, 0, fcolor, Scr_Debugger_WhiteMaterial());
}

static void List_DrawText(const char *text, int maxChars, float x, float y,
                          const unsigned char *rgba, int style)
{
    refexport_t *re = (refexport_t *)imp_re;
    float fcolor[4];

    fcolor[0] = (float)rgba[0] / 255.0f;
    fcolor[1] = (float)rgba[1] / 255.0f;
    fcolor[2] = (float)rgba[2] / 255.0f;
    fcolor[3] = (float)rgba[3] / 255.0f;

    re->DrawText(text, maxChars, Scr_Debugger_ConsoleFont(), x, y, 1.0f, 1.0f, fcolor, style);
}

static float List_TextHeight(void)
{
    refexport_t *re = (refexport_t *)imp_re;
    return (float)re->TextHeight(Scr_Debugger_ConsoleFont());
}

void Scr_AbstractScriptList_Init(Scr_AbstractScriptList *self)
{
    UI_LinesComponent_Init((UI_LinesComponent *)self);
    self->entryCount = 0;
    self->entries = 0;
}

void Scr_AbstractScriptList_Shutdown(Scr_AbstractScriptList *self)
{
    if (self->entries) {
        Z_FreeInternal(self->entries);
        self->entries = 0;
    }
}

void Scr_AbstractScriptList_Draw(Scr_AbstractScriptList *self,
                                 float x, float y, float w, float h,
                                 float a, float b)
{
    UI_LinesComponent *lc = (UI_LinesComponent *)self;
    unsigned char fg[4];
    unsigned char sel[4];
    float lineH;
    int first, line;
    int i;

    (void)a;
    (void)b;

    {
        refexport_t *re = (refexport_t *)imp_re;
        float black[4] = { 0, 0, 0, 0 };
        re->DrawStretchPic(x, y, w, h, 0, 0, 0, 0, black, Scr_Debugger_WhiteMaterial());
    }

    CL_LookupColor(55, fg);
    CL_LookupColor(55, sel);

    lineH = List_TextHeight();
    if (lineH <= 0.0f)
        return;

    first = (int)(b / lineH);
    if ((float)first < lc->scrollTop)
        first++;

    for (i = first; i < self->entryCount; i++) {
        float rowY = y + (float)i * lineH;
        const char *name;

        if (rowY > y + h)
            break;

        if (i == lc->selectedLine) {
            List_DrawBox(x, rowY, w, lineH, sel);
        }

        name = Scr_ScriptWindow_GetFilename(self->entries[i]);
        if (name && name[0]) {
            List_DrawText(name, 0x7FFFFFFF, x, rowY, fg, 0);
        }
    }
    (void)line;
    (void)first;
}

void Scr_AbstractScriptList_AddEntry(Scr_AbstractScriptList *self,
                                     Scr_ScriptWindow *win, int select)
{
    int insertAt;
    int count = self->entryCount;
    int i;
    Scr_ScriptWindow **dst;

    if (select) {
        insertAt = self->selectedLine;
        if (insertAt < 0)
            insertAt = self->entryCount;
    } else {
        insertAt = self->entryCount;
    }

    if (count > 0) {
        Scr_ScriptWindow **cur = self->entries;
        for (i = 0; i < count; i++) {
            if (cur[i] == win) {
                if (insertAt <= i) {

                    memmove(&self->entries[insertAt + 1], &self->entries[insertAt],
                            (i - insertAt) * 4);
                } else {

                    insertAt--;
                    memmove(&self->entries[i], &self->entries[i + 1],
                            (insertAt - i) * 4);
                }
                self->entries[insertAt] = win;
                if (!Sys_IsRemoteDebugServer())
                    self->vtbl->SetSelectedLineFocus(self, insertAt, 1);
                return;
            }
        }
    }

    {
        int newCount = count + 1;
        Scr_ScriptWindow **nw = Z_MallocInternal(newCount * 4);
        Scr_ScriptWindow **old = self->entries;

        if (old) {

            memcpy(nw, old, self->entryCount * 4);
            memcpy((char *)nw + self->entryCount * 4 + 4,
                   (char *)old + self->entryCount * 4,
                   (self->entryCount - insertAt) * 4);
            Z_FreeInternal(self->entries);
        }
        self->entries = nw;
        self->entryCount = newCount;

        {
            const char *name = Scr_ScriptWindow_GetFilename(win);
            int len = 0;
            if (name) {
                while (name[len])
                    len++;
            }
            (void)len;
        }
        UI_LinesComponent_UpdateHeight((UI_LinesComponent *)self);
    }

    self->entries[insertAt] = win;
    if (!Sys_IsRemoteDebugServer())
        self->vtbl->SetSelectedLineFocus(self, insertAt, 1);
}

int Scr_AbstractScriptList_AddEntryName(Scr_AbstractScriptList *self,
                                        const char *name, int select)
{
    int count = Scr_Debugger_GetScriptWindowCount();
    Scr_ScriptWindow **windows = Scr_Debugger_GetScriptWindows();
    int i;

    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        Scr_ScriptWindow *win = windows[i];
        const char *fn = Scr_ScriptWindow_GetFilename(win);
        if (I_stricmp(fn, name) == 0) {
            Scr_AbstractScriptList_AddEntry(self, win, select);
            return 1;
        }
    }
    return 0;
}

void Scr_AbstractScriptList_DeleteEntryInternal(Scr_AbstractScriptList *self)
{
    int sel;

    if (Sys_IsRemoteDebugServer())
        return;

    sel = self->selectedLine;
    if (sel < 0)
        return;

    {
        Scr_ScriptWindow **at = &self->entries[sel];
        memmove(at, at + 1, (self->entryCount - sel - 1) * 4);
        self->entryCount--;
        UI_LinesComponent_UpdateHeight((UI_LinesComponent *)self);
    }
}

void Scr_AbstractScriptList_DeleteEntry(Scr_AbstractScriptList *self)
{
    Scr_AbstractScriptList_DeleteEntryInternal(self);
    self->vtbl->SetSelectedLineFocus(self, self->selectedLine, 1);
}

void Scr_AbstractScriptList_BackspaceEntry(Scr_AbstractScriptList *self)
{
    Scr_AbstractScriptList_DeleteEntryInternal(self);
    self->vtbl->SetSelectedLineFocus(self, self->selectedLine - 1, 1);
}

void Scr_AbstractScriptList_CopyEntry(Scr_AbstractScriptList *self)
{
    int sel;

    if (Sys_IsRemoteDebugServer())
        return;

    sel = self->selectedLine;
    if (sel < 0)
        return;

    Sys_SetClipboardData(Scr_ScriptWindow_GetFilename(self->entries[sel]));
}

void Scr_AbstractScriptList_PasteEntry(Scr_AbstractScriptList *self)
{
    char *clip;

    if (Sys_IsRemoteDebugServer())
        return;

    clip = Sys_GetClipboardData();
    if (!clip)
        return;

    if (clip[0])
        Scr_AbstractScriptList_AddEntryName(self, clip, 1);

    Z_FreeInternal(clip);
}

int Scr_CompareScriptWindowsNames(const void *a, const void *b)
{
    return I_stricmp(*(const char *const *)a, *(const char *const *)b);
}

int Scr_ScriptList_KeyEvent(Scr_ScriptList *self, float *pos, int key)
{

    if (Key_IsDown(158) || Key_IsDown(159) || Key_IsDown(160))
        return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);

    if (key != 13 && key != 191 && key != 223)
        return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);

    {
        int sel = self->selectedLine;
        if (sel >= 0) {
            Scr_AbstractScriptList *dst = Scr_Debugger_GetScriptList();
            Scr_AbstractScriptList_AddEntry(dst, self->entries[sel], 1);
        }
    }
    return 1;
}

void Scr_ScriptList_LoadScriptPos(Scr_ScriptList *self)
{
    extern void *Scr_GetStringList(const char *name, int *outFreeFlag);
    extern struct Scr_StringNode_s *Scr_StringNode_Text(void *node);

    static const char *kPosStore = "scriptdebugger_scriptpos";
    static const char *kPosFormat = "%d %d %d %d";

    int freeFlag = 0;
    void *list = Scr_GetStringList(kPosStore, &freeFlag);
    char *line;
    int i;

    struct StrNode {
        char *text;
        struct StrNode *next;
    };
    struct StrNode *node = (struct StrNode *)list;

    while (node) {
        struct StrNode *next = node->next;
        int v_line = -1, v_a = 0, v_b = 0, v_file = 0;

        sscanf(node->text, kPosFormat, &v_file, &v_line, &v_a, &v_b);

        line = node->text;

        for (i = 0; i < self->entryCount; i++) {
            Scr_ScriptWindow *win = self->entries[i];
            const char *fn = Scr_ScriptWindow_GetFilename(win);

            if (I_stricmp(fn, line) == 0) {
                int wantLine = v_line;
                if (wantLine < -1 || wantLine >= win->lineCount)
                    wantLine = -1;
                win->selectedLine = wantLine;
                win->hasFocus = 0;
                win->focusFromMouse = 0;
                win->scrollTop = (float)v_a;
                win->h = (float)v_b;
                break;
            }
        }

        Z_FreeInternal(node);
        node = next;
    }

    if (freeFlag)
        Z_FreeInternal(list);
    (void)kPosStore;
}

void Scr_ScriptList_Shutdown(Scr_ScriptList *self)
{
    static const char *kPosStore = "scriptdebugger_scriptpos";
    static const char *kPosFormat = "%s %d %d\n";
    int handle = FS_FOpenTextFileWrite(kPosStore);
    int i;

    if (handle) {
        for (i = 0; i < self->entryCount; i++) {
            Scr_ScriptWindow *win = self->entries[i];
            const char *fn = Scr_ScriptWindow_GetFilename(win);
            if (fn && fn[0]) {
                char *rec = va(kPosFormat, win->currentLine,
                               (int)win->scrollTop);
                int len = 0;
                while (rec[len])
                    len++;
                FS_Write(rec, len, handle);
            }
        }
        FS_FCloseFile(handle);
    }

    {
        extern void *Scr_Debugger_GetSourceScratch(void);
        Z_FreeInternal(Scr_Debugger_GetSourceScratch());
    }

    for (i = 0; i < self->entryCount; i++) {
        Scr_ScriptWindow *win = self->entries[i];
        if (win)
            win->vtbl->dtor(win, 0);
    }

    if (self->entries) {
        Z_FreeInternal(self->entries);
        self->entries = 0;
    }
    (void)kPosStore;
}

void Scr_ScriptList_AddFile(Scr_ScriptList *self, const char *filename,
                            struct Scr_AddFileInfo *info);

void Scr_ScriptList_Init(Scr_ScriptList *self);

int Scr_OpenScriptList_SetSelectedLineFocus(Scr_OpenScriptList *self,
                                            int line, int fromMouse)
{
    if (!UI_LinesComponent_SetSelectedLineFocus((UI_LinesComponent *)self,
                                                line, fromMouse))
        return 0;

    if (line < 0)
        return 1;

    {
        Scr_ScriptWindow *win = self->entries[line];
        int fileIndex = win->scriptFileIndex;
        if (fileIndex < 0)
            return 1;

        UI_LinesComponent_SetSelectedLineFocus(Scr_Debugger_GetSourceLines(),
                                               fileIndex, fromMouse);
    }
    return 1;
}

int Scr_OpenScriptList_KeyEvent(Scr_OpenScriptList *self, float *pos, int key)
{
    int ctrl = Key_IsDown(158) || Key_IsDown(159);
    int shift = Key_IsDown(160);

    if (!ctrl && !shift) {
        if (key == 127) {
            Scr_AbstractScriptList_BackspaceEntry((Scr_AbstractScriptList *)self);
            return 1;
        }
        if (key != 162)
            return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);
        Scr_AbstractScriptList_DeleteEntry((Scr_AbstractScriptList *)self);
        return 1;
    }

    if (!ctrl && shift) {
        if (key == 161) {
            Scr_AbstractScriptList_PasteEntry((Scr_AbstractScriptList *)self);
            return 1;
        }
        if (key != 162)
            return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);
        Scr_AbstractScriptList_CopyEntry((Scr_AbstractScriptList *)self);
        Scr_AbstractScriptList_DeleteEntry((Scr_AbstractScriptList *)self);
        return 1;
    }

    if (ctrl && !shift && (unsigned)(key - 99) <= 62) {
        Scr_AbstractScriptList_CopyEntry((Scr_AbstractScriptList *)self);
        return 1;
    }

    return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);
}

int Scr_OpenScriptList_ReadFromFile(Scr_OpenScriptList *self)
{
    extern void *Scr_GetStringList(const char *name, int *outFreeFlag);
    static const char *kOpenStore = "scriptdebugger_openscripts";

    struct StrNode {
        char *text;
        struct StrNode *next;
    };

    int freeFlag = 0;
    void *list = Scr_GetStringList(kOpenStore, &freeFlag);
    int ok = (list != 0);
    struct StrNode *node = (struct StrNode *)list;
    int selectTo = Scr_Debugger_GetScriptWindowCount();

    self->savedFileList = 0;

    while (node) {
        struct StrNode *next = node->next;

        if (Scr_AbstractScriptList_AddEntryName((Scr_AbstractScriptList *)self,
                                                node->text, 0)) {

            Z_FreeInternal(node);
        } else {

            node->text = CopyStringInternal(node->text);
            node->next = (struct StrNode *)self->savedFileList;
            self->savedFileList = node;
        }
        node = next;
    }

    if (!Sys_IsRemoteDebugServer())
        UI_LinesComponent_SetSelectedLineFocus(Scr_Debugger_GetSourceLines(),
                                               selectTo, 0);

    if (freeFlag)
        Z_FreeInternal(list);
    return ok;
}

void Scr_OpenScriptList_Init(Scr_OpenScriptList *self)
{
    UI_LinesComponent_Init((UI_LinesComponent *)self);
    self->entryCount = 0;
    self->entries = 0;
    Scr_OpenScriptList_ReadFromFile(self);
}

void Scr_OpenScriptList_Shutdown(Scr_OpenScriptList *self)
{
    static const char *kOpenStore = "scriptdebugger_openscripts";
    struct StrNode {
        char *text;
        struct StrNode *next;
    };

    int handle = FS_FOpenTextFileWrite(kOpenStore);

    if (handle) {
        const char nl = '\n';
        int i;

        {
            struct StrNode *n = (struct StrNode *)self->savedFileList;
            while (n) {
                struct StrNode *next = n->next;
                char *name = n->text;
                int len = 0;
                while (name[len])
                    len++;
                FS_Write(name, len, handle);
                FS_Write(&nl, 1, handle);
                Z_FreeInternal(n->text);
                Z_FreeInternal(n);
                n = next;
            }
            self->savedFileList = 0;
        }

        for (i = 0; i < self->entryCount; i++) {
            const char *fn = Scr_ScriptWindow_GetFilename(self->entries[i]);
            if (fn && fn[0]) {
                int len = 0;
                while (fn[len])
                    len++;
                FS_Write(fn, len, handle);
                FS_Write(&nl, 1, handle);
            }
        }
        FS_FCloseFile(handle);
    }

    if (self->entries) {
        Z_FreeInternal(self->entries);
        self->entries = 0;
    }
    (void)kOpenStore;
}

extern int Scr_VM_GetCallStackDepth(void);
extern const char *Scr_VM_GetCallStackCodePos(int frameFromTop, int *outIsTopFrame);
extern int Scr_VM_GetFunctionCodePos(void);
extern int Scr_GetPrevSourcePos(const char *codePos, int isTopFrame);

void Scr_ScriptCallStack_Draw(Scr_ScriptCallStack *self,
                              float x, float y, float w, float h,
                              float a, float b)
{
    UI_LinesComponent *lc = (UI_LinesComponent *)self;
    unsigned char fg[4];
    unsigned char sel[4];
    float lineH;
    int first, i;

    (void)a;

    {
        refexport_t *re = (refexport_t *)imp_re;
        float black[4] = { 0, 0, 0, 0 };
        re->DrawStretchPic(x, y, w, h, 0, 0, 0, 0, black, Scr_Debugger_WhiteMaterial());
    }

    CL_LookupColor(55, fg);
    CL_LookupColor(55, sel);

    lineH = List_TextHeight();
    if (lineH <= 0.0f)
        return;

    first = (int)(b / lineH);
    if ((float)first < lc->scrollTop)
        first++;

    for (i = first; i < self->frameCount; i++) {
        float rowY = y + (float)i * lineH;
        char label[129];

        if (rowY > y + h)
            break;

        if (i == lc->selectedLine) {
            List_DrawBox(x, rowY, w, lineH, sel);
        }

        if (self->frames[i].sourceBuffer < 0) {
            I_strncpyz(label, "<unknown>", 129);
        } else {
            Scr_GetSourcePos(self->frames[i].sourceBuffer,
                             (const char *)self->frames[i].sourcePos,
                             label, 129);
        }

        {
            int len = 0;
            while (label[len])
                len++;
            if (len)
                List_DrawText(label, len, x, rowY, fg, 0);
        }
    }
}

int Scr_ScriptCallStack_KeyEvent(Scr_ScriptCallStack *self, float *pos, int key)
{
    return UI_LinesComponent_KeyEvent((UI_LinesComponent *)self, pos, key);
}

void Scr_ScriptCallStack_Init(Scr_ScriptCallStack *self)
{
    refexport_t *re = (refexport_t *)imp_re;
    int charW, charH, cols;

    UI_LinesComponent_Init((UI_LinesComponent *)self);

    charW = re->TextWidth("W", 1, Scr_Debugger_ConsoleFont());
    charH = re->TextHeight(Scr_Debugger_ConsoleFont());
    cols = 64;

    self->w = (float)(charW * cols + charH);
    self->h = (float)charH;
}

void Scr_ScriptCallStack_UpdateStack(Scr_ScriptCallStack *self)
{
    int depth = Scr_VM_GetCallStackDepth();
    int i;

    if (depth == 0) {
        self->frameCount = 0;
        return;
    }

    self->frameCount = depth + 1;
    if (depth < 0)
        return;

    for (i = 0; i <= depth; i++) {
        const char *codePos;
        int isTop = 0;

        if (i != 0) {
            codePos = Scr_VM_GetCallStackCodePos(i, &isTop);
        } else {

            codePos = (const char *)(unsigned long)(Scr_VM_GetFunctionCodePos() + 1);
            isTop = 0;
        }

        if ((unsigned long)codePos != (unsigned long)0u - 1u && codePos != 0) {
            self->frames[i].sourceBuffer = Scr_GetSourceBuffer(codePos - 1);
            self->frames[i].sourcePos =
                (int)(unsigned long)Scr_GetPrevSourcePos(codePos - 1, isTop);
        } else {
            self->frames[i].sourceBuffer = -1;
            self->frames[i].sourcePos = 0;
        }
    }
}

int Scr_ScriptCallStack_SetSelectedLineFocus(Scr_ScriptCallStack *self,
                                             int line, int fromMouse)
{
    if (!UI_LinesComponent_SetSelectedLineFocus((UI_LinesComponent *)self,
                                                line, fromMouse))
        return 0;

    if (line >= 0) {
        int buf = self->frames[line].sourceBuffer;
        if (buf >= 0) {
            int pos = self->frames[line].sourcePos;
            Scr_SelectScriptLine(buf, Scr_GetLineNum(buf, (const char *)(unsigned long)pos));
        }
    }
    return 1;
}

extern void Scr_AbstractScriptList_dtor(Scr_AbstractScriptList *self, unsigned int flags);
extern void Scr_ScriptList_dtor(Scr_ScriptList *self, unsigned int flags);
extern void Scr_OpenScriptList_dtor(Scr_OpenScriptList *self, unsigned int flags);
extern void Scr_ScriptCallStack_dtor(Scr_ScriptCallStack *self, unsigned int flags);

extern UI_Component *UI_Component_GetCompAtLocation(UI_Component *self, float *pos);

const Scr_AbstractScriptList_vtbl Scr_AbstractScriptList_vtable = {
     Scr_AbstractScriptList_Init,
     Scr_AbstractScriptList_Shutdown,
     Scr_AbstractScriptList_dtor,
     Scr_AbstractScriptList_Draw,
     (int (*)(Scr_AbstractScriptList *, float *, int))UI_LinesComponent_KeyEvent,
     (UI_Component *(*)(Scr_AbstractScriptList *, float *))UI_Component_GetCompAtLocation,
     (void (*)(Scr_AbstractScriptList *, const char *))UI_LinesComponent_AddText,
     (int (*)(Scr_AbstractScriptList *, int, int))UI_LinesComponent_SetSelectedLineFocus,
};

const Scr_ScriptList_vtbl Scr_ScriptList_vtable = {
     Scr_ScriptList_Init,
     Scr_ScriptList_Shutdown,
     Scr_ScriptList_dtor,
     (void (*)(Scr_ScriptList *, float, float, float, float, float, float))Scr_AbstractScriptList_Draw,
     Scr_ScriptList_KeyEvent,
     (UI_Component *(*)(Scr_ScriptList *, float *))UI_Component_GetCompAtLocation,
     (void (*)(Scr_ScriptList *, const char *))UI_LinesComponent_AddText,
     (int (*)(Scr_ScriptList *, int, int))UI_LinesComponent_SetSelectedLineFocus,
};

const Scr_OpenScriptList_vtbl Scr_OpenScriptList_vtable = {
     Scr_OpenScriptList_Init,
     Scr_OpenScriptList_Shutdown,
     Scr_OpenScriptList_dtor,
     (void (*)(Scr_OpenScriptList *, float, float, float, float, float, float))Scr_AbstractScriptList_Draw,
     Scr_OpenScriptList_KeyEvent,
     (UI_Component *(*)(Scr_OpenScriptList *, float *))UI_Component_GetCompAtLocation,
     (void (*)(Scr_OpenScriptList *, const char *))UI_LinesComponent_AddText,
     Scr_OpenScriptList_SetSelectedLineFocus,
};

const Scr_ScriptCallStack_vtbl Scr_ScriptCallStack_vtable = {
     Scr_ScriptCallStack_Init,
     (void (*)(Scr_ScriptCallStack *))UI_Component_Shutdown,
     Scr_ScriptCallStack_dtor,
     Scr_ScriptCallStack_Draw,
     Scr_ScriptCallStack_KeyEvent,
     (UI_Component *(*)(Scr_ScriptCallStack *, float *))UI_Component_GetCompAtLocation,
     (void (*)(Scr_ScriptCallStack *, const char *))UI_LinesComponent_AddText,
     Scr_ScriptCallStack_SetSelectedLineFocus,
};

#endif
