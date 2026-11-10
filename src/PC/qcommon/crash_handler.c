#include "crash_handler.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#    include "cod2x.h"
#endif

#ifndef COD2_GIT_HASH
#    define COD2_GIT_HASH "unknown"
#endif
#define CR_BUILD_STAMP __DATE__ " " __TIME__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#ifdef _WIN32
#    include <io.h>
#    include <fcntl.h>
#    include <signal.h>
#else
#    include <unistd.h>
#endif

static char cr_appName[64] = "CoD2";
static char cr_version[64] = "?";
static char cr_buildDate[64] = "?";
static char cr_cmdline[2048] = "";
static char cr_exePath[1024] = "";

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
static Sys_ReportReceiver cr_receiver;

void Sys_SetReportReceiver(Sys_ReportReceiver receiver)
{
    cr_receiver = receiver;
}

void Sys_CrashReceive(const char *path, const char *reason)
{
    if (cr_receiver)
        cr_receiver(path, reason);
}
#endif

#define CR_MAX_FIELDS 24
static struct {
    char name[48];
    char value[256];
} cr_fields[CR_MAX_FIELDS];
static int cr_fieldCount = 0;
static volatile int cr_inHandler = 0;

void Sys_CrashSetField(const char *name, const char *value)
{
    int i;
    if (!name || !name[0])
        return;
    if (!value)
        value = "";
    for (i = 0; i < cr_fieldCount; i++) {
        if (strncmp(cr_fields[i].name, name, sizeof(cr_fields[i].name)) == 0) {
            snprintf(cr_fields[i].value, sizeof(cr_fields[i].value), "%s", value);
            return;
        }
    }
    if (cr_fieldCount >= CR_MAX_FIELDS)
        return;
    snprintf(cr_fields[cr_fieldCount].name, sizeof(cr_fields[cr_fieldCount].name), "%s", name);
    snprintf(cr_fields[cr_fieldCount].value, sizeof(cr_fields[cr_fieldCount].value), "%s", value);
    cr_fieldCount++;
}

static void cr_emit(int fd, const char *fmt, ...)
{
    char buf[2048];
    int n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0)
        return;
    if (n > (int)sizeof(buf))
        n = (int)sizeof(buf);
    if (fd >= 0) {
        int _w = (int)write(fd, buf, n);
        (void)_w;
    }
    {
        int _w = (int)write(2 , buf, n);
        (void)_w;
    }
}

static void cr_emit_common(int fd, const char *crashDesc)
{
    time_t now = time(NULL);
    char when[64];
    struct tm *tmv = localtime(&now);
    int i;
    when[0] = '\0';
    if (tmv)
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", tmv);

    cr_emit(fd, "================ %s CRASH REPORT ================\n\n", cr_appName);
    cr_emit(fd, "crash      : %s\n", crashDesc);
    cr_emit(fd, "time       : %s\n", when);
    cr_emit(fd, "version    : %s (game data %s)\n", cr_version, cr_buildDate);
    cr_emit(fd, "build      : %s  (git %s)\n", CR_BUILD_STAMP, COD2_GIT_HASH);
    if (cr_exePath[0])
        cr_emit(fd, "executable : %s\n", cr_exePath);
    cr_emit(fd, "command    : %s\n", cr_cmdline[0] ? cr_cmdline : "(none)");
    for (i = 0; i < cr_fieldCount; i++)
        cr_emit(fd, "%-10s : %s\n", cr_fields[i].name, cr_fields[i].value);
    cr_emit(fd, "\n");
}

#ifdef _WIN32

#    include <windows.h>
#    include <dbghelp.h>

static HWND cr_win_edit = NULL;

static LRESULT CALLBACK cr_win_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_SIZE && cr_win_edit) {
        MoveWindow(cr_win_edit, 0, 0, LOWORD(l), HIWORD(l), TRUE);
        return 0;
    }
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

static void cr_win_popup(const char *path, const char *crashDesc)
{
    static char text[256 * 1024];
    static char title[300];
    FILE *f;
    size_t rd = 0, j = 0;
    WNDCLASSA wc;
    HWND win;
    HFONT font;
    MSG msg;
    static char raw[200 * 1024];
    size_t i;

    f = fopen(path, "rb");
    if (f) {
        rd = fread(raw, 1, sizeof(raw) - 1, f);
        fclose(f);
    }
    raw[rd] = '\0';
    for (i = 0; i < rd && j < sizeof(text) - 2; i++) {
        if (raw[i] == '\n' && (i == 0 || raw[i - 1] != '\r'))
            text[j++] = '\r';
        text[j++] = raw[i];
    }
    text[j] = '\0';
    if (rd == 0)
        snprintf(text, sizeof(text), "%s crashed:\r\n%s\r\n(report file %s)", cr_appName, crashDesc, path);
    snprintf(title, sizeof(title), "%s crashed - %s", cr_appName, crashDesc);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = cr_win_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "CoD2CrashWnd";
    RegisterClassA(&wc);

    win = CreateWindowExA(WS_EX_TOPMOST, "CoD2CrashWnd", title,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1000, 700,
                          NULL, NULL, wc.hInstance, NULL);
    if (!win) {
        MessageBoxA(NULL, text, title, MB_OK | MB_ICONERROR);
        return;
    }

    cr_win_edit = CreateWindowExA(0, "EDIT", NULL,
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
                                      ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                  0, 0, 1000, 700, win, NULL, wc.hInstance, NULL);
    font = CreateFontA(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       FIXED_PITCH | FF_MODERN, "Consolas");
    if (font)
        SendMessageA(cr_win_edit, WM_SETFONT, (WPARAM)font, TRUE);
    SetWindowTextA(cr_win_edit, text);

    ShowWindow(win, SW_SHOW);
    SetForegroundWindow(win);
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

static void cr_win_regs(int fd, CONTEXT *c)
{
    if (!c)
        return;
#    if defined(_WIN64)
    cr_emit(fd, "registers:\n");
    cr_emit(fd, "  rip=%016llx rsp=%016llx rbp=%016llx rflags=%08lx\n",
            (unsigned long long)c->Rip, (unsigned long long)c->Rsp, (unsigned long long)c->Rbp, (unsigned long)c->EFlags);
    cr_emit(fd, "  rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n",
            (unsigned long long)c->Rax, (unsigned long long)c->Rbx, (unsigned long long)c->Rcx, (unsigned long long)c->Rdx);
    cr_emit(fd, "  rsi=%016llx rdi=%016llx r8 =%016llx r9 =%016llx\n",
            (unsigned long long)c->Rsi, (unsigned long long)c->Rdi, (unsigned long long)c->R8, (unsigned long long)c->R9);
    {   /* exe base + rip-relative offset, so the crash site is symbolizable from
         * the .map even when StackWalk yields no frames. */
        HMODULE hm = GetModuleHandleA(NULL);
        cr_emit(fd, "  exe_base=%016llx  rip_rel=0x%llx\n",
                (unsigned long long)(uintptr_t)hm,
                (unsigned long long)((uintptr_t)c->Rip - (uintptr_t)hm));
    }
    {   /* Identify the module holding rip (usually a system DLL when StackWalk
         * fails) and the faulting thread, to tell main-thread init from a worker. */
        HMODULE hmod = NULL;
        char modName[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)c->Rip, &hmod) && hmod) {
            GetModuleFileNameA(hmod, modName, sizeof(modName));
            cr_emit(fd, "  rip module=%s +0x%llx\n", modName,
                    (unsigned long long)((uintptr_t)c->Rip - (uintptr_t)hmod));
        } else {
            cr_emit(fd, "  rip module=<unknown>\n");
        }
        cr_emit(fd, "  thread=%lu\n", (unsigned long)GetCurrentThreadId());
    }
    {   /* Raw stack words near rsp: lets the call chain be read by eye when the
         * unwinder fails (e.g. fault in a DLL with no frame info). */
        uintptr_t sp = (uintptr_t)c->Rsp;
        int i;
        cr_emit(fd, "  stack@rsp:");
        for (i = 0; i < 24; i++) {
            uintptr_t a = sp + (uintptr_t)i * sizeof(uintptr_t);
            if (IsBadReadPtr((void *)a, sizeof(uintptr_t))) {
                cr_emit(fd, " <unmapped@+0x%x>", (unsigned)(i * (int)sizeof(uintptr_t)));
                break;
            }
            if ((i & 3) == 0)
                cr_emit(fd, "\n   +0x%03x:", (unsigned)(i * (int)sizeof(uintptr_t)));
            cr_emit(fd, " %016llx", (unsigned long long)*(uintptr_t *)a);
        }
        cr_emit(fd, "\n");
    }
#    else
    cr_emit(fd, "registers:\n");
    cr_emit(fd, "  eip=%08lx esp=%08lx ebp=%08lx eflags=%08lx\n",
            (unsigned long)c->Eip, (unsigned long)c->Esp, (unsigned long)c->Ebp, (unsigned long)c->EFlags);
    cr_emit(fd, "  eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx\n",
            (unsigned long)c->Eax, (unsigned long)c->Ebx, (unsigned long)c->Ecx, (unsigned long)c->Edx);
    cr_emit(fd, "  esi=%08lx edi=%08lx\n", (unsigned long)c->Esi, (unsigned long)c->Edi);
#    endif
    cr_emit(fd, "\n");
}

