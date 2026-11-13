#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>

#include "PC/gfx_d3d/r_rendercmds.c"

GfxCmdArray *s_cmdList;
int cod2_native_trace_enabled;
static dvar_t specularScale;
const dvar_t *r_specularColorScale = &specularScale;
refimport_t ri;
DxGlobals dx;
void *imp_dx = &dx;
r_globals_t rg;
GfxBackEndData *frontEndDataOut;
GfxDebugFrameGlob s_debugFrameGlob;

/* The CoD2x wrapper reaches the ordinary producer for these plain strings.
 * IP hiding and extended text parsing have their own fixtures. */
#if defined(COD2_CODX) && COD2_CODX
const char *Cod2x_HideIpText(const char *text, char *buffer, size_t size)
{ (void)buffer; (void)size; return text; }
int R_Cod2xDrawText(const char *text, int maxChars, FontHandle font,
                   float x, float y, float xs, float ys, const vec_t *color,
                   int style, int cursorPos, int cursor)
{
    (void)text; (void)maxChars; (void)font; (void)x; (void)y;
    (void)xs; (void)ys; (void)color; (void)style; (void)cursorPos; (void)cursor;
    return 0;
}
#endif

void R_ConvertColorToBytes(const vec_t *color, byte *bytes)
{
    int i;
    for (i = 0; i < 4; ++i)
        bytes[i] = (byte)(color[i] * 255);
}

_Static_assert(offsetof(GfxCmdCall, subCmd) == 8, "STABS i386 subCmd at 4 gains native pointer alignment");
_Static_assert(sizeof(GfxCmdSetLightProperties) == 80, "STABS 76 grows by one pointer");
_Static_assert(sizeof(GfxCmdBeginView) == 64, "STABS 48: sceneDef and viewParms pointers grow");
_Static_assert(offsetof(GfxCmdDrawText, text) == 52, "native text payload offset");
_Static_assert(offsetof(GfxCmdDrawTextInSpace, text) == 52, "native space-text payload offset");

typedef struct {
    int physical, critical, logical, logicalCritical;
} Counts;

static Counts Count(const GfxCmdArray *list)
{
    return (Counts){list->usedTotal, list->usedCritical,
                    list->logicalUsedTotal, list->logicalUsedCritical};
}

static void CheckCounts(const GfxCmdArray *list, Counts expected)
{
    assert(list->usedTotal == expected.physical);
    assert(list->usedCritical == expected.critical);
    assert(list->logicalUsedTotal == expected.logical);
    assert(list->logicalUsedCritical == expected.logicalCritical);
}

static void CheckRejected(const GfxCmdArray *list, Counts before)
{
    CheckCounts(list, before);
    assert(!list->lastCmd);
}

static const GfxCmdHeader *HeaderAt(const GfxCmdArray *list, int used, int offset)
{
    assert(offset >= 0 && !(offset & 7) && offset <= used - 8);
    const GfxCmdHeader *header = (const void *)(list->cmds + offset);
    assert(header->byteCount >= 8 && !(header->byteCount & 7));
    assert(header->byteCount <= used - offset);
    return header;
}

static int WalkHeaders(const GfxCmdArray *list, int used)
{
    int offset = 0, count = 0;
    while (offset < used) {
        offset += HeaderAt(list, used, offset)->byteCount;
        ++count;
    }
    assert(offset == used);
    return count;
}

/* Independent command-stream oracle, not the renderer backend. Check native
 * header strides, call targets and return addresses without drawing anything. */
static void WalkControlFlow(const GfxCmdArray *list, int used, const char *textOrder)
{
    int offset = 0, returns[8], depth = 0, textCount = 0, steps = 0;
    for (;;) {
        assert(++steps <= 100000);
        const GfxCmdHeader *header = HeaderAt(list, used, offset);
        int next = offset + header->byteCount;
        if (!header->id) {
            assert(!depth && !textOrder[textCount]);
            return;
        }
        if (header->id == 1 || header->id == 2) {
            const GfxCmdCall *call = (const void *)header;
            uintptr_t target = (uintptr_t)call->subCmd;
            uintptr_t base = (uintptr_t)list->cmds;
            assert(target >= base && target < base + used && !((target - base) & 7));
            if (header->id == 2) {
                assert(depth < 8);
                returns[depth++] = next;
            }
            offset = (int)(target - base);
        } else if (header->id == 3) {
            assert(depth > 0);
            offset = returns[--depth];
        } else {
            if (header->id == 21) {
                const GfxCmdDrawText *text = (const void *)header;
                assert(textOrder[textCount] && text->text[0] == textOrder[textCount++]);
                assert(text->text[1] == '\0');
            }
            offset = next;
        }
    }
}

