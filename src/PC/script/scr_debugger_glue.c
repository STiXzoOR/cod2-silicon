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

#    include <string.h>
#    include <stdlib.h>

extern void *imp_re;
extern void *imp_cls;
extern void *imp_scrVarPub;

extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);

extern struct scrVmPub_t scrVmPub;

extern struct scrParserPub_t scrParserPub;
extern struct scrParserGlob_t scrParserGlob;

extern qkey_t *keys;

extern unsigned char *Scr_GetProgramBuffer(void);
extern void Scr_AddManualBreakpoint(const char *filename, int line);

extern unsigned char Scr_GetBreakpointType(const char **pp);
extern void Scr_RemoveAssignmentBreakpoint(unsigned char *codePos);

extern int Scr_Debugger_CodePosToLine(const char *codePos, const char **filenameOut);
extern int Scr_GetSourceBuffer(const char *codePos);

extern int SDL_SetClipboardText(const char *text);

int Key_IsDown(int key)
{
    return keys && key >= 0 && key < 256 && keys[key].down;
}

float G_DEBUGGER_LINE_HEIGHT = 12.0f;
float G_DEBUGGER_TEXT_SCALE = 1.0f;
float G_DEBUGGER_WATCH_WIDTH = 256.0f;
float G_DEBUGGER_WATCH_HEIGHT = 240.0f;
int G_SCR_DEBUG_CONSOLE_MODE = 0;
float G_DEBUGGER_FADE_INV = 0.001f;
float drawColorScaleH = 2.0f;
float drawColorScaleW = 0.5f;

struct Material *g_debuggerSelectBarMaterial = 0;
struct Material *g_debuggerWatchBackground = 0;
struct Material *g_debuggerInputBackground = 0;

Scr_ScriptWatch g_scriptWatch;
Scr_ScriptWatch *scr_debuggerWatch = &g_scriptWatch;

static int g_debuggerVmStateBacking[2] = { 0, 0 };
int *scr_debuggerVmState = g_debuggerVmStateBacking;
int scr_debuggerConnected = 0;
int scr_debuggerStepping = 0;
int scr_debuggerStepLine = -1;
Scr_WatchElement_s *scr_debuggerWatchRoot = 0;
char scr_debuggerFindText[256] = { 0 };

FontHandle Scr_Debugger_ConsoleFont(void)
{
    return cls->consoleFont;
}

MaterialHandle Scr_Debugger_WhiteMaterial(void)
{
    return cls->whiteMaterial;
}

MaterialHandle Scr_GetBreakpointMarkerMaterial(int breakpointKind)
{
    (void)breakpointKind;
    return cls->whiteMaterial;
}

static Scr_ScriptList g_scriptList;
static void *g_sourceScratch;

Scr_AbstractScriptList *Scr_Debugger_GetScriptList(void)
{
    return (Scr_AbstractScriptList *)&g_scriptList;
}

Scr_ScriptWindow **Scr_Debugger_GetScriptWindows(void)
{
    return g_scriptList.entries;
}

int Scr_Debugger_GetScriptWindowCount(void)
{
    return g_scriptList.entryCount;
}

UI_LinesComponent *Scr_Debugger_GetSourceLines(void)
{
    return (UI_LinesComponent *)&g_scriptList;
}

void *Scr_Debugger_GetSourceScratch(void)
{
    return g_sourceScratch;
}

void Scr_AbstractScriptList_dtor(Scr_AbstractScriptList *self, unsigned int flags)
{
    self->vtbl->Shutdown(self);
    if (flags & 1u)
        Z_FreeInternal(self);
}

void Scr_ScriptList_dtor(Scr_ScriptList *self, unsigned int flags)
{
    self->vtbl->Shutdown(self);
    if (flags & 1u)
        Z_FreeInternal(self);
}

void Scr_OpenScriptList_dtor(Scr_OpenScriptList *self, unsigned int flags)
{
    self->vtbl->Shutdown(self);
    if (flags & 1u)
        Z_FreeInternal(self);
}

void Scr_ScriptCallStack_dtor(Scr_ScriptCallStack *self, unsigned int flags)
{
    self->vtbl->Shutdown(self);
    if (flags & 1u)
        Z_FreeInternal(self);
}

int Scr_VM_GetCallStackDepth(void)
{
    int count = scrVmPub.function_count;
    return count > 0 ? count - 1 : 0;
}

int Scr_VM_GetFunctionCodePos(void)
{
    int count = scrVmPub.function_count;
    if (count <= 0)
        return 0;
    return (int)(intptr_t)scrVmPub.function_frame_start[count - 1].fs.pos;
}