static void cr_win_backtrace(int fd, CONTEXT *ctxIn)
{
    HANDLE proc = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();
    CONTEXT ctx;
    STACKFRAME64 frame;
    DWORD machine;
    int depth = 0;
    char symbuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO *sym = (SYMBOL_INFO *)symbuf;

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
    SymInitialize(proc, NULL, TRUE);

    if (ctxIn)
        ctx = *ctxIn;
    else
        RtlCaptureContext(&ctx);

    memset(&frame, 0, sizeof(frame));
#    if defined(_WIN64)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrStack.Offset = ctx.Rsp;
#    else
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = ctx.Eip;
    frame.AddrFrame.Offset = ctx.Ebp;
    frame.AddrStack.Offset = ctx.Esp;
#    endif
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    /* call-through-NULL (or into an unmapped page): rip/eip is bogus and the
     * frame pointer is gone, so a normal StackWalk yields nothing. The failed
     * CALL pushed the return address, so [rsp]/[esp] is the caller -- recover it
     * so the backtrace at least shows the call site. */
    if ((frame.AddrPC.Offset == 0 || SymGetModuleBase64(proc, frame.AddrPC.Offset) == 0) && frame.AddrStack.Offset) {
#    if defined(_WIN64)
        if (!IsBadReadPtr((void *)(uintptr_t)frame.AddrStack.Offset, 8)) {
            DWORD64 ret = *(DWORD64 *)(uintptr_t)frame.AddrStack.Offset;
            cr_emit(fd, "  (bad-call rip=0x%llx) return address [rsp] = 0x%llx\n",
                    (unsigned long long)frame.AddrPC.Offset, (unsigned long long)ret);
            if (ret) {
                frame.AddrPC.Offset = ret;
                frame.AddrStack.Offset += 8;
                ctx.Rip = ret;
                ctx.Rsp = frame.AddrStack.Offset;
            }
        }
#    else
        DWORD ret = 0;
        if (!IsBadReadPtr((void *)(uintptr_t)frame.AddrStack.Offset, 4)) {
            ret = *(DWORD *)(uintptr_t)frame.AddrStack.Offset;
            cr_emit(fd, "  (bad-call eip=0x%08lx) return address [esp] = 0x%08lx\n",
                    (unsigned long)frame.AddrPC.Offset, (unsigned long)ret);
        }
        if (ret) {
            frame.AddrPC.Offset = ret;
            frame.AddrStack.Offset += 4;
            ctx.Eip = ret;
            ctx.Esp = frame.AddrStack.Offset;
        }
#    endif
    }

    cr_emit(fd, "backtrace (most recent first):\n");
    {
        /* Always symbolize the faulting instruction directly: on x64 StackWalk64 can produce
           zero frames when unwind info is unavailable (rbp=0), leaving no source line at all. */
#    if defined(_WIN64)
        DWORD64 fpc = (DWORD64)ctx.Rip;
#    else
        DWORD64 fpc = (DWORD64)ctx.Eip;
#    endif
        DWORD64 fdisp = 0, fmod = SymGetModuleBase64(proc, fpc);
        char fmodName[MAX_PATH] = "?";
        IMAGEHLP_LINE64 fline;
        DWORD flineDisp = 0;
        const char *fname = "??";
        if (fmod)
            GetModuleFileNameA((HMODULE)(uintptr_t)fmod, fmodName, sizeof(fmodName));
        memset(sym, 0, sizeof(symbuf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 500;
        if (SymFromAddr(proc, fpc, &fdisp, sym))
            fname = sym->Name;
        memset(&fline, 0, sizeof(fline));
        fline.SizeOfStruct = sizeof(fline);
        if (SymGetLineFromAddr64(proc, fpc, &flineDisp, &fline))
            cr_emit(fd, "  [faulting rip] %s+0x%llx  (%s:%lu)\n", fname,
                    (unsigned long long)fdisp, fline.FileName, (unsigned long)fline.LineNumber);
        else
            cr_emit(fd, "  [faulting rip] %s+0x%llx  [%s+0x%llx]\n", fname,
                    (unsigned long long)fdisp, fmodName,
                    (unsigned long long)(fmod ? fpc - fmod : 0));
    }
    while (depth < 64 && StackWalk64(machine, proc, thread, &frame, &ctx, NULL,
                                     SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        DWORD64 pc = frame.AddrPC.Offset;
        DWORD64 disp = 0;
        DWORD64 modBase;
        char modName[MAX_PATH] = "?";
        const char *name = "??";
        IMAGEHLP_LINE64 line;
        DWORD lineDisp = 0;
        if (pc == 0)
            break;

        modBase = SymGetModuleBase64(proc, pc);
        if (modBase) {
            HMODULE hm = (HMODULE)(uintptr_t)modBase;
            GetModuleFileNameA(hm, modName, sizeof(modName));
        }
        memset(sym, 0, sizeof(symbuf));
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = 500;
        if (SymFromAddr(proc, pc, &disp, sym))
            name = sym->Name;

        memset(&line, 0, sizeof(line));
        line.SizeOfStruct = sizeof(line);
        if (SymGetLineFromAddr64(proc, pc, &lineDisp, &line))
            cr_emit(fd, "  #%-2d 0x%016llx  %s+0x%llx  (%s:%lu)  [%s+0x%llx]\n",
                    depth, (unsigned long long)pc, name, (unsigned long long)disp,
                    line.FileName, (unsigned long)line.LineNumber,
                    modName, (unsigned long long)(pc - modBase));
        else
            cr_emit(fd, "  #%-2d 0x%016llx  %s+0x%llx  [%s+0x%llx]\n",
                    depth, (unsigned long long)pc, name, (unsigned long long)disp,
                    modName, (unsigned long long)(modBase ? pc - modBase : 0));
        depth++;
    }
    cr_emit(fd, "\n");

    /* Stack-scan fallback: when the fault is in a system DLL (StackWalk yields no
     * usable frames), scan the stack for values that land in our exe -- likely
     * return addresses -- and print exe-relative offsets so cod2_win32.map can
     * localize the caller. */
    {
        uintptr_t base = (uintptr_t)GetModuleHandleA(NULL);
        uintptr_t lo = base, hi = base + 0x900000; /* generous exe span */
#if defined(_WIN64)
        uintptr_t sp = (uintptr_t)ctx.Rsp;
#else
        uintptr_t sp = (uintptr_t)ctx.Esp;
#endif
        uintptr_t cur;
        int found = 0;
        cr_emit(fd, "stack scan (exe-range return-address candidates):\n");
        for (cur = sp; cur < sp + 0x6000 && found < 48; cur += sizeof(uintptr_t)) {
            uintptr_t v;
            if (IsBadReadPtr((void *)cur, sizeof(v)))
                break;
            v = *(uintptr_t *)cur;
            if (v >= lo && v < hi) {
                cr_emit(fd, "  [rsp+0x%04llx] 0x%016llx  rel=0x%llx\n",
                        (unsigned long long)(cur - sp), (unsigned long long)v,
                        (unsigned long long)(v - base));
                found++;
            }
        }
        if (!found)
            cr_emit(fd, "  (none found)\n");
        cr_emit(fd, "\n");

        /* Second pass: resolve EVERY code-like stack value to its module (any DLL),
         * so a fault inside a system DLL callback shows the caller chain. */
        cr_emit(fd, "stack scan (all module-resolved code addresses):\n");
        {
            int shown = 0;
            for (cur = sp; cur < sp + 0x2000 && shown < 40; cur += sizeof(uintptr_t)) {
                uintptr_t v;
                HMODULE hm2 = NULL;
                char mn[MAX_PATH];
                const char *bn;
                if (IsBadReadPtr((void *)cur, sizeof(v)))
                    break;
                v = *(uintptr_t *)cur;
                if (v < 0x10000)
                    continue;
                if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                        (LPCSTR)v, &hm2) || !hm2)
                    continue;
                if (!GetModuleFileNameA(hm2, mn, sizeof(mn)))
                    continue;
                bn = mn;
                { const char *p; for (p = mn; *p; p++) if (*p == '\\' || *p == '/') bn = p + 1; }
                cr_emit(fd, "  [rsp+0x%04llx] 0x%016llx  %s+0x%llx\n",
                        (unsigned long long)(cur - sp), (unsigned long long)v,
                        bn, (unsigned long long)(v - (uintptr_t)hm2));
                shown++;
            }
            if (!shown)
                cr_emit(fd, "  (none found)\n");
        }
        cr_emit(fd, "\n");
    }

    /* last surface dispatched by the render backend (helps localize model/tess
     * faults that land in D3D/heap code with no exe frames). */
    {
        extern volatile struct RbDrawDbg {
            int type; const void *surface; int entityIndex; const void *material;
            int iteration; int drawSurfCount; unsigned int sort;
        } g_rb_draw_dbg;
        cr_emit(fd, "last RB draw surface: type=%d surface=%p entityIndex=%d material=%p iter=%d/%d sort=%x\n",
                g_rb_draw_dbg.type, g_rb_draw_dbg.surface, g_rb_draw_dbg.entityIndex,
                g_rb_draw_dbg.material, g_rb_draw_dbg.iteration, g_rb_draw_dbg.drawSurfCount,
                g_rb_draw_dbg.sort);
        { extern volatile int g_rigid_step; cr_emit(fd, "rigid draw step=%d (1=strm 2=push 3=wmtx 4=drawtech 5=done)\n", g_rigid_step); }
        { extern volatile int g_lastdraw[6]; cr_emit(fd, "last DrawIndexedPrim: baseVertex=%d minIndex=%d numVerts=%d startIndex=%d primCount=%d (#%d)\n\n",
                g_lastdraw[0], g_lastdraw[1], g_lastdraw[2], g_lastdraw[3], g_lastdraw[4], g_lastdraw[5]); }
    }
    SymCleanup(proc);
}

static const char *cr_win_exname(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
        return "ACCESS_VIOLATION (segfault)";
    case EXCEPTION_STACK_OVERFLOW:
        return "STACK_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_PRIV_INSTRUCTION:
        return "PRIV_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:
        return "IN_PAGE_ERROR";
    case 0xC0000409:
        return "STACK_BUFFER_OVERRUN (stack smashing)";
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        return "DATATYPE_MISALIGNMENT";
    default:
        return "exception";
    }
}

#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* 1 when the fault belongs to the CoD2x layer: the first frame of the faulting
   thread inside the game executable is CoD2x-layer code (cod2x_report.c). */
static int cr_win_owned(CONTEXT *context)
{
    HANDLE process = GetCurrentProcess();
    HMODULE exe = GetModuleHandleA(NULL);
    CONTEXT copy;
    STACKFRAME64 frame;
    int depth, owned = 0;
    if (!context)
        return 0;
    copy = *context;
    memset(&frame, 0, sizeof(frame));
#        if defined(_WIN64)
    frame.AddrPC.Offset = copy.Rip;
    frame.AddrFrame.Offset = copy.Rbp;
    frame.AddrStack.Offset = copy.Rsp;
#        else
    frame.AddrPC.Offset = copy.Eip;
    frame.AddrFrame.Offset = copy.Ebp;
    frame.AddrStack.Offset = copy.Esp;
#        endif
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = AddrModeFlat;
    SymInitialize(process, NULL, TRUE);
    for (depth = 0; depth < 64; ++depth) {
        HMODULE module = NULL;
#        if defined(_WIN64)
        const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#        else
        const DWORD machine = IMAGE_FILE_MACHINE_I386;
#        endif
        if (depth && !StackWalk64(machine, process, GetCurrentThread(), &frame, &copy, NULL,
                                  SymFunctionTableAccess64, SymGetModuleBase64, NULL))
            break;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)(uintptr_t)frame.AddrPC.Offset, &module) &&
            module == exe) {
            owned = Cod2x_InLayer((const void *)(uintptr_t)frame.AddrPC.Offset);
            break;
        }
    }
    SymCleanup(process);
    return owned;
}
#    endif