static const vec4_t textColor = {0, 0.25f, 0.5f, 1};
static const byte textBytes[4] = {0, 63, 127, 255};
static const vec3_t textOrg = {1, 2, 3};
static const vec3_t textX = {0.25f, 0.5f, 0.75f};
static const vec3_t textY = {-1, -2, -3};
static Font_s textFont;

static void EmitText(const char *text, int space)
{
    if (space)
        R_AddCmdDrawTextInSpace(text, &textFont, textOrg, textX, textY, textColor);
    else
        R_AddCmdDrawTextWithCursor(text, 3, &textFont, 10.25f, -20.5f,
                                  0.75f, 1.25f, textColor, 7, 0, '|');
}

static void CheckText(const GfxCmdHeader *header, const char *text, int space)
{
    int len = (int)strlen(text);
    assert(header->id == (space ? 22 : 21));
    assert(header->byteCount == ((52 + len + 1 + 7) & ~7));
    if (space) {
        const GfxCmdDrawTextInSpace *cmd = (const void *)header;
        assert(cmd->font == &textFont);
        assert(!memcmp(cmd->org, textOrg, sizeof(textOrg)));
        assert(!memcmp(cmd->xPixelStep, textX, sizeof(textX)));
        assert(!memcmp(cmd->yPixelStep, textY, sizeof(textY)));
        assert(!memcmp(cmd->color.array, textBytes, sizeof(textBytes)));
        assert(!memcmp(cmd->text, text, len + 1));
    } else {
        const GfxCmdDrawText *cmd = (const void *)header;
        assert(cmd->font == &textFont && cmd->x == 10.25f && cmd->y == -20.5f);
        assert(cmd->xScale == 0.75f && cmd->yScale == 1.25f);
        assert(cmd->style == 7 && cmd->maxChars == 3 && cmd->cursorPos == 0 && cmd->cursor == '|');
        assert(!memcmp(cmd->color.array, textBytes, sizeof(textBytes)));
        assert(!memcmp(cmd->text, text, len + 1));
    }
    /* Producers must stop at the NUL, not overwrite the native padding. */
    for (int i = 52 + len + 1; i < header->byteCount; ++i)
        assert(((const byte *)header)[i] == 0xa5);
}

static void CheckTextProducers(void)
{
    static GfxCmdArray list;
    s_cmdList = &list;
    memset(list.cmds, 0xa5, sizeof(list.cmds));
    R_ResetCmdArray(&list);
    char text[18];
    int records = 0;
    for (int len = 0; len <= 16; ++len) {
        memset(text, 'a' + len, len);
        text[len] = '\0';
        for (int space = 0; space < 2; ++space) {
            Counts before = Count(&list);
            GfxCmdHeader *previous = list.lastCmd;
            EmitText(text, space);
            if (!len && space) {
                CheckCounts(&list, before);
                assert(list.lastCmd == previous);
                continue;
            }
            CheckText((const void *)(list.cmds + before.physical), text, space);
            int logical = ((space ? 48 : 44) + len + 1 + 3) & ~3;
            CheckCounts(&list, (Counts){before.physical + ((52 + len + 8) & ~7),
                                       0, before.logical + logical, 0});
            ++records;
            R_AddCmdTouchAllImages();
            assert(list.lastCmd->id == 33 && list.lastCmd->byteCount == 8);
            ++records;
        }
    }
    Counts before = Count(&list);
    GfxCmdHeader *previous = list.lastCmd;
    R_AddCmdDrawTextWithCursor("", 0, &textFont, 0, 0, 1, 1, textColor, 0, -1, 0);
    CheckCounts(&list, before);
    assert(list.lastCmd == previous);
    assert(WalkHeaders(&list, list.usedTotal) == records);

    /* Real producers at the last accepted logical byte. The following command
     * must fail without changing either budget or the preceding payload. */
    for (int space = 0; space < 2; ++space) {
        for (int len = 1; len <= 8; ++len) {
            R_ResetCmdArray(&list);
            memset(list.cmds, 0xa5, sizeof(list.cmds));
            memset(text, 'T', len); text[len] = '\0';
            int charge = ((space ? 48 : 44) + len + 1 + 3) & ~3;
            int filler = (188416 - charge) / 4;
            for (int n = 0; n < filler; ++n) R_AddCmdTouchAllImages();
            int offset = list.usedTotal;
            EmitText(text, space);
            assert(list.logicalUsedTotal == 188416);
            CheckText((const void *)(list.cmds + offset), text, space);
            before = Count(&list);
            EmitText(text, space);
            CheckRejected(&list, before);
            CheckText((const void *)(list.cmds + offset), text, space);
            assert(WalkHeaders(&list, list.usedTotal) == filler + 1);
        }
    }
    puts("actual text producers: lengths 0..16, payload/alignment and 16 exact-budget boundaries passed");
}

