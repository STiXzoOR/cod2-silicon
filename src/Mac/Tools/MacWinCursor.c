#include "common_types.h"
#include "imports.h"
#include "bytematch.h"

static EventLoopTimerRef sTimerRef;
__attribute__((used)) static bool sVisibleCursor = 1;
__attribute__((used)) static HCURSOR sCurrentWinCursor = (HCURSOR)-1;
static TCursorList sCursorList;
static WinCursor *sCurrentCursor;
static HCURSOR sSavedWinCursor;

bool WinIcon_Load(const WinIcon *_this, const UInt8 *inIcon);
HCURSOR SetWinCursor(HCURSOR hCursor);
void SwitchToWinCursor(void);
void SwitchToMacCursor(void);
void CursorTimerProc(void);
static void __static_initialization_and_destruction_0(void);
static void GLOBAL__D_LoadCursorFromFileA(void);
static void GLOBAL__I_LoadCursorFromFileA(void);
void WinIcon_WinIcon(const WinIcon *_this);
bool WinCursor_ReadCursor(const WinCursor *_this, const UInt8 *inCursor);
bool WinCursor_Load(const WinCursor *_this, const char *inFileName);
HCURSOR LoadCursorFromFileA(LPCSTR lpFileName);
void ZN7WinIconD2Ev(void);
void ZN7WinIconD1Ev(void);
void WinCursor_WinCursor(const WinCursor *_this, const WinCursor *inCursor);
void ZN9WinCursorD2Ev(void);
void ZN9WinCursorD1Ev(void);
void ZNSt6vectorI9WinCursorSaIS0_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS0_S2_EERKS0_(void);

bool WinIcon_Load(const WinIcon *_this, const UInt8 *inIcon)
{
    return 0;
}

__attribute__((noinline)) HCURSOR SetWinCursor(HCURSOR hCursor)
{
    HCURSOR old = sCurrentWinCursor;
    sCurrentWinCursor = hCursor;
    return old;
}

void SwitchToWinCursor(void)
{
    HCURSOR saved = sSavedWinCursor;
    sSavedWinCursor = NULL;
    SetWinCursor(saved);
}

void SwitchToMacCursor(void)
{
    HCURSOR saved = sSavedWinCursor;
    if (saved != NULL)
        return;
    sSavedWinCursor = SetWinCursor((HCURSOR)-1);
}

void CursorTimerProc(void)
{
}

void __static_initialization_and_destruction_0(void)
{
}

void GLOBAL__D_LoadCursorFromFileA(void)
{
}

void GLOBAL__I_LoadCursorFromFileA(void)
{
}

void WinIcon_WinIcon(const WinIcon *_this)
{
}

bool WinCursor_ReadCursor(const WinCursor *_this, const UInt8 *inCursor)
{
    return 0;
}

bool WinCursor_Load(const WinCursor *_this, const char *inFileName)
{
    return 0;
}

HCURSOR LoadCursorFromFileA(LPCSTR lpFileName)
{
    return 0;
}

void ZN7WinIconD2Ev(void)
{
}

void ZN7WinIconD1Ev(void)
{
}

void WinCursor_WinCursor(const WinCursor *_this, const WinCursor *inCursor)
{
}

void ZN9WinCursorD2Ev(void)
{
}

void ZN9WinCursorD1Ev(void)
{
}

void ZNSt6vectorI9WinCursorSaIS0_EE13_M_insert_auxEN9__gnu_cxx17__normal_iteratorIPS0_S2_EERKS0_(void)
{
}