static void cr_win_do(EXCEPTION_POINTERS *ep, const char *descIn)
{
    char path[1024];
    char desc[256];
    int fd;
    DWORD pid = GetCurrentProcessId();

    if (cr_inHandler)
        return;
    cr_inHandler = 1;

    if (ep && ep->ExceptionRecord) {
        DWORD code = ep->ExceptionRecord->ExceptionCode;
        if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
            snprintf(desc, sizeof(desc), "%s at address 0x%p (%s)", cr_win_exname(code),
                     (void *)ep->ExceptionRecord->ExceptionInformation[1],
                     ep->ExceptionRecord->ExceptionInformation[0] ? "write" : "read");
        else
            snprintf(desc, sizeof(desc), "%s (code 0x%08lx)", cr_win_exname(code), (unsigned long)code);
    } else {
        snprintf(desc, sizeof(desc), "%s", descIn ? descIn : "fatal error");
    }

    snprintf(path, sizeof(path), "cod2_crash_%lu.txt", (unsigned long)pid);
    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    cr_emit_common(fd, desc);
    cr_win_regs(fd, ep ? ep->ContextRecord : NULL);
    cr_win_backtrace(fd, ep ? ep->ContextRecord : NULL);
    cr_emit(fd, "================ END CRASH REPORT ================\n");
    if (fd >= 0)
        close(fd);
#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    /* A dedicated server here writes no CoD2x report: every fault but a
       CoD2x-layer one goes to the report receiver. */
    if (!cr_win_owned(ep ? ep->ContextRecord : NULL))
        Sys_CrashReceive(path, desc);
#    endif

    {
        int fd2 = open("Z:\\tmp\\cod2_crash_latest.txt", O_CREAT | O_TRUNC | O_WRONLY, 0644);
        if (fd2 >= 0) {
            cr_emit_common(fd2, desc);
            cr_win_regs(fd2, ep ? ep->ContextRecord : NULL);
            cr_win_backtrace(fd2, ep ? ep->ContextRecord : NULL);
            cr_emit(fd2, "================ END CRASH REPORT ================\n");
            close(fd2);
        }
    }
    if (getenv("COD2_CRASH_NOPOPUP"))
        return;

    cr_win_popup(path, desc);
}

static LONG WINAPI cr_seh_filter(EXCEPTION_POINTERS *ep)
{
    cr_win_do(ep, NULL);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void cr_abort_handler(int sig)
{
    (void)sig;
    cr_win_do(NULL, "abort() / stack smashing detected");
    _exit(3);
}

void Sys_InstallCrashHandler(const char *appName, const char *version,
                             const char *buildDate, const char *cmdline)
{
    if (appName && appName[0])
        snprintf(cr_appName, sizeof(cr_appName), "%s", appName);
    if (version && version[0])
        snprintf(cr_version, sizeof(cr_version), "%s", version);
    if (buildDate && buildDate[0])
        snprintf(cr_buildDate, sizeof(cr_buildDate), "%s", buildDate);
    if (cmdline)
        snprintf(cr_cmdline, sizeof(cr_cmdline), "%s", cmdline);
    GetModuleFileNameA(NULL, cr_exePath, sizeof(cr_exePath));

    /* Reserve guard-stack space so the SEH filter can still run (and write a report) on a
       stack overflow -- otherwise the process dies silently with no crash file. */
    {
        ULONG guarantee = 65536;
        SetThreadStackGuarantee(&guarantee);
    }

    SetUnhandledExceptionFilter(cr_seh_filter);
    signal(SIGABRT, cr_abort_handler);
}

#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* What cod2x_report.c sends besides a crash: CoD2x's Windows forms. */
const char *Sys_CrashDirectory(void)
{
    return ".";
}

/* CoD2x's Windows reports say win-client, also from a dedicated server. */
const char *Sys_Cod2xReportType(void)
{
    return "win-client";
}

#        if !defined(CR_COD2X_DIALOG)
size_t Sys_Cod2xSystemInfo(char *out, size_t size)
{
    return (size_t)snprintf(out, size, "{}");
}

size_t Sys_Cod2xIdentity(char *out, size_t size)
{
    return (size_t)snprintf(out, size, "\"installId\":\"\",\"hwid2\":\"\",");
}

size_t Sys_Cod2xLogLines(char *out, size_t size)
{
    return Cod2x_ConsoleLogs(out, size);
}
#        endif
#    endif

#elif defined(__EMSCRIPTEN__)

void Sys_InstallCrashHandler(const char *appName, const char *version,
                             const char *buildDate, const char *cmdline)
{
    (void)appName;
    (void)version;
    (void)buildDate;
    (void)cmdline;
}

#else

#    include <unistd.h>
#    include <signal.h>
#    include <fcntl.h>
#    include <sys/utsname.h>
#    include <sys/wait.h>

#    if !defined(__APPLE__) || defined(COD2_X64)
#        include <execinfo.h>
#    endif
#    if defined(__APPLE__) && defined(COD2_X64)
#        include <mach-o/dyld.h>
#    endif
#    define __USE_GNU
#    if defined(__APPLE__) && defined(COD2_X64)
#        include <sys/ucontext.h>
#    else
#    include <ucontext.h>
#    endif
#    include <dlfcn.h>

static const char *cr_sig_name(int sig)
{
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV (segmentation fault)";
    case SIGABRT:
        return "SIGABRT (abort / stack smashing detected)";
    case SIGFPE:
        return "SIGFPE (arithmetic exception)";
    case SIGILL:
        return "SIGILL (illegal instruction)";
    case SIGBUS:
        return "SIGBUS (bus error)";
    case SIGTRAP:
        return "SIGTRAP (trap)";
    default:
        return "fatal signal";
    }
}