const char *Scr_VM_GetCallStackCodePos(int frameFromTop, int *outIsTopFrame)
{
    int count = scrVmPub.function_count;
    int idx;

    if (outIsTopFrame)
        *outIsTopFrame = 0;
    if (count <= 0 || frameFromTop < 0 || frameFromTop >= count)
        return 0;

    idx = count - 1 - frameFromTop;
    if (idx < 0)
        return 0;
    if (idx == 0 && outIsTopFrame)
        *outIsTopFrame = 1;
    return scrVmPub.function_frame_start[idx].fs.pos;
}

int Scr_GetLineNum(int bufIndex, const char *codePos)
{
    SourceBufferInfo *tbl = scrParserPub.sourceBufferLookup;
    const char *sourceBuf;
    unsigned int sourcePos = (unsigned int)(uintptr_t)codePos;
    unsigned int i;
    int line = 0;

    if (!tbl || bufIndex < 0 || (unsigned int)bufIndex >= scrParserPub.sourceBufferLookupLen)
        return 0;
    sourceBuf = tbl[bufIndex].sourceBuf;
    if (!sourceBuf || sourcePos == 0)
        return 0;
    for (i = 0; i != sourcePos; i++) {
        if (!sourceBuf[i])
            line++;
    }
    return line;
}

int Scr_GetSourcePos(int bufIndex, const char *codePos, char *out, int outLen)
{
    SourceBufferInfo *tbl = scrParserPub.sourceBufferLookup;
    const char *filename = "";
    int line;

    if (out && outLen > 0)
        out[0] = '\0';

    if (tbl && bufIndex >= 0 &&
        (unsigned int)bufIndex < scrParserPub.sourceBufferLookupLen &&
        tbl[bufIndex].buf)
        filename = tbl[bufIndex].buf;

    line = Scr_GetLineNum(bufIndex, codePos);

    if (out && outLen > 0) {

        int n = (int)strlen(filename);
        if (n >= outLen)
            n = outLen - 1;
        memcpy(out, filename, n);
        out[n] = '\0';
        {
            char num[32];
            int k = 0, m;
            num[k++] = '(';
            {
                char tmp[16];
                int t = 0;
                int v = line + 1;
                if (v == 0)
                    tmp[t++] = '0';
                while (v > 0 && t < 15) {
                    tmp[t++] = (char)('0' + v % 10);
                    v /= 10;
                }
                while (t > 0)
                    num[k++] = tmp[--t];
            }
            num[k++] = ')';
            num[k] = '\0';
            for (m = 0; num[m] && n < outLen - 1; m++)
                out[n++] = num[m];
            out[n] = '\0';
        }
    }
    return line;
}

int Scr_GetPrevSourcePos(const char *codePos, int isTopFrame)
{
    const char *fn = 0;
    int line;
    (void)isTopFrame;

    line = Scr_Debugger_CodePosToLine(codePos, &fn);
    return line;
}

char *Scr_GetSourcePosOfType(int fileIndex, int codePos, int type, int *outSourcePos)
{
    (void)fileIndex;
    (void)codePos;
    (void)type;
    if (outSourcePos)
        *outSourcePos = 0;
    return 0;
}

char *Scr_GetOpcodePosOfType(int fileIndex, int sourceStart, int sourceEnd,
                             int type, int *outSourcePos)
{
    (void)fileIndex;
    (void)sourceStart;
    (void)sourceEnd;
    (void)type;
    if (outSourcePos)
        *outSourcePos = 0;
    return 0;
}

const char *Scr_GetClosestSourcePosOfType(int bufIndex, int sourcePos, int type)
{
    (void)bufIndex;
    (void)sourcePos;
    (void)type;
    return 0;
}

void Scr_SelectScriptLine(int bufIndex, int line)
{
    UI_LinesComponent *src = Scr_Debugger_GetSourceLines();
    (void)bufIndex;
    if (src && src->vtbl && src->vtbl->SetSelectedLineFocus)
        src->vtbl->SetSelectedLineFocus(src, line, 0);
}

extern int FS_FOpenFileByMode(const char *filename, fileHandle_t *file, int mode);
extern int FS_Read(void *buffer, int len, fileHandle_t h);
extern void FS_FCloseFile(fileHandle_t h);