static void CheckDelayedDrawing(void)
{
    static GfxCmdArray list;
    s_cmdList = &list;
    R_ResetCmdArray(&list);
    EmitText("A", 0);
    int outer = R_BeginDelayedDrawing();
    EmitText("B", 0);
    int inner = R_BeginDelayedDrawing();
    EmitText("C", 0);
    R_EndDelayedDrawing(inner);
    assert(((GfxCmdCall *)(list.cmds + inner))->subCmd == list.cmds + list.usedTotal);
    R_IssueDelayedDrawing(inner);
    EmitText("D", 0);
    R_EndDelayedDrawing(outer);
    assert(((GfxCmdCall *)(list.cmds + outer))->subCmd == list.cmds + list.usedTotal);
    EmitText("E", 0);
    R_IssueDelayedDrawing(outer);
    EmitText("F", 0);
    R_IssueDelayedDrawing(outer);
    assert(R_AllocCriticalCmd(4, 0));
    CheckCounts(&list, (Counts){440, 104, 340, 52});
    assert(WalkHeaders(&list, list.usedTotal) == 14);
    WalkControlFlow(&list, list.usedTotal, "AEBCDFBCD");

    R_ResetCmdArray(&list);
    for (int n = 0; n < 47104; ++n) R_AddCmdTouchAllImages();
    for (int n = 0; n < 2042; ++n) R_AddCmdSaveScreen();
    int marker = R_BeginDelayedDrawing();
    R_EndDelayedDrawing(marker);
    R_IssueDelayedDrawing(marker);
    assert(R_AllocCriticalCmd(4, 0));
    CheckCounts(&list, (Counts){393216, 16384, 196608, 8192});
    assert(WalkHeaders(&list, list.usedTotal) == 49150);
    WalkControlFlow(&list, list.usedTotal, "");
    Counts before = Count(&list);
    assert(!R_AllocDelayedCall(1, NULL));
    CheckRejected(&list, before);
    puts("actual delayed drawing: nested/repeated calls and exact-reserve empty call traversal passed");
}

static void CheckCriticalOrdering(void)
{
    static GfxCmdArray list;
    for (int ordering = 0; ordering < 3; ++ordering) {
        s_cmdList = &list;
        R_ResetCmdArray(&list);
        if (ordering == 0)
            for (int n = 0; n < 2048; ++n) R_AddCmdSaveScreen();
        for (int n = 0; n < 47104; ++n) {
            if (ordering == 1 && !(n % 23)) R_AddCmdSaveScreen();
            R_AddCmdTouchAllImages();
            assert(list.lastCmd);
        }
        if (ordering == 2)
            for (int n = 0; n < 2048; ++n) R_AddCmdSaveScreen();
        CheckCounts(&list, (Counts){393216, 16384, 196608, 8192});
        assert(WalkHeaders(&list, list.usedTotal) == 49152);
        Counts before = Count(&list);
        R_AddCmdTouchAllImages(); CheckRejected(&list, before);
        R_AddCmdSaveScreen(); CheckRejected(&list, before);
    }
    puts("actual critical-first/interleaved/noncritical-first producers: identical exact budgets and rejection passed");
}