static void cr_posix_regs(int fd, void *ucontext)
{
    ucontext_t *uc = (ucontext_t *)ucontext;
    if (!uc)
        return;
#    if defined(__APPLE__) && defined(COD2_X64) && (defined(__arm64__) || defined(__aarch64__))
    {
        const _STRUCT_ARM_THREAD_STATE64 *r = &uc->uc_mcontext->__ss;
        cr_emit(fd, "registers:\n");
        cr_emit(fd, "  pc=%016llx sp=%016llx fp=%016llx lr=%016llx cpsr=%08x\n",
                (unsigned long long)__darwin_arm_thread_state64_get_pc(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_sp(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_fp(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_lr(*r), r->__cpsr);
        for (int i = 0; i < 29; i++)
            cr_emit(fd, "  x%-2d=%016llx%s", i, (unsigned long long)r->__x[i], i % 3 == 2 ? "\n" : " ");
        cr_emit(fd, "\n");
    }
#    elif defined(__x86_64__) || defined(_M_X64)
    {
        greg_t *r = uc->uc_mcontext.gregs;
        cr_emit(fd, "registers:\n");
        cr_emit(fd, "  rip=%016llx rsp=%016llx rbp=%016llx efl=%08llx\n",
                (unsigned long long)r[REG_RIP], (unsigned long long)r[REG_RSP],
                (unsigned long long)r[REG_RBP], (unsigned long long)r[REG_EFL]);
        cr_emit(fd, "  rax=%016llx rbx=%016llx rcx=%016llx rdx=%016llx\n",
                (unsigned long long)r[REG_RAX], (unsigned long long)r[REG_RBX],
                (unsigned long long)r[REG_RCX], (unsigned long long)r[REG_RDX]);
        cr_emit(fd, "  rsi=%016llx rdi=%016llx r8 =%016llx r9 =%016llx\n",
                (unsigned long long)r[REG_RSI], (unsigned long long)r[REG_RDI],
                (unsigned long long)r[REG_R8], (unsigned long long)r[REG_R9]);
    }
#    elif defined(__i386__)
    {
        greg_t *r = uc->uc_mcontext.gregs;
        cr_emit(fd, "registers:\n");
        cr_emit(fd, "  eip=%08x esp=%08x ebp=%08x efl=%08x\n",
                (unsigned)r[REG_EIP], (unsigned)r[REG_ESP], (unsigned)r[REG_EBP], (unsigned)r[REG_EFL]);
        cr_emit(fd, "  eax=%08x ebx=%08x ecx=%08x edx=%08x\n",
                (unsigned)r[REG_EAX], (unsigned)r[REG_EBX], (unsigned)r[REG_ECX], (unsigned)r[REG_EDX]);
        cr_emit(fd, "  esi=%08x edi=%08x\n", (unsigned)r[REG_ESI], (unsigned)r[REG_EDI]);
    }
#    else
    cr_emit(fd, "registers: (unsupported arch)\n");
#    endif
    cr_emit(fd, "\n");
}

static void cr_posix_backtrace(int fd)
{
#    if !defined(__APPLE__) || defined(COD2_X64)
    void *frames[64];
    int n = backtrace(frames, 64);
    Dl_info dli;
    int i;

    cr_emit(fd, "backtrace (most recent first, %d frames):\n", n);
    for (i = 0; i < n; i++) {
        unsigned long addr = (unsigned long)frames[i];
        if (dladdr(frames[i], &dli) && dli.dli_fname) {
            unsigned long base = (unsigned long)dli.dli_fbase;
            const char *modname = strrchr(dli.dli_fname, '/');
            modname = modname ? modname + 1 : dli.dli_fname;
            if (dli.dli_sname)
                cr_emit(fd, "  #%-2d 0x%012lx  %s+0x%lx  [%s+0x%lx]\n", i, addr,
                        dli.dli_sname, addr - (unsigned long)dli.dli_saddr,
                        modname, addr - base);
            else
                cr_emit(fd, "  #%-2d 0x%012lx  [%s+0x%lx]\n", i, addr, modname, addr - base);
        } else {
            cr_emit(fd, "  #%-2d 0x%012lx\n", i, addr);
        }
    }
    cr_emit(fd, "\n");

#    if !defined(__APPLE__) || !defined(COD2_X64)
    if (cr_exePath[0] && n > 0) {
        char *argv[64 + 5];
        char addrbuf[64][20];
        int argc = 0, k;
        pid_t pid;
        argv[argc++] = (char *)"addr2line";
        argv[argc++] = (char *)"-a";
        argv[argc++] = (char *)"-f";
        argv[argc++] = (char *)"-C";
        argv[argc++] = (char *)"-p";
        argv[argc++] = (char *)"-e";
        argv[argc++] = cr_exePath;
        for (k = 0; k < n && k < 64; k++) {
            snprintf(addrbuf[k], sizeof(addrbuf[k]), "0x%lx", (unsigned long)frames[k]);
            argv[argc++] = addrbuf[k];
        }
        argv[argc] = NULL;
        cr_emit(fd, "resolved backtrace:\n");
        pid = fork();
        if (pid == 0) {
            if (fd >= 0)
                dup2(fd, 1);
            else {
                int dn = open("/dev/null", O_WRONLY);
                if (dn >= 0)
                    dup2(dn, 1);
            }
            dup2(1, 2);
            execvp("addr2line", argv);
            _exit(127);
        } else if (pid > 0) {
            int st;
            waitpid(pid, &st, 0);
        }
        cr_emit(fd, "\n");
    }

    if (cr_exePath[0]) {
        cr_emit(fd, "to resolve manually: addr2line -f -C -e %s", cr_exePath);
        for (i = 0; i < n; i++)
            cr_emit(fd, " 0x%lx", (unsigned long)frames[i]);
        cr_emit(fd, "\n\n");
    }
#    else
    cr_emit(fd, "module offsets above can be resolved with atos and the matching arm64 executable.\n\n");
#    endif
    cr_emit(fd, "raw frames:\n");
    if (fd >= 0)
        backtrace_symbols_fd(frames, n, fd);
    cr_emit(fd, "\n");
#    else
    cr_emit(fd, "backtrace: (unavailable on this platform)\n\n");
#    endif
}

static void cr_posix_osinfo(int fd)
{
    struct utsname u;
    if (uname(&u) == 0)
        cr_emit(fd, "os         : %s %s %s (%s)\n", u.sysname, u.release, u.version, u.machine);
}

#    if defined(__APPLE__) && defined(COD2_X64) && COD2_X64
static char cr_reportDirectory[1024] = ".";

void Sys_CrashSetDirectory(const char *path)
{
    if (path && path[0])
        snprintf(cr_reportDirectory, sizeof(cr_reportDirectory), "%s", path);
}

#        if !(defined(COD2_CODX) && COD2_CODX)
/* The original engine's freeze report; CoD2x builds write CoD2x's form below. */
void Sys_CrashFreezeReport(void *context)
{
    char path[1200];
    int fd;
    snprintf(path, sizeof(path), "%s/cod2_freeze_%d.txt", cr_reportDirectory, (int)getpid());
    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    cr_emit_common(fd, "main thread heartbeat stopped for more than 12 seconds (process still running)");
    cr_posix_osinfo(fd);
    cr_posix_regs(fd, context);
    cr_posix_backtrace(fd);
    cr_emit(fd, "================ END FREEZE REPORT ================\n");
    if (fd >= 0) {
        close(fd);
        cr_emit(-1, "Native freeze diagnostic saved to %s\n", path);
    }
}
#        endif
#    endif

#    if defined(__linux__)
#        include <X11/Xlib.h>
#        include <X11/keysym.h>

static void cr_x11_fallback(const char *path)
{
    static char buf[200 * 1024];
    static char *lines[8192];
    int nlines = 0;
    FILE *f;
    size_t rd = 0, i;
    Display *d;
    Window w;
    Atom wmDelete;
    XFontStruct *fnt;
    GC gc;
    int s, fh, asc, top = 0, winH = 700, running = 1;

    f = fopen(path, "rb");
    if (f) {
        rd = fread(buf, 1, sizeof(buf) - 1, f);
        fclose(f);
    }
    buf[rd] = '\0';
    lines[nlines++] = buf;
    for (i = 0; i < rd && nlines < 8192; i++)
        if (buf[i] == '\n') {
            buf[i] = '\0';
            if (i + 1 < rd)
                lines[nlines++] = &buf[i + 1];
        }

    d = XOpenDisplay(NULL);
    if (!d)
        return;
    s = DefaultScreen(d);
    w = XCreateSimpleWindow(d, RootWindow(d, s), 0, 0, 1000, 700, 0,
                            BlackPixel(d, s), WhitePixel(d, s));
    XStoreName(d, w, "CoD2 crashed - scroll: arrows/wheel/PgUp/PgDn, Esc to close");
    wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, w, &wmDelete, 1);
    XSelectInput(d, w, ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask);
    fnt = XLoadQueryFont(d, "9x15");
    if (!fnt)
        fnt = XLoadQueryFont(d, "fixed");
    gc = XCreateGC(d, w, 0, NULL);
    if (fnt)
        XSetFont(d, gc, fnt->fid);
    fh = fnt ? fnt->ascent + fnt->descent : 15;
    asc = fnt ? fnt->ascent : 12;
    XMapWindow(d, w);

    while (running) {
        XEvent e;
        XNextEvent(d, &e);
        if (e.type == Expose || e.type == ConfigureNotify) {
            int rows, k;
            if (e.type == ConfigureNotify)
                winH = e.xconfigure.height;
            XClearWindow(d, w);
            rows = winH / fh;
            for (k = 0; k < rows && top + k < nlines; k++)
                XDrawString(d, w, gc, 4, asc + k * fh, lines[top + k], (int)strlen(lines[top + k]));
        } else if (e.type == KeyPress) {
            KeySym key = XLookupKeysym(&e.xkey, 0);
            int rows = winH / fh;
            if (key == XK_Escape || key == XK_q)
                running = 0;
            else if (key == XK_Down)
                top += 1;
            else if (key == XK_Up)
                top -= 1;
            else if (key == XK_Next)
                top += rows - 2;
            else if (key == XK_Prior)
                top -= rows - 2;
            else if (key == XK_Home)
                top = 0;
            else if (key == XK_End)
                top = nlines - rows;
            if (top < 0)
                top = 0;
            if (top > nlines - 1)
                top = nlines - 1;
            XClearArea(d, w, 0, 0, 0, 0, True);
        } else if (e.type == ButtonPress) {
            if (e.xbutton.button == 5)
                top += 3;
            else if (e.xbutton.button == 4)
                top -= 3;
            if (top < 0)
                top = 0;
            if (top > nlines - 1)
                top = nlines - 1;
            XClearArea(d, w, 0, 0, 0, 0, True);
        } else if (e.type == ClientMessage) {
            running = 0;
        }
    }
    XCloseDisplay(d);
}
#    endif

static void cr_posix_popup(const char *path, const char *desc)
{
    char title[300], fnarg[1100], titlearg[320];
    pid_t pid;
    if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY"))
        return;
    snprintf(title, sizeof(title), "%s crashed - %s", cr_appName, desc);
    snprintf(titlearg, sizeof(titlearg), "--title=%s", title);
    snprintf(fnarg, sizeof(fnarg), "--filename=%s", path);
    pid = fork();
    if (pid == 0) {

        execlp("zenity", "zenity", "--text-info", fnarg, titlearg,
               "--width=1000", "--height=680", "--font=monospace 9", (char *)NULL);
        execlp("kdialog", "kdialog", "--title", title, "--textbox", path, "1000", "680", (char *)NULL);
        execlp("xmessage", "xmessage", "-center", "-file", path, (char *)NULL);
#    if defined(__linux__)
        cr_x11_fallback(path);
#    endif
        _exit(127);
    }

}


#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* ---- CoD2x crash and freeze logs (CoD2x linux/exception.cpp) -----------------
   "cod2x_crash.YYYYMMDD_HHMMSS.<pid>.log" in CoD2x's sections, on Linux and macOS.
   The registers, frame chain and raw stack are this machine's (64-bit); macOS lists
   its loaded images where Linux copies /proc/self/maps.
   A fault in CoD2x-layer code also leaves "<log>.pending.json", CoD2x's report,
   which is sent to CoD2x on the next start. Every other fault goes to the report
   receiver only. */
#        include <sys/resource.h>
#        include <sys/time.h>
#        if defined(__APPLE__)
#            include <mach/mach.h>
#            include <pthread.h>
#            include <sys/sysctl.h>
#        else
#            include <sched.h>
#            include <sys/sysinfo.h>
#            include <sys/syscall.h>
#            include <sys/uio.h>
#            include <gnu/libc-version.h>
#        endif

static char cr_cod2xPath[1200];

/* Read once at start: the handler only copies them. */
static char cr_sysDistro[160], cr_sysKernel[96], cr_sysArch[65], cr_sysLibc[32], cr_sysCpu[128];
static char cr_sysVirt[64], cr_sysAffinity[64];
static int cr_sysPhysical, cr_sysRoot;
static long cr_sysStackKB = -1, cr_sysCoreKB = -1, cr_sysFiles = -1;
static time_t cr_sysStart;

static long cr_limit(int resource, int kilobytes)
{
    struct rlimit limit;
    if (getrlimit(resource, &limit) != 0 || limit.rlim_cur == RLIM_INFINITY)
        return -1;
    return kilobytes ? (long)(limit.rlim_cur / 1024) : (long)limit.rlim_cur;
}

static void cr_cod2x_system(void)
{
    struct utsname name;
    cr_sysStart = time(NULL);
    cr_sysRoot = geteuid() == 0;
    cr_sysStackKB = cr_limit(RLIMIT_STACK, 1);
    cr_sysCoreKB = cr_limit(RLIMIT_CORE, 1);
    cr_sysFiles = cr_limit(RLIMIT_NOFILE, 0);
    if (uname(&name) == 0) {
        snprintf(cr_sysKernel, sizeof(cr_sysKernel), "%s", name.release);
        snprintf(cr_sysArch, sizeof(cr_sysArch), "%s", name.machine);
    }
#        if defined(__APPLE__)
    {
        char version[64] = "";
        size_t size = sizeof(version);
        int value = 0;
        sysctlbyname("kern.osproductversion", version, &size, NULL, 0);
        snprintf(cr_sysDistro, sizeof(cr_sysDistro), "macOS %s", version);
        size = sizeof(cr_sysCpu);
        sysctlbyname("machdep.cpu.brand_string", cr_sysCpu, &size, NULL, 0);
        size = sizeof(value);
        if (sysctlbyname("hw.physicalcpu", &value, &size, NULL, 0) == 0)
            cr_sysPhysical = value;
        value = 0;
        size = sizeof(value);
        sysctlbyname("kern.hv_vmm_present", &value, &size, NULL, 0);
        snprintf(cr_sysVirt, sizeof(cr_sysVirt), "%s", value ? "vm" : "none");
    }
#        else
    {
        char line[512], *value;
        FILE *file = fopen("/etc/os-release", "r");
        int physical[256], count = 0, current = -1, i;
        cpu_set_t set;
        snprintf(cr_sysLibc, sizeof(cr_sysLibc), "glibc %s", gnu_get_libc_version());
        while (file && fgets(line, sizeof(line), file)) {
            if (strncmp(line, "PRETTY_NAME=", 12))
                continue;
            value = line + 12;
            value += *value == '"';
            value[strcspn(value, "\"\n")] = '\0';
            snprintf(cr_sysDistro, sizeof(cr_sysDistro), "%s", value);
        }
        if (file)
            fclose(file);
        file = fopen("/proc/cpuinfo", "r");
        while (file && fgets(line, sizeof(line), file)) {
            if (!strncmp(line, "model name", 10) && !cr_sysCpu[0] && (value = strchr(line, ':')) != NULL) {
                value += 1 + (value[1] == ' ');
                value[strcspn(value, "\n")] = '\0';
                snprintf(cr_sysCpu, sizeof(cr_sysCpu), "%s", value);
            } else if (!strncmp(line, "physical id", 11) && (value = strchr(line, ':')) != NULL) {
                current = atoi(value + 1);
            } else if (!strncmp(line, "core id", 7) && (value = strchr(line, ':')) != NULL) {
                int core = current * 4096 + atoi(value + 1);
                for (i = 0; i < count && physical[i] != core; ++i) {}
                if (i == count && count < 256)
                    physical[count++] = core;
            }
        }
        if (file)
            fclose(file);
        cr_sysPhysical = count;
        snprintf(cr_sysVirt, sizeof(cr_sysVirt), "none");
        if (access("/.dockerenv", F_OK) == 0)
            snprintf(cr_sysVirt, sizeof(cr_sysVirt), "docker");
        else if ((file = fopen("/sys/class/dmi/id/sys_vendor", "r")) != NULL) {
            if (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = '\0';
                if (strstr(line, "QEMU") || strstr(line, "VMware") || strstr(line, "innotek") ||
                    strstr(line, "Microsoft") || strstr(line, "Xen") || strstr(line, "KVM"))
                    snprintf(cr_sysVirt, sizeof(cr_sysVirt), "%s", line);
            }
            fclose(file);
        }
        if (sched_getaffinity(0, sizeof(set), &set) == 0)
            snprintf(cr_sysAffinity, sizeof(cr_sysAffinity), "%d of %ld", CPU_COUNT(&set),
                     sysconf(_SC_NPROCESSORS_CONF));
    }
#        endif
}

static void cr_cod2x_path(char *path, size_t size, const char *kind)
{
    time_t now = time(NULL);
    struct tm when;
    const char *directory = ".";
#        if defined(__APPLE__)
    directory = cr_reportDirectory;
#        endif
    localtime_r(&now, &when);
    snprintf(path, size, "%s/cod2x_%s.%04d%02d%02d_%02d%02d%02d.%d.log", directory, kind, when.tm_year + 1900,
             when.tm_mon + 1, when.tm_mday, when.tm_hour, when.tm_min, when.tm_sec, (int)getpid());
}

/* Reads this process's memory without faulting. */
static int cr_cod2x_read(uintptr_t address, void *out, size_t length)
{
#        if defined(__APPLE__)
    vm_size_t read = 0;
    return vm_read_overwrite(mach_task_self(), (vm_address_t)address, length, (vm_address_t)out, &read) ==
               KERN_SUCCESS &&
           read == length;
#        else
    struct iovec local = {out, length}, remote = {(void *)address, length};
    return process_vm_readv(getpid(), &local, 1, &remote, 1, 0) == (ssize_t)length;
#        endif
}

/* Module name, offset in it and "symbol+0xDISP" of an address. */
static void cr_cod2x_resolve(uintptr_t address, char *module, size_t moduleSize, uintptr_t *offset, char *symbol,
                             size_t symbolSize)
{
    Dl_info info;
    const char *name;
    module[0] = symbol[0] = '\0';
    *offset = address;
    if (!dladdr((void *)address, &info) || !info.dli_fname) {
        snprintf(module, moduleSize, "<unknown>");
        return;
    }
    name = strrchr(info.dli_fname, '/');
    snprintf(module, moduleSize, "%s", name ? name + 1 : info.dli_fname);
    *offset = address - (uintptr_t)info.dli_fbase;
    if (info.dli_sname)
        snprintf(symbol, symbolSize, "%s+0x%lx", info.dli_sname, (unsigned long)(address - (uintptr_t)info.dli_saddr));
}

/* "module+0xOFFSET (symbol+0xDISP)" */
static void cr_cod2x_describe(uintptr_t address, char *out, size_t size)
{
    char module[128], symbol[256];
    uintptr_t offset;
    cr_cod2x_resolve(address, module, sizeof(module), &offset, symbol, sizeof(symbol));
    if (!strcmp(module, "<unknown>"))
        snprintf(out, size, "<unknown>");
    else if (symbol[0])
        snprintf(out, size, "%s+0x%lx (%s)", module, (unsigned long)offset, symbol);
    else
        snprintf(out, size, "%s+0x%lx", module, (unsigned long)offset);
}

static void cr_cod2x_frame(void *ucontext, uintptr_t *pc, uintptr_t *sp, uintptr_t *fp, uintptr_t *lr)
{
    ucontext_t *uc = (ucontext_t *)ucontext;
    *pc = *sp = *fp = *lr = 0;
    if (!uc)
        return;
#        if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
    *pc = (uintptr_t)__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss);
    *sp = (uintptr_t)__darwin_arm_thread_state64_get_sp(uc->uc_mcontext->__ss);
    *fp = (uintptr_t)__darwin_arm_thread_state64_get_fp(uc->uc_mcontext->__ss);
    *lr = (uintptr_t)__darwin_arm_thread_state64_get_lr(uc->uc_mcontext->__ss);
#        elif defined(__APPLE__) && defined(__x86_64__)
    *pc = (uintptr_t)uc->uc_mcontext->__ss.__rip;
    *sp = (uintptr_t)uc->uc_mcontext->__ss.__rsp;
    *fp = (uintptr_t)uc->uc_mcontext->__ss.__rbp;
#        elif defined(__x86_64__)
    *pc = (uintptr_t)uc->uc_mcontext.gregs[REG_RIP];
    *sp = (uintptr_t)uc->uc_mcontext.gregs[REG_RSP];
    *fp = (uintptr_t)uc->uc_mcontext.gregs[REG_RBP];
#        endif
}

/* The code addresses of the faulting thread, newest first: the fault, the link
   register where there is one, then the frame record chain ([FP] is the caller's
   frame, [FP + 8] the return address). */
static int cr_cod2x_chain(void *ucontext, uintptr_t *out, int max)
{
    uintptr_t pc, sp, fp, lr;
    int count = 0;
    cr_cod2x_frame(ucontext, &pc, &sp, &fp, &lr);
    if (pc && count < max)
        out[count++] = pc;
    if (lr && count < max)
        out[count++] = lr;
    while (count < max && fp && !(fp & 7)) {
        uintptr_t record[2];
        if (!cr_cod2x_read(fp, record, sizeof(record)) || !record[1])
            break;
        out[count++] = record[1];
        if (record[0] <= fp)
            break;
        fp = record[0];
    }
    return count;
}

/* 1 when the fault belongs to the CoD2x layer: the first address of the faulting
   thread inside this executable is CoD2x-layer code. A fault in a system library
   counts for the code of ours that called it. */
static int cr_cod2x_owned(void *ucontext)
{
    uintptr_t chain[64];
    Dl_info self, info;
    int count = cr_cod2x_chain(ucontext, chain, 64), i;
    if (!dladdr((void *)&Sys_InstallCrashHandler, &self))
        return 0;
    for (i = 0; i < count; ++i) {
        if (dladdr((void *)chain[i], &info) && info.dli_fbase == self.dli_fbase)
            return Cod2x_InLayer((const void *)chain[i]);
    }
    return 0;
}

static unsigned long cr_cod2x_thread(void)
{
#        if defined(__APPLE__)
    uint64_t id = 0;
    pthread_threadid_np(NULL, &id);
    return (unsigned long)id;
#        else
    return (unsigned long)syscall(SYS_gettid);
#        endif
}

static void cr_cod2x_registers(int fd, void *ucontext)
{
    ucontext_t *uc = (ucontext_t *)ucontext;
    int i;
    if (!uc)
        return;
    cr_emit(fd, "\n=== REGISTERS ===\n");
#        if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
    {
        const _STRUCT_ARM_THREAD_STATE64 *r = &uc->uc_mcontext->__ss;
        cr_emit(fd, "PC=0x%016llx  SP=0x%016llx  FP=0x%016llx  LR=0x%016llx\n",
                (unsigned long long)__darwin_arm_thread_state64_get_pc(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_sp(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_fp(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_lr(*r));
        for (i = 0; i < 29; ++i)
            cr_emit(fd, "X%-2d=0x%016llx%s", i, (unsigned long long)r->__x[i], i % 4 == 3 || i == 28 ? "\n" : "  ");
        cr_emit(fd, "CPSR=0x%08x\n", r->__cpsr);
    }
#        elif defined(__x86_64__) && !defined(__APPLE__)
    {
        greg_t *r = uc->uc_mcontext.gregs;
        static const struct {
            const char *name;
            int index;
        } names[] = {{"RIP", REG_RIP}, {"RAX", REG_RAX}, {"RBX", REG_RBX}, {"RCX", REG_RCX}, {"RDX", REG_RDX},
                     {"RSI", REG_RSI}, {"RDI", REG_RDI}, {"RBP", REG_RBP}, {"RSP", REG_RSP}, {"R8", REG_R8},
                     {"R9", REG_R9},   {"R10", REG_R10}, {"R11", REG_R11}, {"R12", REG_R12}, {"R13", REG_R13},
                     {"R14", REG_R14}, {"R15", REG_R15}, {"EFL", REG_EFL}};
        for (i = 0; i < (int)(sizeof(names) / sizeof(names[0])); ++i)
            cr_emit(fd, "%s=0x%016llx%s", names[i].name, (unsigned long long)r[names[i].index],
                    i % 4 == 3 || i == (int)(sizeof(names) / sizeof(names[0])) - 1 ? "\n" : "  ");
    }
#        else
    (void)i;
    cr_emit(fd, "(not available on this machine)\n");
#        endif
}

/* The log after its first lines: CoD2x's sections. */
static void cr_cod2x_sections(int fd, const char *path, void *ucontext, const char *what)
{
    char description[512];
    uintptr_t pc, sp, fp, lr, chain[64], words[64];
    void *frames[64];
    int count, i;
    cr_cod2x_frame(ucontext, &pc, &sp, &fp, &lr);
    if (ucontext) {
        cr_cod2x_describe(pc, description, sizeof(description));
        cr_emit(fd, "Address:      0x%016lx  %s\n", (unsigned long)pc, description);
    }
    cr_cod2x_registers(fd, ucontext);

    cr_emit(fd, "\n=== STACK (frame pointer chain) ===\n");
    count = cr_cod2x_chain(ucontext, chain, 64);
    for (i = 0; i < count; ++i) {
        cr_cod2x_describe(chain[i], description, sizeof(description));
        cr_emit(fd, "%3d  0x%016lx  %s\n", i, (unsigned long)chain[i], description);
    }

    cr_emit(fd, "\n=== STACK (backtrace) ===\n");
    count = backtrace(frames, 64);
    for (i = 0; i < count; ++i) {
        cr_cod2x_describe((uintptr_t)frames[i], description, sizeof(description));
        cr_emit(fd, "%3d  0x%016lx  %s\n", i, (unsigned long)(uintptr_t)frames[i], description);
    }

    cr_emit(fd, "\n=== RAW STACK ===\n");
    if (sp && cr_cod2x_read(sp, words, sizeof(words))) {
        for (i = 0; i < 64; ++i)
            cr_emit(fd, "%s%016lx", i % 4 == 0 ? (i ? "\n0x" : "0x") : " ", (unsigned long)words[i]);
        cr_emit(fd, "\n");
    }

    /* Where every library was, so the addresses above can be resolved later. */
    cr_emit(fd, "\n=== MAPS ===\n");
#        if defined(__APPLE__)
    {
        uint32_t images = _dyld_image_count(), image;
        char line[1200];
        for (image = 0; image < images; ++image) {
            int length = snprintf(line, sizeof(line), "0x%016lx %s\n",
                                  (unsigned long)(uintptr_t)_dyld_get_image_header(image), _dyld_get_image_name(image));
            if (fd >= 0 && length > 0 && write(fd, line, (size_t)length) < 0)
                break;
        }
    }
#        else
    {
        int maps = open("/proc/self/maps", O_RDONLY);
        if (maps >= 0) {
            char buffer[1024];
            ssize_t length;
            while ((length = read(maps, buffer, sizeof(buffer))) > 0)
                if (fd >= 0 && write(fd, buffer, (size_t)length) < 0)
                    break;
            close(maps);
        }
    }
#        endif
    cr_emit(fd, "(written to %s only)\n", path);
    cr_emit(fd, "\n%s report saved to %s\n", what, path);
}

static const char *cr_cod2x_signal(int sig)
{
    switch (sig) {
    case SIGSEGV:
        return "SIGSEGV (segmentation fault)";
    case SIGBUS:
        return "SIGBUS (bus error)";
    case SIGFPE:
        return "SIGFPE (arithmetic exception)";
    case SIGILL:
        return "SIGILL (illegal instruction)";
    case SIGABRT:
        return "SIGABRT (abort)";
    default:
        return "unknown signal";
    }
}

/* ---- CoD2x's report (crash JSON of linux/exception.cpp), built in the handler ---- */
static char cr_json[512 * 1024];
static size_t cr_jsonLength;

static void cr_jraw(const char *format, ...)
{
    va_list args;
    int written;
    if (cr_jsonLength + 1 >= sizeof(cr_json))
        return;
    va_start(args, format);
    written = vsnprintf(cr_json + cr_jsonLength, sizeof(cr_json) - cr_jsonLength, format, args);
    va_end(args);
    if (written > 0)
        cr_jsonLength += (size_t)written;
    if (cr_jsonLength >= sizeof(cr_json))
        cr_jsonLength = sizeof(cr_json) - 1;
}

static void cr_jstr(const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    cr_jraw("\"");
    for (; *p; ++p) {
        if (*p == '"' || *p == '\\')
            cr_jraw("\\%c", *p);
        else if (*p == '\n')
            cr_jraw("\\n");
        else if (*p == '\r')
            cr_jraw("\\r");
        else if (*p == '\t')
            cr_jraw("\\t");
        else if (*p < 0x20)
            cr_jraw("\\u%04x", *p);
        else
            cr_jraw("%c", *p);
    }
    cr_jraw("\"");
}

static void cr_jkv(const char *key, const char *value)
{
    cr_jraw("\"%s\":", key);
    cr_jstr(value);
    cr_jraw(",");
}

static void cr_jframe(int number, uintptr_t address)
{
    char module[128], symbol[256];
    uintptr_t offset;
    cr_cod2x_resolve(address, module, sizeof(module), &offset, symbol, sizeof(symbol));
    cr_jraw("%s{\"frameNumber\":%d,\"instructionAddress\":\"0x%08lx\",\"moduleOffset\":\"0x%08lx\",", number ? "," : "",
            number, (unsigned long)address, (unsigned long)offset);
    cr_jkv("moduleName", module);
    cr_jkv("sectionName", "");
    cr_jraw("\"resolvedSymbol\":");
    cr_jstr(symbol[0] ? symbol : "<no symbol>");
    cr_jraw(",\"instruction\":\"\"}");
}

static void cr_jmemory(const char *name, uintptr_t address, size_t length)
{
    unsigned char bytes[512];
    size_t i;
    cr_jraw("{");
    cr_jkv("name", name);
    cr_jraw("\"address\":\"0x%08lx\",", (unsigned long)address);
    if (length > sizeof(bytes))
        length = sizeof(bytes);
    if (address && cr_cod2x_read(address, bytes, length)) {
        cr_jraw("\"hex\":\"");
        for (i = 0; i < length; ++i)
            cr_jraw("%02x", bytes[i]);
        cr_jraw("\"");
    } else {
        cr_jraw("\"hex\":\"\",\"note\":\"unreadable\"");
    }
    cr_jraw("}");
}

static void cr_jregisters(void *ucontext)
{
    ucontext_t *uc = (ucontext_t *)ucontext;
    int i;
    cr_jraw("\"registers\":{");
    if (!uc) {
        cr_jraw("}");
        return;
    }
#        if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
    {
        const _STRUCT_ARM_THREAD_STATE64 *r = &uc->uc_mcontext->__ss;
        cr_jraw("\"PC\":\"0x%016llx\",\"SP\":\"0x%016llx\",\"FP\":\"0x%016llx\",\"LR\":\"0x%016llx\",",
                (unsigned long long)__darwin_arm_thread_state64_get_pc(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_sp(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_fp(*r),
                (unsigned long long)__darwin_arm_thread_state64_get_lr(*r));
        for (i = 0; i < 29; ++i)
            cr_jraw("\"X%d\":\"0x%016llx\",", i, (unsigned long long)r->__x[i]);
        cr_jraw("\"CPSR\":\"0x%08x\"", r->__cpsr);
    }
#        elif defined(__x86_64__) && !defined(__APPLE__)
    {
        greg_t *r = uc->uc_mcontext.gregs;
        static const struct {
            const char *name;
            int index;
        } names[] = {{"RIP", REG_RIP}, {"RAX", REG_RAX}, {"RBX", REG_RBX}, {"RCX", REG_RCX}, {"RDX", REG_RDX},
                     {"RSI", REG_RSI}, {"RDI", REG_RDI}, {"RBP", REG_RBP}, {"RSP", REG_RSP}, {"R8", REG_R8},
                     {"R9", REG_R9},   {"R10", REG_R10}, {"R11", REG_R11}, {"R12", REG_R12}, {"R13", REG_R13},
                     {"R14", REG_R14}, {"R15", REG_R15}, {"EFLAGS", REG_EFL}};
        for (i = 0; i < (int)(sizeof(names) / sizeof(names[0])); ++i)
            cr_jraw("%s\"%s\":\"0x%016llx\"", i ? "," : "", names[i].name, (unsigned long long)r[names[i].index]);
    }
#        else
    (void)i;
#        endif
    cr_jraw("}");
}

static void cr_jmodules(void)
{
    int first = 1;
    cr_jraw("\"modules\":[");
#        if defined(__APPLE__)
    {
        uint32_t images = _dyld_image_count(), image;
        for (image = 0; image < images && image < 64; ++image) {
            const char *path = _dyld_get_image_name(image), *name;
            if (!path)
                continue;
            name = strrchr(path, '/');
            cr_jraw("%s{\"baseAddress\":\"0x%08lx\",", first ? "" : ",",
                    (unsigned long)(uintptr_t)_dyld_get_image_header(image));
            first = 0;
            cr_jkv("moduleName", name ? name + 1 : path);
            cr_jraw("\"path\":");
            cr_jstr(path);
            cr_jraw("}");
        }
    }
#        else
    {
        static char buffer[65536];
        char seen[64][256], *line, *next;
        int seenCount = 0, fd = open("/proc/self/maps", O_RDONLY), i;
        ssize_t total = fd >= 0 ? read(fd, buffer, sizeof(buffer) - 1) : -1;
        if (fd >= 0)
            close(fd);
        buffer[total > 0 ? total : 0] = '\0';
        for (line = buffer; line && *line; line = next) {
            unsigned long start = 0;
            char permissions[8] = {0}, path[256] = {0};
            const char *name;
            next = strchr(line, '\n');
            if (next)
                *next++ = '\0';
            if (sscanf(line, "%lx-%*x %7s %*x %*x:%*x %*u %255[^\n]", &start, permissions, path) < 3 ||
                path[0] != '/' || permissions[2] != 'x')
                continue;
            name = strrchr(path, '/') + 1;
            for (i = 0; i < seenCount && strcmp(seen[i], name); ++i) {}
            if (i < seenCount || seenCount == 64)
                continue;
            snprintf(seen[seenCount++], sizeof(seen[0]), "%s", name);
            cr_jraw("%s{\"baseAddress\":\"0x%08lx\",", first ? "" : ",", start);
            first = 0;
            cr_jkv("moduleName", name);
            cr_jraw("\"path\":");
            cr_jstr(path);
            cr_jraw("}");
        }
    }
#        endif
    cr_jraw("]");
}

/* The server dvars CoD2x's report carries (empty before the dvars exist). */
static void cr_jcvars(void)
{
    static const char *const names[] = {"sv_hostname", "mapname",       "g_gametype", "fs_game",      "dedicated",
                                        "net_port",    "sv_maxclients", "protocol",   "sv_punkbuster"};
    int first = 1;
    size_t i;
    cr_jraw("\"cvars\":{");
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        char value[256];
        if (!Cod2x_DvarText(names[i], value, sizeof(value)))
            continue;
        cr_jraw("%s\"%s\":", first ? "" : ",", names[i]);
        cr_jstr(value);
        first = 0;
    }
    cr_jraw("}");
}


static void cr_jlines(const char *field, const char *text)
{
    const char *p = text ? text : "";
    int first = 1;
    cr_jraw("\"%s\":[", field);
    while (*p) {
        const char *start = p;
        char line[1024];
        size_t length;
        while (*p && *p != '\n' && *p != '\r')
            ++p;
        length = (size_t)(p - start);
        if (length) {
            if (length >= sizeof(line))
                length = sizeof(line) - 1;
            memcpy(line, start, length);
            line[length] = '\0';
            cr_jraw("%s", first ? "" : ",");
            cr_jstr(line);
            first = 0;
        }
        while (*p == '\n' || *p == '\r')
            ++p;
    }
    cr_jraw("]");
}

static void cr_cod2x_json(const char *kind, int sig, siginfo_t *info, void *ucontext, const char *path,
                          const char *reason)
{
    static char logger[64 * 1024];
    char module[128], symbol[256], when[32], fault[512], exe[1024] = "", host[128] = "";
    uintptr_t pc, sp, fp, lr, offset, chain[64], faultAddress;
    void *frames[64];
    unsigned long thread = cr_cod2x_thread();
    long cores = sysconf(_SC_NPROCESSORS_ONLN);
    time_t now = time(NULL);
    struct tm local;
    struct utsname name;
    char os[256];
    int count, i;
    unsigned long totalMB = 0, freeMB = 0, swapTotalMB = 0, swapFreeMB = 0, residentMB = 0;
    long peakMB = -1, virtualPeakMB = -1, threads = -1, loadUptime = 0;
    double load = 0;

    cr_cod2x_frame(ucontext, &pc, &sp, &fp, &lr);
    faultAddress = info && sig != SIGABRT ? (uintptr_t)info->si_addr : 0;
    cr_cod2x_resolve(pc, module, sizeof(module), &offset, symbol, sizeof(symbol));
    localtime_r(&now, &local);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &local);
    if (reason)
        snprintf(fault, sizeof(fault), "%s", reason);
    else
        snprintf(fault, sizeof(fault), "%s (code %d) at address 0x%08lx%s", cr_cod2x_signal(sig),
                 info ? info->si_code : 0, (unsigned long)faultAddress,
                 faultAddress && faultAddress < 0x10000 ? " (near NULL)" : "");
    snprintf(exe, sizeof(exe), "%s", cr_exePath);

    cr_jsonLength = 0;
    cr_json[0] = '\0';
    cr_jraw("{");
    cr_jkv("version", COD2X_RELEASE);
    cr_jkv("type", "linux-server");
    cr_jkv("installId", "");

    cr_jraw("\"crashInfo\":{");
    cr_jkv("kind", kind);
    cr_jraw("\"exceptionCode\":\"0x%08x\",", (unsigned)sig);
    cr_jkv("exceptionName", sig ? cr_cod2x_signal(sig) : "");
    cr_jraw("\"exceptionAddress\":\"0x%08lx\",", (unsigned long)pc);
    cr_jkv("moduleName", module[0] ? module : "<unknown>");
    cr_jraw("\"fileOffset\":\"0x%08lx\",", (unsigned long)offset);
    cr_jkv("faultDescription", fault);
    cr_jraw("\"crashThreadId\":\"%lu\",\"currentThreadId\":\"%lu\",", thread, thread);
    cr_jkv("time", when);
    cr_jraw("\"test\":0,\"signalCode\":%d},", info ? info->si_code : 0);

    cr_jraw("\"crashContext\":{");
    cr_jregisters(ucontext);
    cr_jraw(",\"memory\":[");
    cr_jmemory("stack", sp, 256);
    cr_jraw(",");
    cr_jmemory("code", pc >= 32 ? pc - 32 : 0, 64);
    cr_jraw("],\"threads\":[],\"stackCodePointers\":[]},");

    cr_jraw("\"stacks\":[{");
    cr_jkv("threadRole", "crashing thread");
    cr_jkv("threadName", "");
    cr_jraw("\"threadId\":\"%lu\",", thread);
    cr_jkv("method", "frame pointer chain");
    cr_jraw("\"frames\":[");
    count = cr_cod2x_chain(ucontext, chain, 64);
    for (i = 0; i < count; ++i)
        cr_jframe(i, chain[i]);
    cr_jraw("],\"diagnostics\":{\"method\":\"frame pointer chain (frame pointer unwind)\"}},{");
    cr_jkv("threadRole", "crashing thread");
    cr_jkv("threadName", "");
    cr_jraw("\"threadId\":\"%lu\",", thread);
    cr_jkv("method", "backtrace");
    cr_jraw("\"frames\":[");
    count = backtrace(frames, 64);
    for (i = 0; i < count; ++i)
        cr_jframe(i, (uintptr_t)frames[i]);
#        if defined(__APPLE__)
    cr_jraw("],\"diagnostics\":{\"method\":\"libSystem backtrace\"}}],");
#        else
    cr_jraw("],\"diagnostics\":{\"method\":\"glibc backtrace\"}}],");
#        endif

    memset(&name, 0, sizeof(name));
    uname(&name);
#        if defined(__APPLE__)
    {
        /* A Mac's computer name usually carries its owner's name; CoD2x's
           Windows client report has no computer name either. */
        uint64_t memory = 0;
        size_t size = sizeof(memory);
        struct xsw_usage swap;
        vm_statistics64_data_t vm;
        mach_msg_type_number_t vmCount = HOST_VM_INFO64_COUNT;
        mach_task_basic_info_data_t task;
        mach_msg_type_number_t taskCount = MACH_TASK_BASIC_INFO_COUNT;
        thread_act_array_t list;
        mach_msg_type_number_t listCount;
        struct loadavg averages;
        struct timeval boot;
        if (sysctlbyname("hw.memsize", &memory, &size, NULL, 0) == 0)
            totalMB = (unsigned long)(memory / (1024 * 1024));
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &vmCount) == KERN_SUCCESS)
            freeMB = (unsigned long)(((uint64_t)vm.free_count * vm_page_size) / (1024 * 1024));
        size = sizeof(swap);
        if (sysctlbyname("vm.swapusage", &swap, &size, NULL, 0) == 0) {
            swapTotalMB = (unsigned long)(swap.xsu_total / (1024 * 1024));
            swapFreeMB = (unsigned long)(swap.xsu_avail / (1024 * 1024));
        }
        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&task, &taskCount) == KERN_SUCCESS) {
            residentMB = (unsigned long)(task.resident_size / (1024 * 1024));
            peakMB = (long)(task.resident_size_max / (1024 * 1024));
        }
        if (task_threads(mach_task_self(), &list, &listCount) == KERN_SUCCESS) {
            threads = (long)listCount;
            for (i = 0; i < (int)listCount; ++i)
                mach_port_deallocate(mach_task_self(), list[i]);
            vm_deallocate(mach_task_self(), (vm_address_t)list, listCount * sizeof(list[0]));
        }
        size = sizeof(averages);
        if (sysctlbyname("vm.loadavg", &averages, &size, NULL, 0) == 0 && averages.fscale)
            load = (double)averages.ldavg[0] / averages.fscale;
        size = sizeof(boot);
        if (sysctlbyname("kern.boottime", &boot, &size, NULL, 0) == 0)
            loadUptime = (long)(now - boot.tv_sec);
    }
#        else
    {
        struct sysinfo system;
        unsigned long unit;
        char text[8192], *field;
        int fd;
        gethostname(host, sizeof(host) - 1);
        memset(&system, 0, sizeof(system));
        sysinfo(&system);
        unit = system.mem_unit ? system.mem_unit : 1;
        totalMB = (unsigned long)(((unsigned long long)system.totalram * unit) / (1024 * 1024));
        freeMB = (unsigned long)(((unsigned long long)system.freeram * unit) / (1024 * 1024));
        swapTotalMB = (unsigned long)(((unsigned long long)system.totalswap * unit) / (1024 * 1024));
        swapFreeMB = (unsigned long)(((unsigned long long)system.freeswap * unit) / (1024 * 1024));
        load = system.loads[0] / 65536.0;
        loadUptime = (long)system.uptime;
        if ((fd = open("/proc/self/statm", O_RDONLY)) >= 0) {
            ssize_t length = read(fd, text, sizeof(text) - 1);
            unsigned long pages = 0, resident = 0;
            close(fd);
            text[length > 0 ? length : 0] = '\0';
            if (sscanf(text, "%lu %lu", &pages, &resident) == 2)
                residentMB = (resident * (unsigned long)sysconf(_SC_PAGESIZE)) / (1024 * 1024);
        }
        if ((fd = open("/proc/self/status", O_RDONLY)) >= 0) {
            ssize_t length = read(fd, text, sizeof(text) - 1);
            close(fd);
            text[length > 0 ? length : 0] = '\0';
            if ((field = strstr(text, "VmPeak:")) != NULL)
                virtualPeakMB = atol(field + 7) / 1024;
            if ((field = strstr(text, "VmHWM:")) != NULL)
                peakMB = atol(field + 6) / 1024;
            if ((field = strstr(text, "Threads:")) != NULL)
                threads = atol(field + 8);
        }
    }
#        endif

    cr_jraw("\"systemInfo\":{\"system\":{");
    snprintf(os, sizeof(os), "%s %s %s", name.sysname, name.release, name.machine);
    cr_jkv("os", os);
    cr_jkv("distro", cr_sysDistro);
    cr_jkv("kernel", cr_sysKernel[0] ? cr_sysKernel : name.release);
    cr_jkv("arch", cr_sysArch[0] ? cr_sysArch : name.machine);
    cr_jkv("libc", cr_sysLibc);
    cr_jkv("cpuModel", cr_sysCpu);
    cr_jkv("hostname", host);
    cr_jraw("\"cpuCores\":\"%ld\",\"cpuCoresPhysical\":\"%d\",", cores > 0 ? cores : 0, cr_sysPhysical);
    cr_jkv("cpuAffinity", cr_sysAffinity);
    cr_jkv("virtualization", cr_sysVirt);
    cr_jraw("\"isRoot\":%s,\"ulimitStackKB\":\"%ld\",\"ulimitCoreKB\":\"%ld\",\"ulimitNoFile\":\"%ld\",",
            cr_sysRoot ? "true" : "false", cr_sysStackKB, cr_sysCoreKB, cr_sysFiles);
    cr_jraw("\"loadAverage\":\"%.2f\",\"uptimeSeconds\":\"%ld\",", load, loadUptime);
    /* The build: CoD2x's own servers say "cod2_lnxded" here. */
    cr_jraw("\"platform\":");
    cr_jstr(COD2X_VERSION " " COD2X_PLATFORM);
    cr_jraw("},\"memory\":{\"physicalTotalMB\":\"%lu\",\"physicalAvailableMB\":\"%lu\",\"swapTotalMB\":\"%lu\","
            "\"swapFreeMB\":\"%lu\"},",
            totalMB, freeMB, swapTotalMB, swapFreeMB);
    cr_jraw("\"processMemory\":{\"workingSetMB\":\"%lu\",\"peakWorkingSetMB\":\"%ld\",\"virtualPeakMB\":\"%ld\","
            "\"threadCount\":\"%ld\",\"processUptimeSeconds\":\"%ld\"},",
            residentMB, peakMB, virtualPeakMB, threads, (long)(now - cr_sysStart));
    cr_jraw("\"process\":{\"processId\":\"%d\",\"executablePath\":", (int)getpid());
    cr_jstr(exe);
    cr_jraw(",");
    cr_jkv("reportFile", path);
    cr_jraw("\"commandLine\":");
    cr_jstr(cr_cmdline);
    cr_jraw("},");
    cr_jmodules();
    cr_jraw(",");
    cr_jcvars();
    cr_jraw("},");
    Cod2x_LoggerRecent(logger, sizeof(logger));
    cr_jlines("loggerLines", logger);
    cr_jraw(",");
    cr_jlines("logLines", "");
    cr_jraw("}");
}

