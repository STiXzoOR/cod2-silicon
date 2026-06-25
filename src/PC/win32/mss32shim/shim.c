#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void LOG(const char *fmt, ...)
{
    char buf[600];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    buf[sizeof(buf) - 1] = 0;
    {
        FILE *f = fopen("Z:\\tmp\\shim.log", "a");
        if (f) {
            fputs(buf, f);
            fclose(f);
        }
    }
}

static HANDLE g_mainThread;
static DWORD g_watchAddr;
static int g_fired;
static DWORD g_flakXModel;

static int g_syncLogs;
static int g_syncSkips;
static const char *SafeStr(unsigned p)
{
    if (p < 0x10000 || p >= 0x7f000000)
        return "<nonptr>";
    if (IsBadStringPtrA((LPCSTR)p, 64))
        return "<unreadable>";
    return (const char *)p;
}
static void DumpSyncEntry(CONTEXT *c, const char *which)
{
    unsigned esi = c->Esi, edi = c->Edi, base = esi - 0x40;
    unsigned f00 = *(unsigned *)(base + 0x00);
    unsigned f04 = *(unsigned *)(base + 0x04);
    unsigned f1c = *(unsigned *)(base + 0x1c);
    unsigned f2c = *(unsigned *)(base + 0x2c);
    unsigned f40b = *(unsigned char *)(base + 0x40);
    unsigned handle = *(unsigned *)edi;
    LOG("  [SYNC-NULL %s] idx=%d entry=0x%08x  +0x00=0x%08x +0x04=0x%08x "
        "+0x1c=0x%08x(NULL) +0x2c=0x%08x +0x40b=0x%02x  *edi=0x%08x  "
        "name@+0=%.48s  *edi->str@+0=%.48s\n",
        which, (int)c->Ebp, base, f00, f04, f1c, f2c, f40b, handle,
        SafeStr(f00),
        (handle >= 0x10000 && handle < 0x7f000000 && !IsBadReadPtr((void *)handle, 4))
            ? SafeStr(*(unsigned *)handle)
            : "<n/a>");
}