static int initDebugCalls, createBufferCalls, unlockCalls, lockCalls;
static int executeCalls, endCalls, updateDebugCalls, transferDebugCalls;
static GfxBackEndData *expectedExecution;
static int expectedExecutionBytes;
void R_InitDebugEntry(DebugGlobals *entry) { (void)entry; ++initDebugCalls; }
void RB_CreateDynamicBuffers(void) { ++createBufferCalls; }
void R_UnlockSkinnedCache(void) { ++unlockCalls; dx.skinnedCacheLockAddr = NULL; }
void R_LockSkinnedCache(GfxLockType lock)
{ assert(lock == 1); ++lockCalls; dx.skinnedCacheLockAddr = (void *)1; }
void R_TransferDebugGlobals(DebugGlobals *entry)
{ assert(entry == &frontEndDataOut->debugGlobals); ++transferDebugCalls; }
static void UpdateDebug(void) { ++updateDebugCalls; }
void RB_ExecuteRenderCommands(const void *data)
{
    assert(data == expectedExecution);
    const GfxCmdArray *list = &expectedExecution->commands;
    CheckCounts(list, (Counts){0});
    assert(!list->lastCmd);
    assert(WalkHeaders(list, expectedExecutionBytes) == 3);
    const GfxCmdHeader *first = HeaderAt(list, expectedExecutionBytes, 0);
    assert(first->id == 33);
    const GfxCmdHeader *second = HeaderAt(list, expectedExecutionBytes, first->byteCount);
    assert(second->id == 13);
    const GfxCmdSetViewport *viewport = (const void *)second;
    assert(viewport->viewport.x == 3 && viewport->viewport.y == 5);
    assert(viewport->viewport.width == 640 && viewport->viewport.height == 480);
    const GfxCmdHeader *end = HeaderAt(list, expectedExecutionBytes, first->byteCount + second->byteCount);
    assert(!end->id && end->byteCount == 8);
    WalkControlFlow(list, expectedExecutionBytes, "");
    ++executeCalls;
}
void RB_EndFrame(void) { ++endCalls; }

static void FillResetCommands(void)
{
    R_AddCmdTouchAllImages();
    R_AddCmdSetViewport(3, 5, 640, 480);
    CheckCounts(s_cmdList, (Counts){32, 24, 24, 20});
}

static void CheckFrameDataReset(const GfxBackEndData *data)
{
    CheckCounts(&data->commands, (Counts){0});
    assert(!data->commands.lastCmd);
    assert(!data->surfPos && !data->logicalSurfPos && !data->drawSurfCount);
    assert(!data->entityCount && !data->polyCount && !data->polyVertCount && !data->viewParmCount);
    assert(!data->debugGlobals.vertCount && !data->debugGlobals.stringCount && !data->debugGlobals.lineCount);
}