/* "<log>.pending.json": open and write only, safe in the handler. */
static void cr_cod2x_pending(const char *path)
{
    char pending[1300];
    int fd;
    snprintf(pending, sizeof(pending), "%s.pending.json", path);
    fd = open(pending, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0)
        return;
    if (write(fd, cr_json, cr_jsonLength) < 0) {
        /* nothing more can be done in the handler */
    }
    close(fd);
}

/* After the log: CoD2x's report when the fault is CoD2x's, else the receiver. */
static void cr_cod2x_route(const char *kind, int sig, siginfo_t *info, void *ucontext, const char *path,
                           const char *reason)
{
    if (cr_cod2x_owned(ucontext)) {
        cr_cod2x_json(kind, sig, info, ucontext, path, sig ? NULL : reason);
        cr_cod2x_pending(path);
    } else {
        Sys_CrashReceive(path, reason);
    }
}

static void cr_cod2x_crash(int sig, siginfo_t *info, void *ucontext, const char *reason)
{
    int fd;
    cr_cod2x_path(cr_cod2xPath, sizeof(cr_cod2xPath), "crash");
#        if defined(__APPLE__)
    fd = open(cr_cod2xPath, O_CREAT | O_TRUNC | O_WRONLY, 0600);
#        else
    fd = open(cr_cod2xPath, O_CREAT | O_TRUNC | O_WRONLY, 0644);
#        endif
    cr_emit(fd, "\n=== COD2X CRASH ===\n");
    cr_emit(fd, "Version:      %s\n", COD2X_RELEASE);
    cr_emit(fd, "Signal:       %d %s, code %d\n", sig, cr_cod2x_signal(sig), info ? info->si_code : 0);
    if (info && sig != SIGABRT) {
        uintptr_t fault = (uintptr_t)info->si_addr;
        cr_emit(fd, "Fault addr:   0x%016lx%s\n", (unsigned long)fault, fault < 0x10000 ? " (near NULL)" : "");
    }
    cr_cod2x_sections(fd, cr_cod2xPath, ucontext, "Crash");
    if (fd >= 0)
        close(fd);
    cr_cod2x_route("crash", sig, info, ucontext, cr_cod2xPath, reason);
}