void *Scr_GetStringList(const char *name, int *outFreeFlag)
{
    fileHandle_t h = 0;
    int len, i;
    char *buf;

    if (outFreeFlag)
        *outFreeFlag = 0;
    len = FS_FOpenFileByMode(name, &h, 0 );
    if (len < 0)
        return 0;

    buf = (char *)Z_MallocInternal(len + 2);
    if (len > 0)
        FS_Read(buf, len, h);
    FS_FCloseFile(h);
    buf[len] = '\0';
    buf[len + 1] = '\0';

    for (i = 0; i < len; i++) {
        if (buf[i] == '\r' || buf[i] == '\n')
            buf[i] = '\0';
    }
    if (outFreeFlag)
        *outFreeFlag = 1;
    return buf;
}

static UI_LinesComponent *g_miscScrollPaneComp;
void Scr_SetMiscScrollPaneComp(UI_LinesComponent *comp)
{
    g_miscScrollPaneComp = comp;
}

void Scr_ScriptList_Init(Scr_ScriptList *self)
{
    Scr_AbstractScriptList_Init((Scr_AbstractScriptList *)self);
}

int Scr_SortWatchElementCompare(const void *a, const void *b)
{
    Scr_WatchElement_s *ea = *(Scr_WatchElement_s *const *)a;
    Scr_WatchElement_s *eb = *(Scr_WatchElement_s *const *)b;
    int ka = *(int *)((char *)ea + 0x1A0);
    int kb = *(int *)((char *)eb + 0x1A0);
    return ka - kb;
}

int Scr_ScriptWatch_MouseClickDepth(Scr_ScriptWatch *self, float *pos)
{
    float charW;
    (void)self;
    if (!pos)
        return 0;
    charW = G_DEBUGGER_LINE_HEIGHT * 0.5f;
    if (charW <= 0.0f)
        return 0;
    {
        int col = (int)(pos[0] / charW);
        return col < 0 ? 0 : col;
    }
}

void Scr_SetTempBreakpoint(const char *codePos, int remote)
{
    const char *fn = 0;
    int line;
    if (!codePos || remote)
        return;
    line = Scr_Debugger_CodePosToLine(codePos, &fn);
    if (fn && line > 0)
        Scr_AddManualBreakpoint(fn, line);
}

void Scr_FreeLineBreakpoint(Scr_Breakpoint *bp)
{
    if (!bp)
        return;
    if (bp->codePos) {
        unsigned char *cp = (unsigned char *)(intptr_t)bp->codePos;

        if (Scr_GetProgramBuffer())
            Scr_RemoveAssignmentBreakpoint(cp);
    }
    Z_FreeInternal(bp);
}

static int g_breakOnAllAssignments;
void Scr_BreakOnAllAssignmentPos(void)
{
    g_breakOnAllAssignments = 1;
}
void Scr_UnbreakAllAssignmentPos(void)
{
    g_breakOnAllAssignments = 0;
}

extern void Con_ToggleConsole_f(void);
static int g_dbgConsoleOpen;

void Con_OpenConsole(void)
{
    if (!g_dbgConsoleOpen) {
        Con_ToggleConsole_f();
        g_dbgConsoleOpen = 1;
    }
}

void Con_OpenConsoleOutput(void)
{
    Con_OpenConsole();
}

void Sys_SetClipboardData(const char *text)
{
    if (text)
        SDL_SetClipboardText(text);
}

int Sys_IsRemoteDebugServer(void)
{
    return 0;
}

#    define SCR_DBG_MSG_MAX 4096
static unsigned char g_dbgMsgBuf[SCR_DBG_MSG_MAX];
static int g_dbgMsgLen;

static void Sys_PutDebugSocketByte(unsigned char b)
{
    if (g_dbgMsgLen < SCR_DBG_MSG_MAX)
        g_dbgMsgBuf[g_dbgMsgLen++] = b;
}

void Sys_WriteDebugSocketMessageType(int type)
{

    g_dbgMsgLen = 0;
    Sys_PutDebugSocketByte((unsigned char)(type & 0xFF));
}

void Sys_WriteDebugSocketInt(int value)
{

    Sys_PutDebugSocketByte((unsigned char)((value >> 24) & 0xFF));
    Sys_PutDebugSocketByte((unsigned char)((value >> 16) & 0xFF));
    Sys_PutDebugSocketByte((unsigned char)((value >> 8) & 0xFF));
    Sys_PutDebugSocketByte((unsigned char)(value & 0xFF));
}

void Sys_WriteDebugSocketString(const char *str)
{

    int len = str ? (int)strlen(str) : 0;
    int i;
    Sys_WriteDebugSocketInt(len);
    for (i = 0; i < len; i++)
        Sys_PutDebugSocketByte((unsigned char)str[i]);
}

void Sys_EndWriteDebugSocket(void)
{

    g_dbgMsgLen = 0;
}

int Sys_ReadDebugSocketInt(void)
{

    return 0;
}

#endif