static void CheckResetEntryPoints(void)
{
    rg.registered = 1;
    ri.CL_UpdateDebugData = UpdateDebug;
    frontEndDataOut = NULL;
    R_InitBackendData();
    assert(frontEndDataOut == &s_nativeBackEndData && s_cmdList == &frontEndDataOut->commands);
    CheckFrameDataReset(frontEndDataOut);
    FillResetCommands();
    R_InitBackendData();
    assert(initDebugCalls == 2 && createBufferCalls == 2);
    CheckCounts(s_cmdList, (Counts){0}); assert(!s_cmdList->lastCmd);

    FillResetCommands();
    rg.registered = 0;
    R_AbortRenderCommands();
    CheckCounts(s_cmdList, (Counts){32, 24, 24, 20});
    rg.registered = 1;
    int unlockBefore = unlockCalls;
    R_AbortRenderCommands();
    assert(unlockCalls == unlockBefore + 1);
    CheckCounts(s_cmdList, (Counts){0}); assert(!s_cmdList->lastCmd);

    FillResetCommands();
    frontEndDataOut->surfPos = 64; frontEndDataOut->logicalSurfPos = 56;
    frontEndDataOut->drawSurfCount = frontEndDataOut->entityCount = 1;
    frontEndDataOut->polyCount = frontEndDataOut->polyVertCount = frontEndDataOut->viewParmCount = 1;
    frontEndDataOut->debugGlobals.vertCount = frontEndDataOut->debugGlobals.stringCount = frontEndDataOut->debugGlobals.lineCount = 1;
    expectedExecution = frontEndDataOut;
    expectedExecutionBytes = s_cmdList->usedTotal + 8;
    R_EndFrame();
    assert(executeCalls == 1 && endCalls == 1 && updateDebugCalls == 1 && transferDebugCalls == 1);
    assert(frontEndDataOut == &s_nativeBackEndData && s_cmdList == &frontEndDataOut->commands);
    CheckFrameDataReset(frontEndDataOut);

    FillResetCommands();
    GfxBackEndData *savedData = frontEndDataOut;
    GfxCmdArray *savedList = s_cmdList;
    Counts savedCounts = Count(savedList);
    GfxCmdHeader *savedLast = savedList->lastCmd;
    byte savedBytes[32]; memcpy(savedBytes, savedList->cmds, sizeof(savedBytes));
    for (int cycle = 0; cycle < 2; ++cycle) {
        dx.skinnedCacheLockAddr = (void *)1;
        R_BeginDebugFrame();
        assert(frontEndDataOut == &s_debugFrameGlob.frontEndDataOut);
        assert(s_cmdList == &frontEndDataOut->commands && s_debugFrameGlob.restoreSkinnedCache);
        CheckCounts(s_cmdList, (Counts){0});
        FillResetCommands();
        expectedExecution = frontEndDataOut;
        expectedExecutionBytes = s_cmdList->usedTotal + 8;
        R_EndDebugFrame();
        assert(frontEndDataOut == savedData && s_cmdList == savedList);
        CheckCounts(savedList, savedCounts);
        assert(savedList->lastCmd == savedLast && !memcmp(savedBytes, savedList->cmds, sizeof(savedBytes)));
        assert(!s_debugFrameGlob.restoreCmdList && !s_debugFrameGlob.restoreFrontEndDataOut && !s_debugFrameGlob.restoreSkinnedCache);
        CheckCounts(&s_debugFrameGlob.frontEndDataOut.commands, (Counts){0});
    }
    assert(executeCalls == 3 && endCalls == 3 && lockCalls == 2);
    R_ResetFrameBackendData();
    CheckFrameDataReset(frontEndDataOut);
    puts("actual init/abort/end-frame/debug-frame resets: counters, retained stream and list/cache restoration passed");
}