static LONG CALLBACK Veh(EXCEPTION_POINTERS *ep)
{
    CONTEXT *c = ep->ContextRecord;
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP) {
        if (c->Dr6 & 1) {
            unsigned val = g_watchAddr ? *(unsigned *)g_watchAddr : 0;
            LOG("WRITE to XModel+0  eip=0x%08x  newValue=0x%08x  (hit #%d)\n",
                (unsigned)c->Eip, val, ++g_fired);
        }
        c->Dr6 = 0;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    if (ep->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
        if (c->Eip == 0x44e783) {
            if (g_syncLogs++ < 24)
                DumpSyncEntry(c, "L1");
            c->Eip = 0x44e7a8;
            g_syncSkips++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (c->Eip == 0x44e7f8) {
            if (g_syncLogs++ < 24)
                DumpSyncEntry(c, "L2");
            c->Eip = 0x44e818;
            g_syncSkips++;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static DWORD WINAPI ArmThread(LPVOID p)
{
    CONTEXT ctx;
    (void)p;
    SuspendThread(g_mainThread);
    memset(&ctx, 0, sizeof ctx);
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(g_mainThread, &ctx);
    ctx.Dr0 = g_watchAddr;

    ctx.Dr7 = (ctx.Dr7 & ~0x000f0003u) | 0x1u | (0x1u << 16) | (0x3u << 18);
    SetThreadContext(g_mainThread, &ctx);
    ResumeThread(g_mainThread);
    return 0;
}

static void ArmWatch(DWORD addr)
{
    HANDLE t;
    if (!g_mainThread)
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(),
                        &g_mainThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    g_watchAddr = addr;
    t = CreateThread(0, 0, ArmThread, 0, 0, 0);
    if (t) {
        WaitForSingleObject(t, 3000);
        CloseHandle(t);
    }
}

typedef void *(__cdecl *XMP_t)(const char *name, void *a, void *b);
typedef float *(__cdecl *Alloc_t)(int size);
static XMP_t g_realXMP;
static int g_armed;
static int g_calls;

static Alloc_t g_realAlloc;
static int g_inFlak;
static int g_allocArmed;

static float *__cdecl MyAlloc(int size)
{
    float *p = g_realAlloc(size);
    if (g_inFlak && !g_allocArmed && size >= 0x90 && size <= 0x100 && p) {
        g_allocArmed = 1;
        g_flakXModel = (DWORD)p;
        LOG(">>> XModel alloc'd size=%d at 0x%08x — arming watch on +0\n",
            size, (unsigned)(DWORD)p);
        ArmWatch((DWORD)p);
    }
    return p;
}

typedef void *(__cdecl *S97_t)(void *surfsCtx, int a1, void *streamCursor, void *alloc);
static S97_t g_realS97;
static int g_s97logs;

static void *__cdecl MyS97(void *surfsCtx, int a1, void *streamCursor, void *alloc)
{
    if (g_flakXModel && g_s97logs < 8) {
        DWORD sc = (DWORD)surfsCtx;
        g_s97logs++;
        LOG("  S97 surfsCtx=0x%08x (==xmodel? %d, delta=0x%x)  *sc=0x%08x  streamCursor=0x%08x  alloc=0x%08x\n",
            sc, sc == g_flakXModel, (int)(sc - g_flakXModel),
            (sc >= 0x10000) ? *(unsigned *)sc : 0, (unsigned)(DWORD)streamCursor, (unsigned)(DWORD)alloc);
    }
    return g_realS97(surfsCtx, a1, streamCursor, alloc);
}

typedef void *(__cdecl *S98_t)(void *xmodel);
static S98_t g_realS98;
static int g_s98logs;
static void *__cdecl MyS98(void *xmodel)
{
    void *r = g_realS98(xmodel);
    if (g_flakXModel && (DWORD)xmodel == g_flakXModel && g_s98logs < 4) {
        g_s98logs++;
        LOG("  S98 xmodel=0x%08x -> return=0x%08x (==xmodel? %d)\n",
            (unsigned)(DWORD)xmodel, (unsigned)(DWORD)r, r == xmodel);
    }
    return r;
}

void *g_realLoader __attribute__((used));
static int g_loaderLogs;
__attribute__((used)) void __cdecl LogSurfsCtx(DWORD surfsCtx, DWORD eax, DWORD retaddr)
{
    if (g_flakXModel && g_loaderLogs < 6) {
        g_loaderLogs++;
        LOG("  LOADER@0x491bc0 surfsCtx(arg2)=0x%08x eax=0x%08x (flakXModel=0x%08x ==%d) caller=0x%08x\n",
            (unsigned)surfsCtx, (unsigned)eax, (unsigned)g_flakXModel,
            surfsCtx == g_flakXModel, (unsigned)retaddr);
    }
}
__attribute__((naked)) static void MyLoader(void)
{

    __asm__ volatile(
        "pushal\n\t"
        "movl 0x2c(%esp), %eax\n\t"
        "movl 0x20(%esp), %edx\n\t"
        "movl 0x1c(%esp), %ecx\n\t"
        "pushl %edx\n\t"
        "pushl %ecx\n\t"
        "pushl %eax\n\t"
        "call _LogSurfsCtx\n\t"
        "addl $12, %esp\n\t"
        "popal\n\t"
        "jmp *_g_realLoader\n\t");
}

static int WINAPI MyMessageBoxA(HWND h, LPCSTR text, LPCSTR caption, UINT type)
{
    (void)h;
    LOG("[MessageBoxA] caption='%s' text='%s' type=0x%x -> auto IDOK\n",
        caption ? caption : "", text ? text : "", (unsigned)type);
    return 1;
}
static int WINAPI MyMessageBoxW(HWND h, const wchar_t *text, const wchar_t *caption, UINT type)
{
    char t[480] = { 0 }, c[200] = { 0 };
    (void)h;
    if (text)
        WideCharToMultiByte(CP_ACP, 0, text, -1, t, sizeof t - 1, 0, 0);
    if (caption)
        WideCharToMultiByte(CP_ACP, 0, caption, -1, c, sizeof c - 1, 0, 0);
    LOG("[MessageBoxW] caption='%s' text='%s' type=0x%x -> auto IDOK\n", c, t, (unsigned)type);
    return 1;
}
static void PatchJmp(void *fn, void *to)
{
    DWORD old;
    unsigned char *t = (unsigned char *)fn;
    if (fn && VirtualProtect(fn, 5, PAGE_EXECUTE_READWRITE, &old)) {
        t[0] = 0xe9;
        *(DWORD *)(t + 1) = (DWORD)to - ((DWORD)fn + 5);
        VirtualProtect(fn, 5, old, &old);
    }
}
static void HookMessageBoxes(void)
{
    HMODULE u32 = GetModuleHandleA("user32.dll");
    if (!u32)
        u32 = LoadLibraryA("user32.dll");
    if (u32) {
        void *a = (void *)GetProcAddress(u32, "MessageBoxA");
        void *w = (void *)GetProcAddress(u32, "MessageBoxW");
        PatchJmp(a, (void *)MyMessageBoxA);
        PatchJmp(w, (void *)MyMessageBoxW);
        LOG(">>> hooked MessageBoxA=%p MessageBoxW=%p (auto-dismiss)\n", a, w);
    }
}

static void *__cdecl MyXMP(const char *name, void *a, void *b)
{
    void *m;
    if (++g_calls <= 500 && name)
        LOG("XMP[%d] '%s'\n", g_calls, name);
    if (!g_armed && name && strstr(name, "flak88_destroy")) {
        g_armed = 1;
        g_inFlak = 1;
        g_realAlloc = (Alloc_t)a;
        LOG(">>> flak88 precache begin; wrapping alloc=%p\n", a);
        m = g_realXMP(name, (void *)MyAlloc, b);
        g_inFlak = 0;
        if (m) {
            unsigned parts = *(unsigned *)m;
            unsigned firstDword = (parts >= 0x10000) ? *(unsigned *)parts : 0;
            LOG(">>> flak88 done: xmodel=0x%08x parts=0x%08x (aligned=%d) parts[0]=0x%08x name@0x88=%s\n",
                (unsigned)(DWORD)m, parts, (parts & 3) == 0, firstDword,
                (*(char **)((char *)m + 0x88)) ? *(char **)((char *)m + 0x88) : "(null)");
        }
        return m;
    }
    return g_realXMP(name, a, b);
}

static unsigned char *g_tramp;

static DWORD WINAPI HookThread(LPVOID p)
{
    DWORD target = 0x490330u;
    DWORD old;
    unsigned char *tr;
    (void)p;

    {
        unsigned char *imm = (unsigned char *)0x4b623fu;
        if (*imm == 0x01 && VirtualProtect(imm, 1, PAGE_EXECUTE_READWRITE, &old)) {
            *imm = 0x00;
            VirtualProtect(imm, 1, old, &old);
            LOG(">>> sound disabled: movb imm @0x4b623f 0x01->0x00 (no .m3d 3D provider)\n");
        } else {
            LOG("!! sound-disable patch: unexpected byte 0x%02x @0x4b623f\n", *imm);
        }
    }

    HookMessageBoxes();
    Sleep(1500);
    tr = (unsigned char *)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) {
        LOG("!! tramp alloc failed\n");
        return 0;
    }
    memcpy(tr, (void *)target, 6);
    tr[6] = 0xe9;
    *(DWORD *)(tr + 7) = (target + 6) - ((DWORD)tr + 11);
    g_tramp = tr;
    g_realXMP = (XMP_t)tr;
    if (VirtualProtect((void *)target, 8, PAGE_EXECUTE_READWRITE, &old)) {
        unsigned char *t = (unsigned char *)target;
        t[0] = 0xe9;
        *(DWORD *)(t + 1) = (DWORD)MyXMP - (target + 5);
        t[5] = 0x90;
        VirtualProtect((void *)target, 8, old, &old);
        LOG(">>> detoured soundfix XModelPrecache @0x%08x -> MyXMP=0x%08x tramp=0x%08x\n",
            (unsigned)target, (unsigned)(DWORD)MyXMP, (unsigned)(DWORD)tr);
    } else {
        LOG("!! VirtualProtect failed on 0x490330\n");
    }

    {
        DWORD *pS97 = (DWORD *)0x68a36cu;
        int w;
        for (w = 0; w < 12000 && *pS97 == 0; w++)
            Sleep(10);
        if (*pS97) {
            g_realS97 = (S97_t)*pS97;
            if (VirtualProtect(pS97, 4, PAGE_READWRITE, &old)) {
                *pS97 = (DWORD)MyS97;
                VirtualProtect(pS97, 4, old, &old);
                LOG(">>> patched refexport[97]@0x68a36c real=0x%08x my=0x%08x\n",
                    (unsigned)(DWORD)g_realS97, (unsigned)(DWORD)MyS97);
            }
        }
    }

    {
        DWORD ld = 0x491bc0u;
        unsigned char *trl = (unsigned char *)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (trl && *(unsigned char *)ld == 0x83) {
            memcpy(trl, (void *)ld, 6);
            trl[6] = 0xe9;
            *(DWORD *)(trl + 7) = (ld + 6) - ((DWORD)trl + 11);
            g_realLoader = trl;
            if (VirtualProtect((void *)ld, 8, PAGE_EXECUTE_READWRITE, &old)) {
                unsigned char *t = (unsigned char *)ld;
                t[0] = 0xe9;
                *(DWORD *)(t + 1) = (DWORD)MyLoader - (ld + 5);
                t[5] = 0x90;
                VirtualProtect((void *)ld, 8, old, &old);
                LOG(">>> detoured loader @0x%08x -> MyLoader=0x%08x\n", (unsigned)ld, (unsigned)(DWORD)MyLoader);
            }
        }
    }

    {
        HMODULE gfx = 0;
        int k;
        for (k = 0; k < 12000 && !gfx; k++) {
            gfx = GetModuleHandleA("gfx_d3d_mp_x86_s.dll");
            if (!gfx)
                Sleep(10);
        }
        if (gfx) {
            {
                DWORD s98 = (DWORD)gfx + 0x61304u;
                if (*(unsigned char *)s98 == 0x55) {
                    unsigned char *tr3 = (unsigned char *)VirtualAlloc(0, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
                    memcpy(tr3, (void *)s98, 6);
                    tr3[6] = 0xe9;
                    *(DWORD *)(tr3 + 7) = (s98 + 6) - ((DWORD)tr3 + 11);
                    g_realS98 = (S98_t)tr3;
                    if (VirtualProtect((void *)s98, 8, PAGE_EXECUTE_READWRITE, &old)) {
                        unsigned char *t = (unsigned char *)s98;
                        t[0] = 0xe9;
                        *(DWORD *)(t + 1) = (DWORD)MyS98 - (s98 + 5);
                        t[5] = 0x90;
                        VirtualProtect((void *)s98, 8, old, &old);
                        LOG(">>> detoured slot98 @0x%08x -> MyS98=0x%08x\n", (unsigned)s98, (unsigned)(DWORD)MyS98);
                    }
                }
            }
        }
    }

    {
        DWORD *pListener = (DWORD *)0xc94ed4u;
        if (*pListener == 0) {
            *pListener = 0xc94cb4u;
            LOG(">>> sound listener 0xc94ed4 set to 0xc94cb4 (was null; no .m3d provider)\n");
        }
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD r, LPVOID x)
{
    (void)x;
    if (r == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        LOG("=== mss32 shim attached ===\n");
        AddVectoredExceptionHandler(1, Veh);
        CreateThread(0, 0, HookThread, 0, 0, 0);
    }
    return TRUE;
}
#endif