#            if defined(__APPLE__)
/* The freeze watch (macOS): CoD2x has no freeze log on Linux, so this one takes
   its crash log's form; CoD2x's report of a freeze has kind "freeze". */
void Sys_CrashFreezeReport(void *context)
{
    static const char reason[] = "main thread heartbeat stopped for more than 12 seconds (process still running)";
    char path[1200];
    int fd;
    cr_cod2x_path(path, sizeof(path), "freeze");
    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    cr_emit(fd, "\n=== COD2X FREEZE ===\n");
    cr_emit(fd, "Version:      %s\n", COD2X_RELEASE);
    cr_emit(fd, "Reason:       %s\n", reason);
    cr_cod2x_sections(fd, path, context, "Freeze");
    if (fd >= 0)
        close(fd);
    cr_cod2x_route("freeze", 0, NULL, context, path, reason);
}
#            endif

/* What cod2x_report.c sends besides a crash: CoD2x's Linux forms. */
const char *Sys_CrashDirectory(void)
{
#            if defined(__APPLE__)
    return cr_reportDirectory;
#            else
    return ".";
#            endif
}

const char *Sys_Cod2xReportType(void)
{
    return "linux-server";
}

static size_t cr_cod2x_add(char *out, size_t size, size_t used, const char *key, const char *value, int last)
{
    const char *p;
    if (used >= size)
        return used;
    used += (size_t)snprintf(out + used, size - used, "\"%s\":\"", key);
    for (p = value; *p && used + 7 < size; ++p) {
        if (*p == '"' || *p == '\\')
            out[used++] = '\\';
        if ((unsigned char)*p >= 0x20)
            out[used++] = *p;
    }
    if (used < size)
        used += (size_t)snprintf(out + used, size - used, "\"%s", last ? "" : ",");
    return used < size ? used : size - 1;
}