int main(void)
{
    static GfxCmdArray list;
    GfxLight light = { 0 };
    GfxLightDef definition = { 0 };
    GfxSceneDef sceneDef = { 0 };
    GfxLodParms lod = { 0 };
    GfxViewParms view = { 0 };
    GfxCmdSetLightProperties *lightCmd;
    GfxCmdBeginView *viewCmd;
    GfxCmdCall *call;

    s_cmdList = &list;
    specularScale.current.value = 1;
    R_AllocCmd(4, 0, 0x19);
    light.def = &definition;
    R_AddCmdLightProperties(0, &light);
    lightCmd = (void *)(list.cmds + 8);
    assert(lightCmd->header.byteCount == sizeof(*lightCmd));
    assert(lightCmd->lightDef == &definition);
    R_AddCmdBeginView(3, &sceneDef, &view, &lod);
    viewCmd = (void *)(list.cmds + 8 + sizeof(*lightCmd));
    assert(viewCmd->header.byteCount == sizeof(*viewCmd));
    assert(viewCmd->viewParms == &view && viewCmd->viewCount == 3);
    call = R_AllocDelayedCall(1, NULL);
    assert(call && ((uintptr_t)call & 7) == 0);
    call->subCmd = lightCmd;
    assert(call->subCmd == lightCmd);
    assert(list.cmds[0] == 0x19 && ((GfxCmdHeader *)list.cmds)->byteCount == 8);
    assert(list.usedTotal == 8 + sizeof(*lightCmd) + sizeof(*viewCmd) + sizeof(*call));
    assert(!R_AllocCmd(65529, 0, 3));
    const float rgba[4] = {1,1,1,1};
    R_ResetCmdArray(&list);
    for (int n = 0; n < 4282; ++n) {
        R_AddCmdDrawStretchPic(0,0,1,1,0,0,1,1,rgba,(void *)1);
        assert(list.lastCmd && list.lastCmd->id == 15);
    }
    assert(list.logicalUsedTotal == 188408 && list.usedTotal == 239792);
    Counts before = Count(&list);
    R_AddCmdDrawStretchPic(0,0,1,1,0,0,1,1,rgba,(void *)1);
    CheckRejected(&list, before);
    int pictureOffset = 0;
    for (int n = 0; n < 4282; ++n) {
        const GfxCmdStretchPic *pic = (const void *)HeaderAt(&list, list.usedTotal, pictureOffset);
        assert(pic->header.id == 15 && pic->header.byteCount == 56);
        assert(pic->material == (void *)1 && pic->x == 0 && pic->y == 0 && pic->w == 1 && pic->h == 1);
        assert(pic->s0 == 0 && pic->t0 == 0 && pic->s1 == 1 && pic->t1 == 1);
        pictureOffset += pic->header.byteCount;
    }
    assert(pictureOffset == list.usedTotal);
    assert(R_AllocCmd(4,0,0x19)); assert(R_AllocCmd(4,0,0x19));
    before = Count(&list);
    assert(!R_AllocCmd(4,0,0x19));
    CheckRejected(&list, before);
    for (int n=0;n<1024;n++) assert(R_AllocDelayedCall(1,NULL));
    before = Count(&list);
    assert(!R_AllocDelayedCall(1,NULL));
    CheckRejected(&list, before);
    assert(list.logicalUsedTotal == 196608 && list.logicalUsedCritical == 8192);
    assert(WalkHeaders(&list, list.usedTotal) == 4282 + 2 + 1024);
    for (int n = 0; n < 2; ++n) {
        const GfxCmdHeader *header = HeaderAt(&list, list.usedTotal, pictureOffset);
        assert(header->id == 25 && header->byteCount == 8);
        pictureOffset += header->byteCount;
    }
    for (int n = 0; n < 1024; ++n) {
        const GfxCmdHeader *header = HeaderAt(&list, list.usedTotal, pictureOffset);
        assert(header->id == 1 && header->byteCount == 16);
        pictureOffset += header->byteCount;
    }
    assert(pictureOffset == list.usedTotal);
    R_ResetCmdArray(&list);
    for (int n=0;n<47104;n++) assert(R_AllocCmd(4,0,0x19));
    assert(!R_AllocCmd(4,0,0x19));
    for (int n=0;n<2048;n++) assert(R_AllocCriticalCmd(4,0));
    assert(!R_AllocCriticalCmd(4,0));
    assert(list.usedTotal == sizeof(list.cmds));
    assert(list.logicalUsedTotal == 196608);
    R_ResetCmdArray(&list);
    const int sizes[][3] = {{4,0,4},{16,1,8},{16,2,8},{4,3,4},{20,4,20},{80,5,76},
      {72,9,72},{4,10,4},{28,11,28},{64,12,48},{20,13,20},{8,14,8},{56,15,44},
      {56,16,48},{48,17,40},{56,18,44},{48,19,36},{40,20,28},{24,23,20},
      {8,24,8},{4,25,4},{8,26,8},{8,27,8},{4,28,4},{16,29,12},{4,33,4}};
    for (unsigned n=0;n<sizeof(sizes)/sizeof(sizes[0]);n++) {
        int previous=list.logicalUsedTotal;
        assert(R_AllocNativeCmd(sizes[n][0],sizes[n][1],1));
        assert(list.logicalUsedTotal-previous == sizes[n][2]);
    }
    for (int len=0;len<=8;len++) {
        int request=(52+len+1+3)&~3;
        assert(R_OriginalCmdSize(request,21)==((44+len+1+3)&~3));
        assert(R_OriginalCmdSize(request,22)==((48+len+1+3)&~3));
    }
    puts("Windows command budget: 4282 pictures, 47104 small commands, exact 8192 critical reserve passed");
    puts("renderer mixed command sizes, traversal and pointer alignment: passed");
    CheckTextProducers();
    CheckDelayedDrawing();
    CheckCriticalOrdering();
    CheckResetEntryPoints();
    return 0;
}