/* CoD2x's exception_getSystemInfoJson (Linux). */
size_t Sys_Cod2xSystemInfo(char *out, size_t size)
{
    char os[256];
    struct utsname name;
    size_t used = 1;
    memset(&name, 0, sizeof(name));
    uname(&name);
    snprintf(os, sizeof(os), "%s %s %s", name.sysname, cr_sysKernel, cr_sysArch);
    out[0] = '{';
    used = cr_cod2x_add(out, size, used, "os", os, 0);
    used = cr_cod2x_add(out, size, used, "distro", cr_sysDistro, 0);
    used = cr_cod2x_add(out, size, used, "arch", cr_sysArch, 0);
    used = cr_cod2x_add(out, size, used, "libc", cr_sysLibc, 0);
    used = cr_cod2x_add(out, size, used, "cpuModel", cr_sysCpu, 0);
    used = cr_cod2x_add(out, size, used, "virtualization", cr_sysVirt, 0);
    used += (size_t)snprintf(out + used, size - used, "\"isElevated\":%s}", cr_sysRoot ? "true" : "false");
    return used;
}

/* CoD2x's Linux reports carry no install id and no hwid2. */
size_t Sys_Cod2xIdentity(char *out, size_t size)
{
    return (size_t)snprintf(out, size, "\"installId\":\"\",\"hwid2\":\"\",");
}

size_t Sys_Cod2xLogLines(char *out, size_t size)
{
    if (size)
        out[0] = '\0';
    return 0;
}
#    endif

static struct sigaction cr_old[8];
static const int cr_sigs[] = { SIGSEGV, SIGABRT, SIGFPE, SIGILL, SIGBUS
#if defined(COD2_X64) && defined(__APPLE__)
    , SIGTRAP
#endif
};

static void cr_posix_handler(int sig, siginfo_t *info, void *ucontext)
{
    char path[1024];
    char desc[256];
    int fd;

    if (cr_inHandler) {
        signal(sig, SIG_DFL);
        raise(sig);
        return;
    }
    cr_inHandler = 1;

    if ((sig == SIGSEGV || sig == SIGBUS) && info)
        snprintf(desc, sizeof(desc), "%s, fault address 0x%lx",
                 cr_sig_name(sig), (unsigned long)(uintptr_t)info->si_addr);
    else
        snprintf(desc, sizeof(desc), "%s", cr_sig_name(sig));

#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    (void)fd;
    cr_cod2x_crash(sig, info, ucontext, desc);
    snprintf(path, sizeof(path), "%s", cr_cod2xPath);
#    else
#        if defined(__APPLE__) && defined(COD2_X64) && COD2_X64
    /* The original engine in the app: reports go to the game's home. */
    snprintf(path, sizeof(path), "%s/cod2_crash_%d.txt", cr_reportDirectory, (int)getpid());
    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0600);
#        else
    snprintf(path, sizeof(path), "cod2_crash_%d.txt", (int)getpid());
    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
#        endif

    cr_emit_common(fd, desc);
    cr_posix_osinfo(fd);
    cr_emit(fd, "\n");
    cr_posix_regs(fd, ucontext);
    cr_posix_backtrace(fd);
    cr_emit(fd, "================ END CRASH REPORT ================\n");
    if (fd >= 0)
        close(fd);
#    endif

    cr_posix_popup(path, desc);

    signal(sig, SIG_DFL);
    raise(sig);
}

void Sys_InstallCrashHandler(const char *appName, const char *version,
                             const char *buildDate, const char *cmdline)
{
    static char altstack[65536];
    stack_t ss;
    struct sigaction sa;
    unsigned i;

    if (appName && appName[0])
        snprintf(cr_appName, sizeof(cr_appName), "%s", appName);
    if (version && version[0])
        snprintf(cr_version, sizeof(cr_version), "%s", version);
    if (buildDate && buildDate[0])
        snprintf(cr_buildDate, sizeof(cr_buildDate), "%s", buildDate);
    if (cmdline)
        snprintf(cr_cmdline, sizeof(cr_cmdline), "%s", cmdline);
#    if defined(__APPLE__) && defined(COD2_X64)
    {
        uint32_t size = sizeof(cr_exePath);
        if (_NSGetExecutablePath(cr_exePath, &size) != 0)
            cr_exePath[0] = '\0';
    }
#    else
    {
        ssize_t n = readlink("/proc/self/exe", cr_exePath, sizeof(cr_exePath) - 1);
        if (n > 0)
            cr_exePath[n] = '\0';
    }
#    endif

#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    cr_cod2x_system();
#    endif
    ss.ss_sp = altstack;
    ss.ss_size = sizeof(altstack);
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);

    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = cr_posix_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    for (i = 0; i < sizeof(cr_sigs) / sizeof(cr_sigs[0]); i++)
        sigaction(cr_sigs[i], &sa, &cr_old[i]);
}

#endif
