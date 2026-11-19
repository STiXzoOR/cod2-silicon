/* CoD2x's Windows crash and freeze reports, reimplemented for this executable
 * (CoD2x src/mss32/exception.cpp, exception_report.cpp, exception_stack.cpp, and the
 * send line of exception_dialog.cpp). "<exe>.crash.<time>.txt" or
 * "<exe>.freeze.<time>.txt" holds the report in CoD2x's sections, then "=== JSON ==="
 * and the JSON CoD2x's service receives; a crash also leaves a small
 * "<exe>.crash.<time>.dmp".
 *
 * The report goes to CoD2x only when the fault is in CoD2x-layer code
 * (cod2x_report.c): then "<exe>.<kind>.<time>.pending.json" is written and sent at
 * once, and the crash dialog shows how the sending goes. Every other fault goes to
 * the report receiver of crash_handler.h and is never sent to CoD2x.
 *
 * The report is the 32-bit game's: on a 64-bit build the register and address
 * fields keep CoD2x's names (EIP, ESP, "ESP+0x...") and hold 64-bit values. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif
#include "PC/qcommon/cod2x.h"
#include "PC/qcommon/cod2x_features.h"
#include "PC/qcommon/crash_handler.h"
#include "win32/win_cod2x.h"

#define WR_MAX_FRAMES 64
#define WR_STACK_SNAPSHOT (256 * 1024)
#define WR_MAX_TARGETS 3
#define WR_MAX_STACKS (WR_MAX_TARGETS * 2)
#define WR_SEND_WAIT_MS 5000
#define WR_FREEZE_SEND_WAIT_MS 7000
#define WR_PENDING_SUFFIX ".pending.json"
#define WR_CRT_RUNTIME_ERROR 0xE0C2D201u
#define WR_CRT_SECURITY_ERROR 0xE0C2D202u
#define WR_COD2X_FATAL 0xE0C2D203u
#define WR_MSVC_CPP 0xE06D7363u
#define WR_HEAP_CORRUPTION 0xC0000374u

#if defined(_WIN64)
#define WR_MACHINE IMAGE_FILE_MACHINE_AMD64
#define WR_PC(c) ((c)->Rip)
#define WR_SP(c) ((c)->Rsp)
#define WR_BP(c) ((c)->Rbp)
#else
#define WR_MACHINE IMAGE_FILE_MACHINE_I386
#define WR_PC(c) ((c)->Eip)
#define WR_SP(c) ((c)->Esp)
#define WR_BP(c) ((c)->Ebp)
#endif

size_t Sys_Cod2xIdentity(char *out, size_t size);

/* ---- Reading memory without faulting ---- */

static size_t WR_Read(uintptr_t address, void *out, size_t length)
{
    size_t done = 0;
    while (done < length) {
        uintptr_t current = address + done;
        size_t left = 0x1000 - (current & 0xFFF), chunk = length - done < left ? length - done : left;
        SIZE_T got = 0;
        if (!current || !ReadProcessMemory(GetCurrentProcess(), (LPCVOID)current, (BYTE *)out + done, chunk, &got) ||
            got != chunk)
            break;
        done += chunk;
    }
    return done;
}

static int WR_Executable(uintptr_t address)
{
    MEMORY_BASIC_INFORMATION info;
    if (!address || !VirtualQuery((LPCVOID)address, &info, sizeof(info)) || info.State != MEM_COMMIT)
        return 0;
    return (info.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

static char *WR_Hex(char *out, uintptr_t value)
{
    sprintf(out, "0x%08llX", (unsigned long long)value);
    return out;
}

/* ---- Modules and symbols ---- */

static int WR_Module(uintptr_t address, char *name, size_t size, uintptr_t *base)
{
    MEMORY_BASIC_INFORMATION info;
    char path[MAX_PATH] = "";
    const char *slash;
    if (name && size)
        name[0] = '\0';
    if (base)
        *base = 0;
    if (!VirtualQuery((LPCVOID)address, &info, sizeof(info)) || !info.AllocationBase ||
        !GetModuleFileNameA((HMODULE)info.AllocationBase, path, sizeof(path)))
        return 0;
    slash = strrchr(path, '\\');
    if (name && size)
        snprintf(name, size, "%s", slash ? slash + 1 : path);
    if (base)
        *base = (uintptr_t)info.AllocationBase;
    return 1;
}

static int WR_Headers(uintptr_t module, IMAGE_NT_HEADERS *headers)
{
    IMAGE_DOS_HEADER dos;
    if (WR_Read(module, &dos, sizeof(dos)) != sizeof(dos) || dos.e_magic != IMAGE_DOS_SIGNATURE)
        return 0;
    return WR_Read(module + (uintptr_t)dos.e_lfanew, headers, sizeof(*headers)) == sizeof(*headers) &&
           headers->Signature == IMAGE_NT_SIGNATURE;
}

static int WR_Section(uintptr_t module, uintptr_t offset, char *out, size_t size)
{
    IMAGE_DOS_HEADER dos;
    IMAGE_NT_HEADERS headers;
    IMAGE_SECTION_HEADER section;
    uintptr_t at;
    int i;
    if (!module || !WR_Headers(module, &headers) || WR_Read(module, &dos, sizeof(dos)) != sizeof(dos))
        return 0;
    at = module + (uintptr_t)dos.e_lfanew + offsetof(IMAGE_NT_HEADERS, OptionalHeader) +
         headers.FileHeader.SizeOfOptionalHeader;
    for (i = 0; i < headers.FileHeader.NumberOfSections; ++i) {
        if (WR_Read(at + (uintptr_t)i * sizeof(section), &section, sizeof(section)) != sizeof(section))
            return 0;
        if (offset >= section.VirtualAddress &&
            offset < section.VirtualAddress + (section.Misc.VirtualSize ? section.Misc.VirtualSize : section.SizeOfRawData)) {
            snprintf(out, size, "%.8s", (const char *)section.Name);
            return 1;
        }
    }
    return 0;
}

static BOOL symbolsReady;
static DWORD symbolsError;
static int symbolsTried;

static void WR_Symbols(void)
{
    if (symbolsTried)
        return;
    symbolsTried = 1;
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
    symbolsReady = SymInitialize(GetCurrentProcess(), NULL, TRUE);
    symbolsError = symbolsReady ? ERROR_SUCCESS : GetLastError();
}

static int WR_Symbol(uintptr_t address, char *out, size_t size, uintptr_t *start)
{
    char buffer[sizeof(SYMBOL_INFO) + 256];
    SYMBOL_INFO *symbol = (SYMBOL_INFO *)buffer;
    DWORD64 displacement = 0;
    memset(buffer, 0, sizeof(buffer));
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    if (!symbolsReady || !SymFromAddr(GetCurrentProcess(), (DWORD64)address, &displacement, symbol) ||
        !symbol->Name[0] || displacement >= 0x10000u)
        return 0;
    if (displacement)
        snprintf(out, size, "%s+0x%llX", symbol->Name, (unsigned long long)displacement);
    else
        snprintf(out, size, "%s", symbol->Name);
    if (start)
        *start = address - (uintptr_t)displacement;
    return 1;
}

/* "module+0xOFFSET (symbol)", or "<unknown>". */
static void WR_Describe(uintptr_t address, char *out, size_t size)
{
    char module[MAX_PATH], symbol[300];
    uintptr_t base;
    if (!WR_Module(address, module, sizeof(module), &base))
        snprintf(out, size, "<unknown>");
    else if (WR_Symbol(address, symbol, sizeof(symbol), NULL))
        snprintf(out, size, "%s+0x%llX (%s)", module, (unsigned long long)(address - base), symbol);
    else
        snprintf(out, size, "%s+0x%llX", module, (unsigned long long)(address - base));
}

/* ---- The call before a return address ---- */

typedef struct {
    char text[96];
    uintptr_t target;
    int known, direct;
} WRCall;

static const char *WR_Register(int number, int extended)
{
#if defined(_WIN64)
    static const char *const names[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                          "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
    return names[(number & 7) + (extended ? 8 : 0)];
#else
    static const char *const names[8] = {"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"};
    (void)extended;
    return names[number & 7];
#endif
}

/* Decodes the CALL instruction that ends right at ret: direct, through a register,
   or through memory at a register plus displacement or an absolute address. */
static int WR_DecodeCall(uintptr_t ret, uintptr_t frame, WRCall *call)
{
    unsigned char code[8];
    memset(call, 0, sizeof(*call));
    if (ret < 8 || WR_Read(ret - 8, code, 8) != 8)
        return 0;
    /* call rel32 */
    if (code[3] == 0xE8) {
        int32_t relative;
        memcpy(&relative, code + 4, 4);
        call->target = ret + (intptr_t)relative;
        call->known = call->direct = 1;
        snprintf(call->text, sizeof(call->text), "call 0x%08llX", (unsigned long long)call->target);
        return 1;
    }
    /* call [absolute], or [rip+disp32] on x86-64 */
    if (code[2] == 0xFF && code[3] == 0x15) {
        int32_t displacement;
        uintptr_t pointer, target = 0;
        memcpy(&displacement, code + 4, 4);
#if defined(_WIN64)
        pointer = ret + (intptr_t)displacement;
#else
        pointer = (uintptr_t)(uint32_t)displacement;
#endif
        snprintf(call->text, sizeof(call->text), "call [0x%08llX]", (unsigned long long)pointer);
        if (WR_Read(pointer, &target, sizeof(target)) == sizeof(target) && target) {
            call->target = target;
            call->known = 1;
        }
        return 1;
    }
    /* call [reg+disp32] */
    if (code[2] == 0xFF && (code[3] & 0xF8) == 0x90 && (code[3] & 7) != 4) {
        int32_t displacement;
        int extended = code[1] == 0x41;
        memcpy(&displacement, code + 4, 4);
        snprintf(call->text, sizeof(call->text), "call [%s%s0x%X]", WR_Register(code[3], extended),
                 displacement < 0 ? "-" : "+", (unsigned)(displacement < 0 ? -displacement : displacement));
        if ((code[3] & 7) == 5 && !extended && frame) {
            uintptr_t target = 0;
            if (WR_Read(frame + (intptr_t)displacement, &target, sizeof(target)) == sizeof(target) && target) {
                call->target = target;
                call->known = 1;
            }
        }
        return 1;
    }
    /* call [reg+disp8] */
    if (code[5] == 0xFF && (code[6] & 0xF8) == 0x50 && (code[6] & 7) != 4) {
        int displacement = (signed char)code[7];
        int extended = code[4] == 0x41;
        snprintf(call->text, sizeof(call->text), "call [%s%s0x%X]", WR_Register(code[6], extended),
                 displacement < 0 ? "-" : "+", (unsigned)(displacement < 0 ? -displacement : displacement));
        if ((code[6] & 7) == 5 && !extended && frame) {
            uintptr_t target = 0;
            if (WR_Read(frame + (intptr_t)displacement, &target, sizeof(target)) == sizeof(target) && target) {
                call->target = target;
                call->known = 1;
            }
        }
        return 1;
    }
    /* call reg */
    if (code[6] == 0xFF && (code[7] & 0xF8) == 0xD0) {
        snprintf(call->text, sizeof(call->text), "call %s", WR_Register(code[7], code[5] == 0x41));
        return 1;
    }
    /* call [reg] */
    if (code[6] == 0xFF && (code[7] & 0xF8) == 0x10 && (code[7] & 7) != 4 && (code[7] & 7) != 5) {
        snprintf(call->text, sizeof(call->text), "call [%s]", WR_Register(code[7], code[5] == 0x41));
        return 1;
    }
    return 0;
}

static int WR_ReturnAddress(uintptr_t address)
{
    WRCall call;
    return WR_Executable(address) && WR_DecodeCall(address, 0, &call);
}

/* "call 0x... (symbol)", "call [ebp-0xC] -> 0x... (symbol)", or the call alone. */
static void WR_CallSite(uintptr_t ret, uintptr_t frame, char *out, size_t size)
{
    WRCall call;
    char symbol[256], module[MAX_PATH];
    uintptr_t base;
    int named;
    out[0] = '\0';
    if (!WR_DecodeCall(ret, frame, &call))
        return;
    if (!call.known) {
        snprintf(out, size, "%s", call.text);
        return;
    }
    named = WR_Module(call.target, module, sizeof(module), &base) && WR_Symbol(call.target, symbol, sizeof(symbol), NULL);
    if (call.direct)
        snprintf(out, size, named ? "%s  (%s)" : "%s", call.text, symbol);
    else if (named)
        snprintf(out, size, "%s -> 0x%08llX  (%s)", call.text, (unsigned long long)call.target, symbol);
    else
        snprintf(out, size, "%s -> 0x%08llX", call.text, (unsigned long long)call.target);
}

/* ---- Threads ---- */

typedef LONG(NTAPI *WRQueryThread)(HANDLE, int, PVOID, ULONG, PULONG);

static WRQueryThread WR_QueryThread(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    return ntdll ? (WRQueryThread)(void *)GetProcAddress(ntdll, "NtQueryInformationThread") : NULL;
}

static void WR_ThreadName(HANDLE thread, char *out, size_t size)
{
    typedef HRESULT(WINAPI * GetDescription)(HANDLE, PWSTR *);
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    GetDescription get = kernel ? (GetDescription)(void *)GetProcAddress(kernel, "GetThreadDescription") : NULL;
    PWSTR wide = NULL;
    out[0] = '\0';
    if (thread && get && SUCCEEDED(get(thread, &wide)) && wide) {
        if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, (int)size, NULL, NULL) <= 0)
            out[0] = '\0';
        LocalFree(wide);
    }
}

/* Names a thread as CoD2x names the game's ("CoD2 MainThread", "CoD2 RenderBackend"). */
void Win_Cod2xNameThread(void *thread, const wchar_t *name)
{
    typedef HRESULT(WINAPI * SetDescription)(HANDLE, PCWSTR);
    HMODULE kernel = GetModuleHandleA("kernel32.dll");
    SetDescription set = kernel ? (SetDescription)(void *)GetProcAddress(kernel, "SetThreadDescription") : NULL;
    if (set && thread)
        set((HANDLE)thread, name);
}

/* The top of a thread's stack (NT_TIB.StackBase in its TEB), 0 when unknown. */
static uintptr_t WR_StackBase(HANDLE thread)
{
    struct {
        LONG exitStatus;
        PVOID teb;
        PVOID clientId[2];
        ULONG_PTR affinity;
        LONG priority, basePriority;
    } basic;
    NT_TIB tib;
    WRQueryThread query = WR_QueryThread();
    memset(&basic, 0, sizeof(basic));
    if (!query || query(thread, 0, &basic, sizeof(basic), NULL) != 0 || !basic.teb ||
        WR_Read((uintptr_t)basic.teb, &tib, sizeof(tib)) != sizeof(tib))
        return 0;
    return (uintptr_t)tib.StackBase;
}

typedef struct {
    char role[64], name[64];
    DWORD id;
    CONTEXT context;
    int hasContext, crashing;
    uintptr_t stackBase, copyAddress;
    size_t copyLength;
    unsigned char *copy;
} WRTarget;

static unsigned char targetCopies[WR_MAX_TARGETS][WR_STACK_SNAPSHOT];
static const WRTarget *walkTarget;

static size_t WR_ThreadRead(uintptr_t address, void *out, size_t length)
{
    const WRTarget *target = walkTarget;
    if (target && target->copyLength && address >= target->copyAddress &&
        address - target->copyAddress + length <= target->copyLength) {
        memcpy(out, target->copy + (address - target->copyAddress), length);
        return length;
    }
    return WR_Read(address, out, length);
}

static BOOL CALLBACK WR_WalkRead(HANDLE process, DWORD64 base, PVOID buffer, DWORD size, LPDWORD read)
{
    size_t got = WR_ThreadRead((uintptr_t)base, buffer, size);
    (void)process;
    if (read)
        *read = (DWORD)got;
    return got == size;
}

/* The faulting thread, then the game's named threads; another thread's context
   and stack are copied while it is suspended, and nothing is allocated meanwhile. */
static int WR_Targets(DWORD crashThread, const CONTEXT *crashContext, WRTarget *targets)
{
    HANDLE snapshot;
    THREADENTRY32 entry;
    int count = 0;
    if (crashThread) {
        HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, crashThread);
        WRTarget *target = &targets[count++];
        memset(target, 0, sizeof(*target));
        snprintf(target->role, sizeof(target->role), "crashing thread");
        target->id = crashThread;
        target->crashing = 1;
        if (crashContext) {
            target->context = *crashContext;
            target->hasContext = 1;
        }
        if (thread) {
            WR_ThreadName(thread, target->name, sizeof(target->name));
            target->stackBase = WR_StackBase(thread);
            CloseHandle(thread);
        }
    }
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return count;
    memset(&entry, 0, sizeof(entry));
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry)) {
        do {
            HANDLE thread;
            char name[64];
            WRTarget *target;
            if (entry.th32OwnerProcessID != GetCurrentProcessId() || entry.th32ThreadID == crashThread ||
                count >= WR_MAX_TARGETS)
                continue;
            thread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION, FALSE,
                                entry.th32ThreadID);
            if (!thread)
                continue;
            WR_ThreadName(thread, name, sizeof(name));
            if (strcmp(name, "CoD2 MainThread") && strcmp(name, "CoD2 RenderBackend")) {
                CloseHandle(thread);
                continue;
            }
            target = &targets[count];
            memset(target, 0, sizeof(*target));
            snprintf(target->role, sizeof(target->role), "%s", name);
            snprintf(target->name, sizeof(target->name), "%s", name);
            target->id = entry.th32ThreadID;
            target->stackBase = WR_StackBase(thread);
            target->copy = targetCopies[count];
            if (entry.th32ThreadID == GetCurrentThreadId()) {
                RtlCaptureContext(&target->context);
                target->hasContext = 1;
            } else if (SuspendThread(thread) != (DWORD)-1) {
                target->context.ContextFlags = CONTEXT_FULL;
                target->hasContext = GetThreadContext(thread, &target->context) != FALSE;
                if (target->hasContext) {
                    uintptr_t sp = WR_SP(&target->context);
                    size_t length = WR_STACK_SNAPSHOT;
                    if (target->stackBase > sp && target->stackBase - sp < length)
                        length = target->stackBase - sp;
                    target->copyAddress = sp;
                    target->copyLength = WR_Read(sp, target->copy, length);
                }
                ResumeThread(thread);
            }
            ++count;
            CloseHandle(thread);
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return count;
}

/* ---- Stack walks ---- */

typedef struct {
    int number;
    uintptr_t address, offset;
    char module[64], section[16], symbol[300], instruction[200];
} WRFrame;

typedef struct {
    char role[64], name[64], method[32];
    DWORD id;
    WRFrame frames[WR_MAX_FRAMES + 2];
    int count;
    /* diagnostics */
    char how[160];
    const char *stopReason, *walkStatus;
    int unwindFrames, repaired, chainTried, chainFrames, snapshot;
    DWORD walkCode;
    char walkMessage[256];
} WRStack;

static WRStack stacks[WR_MAX_STACKS];
static int stackCount;

static void WR_ErrorText(DWORD error, char *out, size_t size)
{
    DWORD length;
    if (error == ERROR_SUCCESS) {
        snprintf(out, size, "The operation completed successfully.");
        return;
    }
    length = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, error, 0, out,
                            (DWORD)size, NULL);
    while (length && (out[length - 1] == '\r' || out[length - 1] == '\n' || out[length - 1] == ' '))
        out[--length] = '\0';
    if (!length)
        snprintf(out, size, "Unknown Win32 error");
}

static int WR_AddFrame(WRStack *stack, int number, uintptr_t address, uintptr_t frame)
{
    MEMORY_BASIC_INFORMATION info;
    WRFrame *out;
    uintptr_t base = 0;
    char path[MAX_PATH] = "";
    if (stack->count >= (int)(sizeof(stack->frames) / sizeof(stack->frames[0])) ||
        !VirtualQuery((LPCVOID)address, &info, sizeof(info)))
        return 0;
    out = &stack->frames[stack->count++];
    memset(out, 0, sizeof(*out));
    out->number = number;
    out->address = address;
    if (!info.AllocationBase) {
        snprintf(out->module, sizeof(out->module), "<unknown>");
        out->offset = address;
    } else {
        const char *slash;
        base = (uintptr_t)info.AllocationBase;
        GetModuleFileNameA((HMODULE)info.AllocationBase, path, sizeof(path));
        slash = strrchr(path, '\\');
        snprintf(out->module, sizeof(out->module), "%s", path[0] ? (slash ? slash + 1 : path) : "<unknown>");
        out->offset = address - base;
    }
    if (!WR_Section(base, out->offset, out->section, sizeof(out->section)))
        snprintf(out->section, sizeof(out->section), "?");
    if (!WR_Symbol(address, out->symbol, sizeof(out->symbol), NULL))
        snprintf(out->symbol, sizeof(out->symbol), "<no symbol>");
    WR_CallSite(address, frame, out->instruction, sizeof(out->instruction));
    return 1;
}

/* Frame 0 of the faulting thread: the exception address itself. */
static void WR_FrameZero(WRStack *stack, const char *module, uintptr_t address, uintptr_t offset)
{
    WRFrame *zero;
    if (!module[0] || stack->count >= (int)(sizeof(stack->frames) / sizeof(stack->frames[0])))
        return;
    memmove(&stack->frames[1], &stack->frames[0], sizeof(stack->frames[0]) * (size_t)stack->count);
    ++stack->count;
    zero = &stack->frames[0];
    memset(zero, 0, sizeof(*zero));
    snprintf(zero->module, sizeof(zero->module), "%s", module);
    zero->address = address;
    zero->offset = offset;
    if (!WR_Section(address - offset, offset, zero->section, sizeof(zero->section)))
        snprintf(zero->section, sizeof(zero->section), "?");
    if (!WR_Symbol(address, zero->symbol, sizeof(zero->symbol), NULL))
        snprintf(zero->symbol, sizeof(zero->symbol), "<no symbol>");
}

/* The caller of a crashing function whose frame does not exist yet: a call through
   a bad pointer, or a fault at the very start of a function. */
static int WR_MissingReturn(const CONTEXT *context, uintptr_t *ret, uintptr_t *spAfter, const char **reason)
{
    uintptr_t pc = WR_PC(context), sp = WR_SP(context), bp = WR_BP(context), callerReturn = 0;
    int k;
    if (!sp)
        return 0;
    if (!WR_Executable(pc)) {
        uintptr_t value = 0;
        if (WR_ThreadRead(sp, &value, sizeof(value)) == sizeof(value) && WR_Executable(value)) {
            *ret = value;
            *spAfter = sp + sizeof(value);
            *reason = "EIP not executable, return address from [ESP]";
            return 1;
        }
        return 0;
    }
    WR_ThreadRead(bp + sizeof(void *), &callerReturn, sizeof(callerReturn));
    for (k = 0; k < 2; ++k) {
        uintptr_t value = 0, start = 0;
        WRCall call;
        int calls;
        if (WR_ThreadRead(sp + sizeof(void *) * (size_t)k, &value, sizeof(value)) != sizeof(value))
            continue;
        if (k == 1) {
            uintptr_t top = 0;
            if (WR_ThreadRead(sp, &top, sizeof(top)) != sizeof(top) || top != bp)
                continue;
        }
        if (value == callerReturn || !WR_Executable(value) || !WR_DecodeCall(value, 0, &call))
            continue;
        if (call.direct) {
            calls = call.target <= pc && pc - call.target < 0x40;
        } else {
            char symbol[300];
            calls = WR_Symbol(pc, symbol, sizeof(symbol), &start) && pc - start < 0x10;
        }
        if (!calls)
            continue;
        *ret = value;
        *spAfter = sp + sizeof(void *) * (size_t)(k + 1);
        *reason = "crash at function entry, return address from the stack";
        return 1;
    }
    return 0;
}

static void WR_WalkStackWalk64(const WRTarget *target, uintptr_t exceptionAddress, WRStack *stack)
{
    CONTEXT context = target->context;
    STACKFRAME64 frame;
    DWORD error = ERROR_SUCCESS;
    const char *reason = "";
    uintptr_t missing = 0, spAfter = 0;
    int number = target->crashing ? 1 : 0, skipped = !target->crashing, bad = 0, i;
    int goodCount = 0, goodNumber = number, goodUnwind = 0, unwind = 0;
    walkTarget = target;
    snprintf(stack->method, sizeof(stack->method), "StackWalk64");
    context.ContextFlags = CONTEXT_FULL;
    if (target->crashing && WR_MissingReturn(&context, &missing, &spAfter, &reason)) {
        WR_PC(&context) = missing;
        WR_SP(&context) = spAfter;
        stack->repaired = 1;
    }
    memset(&frame, 0, sizeof(frame));
    frame.AddrPC.Offset = WR_PC(&context);
    frame.AddrFrame.Offset = WR_BP(&context);
    frame.AddrStack.Offset = WR_SP(&context);
    frame.AddrPC.Mode = frame.AddrFrame.Mode = frame.AddrStack.Mode = frame.AddrReturn.Mode = AddrModeFlat;
    for (i = 0; i < WR_MAX_FRAMES; ++i) {
        uintptr_t address;
        MEMORY_BASIC_INFORMATION info;
        if (!StackWalk64(WR_MACHINE, GetCurrentProcess(), GetCurrentThread(), &frame, &context, WR_WalkRead,
                         SymFunctionTableAccess64, SymGetModuleBase64, NULL) ||
            !frame.AddrPC.Offset) {
            error = GetLastError();
            break;
        }
        address = (uintptr_t)frame.AddrPC.Offset;
        if (!skipped && address == exceptionAddress) {
            skipped = 1;
            continue;
        }
        skipped = 1;
        if (WR_AddFrame(stack, number, address, (uintptr_t)frame.AddrFrame.Offset)) {
            ++number;
            ++unwind;
            /* Two frames in a row outside any module: the walk lost the chain, drop them. */
            if (VirtualQuery((LPCVOID)address, &info, sizeof(info)) && info.AllocationBase) {
                bad = 0;
                goodCount = stack->count;
                goodNumber = number;
                goodUnwind = unwind;
            } else if (++bad >= 2) {
                stack->count = goodCount;
                number = goodNumber;
                unwind = goodUnwind;
                break;
            }
        }
        if (!frame.AddrReturn.Offset)
            break;
    }
    walkTarget = NULL;
    if (stack->repaired)
        snprintf(stack->how, sizeof(stack->how), "StackWalk64 (repaired CONTEXT: %s)", reason);
    else
        snprintf(stack->how, sizeof(stack->how), "StackWalk64 (original CONTEXT)");
    stack->unwindFrames = unwind;
    stack->snapshot = target->copyLength != 0;
    stack->stopReason = "";
    stack->walkStatus = error != ERROR_SUCCESS ? "failed" : "ok";
    stack->walkCode = error;
    if (error != ERROR_SUCCESS)
        WR_ErrorText(error, stack->walkMessage, sizeof(stack->walkMessage));
}

static void WR_WalkFrameChain(const WRTarget *target, uintptr_t exceptionAddress, WRStack *stack)
{
    uintptr_t bp = WR_BP(&target->context), missing = 0, spAfter = 0;
    const char *stop = "end of chain", *reason = "";
    int number = target->crashing ? 1 : 0, count = 0, i;
    walkTarget = target;
    snprintf(stack->method, sizeof(stack->method), "EBP chain");
    if (!target->crashing) {
        if (WR_AddFrame(stack, number, WR_PC(&target->context), bp))
            ++number;
    } else if (WR_MissingReturn(&target->context, &missing, &spAfter, &reason) &&
               WR_AddFrame(stack, number, missing, bp)) {
        ++number;
        ++count;
        stack->repaired = 1;
    }
    for (i = 0; i < WR_MAX_FRAMES; ++i) {
        uintptr_t frame[2];
        if (!bp || (bp & (sizeof(void *) - 1))) {
            stop = !bp ? "end of chain" : "EBP not aligned (caller without frame pointer)";
            break;
        }
        if (target->stackBase && (bp < WR_SP(&target->context) || bp + sizeof(frame) > target->stackBase)) {
            stop = "EBP outside of the stack (caller without frame pointer)";
            break;
        }
        if (WR_ThreadRead(bp, frame, sizeof(frame)) != sizeof(frame)) {
            stop = "frame not readable";
            break;
        }
        if (frame[0] && frame[0] <= bp) {
            stop = "EBP chain does not grow (caller without frame pointer)";
            break;
        }
        if (frame[1] && !(target->crashing && frame[1] == exceptionAddress) && WR_Executable(frame[1]) &&
            WR_AddFrame(stack, number, frame[1], frame[0])) {
            ++number;
            ++count;
        }
        if (!frame[0])
            break;
        bp = frame[0];
    }
    walkTarget = NULL;
    snprintf(stack->how, sizeof(stack->how), "EBP chain (frame pointer unwind)");
    stack->stopReason = stop;
    stack->unwindFrames = count;
    stack->chainTried = 1;
    stack->chainFrames = count;
    stack->snapshot = target->copyLength != 0;
    stack->walkStatus = "n/a";
}

/* ---- The report ---- */

typedef struct {
    const char *kind, *freezeReason;
    DWORD frozenMs, crashThread, currentThread;
    DWORD code;
    uintptr_t address, fileOffset;
    char module[MAX_PATH], fault[512], time[32], minidump[MAX_PATH * 2];
    char parameters[EXCEPTION_MAXIMUM_PARAMETERS][24];
    int parameterCount;
    char nested[4][128];
    int nestedCount;
    const CONTEXT *context;
} WRReport;

static const char *WR_ExceptionName(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW: return "INT_OVERFLOW";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "NONCONTINUABLE_EXCEPTION";
    case EXCEPTION_INVALID_DISPOSITION: return "INVALID_DISPOSITION";
    case EXCEPTION_GUARD_PAGE: return "GUARD_PAGE";
    case EXCEPTION_INVALID_HANDLE: return "INVALID_HANDLE";
    case EXCEPTION_BREAKPOINT: return "BREAKPOINT";
    case EXCEPTION_SINGLE_STEP: return "SINGLE_STEP";
    case WR_HEAP_CORRUPTION: return "HEAP_CORRUPTION";
    case 0xC0000409u: return "STACK_BUFFER_OVERRUN";
    case WR_MSVC_CPP: return "CPP_EXCEPTION";
    case WR_CRT_RUNTIME_ERROR: return "CRT_RUNTIME_ERROR";
    case WR_CRT_SECURITY_ERROR: return "CRT_BUFFER_OVERRUN";
    case WR_COD2X_FATAL: return "COD2X_FATAL_ERROR";
    case 0: return "APPLICATION_FREEZE";
    default: return "Unknown Exception";
    }
}

static void WR_Message(ULONG_PTR pointer, char *out, size_t size)
{
    size_t got;
    char *p;
    out[0] = '\0';
    if (!pointer)
        return;
    got = WR_Read((uintptr_t)pointer, out, size - 1);
    out[got] = '\0';
    for (p = out; *p; ++p)
        if (*p == '\r' || *p == '\n')
            *p = ' ';
}

static void WR_Fault(const EXCEPTION_RECORD *record, char *out, size_t size)
{
    char message[256];
    out[0] = '\0';
    if (!record)
        return;
    switch (record->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_IN_PAGE_ERROR: {
        ULONG_PTR type;
        uintptr_t address;
        const char *operation;
        char nearNull[48] = "";
        if (record->NumberParameters < 2)
            break;
        type = record->ExceptionInformation[0];
        address = (uintptr_t)record->ExceptionInformation[1];
        operation = type == 0 ? "Read of" : type == 1 ? "Write to" : type == 8 ? "Execution (DEP) of" : "Access to";
        if (address < 0x10000)
            snprintf(nearNull, sizeof(nearNull), " (NULL pointer + 0x%X)", (unsigned)address);
        if (record->ExceptionCode == EXCEPTION_IN_PAGE_ERROR && record->NumberParameters >= 3)
            snprintf(out, size, "%s address 0x%08llX%s failed to page in, NTSTATUS 0x%08lX", operation,
                     (unsigned long long)address, nearNull, (unsigned long)record->ExceptionInformation[2]);
        else
            snprintf(out, size, "%s address 0x%08llX%s", operation, (unsigned long long)address, nearNull);
        break;
    }
    case WR_CRT_RUNTIME_ERROR:
        WR_Message(record->NumberParameters >= 2 ? record->ExceptionInformation[1] : 0, message, sizeof(message));
        snprintf(out, size, "Microsoft Visual C++ Runtime error %lu: %s",
                 record->NumberParameters >= 1 ? (unsigned long)record->ExceptionInformation[0] : 0UL,
                 message[0] ? message : "<unknown>");
        break;
    case WR_CRT_SECURITY_ERROR:
        snprintf(out, size, "Buffer overrun detected by the game (/GS security check, code %lu)",
                 record->NumberParameters >= 1 ? (unsigned long)record->ExceptionInformation[0] : 0UL);
        break;
    case WR_COD2X_FATAL:
        WR_Message(record->NumberParameters >= 1 ? record->ExceptionInformation[0] : 0, message, sizeof(message));
        snprintf(out, size, "%s", message[0] ? message : "CoD2x fatal error");
        break;
    case WR_HEAP_CORRUPTION:
        snprintf(out, size, "The process heap is corrupted (memory was written out of bounds or freed twice)");
        break;
    case EXCEPTION_STACK_OVERFLOW:
        snprintf(out, size, "The thread used all of its stack (infinite recursion or a huge local array)");
        break;
    default:
        break;
    }
}

static void WR_Append(Cod2xReportText *text, const char *line)
{
    Cod2x_ReportTextAdd(text, "%s", line);
}

static void WR_HexDump(Cod2xReportText *text, uintptr_t address, const unsigned char *bytes, size_t length, uintptr_t marker)
{
    size_t row, i;
    for (row = 0; row < length; row += 16) {
        Cod2x_ReportTextAdd(text, "  0x%08llX%s ", (unsigned long long)(address + row),
                      marker >= address + row && marker < address + row + 16 ? ">" : " ");
        for (i = 0; i < 16; ++i) {
            if (row + i < length)
                Cod2x_ReportTextAdd(text, " %02X", bytes[row + i]);
            else
                WR_Append(text, "   ");
        }
        WR_Append(text, "  ");
        for (i = 0; i < 16 && row + i < length; ++i)
            Cod2x_ReportTextAdd(text, "%c", bytes[row + i] >= 0x20 && bytes[row + i] < 0x7F ? bytes[row + i] : '.');
        WR_Append(text, "\n");
    }
}

/* ---- System facts ---- */

typedef struct {
    char cpuCores[16], cpuAffinity[48], screen[32], cpuModel[64], gpu[512];
    char memoryLoad[16], physicalTotal[24], physicalAvailable[24], virtualTotal[24], virtualAvailable[24];
    char privateMB[24], workingSet[24], peakWorkingSet[24], largestFree[24], totalFree[24];
    int largeAddressAware;
    char handles[16], gdiObjects[16], userObjects[16];
    char processId[16], exePath[MAX_PATH], workingDirectory[MAX_PATH], uptime[24];
    Win_Cod2xSystemText windows;
} WRSystem;

static void WR_CpuModel(char *out, size_t size)
{
    out[0] = '\0';
#if defined(__i386__) || defined(__x86_64__)
    {
        unsigned int r[12] = {0};
        char brand[49];
        const char *p = brand;
        if (__get_cpuid(0x80000002u, &r[0], &r[1], &r[2], &r[3]) &&
            __get_cpuid(0x80000003u, &r[4], &r[5], &r[6], &r[7]) &&
            __get_cpuid(0x80000004u, &r[8], &r[9], &r[10], &r[11])) {
            memcpy(brand, r, 48);
            brand[48] = '\0';
            while (*p == ' ')
                ++p;
            snprintf(out, size, "%s", p);
        }
    }
#endif
}

/* "<adapter> (driver <version>, <date>)[ [primary]]", "; " between adapters. */
static void WR_Gpu(char *out, size_t size)
{
    size_t used = 0;
    DWORD i;
    out[0] = '\0';
    for (i = 0; i < 16 && used + 1 < size; ++i) {
        DISPLAY_DEVICEA device;
        char version[64] = "", date[32] = "";
        const char *prefix = "\\Registry\\Machine\\";
        HKEY key;
        int written;
        memset(&device, 0, sizeof(device));
        device.cb = sizeof(device);
        if (!EnumDisplayDevicesA(NULL, i, &device, 0))
            break;
        if ((device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) || !device.DeviceString[0] ||
            strstr(out, device.DeviceString))
            continue;
        if (!_strnicmp(device.DeviceKey, prefix, strlen(prefix)) &&
            RegOpenKeyExA(HKEY_LOCAL_MACHINE, device.DeviceKey + strlen(prefix), 0, KEY_READ, &key) == ERROR_SUCCESS) {
            DWORD length = sizeof(version) - 1;
            RegQueryValueExA(key, "DriverVersion", NULL, NULL, (LPBYTE)version, &length);
            length = sizeof(date) - 1;
            RegQueryValueExA(key, "DriverDate", NULL, NULL, (LPBYTE)date, &length);
            RegCloseKey(key);
        }
        written = snprintf(out + used, size - used, "%s%s (driver %s, %s)%s", used ? "; " : "", device.DeviceString,
                           version[0] ? version : "?", date[0] ? date : "?",
                           (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) ? " [primary]" : "");
        if (written < 0)
            break;
        used += (size_t)written;
    }
}

static void WR_System(WRSystem *system)
{
    SYSTEM_INFO info;
    DWORD_PTR processMask = 0, systemMask = 0;
    MEMORYSTATUSEX memory;
    PROCESS_MEMORY_COUNTERS_EX counters;
    uintptr_t address, end;
    unsigned long long totalFree = 0, largestFree = 0;
    MEMORY_BASIC_INFORMATION region;
    IMAGE_NT_HEADERS headers;
    DWORD handles = 0;
    FILETIME created, exited, kernel, user, now;
    typedef BOOL(WINAPI * MemoryInfo)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    MemoryInfo memoryInfo = kernel32 ? (MemoryInfo)(void *)GetProcAddress(kernel32, "K32GetProcessMemoryInfo") : NULL;
    memset(system, 0, sizeof(*system));
    GetSystemInfo(&info);
    snprintf(system->cpuCores, sizeof(system->cpuCores), "%lu", (unsigned long)info.dwNumberOfProcessors);
    if (GetProcessAffinityMask(GetCurrentProcess(), &processMask, &systemMask)) {
        int cores = 0;
        DWORD_PTR mask;
        for (mask = processMask; mask; mask &= mask - 1)
            ++cores;
        snprintf(system->cpuAffinity, sizeof(system->cpuAffinity), "%d (mask=0x%llX)", cores,
                 (unsigned long long)processMask);
    } else {
        snprintf(system->cpuAffinity, sizeof(system->cpuAffinity), "unknown");
    }
    snprintf(system->screen, sizeof(system->screen), "%d x %d", GetSystemMetrics(SM_CXSCREEN),
             GetSystemMetrics(SM_CYSCREEN));
    WR_CpuModel(system->cpuModel, sizeof(system->cpuModel));
    WR_Gpu(system->gpu, sizeof(system->gpu));
    memset(&memory, 0, sizeof(memory));
    memory.dwLength = sizeof(memory);
    GlobalMemoryStatusEx(&memory);
    snprintf(system->memoryLoad, sizeof(system->memoryLoad), "%lu", (unsigned long)memory.dwMemoryLoad);
    snprintf(system->physicalTotal, sizeof(system->physicalTotal), "%llu", memory.ullTotalPhys / (1024 * 1024));
    snprintf(system->physicalAvailable, sizeof(system->physicalAvailable), "%llu", memory.ullAvailPhys / (1024 * 1024));
    snprintf(system->virtualTotal, sizeof(system->virtualTotal), "%llu", memory.ullTotalVirtual / (1024 * 1024));
    snprintf(system->virtualAvailable, sizeof(system->virtualAvailable), "%llu",
             memory.ullAvailVirtual / (1024 * 1024));
    memset(&counters, 0, sizeof(counters));
    counters.cb = sizeof(counters);
    if (memoryInfo && memoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&counters, sizeof(counters))) {
        snprintf(system->privateMB, sizeof(system->privateMB), "%lu",
                 (unsigned long)(counters.PrivateUsage / (1024 * 1024)));
        snprintf(system->workingSet, sizeof(system->workingSet), "%lu",
                 (unsigned long)(counters.WorkingSetSize / (1024 * 1024)));
        snprintf(system->peakWorkingSet, sizeof(system->peakWorkingSet), "%lu",
                 (unsigned long)(counters.PeakWorkingSetSize / (1024 * 1024)));
    }
    address = (uintptr_t)info.lpMinimumApplicationAddress;
    end = (uintptr_t)info.lpMaximumApplicationAddress;
    while (address < end && VirtualQuery((LPCVOID)address, &region, sizeof(region))) {
        uintptr_t next = (uintptr_t)region.BaseAddress + region.RegionSize;
        if (region.State == MEM_FREE) {
            totalFree += region.RegionSize;
            if (region.RegionSize > largestFree)
                largestFree = region.RegionSize;
        }
        if (next <= address)
            break;
        address = next;
    }
    snprintf(system->largestFree, sizeof(system->largestFree), "%llu", largestFree / (1024 * 1024));
    snprintf(system->totalFree, sizeof(system->totalFree), "%llu", totalFree / (1024 * 1024));
    system->largeAddressAware = WR_Headers((uintptr_t)GetModuleHandleA(NULL), &headers) &&
                                (headers.FileHeader.Characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE);
    if (GetProcessHandleCount(GetCurrentProcess(), &handles))
        snprintf(system->handles, sizeof(system->handles), "%lu", (unsigned long)handles);
    snprintf(system->gdiObjects, sizeof(system->gdiObjects), "%lu",
             (unsigned long)GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS));
    snprintf(system->userObjects, sizeof(system->userObjects), "%lu",
             (unsigned long)GetGuiResources(GetCurrentProcess(), GR_USEROBJECTS));
    snprintf(system->processId, sizeof(system->processId), "%lu", (unsigned long)GetCurrentProcessId());
    snprintf(system->exePath, sizeof(system->exePath), "%s", Win_Cod2xExePath());
    GetCurrentDirectoryA(sizeof(system->workingDirectory), system->workingDirectory);
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        ULARGE_INTEGER start, current;
        GetSystemTimeAsFileTime(&now);
        start.LowPart = created.dwLowDateTime;
        start.HighPart = created.dwHighDateTime;
        current.LowPart = now.dwLowDateTime;
        current.HighPart = now.dwHighDateTime;
        snprintf(system->uptime, sizeof(system->uptime), "%llu",
                 (unsigned long long)((current.QuadPart - start.QuadPart) / 10000000ULL));
    }
    Win_Cod2xSystem(&system->windows);
}

/* CoD2x's names for the client states; the engine's state 2 (CA_LOGO) is the
   value CoD2x calls AUTHORIZING. */
static const char *WR_ClientStateName(int state)
{
    static const char *const names[] = {"DISCONNECTED", "CINEMATIC", "AUTHORIZING", "CONNECTING", "CHALLENGING",
                                        "CONNECTED",    "LOADING",   "PRIMED",      "ACTIVE"};
    return state >= 0 && state <= 8 ? names[state] : "UNKNOWN";
}

static const char *const reportDvars[] = {
    "cl_hwid2", "name",          "fs_game",          "mapname",    "g_gametype", "sv_running",
    "dedicated", "developer",    "com_maxfps",       "r_mode",     "r_displayRefresh", "r_fullscreen",
    "r_monitor", "r_aaSamples",  "r_multiGpu",       "r_gpuSync",  "r_rendererPreference", "vid_xpos",
    "vid_ypos",  "cl_maxpackets", "rate",            "snaps",      "fs_basepath", "fs_homepath"};

static volatile LONG frameCount, insideFrame;
static int dvarsReady;

void Win_Cod2xFrameBegin(void)
{
    dvarsReady = 1;
    InterlockedIncrement(&frameCount);
    InterlockedExchange(&insideFrame, 1);
}

void Win_Cod2xFrameEnd(void)
{
    InterlockedExchange(&insideFrame, 0);
}

/* ---- Building the report ---- */

typedef struct {
    char name[8];
    uintptr_t address;
    unsigned char bytes[1024];
    size_t length;
    const char *note;
} WRMemory;

static WRMemory memoryBlocks[9];
static int memoryCount;
static char codePointers[64][400];
static int codePointerCount;

static void WR_AddMemory(const char *name, uintptr_t address, size_t length)
{
    WRMemory *block = &memoryBlocks[memoryCount++];
    memset(block, 0, sizeof(*block));
    snprintf(block->name, sizeof(block->name), "%s", name);
    block->address = address;
    if (address < 0x10000) {
        block->note = "near NULL";
    } else {
        block->length = WR_Read(address, block->bytes, length);
        if (!block->length)
            block->note = "unreadable";
    }
}

static void WR_Context(const CONTEXT *context)
{
    static uintptr_t words[2048];
    size_t got, i;
    memoryCount = codePointerCount = 0;
    if (!context)
        return;
    WR_AddMemory("stack", WR_SP(context), 1024);
    WR_AddMemory("code", WR_PC(context) >= 32 ? WR_PC(context) - 32 : 0, 64);
#if defined(_WIN64)
    WR_AddMemory("EAX", context->Rax, 64);
    WR_AddMemory("EBX", context->Rbx, 64);
    WR_AddMemory("ECX", context->Rcx, 64);
    WR_AddMemory("EDX", context->Rdx, 64);
    WR_AddMemory("ESI", context->Rsi, 64);
    WR_AddMemory("EDI", context->Rdi, 64);
    WR_AddMemory("EBP", context->Rbp, 64);
#else
    WR_AddMemory("EAX", context->Eax, 64);
    WR_AddMemory("EBX", context->Ebx, 64);
    WR_AddMemory("ECX", context->Ecx, 64);
    WR_AddMemory("EDX", context->Edx, 64);
    WR_AddMemory("ESI", context->Esi, 64);
    WR_AddMemory("EDI", context->Edi, 64);
    WR_AddMemory("EBP", context->Ebp, 64);
#endif
    /* Every stack value right after a CALL is a return address candidate. */
    got = WR_Read(WR_SP(context), words, sizeof(words)) / sizeof(words[0]);
    for (i = 0; i < got && codePointerCount < 64; ++i) {
        char where[400], site[200];
        if (!WR_ReturnAddress(words[i]))
            continue;
        WR_Describe(words[i], where, sizeof(where));
        WR_CallSite(words[i], 0, site, sizeof(site));
        snprintf(codePointers[codePointerCount++], sizeof(codePointers[0]), "ESP+0x%04X  0x%08llX  %-60s  %s",
                 (unsigned)(i * sizeof(words[0])), (unsigned long long)words[i], where, site);
    }
}

static void WR_ThreadsText(Cod2xReportText *text, Cod2xReportText *json, const WRReport *report)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    WRQueryThread query = WR_QueryThread();
    THREADENTRY32 entry;
    int first = 1;
    Cod2x_ReportTextAdd(json, "\"threads\":[");
    if (snapshot != INVALID_HANDLE_VALUE) {
        memset(&entry, 0, sizeof(entry));
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry)) {
            do {
                HANDLE thread;
                char start[24] = "", symbol[400] = "", name[64] = "";
                if (entry.th32OwnerProcessID != GetCurrentProcessId())
                    continue;
                thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
                if (thread) {
                    PVOID address = NULL;
                    if (query && query(thread, 9, &address, sizeof(address), NULL) == 0 && address) {
                        WR_Hex(start, (uintptr_t)address);
                        WR_Describe((uintptr_t)address, symbol, sizeof(symbol));
                    }
                    WR_ThreadName(thread, name, sizeof(name));
                    CloseHandle(thread);
                }
                Cod2x_ReportTextAdd(text, "TID=%lu%s%s  START=%s %s  NAME=%s\n", (unsigned long)entry.th32ThreadID,
                              strcmp(report->kind, "freeze") && entry.th32ThreadID == report->crashThread ? " [CRASH]"
                                                                                                          : "",
                              entry.th32ThreadID == report->currentThread ? " [CURRENT]" : "",
                              start[0] ? start : "<unknown>", symbol, name[0] ? name : "<unnamed>");
                Cod2x_ReportTextAdd(json, "%s{\"threadId\":\"%lu\",\"startAddress\":", first ? "" : ",",
                              (unsigned long)entry.th32ThreadID);
                Cod2x_ReportTextString(json, start);
                Cod2x_ReportTextAdd(json, ",\"startSymbol\":");
                Cod2x_ReportTextString(json, symbol);
                Cod2x_ReportTextAdd(json, ",\"threadName\":");
                Cod2x_ReportTextString(json, name);
                Cod2x_ReportTextAdd(json, "}");
                first = 0;
            } while (Thread32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    Cod2x_ReportTextAdd(json, "]");
}

static void WR_ModuleVersion(HMODULE module, char *out, size_t size)
{
    HRSRC resource = FindResourceA(module, MAKEINTRESOURCEA(1), (LPCSTR)RT_VERSION);
    HGLOBAL loaded = resource ? LoadResource(module, resource) : NULL;
    DWORD length = resource ? SizeofResource(module, resource) : 0, i;
    const BYTE *p = loaded ? (const BYTE *)LockResource(loaded) : NULL;
    out[0] = '\0';
    for (i = 0; p && i + sizeof(VS_FIXEDFILEINFO) <= length; i += 4) {
        const VS_FIXEDFILEINFO *info = (const VS_FIXEDFILEINFO *)(p + i);
        if (info->dwSignature == 0xFEEF04BD) {
            snprintf(out, size, "%u.%u.%u.%u", HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                     HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS));
            return;
        }
    }
}

static void WR_ModulesText(Cod2xReportText *text, Cod2xReportText *json)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    MODULEENTRY32 entry;
    int first = 1;
    Cod2x_ReportTextAdd(json, "\"modules\":[");
    if (snapshot != INVALID_HANDLE_VALUE) {
        memset(&entry, 0, sizeof(entry));
        entry.dwSize = sizeof(entry);
        if (Module32First(snapshot, &entry)) {
            do {
                char base[24], size[24], version[96] = "", stamp[24] = "";
                IMAGE_NT_HEADERS headers;
                WR_Hex(base, (uintptr_t)entry.modBaseAddr);
                WR_Hex(size, (uintptr_t)entry.modBaseSize);
                WR_ModuleVersion(entry.hModule, version, sizeof(version));
                /* This executable has no version resource: its version names the build. */
                if (!version[0] && entry.hModule == GetModuleHandleA(NULL))
                    snprintf(version, sizeof(version), "%s %s", COD2X_VERSION, COD2X_PLATFORM);
                if (WR_Headers((uintptr_t)entry.hModule, &headers))
                    WR_Hex(stamp, headers.FileHeader.TimeDateStamp);
                Cod2x_ReportTextAdd(text, "%s  %s  %-28s  %-16s  ts=%s  %s\n", base, size, entry.szModule,
                              version[0] ? version : "-", stamp[0] ? stamp : "?", entry.szExePath);
                Cod2x_ReportTextAdd(json, "%s{\"baseAddress\":\"%s\",\"size\":\"%s\",\"moduleName\":", first ? "" : ",",
                              base, size);
                Cod2x_ReportTextString(json, entry.szModule);
                Cod2x_ReportTextAdd(json, ",\"path\":");
                Cod2x_ReportTextString(json, entry.szExePath);
                Cod2x_ReportTextAdd(json, ",\"version\":");
                Cod2x_ReportTextString(json, version);
                Cod2x_ReportTextAdd(json, ",\"timestamp\":\"%s\"}", stamp);
                first = 0;
            } while (Module32Next(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    Cod2x_ReportTextAdd(json, "]");
}

static void WR_DvarsText(Cod2xReportText *text, Cod2xReportText *json)
{
    size_t i;
    Cod2x_ReportTextAdd(json, "\"cvars\":{");
    if (!dvarsReady)
        WR_Append(text, "<not available before Com_Init>\n");
    for (i = 0; dvarsReady && i < sizeof(reportDvars) / sizeof(reportDvars[0]); ++i) {
        char value[256];
        Cod2x_DvarTextNamed(reportDvars[i], value, sizeof(value));
        Cod2x_ReportTextAdd(text, "%-20s = %s\n", reportDvars[i], value);
        Cod2x_ReportTextAdd(json, "%s", i ? "," : "");
        Cod2x_ReportTextString(json, reportDvars[i]);
        Cod2x_ReportTextAdd(json, ":");
        Cod2x_ReportTextString(json, value);
    }
    Cod2x_ReportTextAdd(json, "}");
}

static void WR_MainFolderText(Cod2xReportText *text, Cod2xReportText *json)
{
    char folder[MAX_PATH * 2];
    DIR *directory;
    struct dirent *entry;
    int first = 1;
    snprintf(folder, sizeof(folder), "%s\\main", Win_Cod2xExeDirectory());
    Cod2x_ReportTextAdd(json, "\"mainFolderFiles\":[");
    directory = opendir(folder);
    if (!directory) {
        char note[MAX_PATH * 2 + 32];
        snprintf(note, sizeof(note), "<failed to open '%s'>", folder);
        Cod2x_ReportTextAdd(text, "%s\n", note);
        Cod2x_ReportTextAdd(json, "{\"entryType\":\"note\",\"message\":");
        Cod2x_ReportTextString(json, note);
        Cod2x_ReportTextAdd(json, "}]");
        return;
    }
    while ((entry = readdir(directory)) != NULL) {
        char path[MAX_PATH * 3], size[32] = "?";
        struct stat info;
        int isDirectory = 0;
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        snprintf(path, sizeof(path), "%s\\%s", folder, entry->d_name);
        if (stat(path, &info) == 0) {
            isDirectory = S_ISDIR(info.st_mode);
            snprintf(size, sizeof(size), "%lld", (long long)info.st_size);
        }
        if (isDirectory)
            Cod2x_ReportTextAdd(text, "       [DIR]  %s\n", entry->d_name);
        else
            Cod2x_ReportTextAdd(text, "  %10s  %s\n", size, entry->d_name);
        Cod2x_ReportTextAdd(json, "%s{\"entryType\":\"%s\"", first ? "" : ",", isDirectory ? "directory" : "file");
        if (!isDirectory)
            Cod2x_ReportTextAdd(json, ",\"size\":\"%s\"", size);
        Cod2x_ReportTextAdd(json, ",\"name\":");
        Cod2x_ReportTextString(json, entry->d_name);
        Cod2x_ReportTextAdd(json, "}");
        first = 0;
    }
    closedir(directory);
    Cod2x_ReportTextAdd(json, "]");
}

static void WR_StackJson(Cod2xReportText *json, const WRStack *stack)
{
    int i;
    char hex[24], hex2[24];
    Cod2x_ReportTextAdd(json, "{\"threadRole\":");
    Cod2x_ReportTextString(json, stack->role);
    Cod2x_ReportTextAdd(json, ",\"threadName\":");
    Cod2x_ReportTextString(json, stack->name);
    Cod2x_ReportTextAdd(json, ",\"threadId\":\"%lu\",\"method\":", (unsigned long)stack->id);
    Cod2x_ReportTextString(json, stack->method);
    Cod2x_ReportTextAdd(json, ",\"frames\":[");
    for (i = 0; i < stack->count; ++i) {
        const WRFrame *frame = &stack->frames[i];
        Cod2x_ReportTextAdd(json, "%s{\"frameNumber\":%d,\"instructionAddress\":\"%s\",\"moduleOffset\":\"%s\",\"moduleName\":",
                      i ? "," : "", frame->number, WR_Hex(hex, frame->address), WR_Hex(hex2, frame->offset));
        Cod2x_ReportTextString(json, frame->module);
        Cod2x_ReportTextAdd(json, ",\"sectionName\":");
        Cod2x_ReportTextString(json, frame->section);
        Cod2x_ReportTextAdd(json, ",\"resolvedSymbol\":");
        Cod2x_ReportTextString(json, frame->symbol);
        Cod2x_ReportTextAdd(json, ",\"instruction\":");
        Cod2x_ReportTextString(json, frame->instruction);
        Cod2x_ReportTextAdd(json, "}");
    }
    {
        char message[256];
        WR_ErrorText(symbolsError, message, sizeof(message));
        Cod2x_ReportTextAdd(json,
                      "],\"diagnostics\":{\"method\":");
        Cod2x_ReportTextString(json, stack->how);
        Cod2x_ReportTextAdd(json,
                      ",\"unwindFrames\":%d,\"contextRepaired\":%s,\"ebpFallbackAttempted\":%s,\"ebpFallbackFrames\":%d,"
                      "\"rawEspScanAttempted\":false,\"rawEspScanFrames\":0,\"stackSnapshot\":%s,\"stopReason\":",
                      stack->unwindFrames, stack->repaired ? "true" : "false", stack->chainTried ? "true" : "false",
                      stack->chainFrames, stack->snapshot ? "true" : "false");
        Cod2x_ReportTextString(json, stack->stopReason);
        Cod2x_ReportTextAdd(json, ",\"symInitializeStatus\":\"%s\",\"symInitializeCode\":%lu,\"symInitializeMessage\":",
                      symbolsReady ? "ok" : "failed", (unsigned long)symbolsError);
        Cod2x_ReportTextString(json, message);
        Cod2x_ReportTextAdd(json, ",\"stackWalkStatus\":\"%s\",\"stackWalkCode\":%lu,\"stackWalkMessage\":", stack->walkStatus,
                      (unsigned long)stack->walkCode);
        Cod2x_ReportTextString(json, stack->walkMessage);
        Cod2x_ReportTextAdd(json, "}}");
    }
}

static void WR_StackText(Cod2xReportText *text, const WRStack *stack)
{
    int i;
    char message[256];
    Cod2x_ReportTextAdd(text, "\n--- [%s]  TID %lu  \"%s\"  method: %s ---\n", stack->role, (unsigned long)stack->id,
                  stack->name, stack->method);
    WR_Append(text, "  #  Module                    Address     Offset      Symbol                                    "
                    "Instruction\n");
    if (!stack->count)
        WR_Append(text, "    Stack unavailable\n");
    for (i = 0; i < stack->count; ++i) {
        const WRFrame *frame = &stack->frames[i];
        Cod2x_ReportTextAdd(text, "%3d  %-24s  0x%08llX  0x%08llX  %-40s  %s\n", frame->number, frame->module,
                      (unsigned long long)frame->address, (unsigned long long)frame->offset, frame->symbol,
                      frame->instruction);
    }
    WR_ErrorText(symbolsError, message, sizeof(message));
    WR_Append(text, "    [diagnostics]\n");
    Cod2x_ReportTextAdd(text, "      method: %s\n", stack->how);
    Cod2x_ReportTextAdd(text, "      unwindFrames: %d\n", stack->unwindFrames);
    Cod2x_ReportTextAdd(text, "      contextRepaired: %s\n", stack->repaired ? "yes" : "no");
    Cod2x_ReportTextAdd(text, "      ebpFallbackFrames: %d\n", stack->chainFrames);
    Cod2x_ReportTextAdd(text, "      stackSnapshot: %s\n", stack->snapshot ? "yes" : "no");
    if (stack->stopReason && stack->stopReason[0])
        Cod2x_ReportTextAdd(text, "      stopReason: %s\n", stack->stopReason);
    Cod2x_ReportTextAdd(text, "      symInitialize: %s (code=%lu, message=\"%s\")\n", symbolsReady ? "ok" : "failed",
                  (unsigned long)symbolsError, message);
    if (!strcmp(stack->walkStatus, "failed"))
        Cod2x_ReportTextAdd(text, "      stackWalk64: failed (code=%lu, message=\"%s\")\n", (unsigned long)stack->walkCode,
                      stack->walkMessage);
    else
        Cod2x_ReportTextAdd(text, "      stackWalk64: %s\n", stack->walkStatus);
}

static void WR_SystemFieldsJson(Cod2xReportText *json, const WRSystem *system)
{
    char versionInfo[512];
    const Win_Cod2xSystemText *w = &system->windows;
    snprintf(versionInfo, sizeof(versionInfo), "%s\n%s\n%s\n%s\n%s\n%s\n%s", w->build, w->edition, w->display,
             w->country, w->virtualization, w->compatibility, w->elevated ? "admin" : "user");
    Cod2x_ReportTextAdd(json, "\"gpu\":");
    Cod2x_ReportTextString(json, system->gpu);
    Cod2x_ReportTextAdd(json, ",\"winBuild\":");
    Cod2x_ReportTextString(json, w->build);
    Cod2x_ReportTextAdd(json, ",\"winEdition\":");
    Cod2x_ReportTextString(json, w->edition);
    Cod2x_ReportTextAdd(json, ",\"winDisplayVersion\":");
    Cod2x_ReportTextString(json, w->display);
    Cod2x_ReportTextAdd(json, ",\"language\":");
    Cod2x_ReportTextString(json, w->country);
    Cod2x_ReportTextAdd(json, ",\"virtualization\":");
    Cod2x_ReportTextString(json, w->virtualization);
    Cod2x_ReportTextAdd(json, ",\"compatibilityFlags\":");
    Cod2x_ReportTextString(json, w->compatibility);
    Cod2x_ReportTextAdd(json, ",\"versionInfo\":");
    Cod2x_ReportTextString(json, versionInfo);
    Cod2x_ReportTextAdd(json, ",\"isElevated\":%s", w->elevated ? "true" : "false");
}

/* Collects the report: *text gets CoD2x's sections, the result is the JSON. */
static char *WR_Build(const WRReport *report, char **textOut)
{
    static char logger[50 * (512 + 32) + 1], logs[64 * 1024 + 1];
    static WRTarget targets[WR_MAX_TARGETS];
    static WRSystem system;
    Cod2xReportText text = {NULL, 0, 0}, json = {NULL, 0, 0}, threadsText = {NULL, 0, 0}, threadsJson = {NULL, 0, 0};
    int freeze = !strcmp(report->kind, "freeze"), count, i;
    char hex[24], identity[512];
    const CONTEXT *context = report->context;

    WR_Symbols();
    count = WR_Targets(report->crashThread, context, targets);
    stackCount = 0;
    for (i = 0; i < count; ++i) {
        WRStack *walk, *chain;
        if (!targets[i].hasContext || stackCount + 2 > WR_MAX_STACKS)
            continue;
        walk = &stacks[stackCount++];
        chain = &stacks[stackCount++];
        memset(walk, 0, sizeof(*walk));
        memset(chain, 0, sizeof(*chain));
        snprintf(walk->role, sizeof(walk->role), "%s", targets[i].role);
        snprintf(walk->name, sizeof(walk->name), "%s", targets[i].name);
        walk->id = targets[i].id;
        *chain = *walk;
        WR_WalkStackWalk64(&targets[i], report->address, walk);
        WR_WalkFrameChain(&targets[i], report->address, chain);
        if (targets[i].crashing) {
            WR_FrameZero(walk, report->module, report->address, report->fileOffset);
            WR_FrameZero(chain, report->module, report->address, report->fileOffset);
        }
    }
    WR_Context(context);
    WR_System(&system);
    Cod2x_LoggerRecent(logger, sizeof(logger));
    Cod2x_ConsoleLogs(logs, sizeof(logs));

    /* The report text */
    WR_Append(&text, freeze ? "=== FREEZE INFO ===\n" : "=== CRASH INFO ===\n");
    if (freeze) {
        Cod2x_ReportTextAdd(&text, "Reason:       %s\n", report->freezeReason ? report->freezeReason : "");
        Cod2x_ReportTextAdd(&text, "Frozen for:   %lu ms\n", (unsigned long)report->frozenMs);
    } else {
        Cod2x_ReportTextAdd(&text, "Code:         0x%08lX  (%s)\n", (unsigned long)report->code, WR_ExceptionName(report->code));
        Cod2x_ReportTextAdd(&text, "Address:      0x%08llX  (%s)\n", (unsigned long long)report->address,
                      report->module[0] ? report->module : "unknown module");
        Cod2x_ReportTextAdd(&text, "File Offset:  0x%08llX\n", (unsigned long long)report->fileOffset);
        if (report->fault[0])
            Cod2x_ReportTextAdd(&text, "Fault:        %s\n", report->fault);
        if (report->parameterCount) {
            WR_Append(&text, "Parameters:  ");
            for (i = 0; i < report->parameterCount; ++i)
                Cod2x_ReportTextAdd(&text, " %s", report->parameters[i]);
            WR_Append(&text, "\n");
        }
        for (i = 0; i < report->nestedCount; ++i)
            Cod2x_ReportTextAdd(&text, "Nested:       %s\n", report->nested[i]);
    }
    Cod2x_ReportTextAdd(&text, "Version:      %s\n", COD2X_RELEASE);
    Cod2x_ReportTextAdd(&text, "Time:         %s\n", report->time);
    if (report->minidump[0])
        Cod2x_ReportTextAdd(&text, "Minidump:     %s\n", report->minidump);
    WR_Append(&text, freeze ? "\n=== FREEZE CONTEXT ===\n" : "\n=== CRASH CONTEXT ===\n");
    if (!freeze)
        Cod2x_ReportTextAdd(&text, "Crash Thread ID:   %lu\n", (unsigned long)report->crashThread);
    Cod2x_ReportTextAdd(&text, "Current Thread ID: %lu\n", (unsigned long)report->currentThread);
    if (context) {
#if defined(_WIN64)
        unsigned long long r[10] = {context->Rip, context->Rax, context->Rbx, context->Rcx, context->Rdx,
                                    context->Rsi, context->Rdi, context->Rbp, context->Rsp, context->EFlags};
#else
        unsigned long long r[10] = {context->Eip, context->Eax, context->Ebx, context->Ecx, context->Edx,
                                    context->Esi, context->Edi, context->Ebp, context->Esp, context->EFlags};
#endif
        WR_Append(&text, "\n=== REGISTERS ===\n");
        Cod2x_ReportTextAdd(&text, "EIP=0x%08llX  EAX=0x%08llX  EBX=0x%08llX  ECX=0x%08llX\n", r[0], r[1], r[2], r[3]);
        Cod2x_ReportTextAdd(&text, "EDX=0x%08llX  ESI=0x%08llX  EDI=0x%08llX  EBP=0x%08llX\n", r[4], r[5], r[6], r[7]);
        Cod2x_ReportTextAdd(&text, "ESP=0x%08llX  EFL=0x%08llX\n", r[8], r[9]);
    }
    WR_Append(&text, "\n=== STACKS ===\n");
    for (i = 0; i < stackCount; ++i)
        WR_StackText(&text, &stacks[i]);
    if (codePointerCount) {
        WR_Append(&text, "\n=== CODE POINTERS ON STACK (raw ESP scan) ===\n");
        for (i = 0; i < codePointerCount; ++i)
            Cod2x_ReportTextAdd(&text, "  %s\n", codePointers[i]);
    }
    if (memoryCount) {
        WR_Append(&text, "\n=== MEMORY ===\n");
        for (i = 0; i < memoryCount; ++i) {
            const WRMemory *block = &memoryBlocks[i];
            Cod2x_ReportTextAdd(&text, "[%s] 0x%08llX%s%s\n", block->name, (unsigned long long)block->address,
                          block->note ? "  " : "", block->note ? block->note : "");
            WR_HexDump(&text, block->address, block->bytes, block->length,
                       !strcmp(block->name, "code") && context ? (uintptr_t)WR_PC(context) : (uintptr_t)-1);
        }
    }
    WR_Append(&text, "\n=== THREADS ===\n");
    WR_ThreadsText(&threadsText, &threadsJson, report);
    if (threadsText.text)
        WR_Append(&text, threadsText.text);
    WR_Append(&text, "\n=== SYSTEM ===\n");
    Cod2x_ReportTextAdd(&text, "Windows:        %s %s (build %s)\n", system.windows.edition, system.windows.display,
                  system.windows.build);
    Cod2x_ReportTextAdd(&text, "Virtualization: %s\n", system.windows.virtualization);
    Cod2x_ReportTextAdd(&text, "Compatibility:  %s\n", system.windows.compatibility);
    Cod2x_ReportTextAdd(&text, "Language:       %s\n", system.windows.country);
    Cod2x_ReportTextAdd(&text, "Elevated:       %s\n", system.windows.elevated ? "yes" : "no");
    Cod2x_ReportTextAdd(&text, "CPU Cores:      %s (affinity %s)\n", system.cpuCores, system.cpuAffinity);
    Cod2x_ReportTextAdd(&text, "Screen:         %s\n", system.screen);
    Cod2x_ReportTextAdd(&text, "GPU:            %s\n", system.gpu);
    WR_Append(&text, "\n=== MEMORY USAGE ===\n");
    Cod2x_ReportTextAdd(&text, "Memory Load:    %s%%\n", system.memoryLoad);
    Cod2x_ReportTextAdd(&text, "Physical:       %s MB available of %s MB\n", system.physicalAvailable, system.physicalTotal);
    Cod2x_ReportTextAdd(&text,
                  "Address space:  %s MB available of %s MB (largest free block %s MB, large address aware: %s)\n",
                  system.virtualAvailable, system.virtualTotal, system.largestFree,
                  system.largeAddressAware ? "yes" : "no");
    Cod2x_ReportTextAdd(&text, "Process:        private %s MB, working set %s MB (peak %s MB)\n", system.privateMB,
                  system.workingSet, system.peakWorkingSet);
    Cod2x_ReportTextAdd(&text, "Handles:        %s, GDI objects %s, USER objects %s\n", system.handles, system.gdiObjects,
                  system.userObjects);
    WR_Append(&text, "\n=== PROCESS ===\n");
    Cod2x_ReportTextAdd(&text, "PID:            %s\n", system.processId);
    Cod2x_ReportTextAdd(&text, "Executable:     %s\n", system.exePath);
    Cod2x_ReportTextAdd(&text, "Command:        %s\n", Win_Cod2xCommandLine());
    Cod2x_ReportTextAdd(&text, "WorkDir:        %s\n", system.workingDirectory);
    Cod2x_ReportTextAdd(&text, "Uptime:         %s s\n", system.uptime);
    WR_Append(&text, "\n=== GAME STATE ===\n");
    Cod2x_ReportTextAdd(&text, "Client state:   %s\n", WR_ClientStateName(Cod2x_ClientState()));
    Cod2x_ReportTextAdd(&text, "Demo playing:   %s\n", Cod2x_FeaturesDemo() ? "yes" : "no");
    Cod2x_ReportTextAdd(&text, "Frames:         %u (inside Com_Frame: %s)\n", (unsigned)frameCount, insideFrame ? "yes" : "no");

    /* The JSON */
    Cod2x_ReportTextAdd(&json, "{\"version\":");
    Cod2x_ReportTextString(&json, COD2X_RELEASE);
    Sys_Cod2xIdentity(identity, sizeof(identity));
    Cod2x_ReportTextAdd(&json, ",\"type\":\"win-client\",%s\"crashInfo\":{\"kind\":", identity);
    Cod2x_ReportTextString(&json, report->kind);
    Cod2x_ReportTextAdd(&json, ",\"exceptionCode\":\"%s\",\"exceptionName\":", WR_Hex(hex, report->code));
    Cod2x_ReportTextString(&json, WR_ExceptionName(report->code));
    Cod2x_ReportTextAdd(&json, ",\"exceptionAddress\":\"%s\",\"moduleName\":", WR_Hex(hex, report->address));
    Cod2x_ReportTextString(&json, report->module[0] ? report->module : (context && !freeze ? "unknown module" : ""));
    Cod2x_ReportTextAdd(&json, ",\"fileOffset\":\"%s\",\"faultDescription\":", WR_Hex(hex, report->fileOffset));
    Cod2x_ReportTextString(&json, report->fault);
    Cod2x_ReportTextAdd(&json, ",\"exceptionParameters\":[");
    for (i = 0; i < report->parameterCount; ++i) {
        Cod2x_ReportTextAdd(&json, i ? "," : "");
        Cod2x_ReportTextString(&json, report->parameters[i]);
    }
    Cod2x_ReportTextAdd(&json, "],\"nestedExceptions\":[");
    for (i = 0; i < report->nestedCount; ++i) {
        Cod2x_ReportTextAdd(&json, i ? "," : "");
        Cod2x_ReportTextString(&json, report->nested[i]);
    }
    Cod2x_ReportTextAdd(&json, "],\"crashThreadId\":\"%lu\",\"currentThreadId\":\"%lu\",\"freezeReason\":",
                  (unsigned long)report->crashThread, (unsigned long)report->currentThread);
    Cod2x_ReportTextString(&json, report->freezeReason ? report->freezeReason : "");
    Cod2x_ReportTextAdd(&json, ",\"frozenMs\":%lu,\"minidumpFile\":", (unsigned long)report->frozenMs);
    Cod2x_ReportTextString(&json, report->minidump);
    Cod2x_ReportTextAdd(&json, ",\"time\":");
    Cod2x_ReportTextString(&json, report->time);
    Cod2x_ReportTextAdd(&json, ",\"test\":0},\"crashContext\":{\"registers\":{");
    {
        static const char *const names[10] = {"EIP", "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP", "EFLAGS"};
        uintptr_t values[10] = {0};
        if (context) {
#if defined(_WIN64)
            uintptr_t r[10] = {context->Rip, context->Rax, context->Rbx, context->Rcx, context->Rdx,
                               context->Rsi, context->Rdi, context->Rbp, context->Rsp, context->EFlags};
#else
            uintptr_t r[10] = {context->Eip, context->Eax, context->Ebx, context->Ecx, context->Edx,
                               context->Esi, context->Edi, context->Ebp, context->Esp, context->EFlags};
#endif
            memcpy(values, r, sizeof(values));
        }
        for (i = 0; i < 10; ++i)
            Cod2x_ReportTextAdd(&json, "%s\"%s\":\"%s\"", i ? "," : "", names[i], context ? WR_Hex(hex, values[i]) : "");
    }
    Cod2x_ReportTextAdd(&json, "},");
    if (threadsJson.text)
        Cod2x_ReportTextAdd(&json, "%s", threadsJson.text);
    Cod2x_ReportTextAdd(&json, ",\"memory\":[");
    for (i = 0; i < memoryCount; ++i) {
        const WRMemory *block = &memoryBlocks[i];
        size_t b;
        Cod2x_ReportTextAdd(&json, "%s{\"name\":\"%s\",\"address\":\"%s\",\"hex\":\"", i ? "," : "", block->name,
                      WR_Hex(hex, block->address));
        for (b = 0; b < block->length; ++b)
            Cod2x_ReportTextAdd(&json, "%02x", block->bytes[b]);
        Cod2x_ReportTextAdd(&json, "\"");
        if (block->note)
            Cod2x_ReportTextAdd(&json, ",\"note\":\"%s\"", block->note);
        Cod2x_ReportTextAdd(&json, "}");
    }
    Cod2x_ReportTextAdd(&json, "],\"stackCodePointers\":[");
    for (i = 0; i < codePointerCount; ++i) {
        Cod2x_ReportTextAdd(&json, i ? "," : "");
        Cod2x_ReportTextString(&json, codePointers[i]);
    }
    Cod2x_ReportTextAdd(&json, "]},\"stacks\":[");
    for (i = 0; i < stackCount; ++i) {
        Cod2x_ReportTextAdd(&json, i ? "," : "");
        WR_StackJson(&json, &stacks[i]);
    }
    Cod2x_ReportTextAdd(&json, "],\"systemInfo\":{\"system\":{\"cpuCores\":");
    Cod2x_ReportTextString(&json, system.cpuCores);
    Cod2x_ReportTextAdd(&json, ",\"cpuAffinity\":");
    Cod2x_ReportTextString(&json, system.cpuAffinity);
    Cod2x_ReportTextAdd(&json, ",\"screenResolution\":");
    Cod2x_ReportTextString(&json, system.screen);
    Cod2x_ReportTextAdd(&json, ",\"cpuModel\":");
    Cod2x_ReportTextString(&json, system.cpuModel);
    Cod2x_ReportTextAdd(&json, ",");
    WR_SystemFieldsJson(&json, &system);
    Cod2x_ReportTextAdd(&json,
                  "},\"memory\":{\"memoryLoadPercent\":\"%s\",\"physicalTotalMB\":\"%s\",\"physicalAvailableMB\":\"%s\","
                  "\"virtualTotalMB\":\"%s\",\"virtualAvailableMB\":\"%s\"},",
                  system.memoryLoad, system.physicalTotal, system.physicalAvailable, system.virtualTotal,
                  system.virtualAvailable);
    Cod2x_ReportTextAdd(&json,
                  "\"processMemory\":{\"privateMB\":\"%s\",\"workingSetMB\":\"%s\",\"peakWorkingSetMB\":\"%s\","
                  "\"largestFreeBlockMB\":\"%s\",\"totalFreeAddressSpaceMB\":\"%s\",\"largeAddressAware\":%s,"
                  "\"handleCount\":\"%s\",\"gdiObjects\":\"%s\",\"userObjects\":\"%s\"},",
                  system.privateMB, system.workingSet, system.peakWorkingSet, system.largestFree, system.totalFree,
                  system.largeAddressAware ? "true" : "false", system.handles, system.gdiObjects, system.userObjects);
    Cod2x_ReportTextAdd(&json, "\"process\":{\"processId\":\"%s\",\"executablePath\":", system.processId);
    Cod2x_ReportTextString(&json, system.exePath);
    Cod2x_ReportTextAdd(&json, ",\"commandLine\":");
    Cod2x_ReportTextString(&json, Win_Cod2xCommandLine());
    Cod2x_ReportTextAdd(&json, ",\"workingDirectory\":");
    Cod2x_ReportTextString(&json, system.workingDirectory);
    Cod2x_ReportTextAdd(&json, ",\"uptimeSeconds\":\"%s\"},\"gameState\":{\"clientState\":\"%s\",\"demoPlaying\":%s,"
                         "\"frameCount\":%u,\"insideComFrame\":%s},",
                  system.uptime, WR_ClientStateName(Cod2x_ClientState()), Cod2x_FeaturesDemo() ? "true" : "false",
                  (unsigned)frameCount, insideFrame ? "true" : "false");

    WR_Append(&text, "\n=== MODULES ===\n");
    WR_ModulesText(&text, &json);
    Cod2x_ReportTextAdd(&json, ",");
    WR_Append(&text, "\n=== CVARS ===\n");
    WR_DvarsText(&text, &json);
    Cod2x_ReportTextAdd(&json, ",");
    WR_Append(&text, "\n=== MAIN FOLDER FILES ===\n");
    WR_MainFolderText(&text, &json);
    Cod2x_ReportTextAdd(&json, "},");
    WR_Append(&text, "\n=== LOGGER ===\n");
    WR_Append(&text, logger);
    if (text.length && text.text[text.length - 1] != '\n')
        WR_Append(&text, "\n");
    WR_Append(&text, "\n=== LOGS ===\n");
    WR_Append(&text, logs);
    if (text.length && text.text[text.length - 1] != '\n')
        WR_Append(&text, "\n");
    Cod2x_ReportTextLines(&json, "loggerLines", logger);
    Cod2x_ReportTextAdd(&json, ",");
    Cod2x_ReportTextLines(&json, "logLines", logs);
    Cod2x_ReportTextAdd(&json, "}");
    free(threadsText.text);
    free(threadsJson.text);
    *textOut = text.text;
    return json.text;
}

static void WR_Fill(WRReport *report, const char *kind, EXCEPTION_POINTERS *exception, DWORD crashThread,
                    const SYSTEMTIME *time)
{
    uintptr_t base = 0;
    memset(report, 0, sizeof(*report));
    report->kind = kind;
    report->crashThread = crashThread;
    report->currentThread = GetCurrentThreadId();
    snprintf(report->time, sizeof(report->time), "%04d-%02d-%02d %02d:%02d:%02d", time->wYear, time->wMonth, time->wDay,
             time->wHour, time->wMinute, time->wSecond);
    if (exception && exception->ExceptionRecord) {
        const EXCEPTION_RECORD *record = exception->ExceptionRecord, *nested;
        DWORD i;
        report->code = record->ExceptionCode;
        report->address = (uintptr_t)record->ExceptionAddress;
        if (WR_Module(report->address, report->module, sizeof(report->module), &base))
            report->fileOffset = report->address - base;
        WR_Fault(record, report->fault, sizeof(report->fault));
        for (i = 0; i < record->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
            WR_Hex(report->parameters[report->parameterCount++], (uintptr_t)record->ExceptionInformation[i]);
        for (nested = record->ExceptionRecord; nested && report->nestedCount < 4;) {
            EXCEPTION_RECORD copy;
            if (WR_Read((uintptr_t)nested, &copy, sizeof(copy)) != sizeof(copy))
                break;
            snprintf(report->nested[report->nestedCount++], sizeof(report->nested[0]), "0x%08lX %s at 0x%08llX",
                     (unsigned long)copy.ExceptionCode, WR_ExceptionName(copy.ExceptionCode),
                     (unsigned long long)(uintptr_t)copy.ExceptionAddress);
            nested = copy.ExceptionRecord;
        }
        report->context = exception->ContextRecord;
    }
}

static int WR_WriteFile(const char *path, const char *data, size_t length, int append)
{
    HANDLE file = CreateFileA(path, append ? FILE_APPEND_DATA : GENERIC_WRITE, FILE_SHARE_READ, NULL,
                              append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written = 0;
    BOOL ok;
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    ok = WriteFile(file, data, (DWORD)length, &written, NULL);
    CloseHandle(file);
    return ok && written == length;
}

/* The report as saved and shown: CRLF line ends. */
static char *WR_Crlf(const char *text, const char *json)
{
    Cod2xReportText out = {NULL, 0, 0};
    const char *parts[4] = {text ? text : "", "\n=== JSON ===\n", json ? json : "", "\n"};
    int i;
    for (i = 0; i < 4; ++i) {
        const char *p;
        for (p = parts[i]; *p; ++p) {
            if (*p == '\n' && (p == parts[i] || p[-1] != '\r'))
                Cod2x_ReportTextAdd(&out, "\r\n");
            else
                Cod2x_ReportTextAdd(&out, "%c", *p);
        }
    }
    return out.text;
}

/* ---- Minidumps ---- */

typedef struct {
    EXCEPTION_POINTERS *exception;
    DWORD thread;
    int full;
    const char *path;
    int ok;
} WRDump;

static wchar_t exeWide[MAX_PATH];

/* A small dump keeps the data segments of this executable only. */
static BOOL CALLBACK WR_DumpFilter(PVOID unused, const PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output)
{
    const wchar_t *path, *name;
    (void)unused;
    if (!input || !output || input->CallbackType != ModuleCallback || !(output->ModuleWriteFlags & ModuleWriteDataSeg))
        return TRUE;
    path = input->Module.FullPath;
    name = path ? wcsrchr(path, L'\\') : NULL;
    name = name ? name + 1 : (path ? path : L"");
    if (_wcsicmp(name, exeWide) != 0)
        output->ModuleWriteFlags &= ~ModuleWriteDataSeg;
    return TRUE;
}

static DWORD WINAPI WR_DumpThread(LPVOID data)
{
    typedef BOOL(WINAPI * WriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                      PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    WRDump *dump = (WRDump *)data;
    HMODULE help = LoadLibraryA("dbghelp.dll");
    WriteDump write = help ? (WriteDump)(void *)GetProcAddress(help, "MiniDumpWriteDump") : NULL;
    MINIDUMP_EXCEPTION_INFORMATION information;
    MINIDUMP_CALLBACK_INFORMATION callback;
    MINIDUMP_TYPE types[2];
    int i;
    dump->ok = 0;
    if (!write)
        return 0;
    information.ThreadId = dump->thread;
    information.ExceptionPointers = dump->exception;
    information.ClientPointers = FALSE;
    callback.CallbackRoutine = WR_DumpFilter;
    callback.CallbackParam = NULL;
    if (dump->full) {
        types[0] = (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithThreadInfo |
                                   MiniDumpWithUnloadedModules);
        types[1] = MiniDumpWithFullMemory;
    } else {
        types[0] = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs |
                                   MiniDumpWithHandleData | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules |
                                   MiniDumpWithProcessThreadData);
        types[1] = MiniDumpNormal;
    }
    for (i = 0; i < 2 && !dump->ok; ++i) {
        HANDLE file = CreateFileA(dump->path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
            return 0;
        dump->ok = write(GetCurrentProcess(), GetCurrentProcessId(), file, types[i],
                         dump->exception ? &information : NULL, NULL, dump->full ? NULL : &callback) != FALSE;
        CloseHandle(file);
    }
    if (!dump->ok)
        DeleteFileA(dump->path);
    return 0;
}

static SYSTEMTIME crashTime;

/* "<exe>.<kind>.<time>.dmp", or ".full.dmp" with all memory, written on a fresh
   thread that may take 30 seconds (3 minutes for a full one). */
int Win_Cod2xDump(EXCEPTION_POINTERS *exception, DWORD thread, int full, const char *kind, char *path, size_t size)
{
    char name[MAX_PATH * 2];
    SYSTEMTIME now;
    WRDump dump;
    HANDLE worker;
    if (!strcmp(kind, "crash") && crashTime.wYear)
        now = crashTime;
    else
        GetLocalTime(&now);
    Win_Cod2xDiagnosticPath(name, sizeof(name), kind, &now, full ? ".full.dmp" : ".dmp");
    MultiByteToWideChar(CP_ACP, 0, Win_Cod2xExeName(), -1, exeWide, MAX_PATH);
    dump.exception = exception;
    dump.thread = thread ? thread : GetCurrentThreadId();
    dump.full = full;
    dump.path = name;
    dump.ok = 0;
    worker = CreateThread(NULL, 0, WR_DumpThread, &dump, 0, NULL);
    if (worker) {
        if (WaitForSingleObject(worker, full ? 180000 : 30000) != WAIT_OBJECT_0) {
            TerminateThread(worker, 1);
            dump.ok = 0;
        }
        CloseHandle(worker);
    } else {
        WR_DumpThread(&dump);
    }
    if (dump.ok && path && size)
        snprintf(path, size, "%s", name);
    return dump.ok;
}

/* ---- Sending, and what the crash dialog says about it ---- */

static volatile LONG sendState = -1, sendSpinner;
static char sendError[256];

static void WR_Sent(int delivered, const char *error)
{
    snprintf(sendError, sizeof(sendError), "%s", error ? error : "");
    InterlockedExchange(&sendState, delivered ? 2 : 3);
}

/* The dialog's send line (CoD2x exception_updateSendLabel); -1 when nothing is
   sent to CoD2x, 1 sending, 2 sent, 3 failed. */
int Win_Cod2xSendStatus(char *text, size_t size)
{
    static const char *const dots[4] = {"", ".", "..", "..."};
    LONG state = InterlockedCompareExchange(&sendState, 0, 0);
    switch (state) {
    case 1:
        snprintf(text, size, "Crash report is being sent to CoD2x developers for analysis%s",
                 dots[InterlockedIncrement(&sendSpinner) % 4]);
        break;
    case 2:
        snprintf(text, size, "Crash report sent to CoD2x developers for analysis.");
        break;
    case 3:
        snprintf(text, size,
                 "Failed to send crash report: %s\nIt will be sent again on the next game start. You can also report "
                 "it on Discord - https://cod2x.me/",
                 sendError[0] ? sendError : "unknown error");
        break;
    case 0:
        snprintf(text, size, "Crash report will be sent to CoD2x developers for analysis.");
        break;
    default:
        text[0] = '\0';
        break;
    }
    return (int)state;
}

int Win_Cod2xSendState(void)
{
    return (int)InterlockedCompareExchange(&sendState, 0, 0);
}

/* The dialog often closes before the report is delivered: give it a moment. */
void Win_Cod2xSendWait(void)
{
    DWORD start = GetTickCount();
    while (InterlockedCompareExchange(&sendState, 0, 0) == 1 && GetTickCount() - start < WR_SEND_WAIT_MS)
        Sleep(20);
}

/* The minimal report, written first without the heap, in case the rest fails. */
static void WR_Minimal(const char *path, EXCEPTION_POINTERS *exception, DWORD thread)
{
    static char buffer[16 * 1024];
    const EXCEPTION_RECORD *record = exception->ExceptionRecord;
    const CONTEXT *context = exception->ContextRecord;
    uintptr_t address = (uintptr_t)record->ExceptionAddress, base = 0;
    char module[MAX_PATH] = "", fault[512];
    int used;
    WR_Module(address, module, sizeof(module), &base);
    WR_Fault(record, fault, sizeof(fault));
    used = snprintf(buffer, sizeof(buffer),
                    "=== CRASH INFO (minimal report, the full report was not completed) ===\r\n"
                    "Code:         0x%08lX  (%s)\r\n"
                    "Address:      0x%08llX  (%s)\r\n"
                    "File Offset:  0x%08llX\r\n"
                    "Fault:        %s\r\n"
                    "Version:      %s\r\n"
                    "Crash Thread: %lu\r\n",
                    (unsigned long)record->ExceptionCode, WR_ExceptionName(record->ExceptionCode),
                    (unsigned long long)address, module[0] ? module : "unknown module",
                    (unsigned long long)(base ? address - base : 0), fault, COD2X_RELEASE, (unsigned long)thread);
    if (context && used > 0 && (size_t)used < sizeof(buffer)) {
        uintptr_t words[128];
        size_t got = WR_Read(WR_SP(context), words, sizeof(words)) / sizeof(words[0]), i;
#if defined(_WIN64)
        used += snprintf(buffer + used, sizeof(buffer) - (size_t)used,
                         "\r\n=== REGISTERS ===\r\n"
                         "EIP=0x%08llX  EAX=0x%08llX  EBX=0x%08llX  ECX=0x%08llX\r\n"
                         "EDX=0x%08llX  ESI=0x%08llX  EDI=0x%08llX  EBP=0x%08llX\r\n"
                         "ESP=0x%08llX  EFL=0x%08lX\r\n",
                         context->Rip, context->Rax, context->Rbx, context->Rcx, context->Rdx, context->Rsi,
                         context->Rdi, context->Rbp, context->Rsp, (unsigned long)context->EFlags);
#else
        used += snprintf(buffer + used, sizeof(buffer) - (size_t)used,
                         "\r\n=== REGISTERS ===\r\n"
                         "EIP=0x%08lX  EAX=0x%08lX  EBX=0x%08lX  ECX=0x%08lX\r\n"
                         "EDX=0x%08lX  ESI=0x%08lX  EDI=0x%08lX  EBP=0x%08lX\r\n"
                         "ESP=0x%08lX  EFL=0x%08lX\r\n",
                         context->Eip, context->Eax, context->Ebx, context->Ecx, context->Edx, context->Esi,
                         context->Edi, context->Ebp, context->Esp, context->EFlags);
#endif
        used += snprintf(buffer + used, sizeof(buffer) - (size_t)used, "\r\n=== RAW STACK ===\r\n");
        for (i = 0; i < got && (size_t)used < sizeof(buffer) - 64; ++i) {
            if (i % 8 == 0)
                used += snprintf(buffer + used, sizeof(buffer) - (size_t)used, "%s0x%08llX:", i ? "\r\n" : "",
                                 (unsigned long long)(WR_SP(context) + i * sizeof(words[0])));
            used += snprintf(buffer + used, sizeof(buffer) - (size_t)used, " %08llX", (unsigned long long)words[i]);
        }
        used += snprintf(buffer + used, sizeof(buffer) - (size_t)used, "\r\n");
    }
    if (used > 0)
        WR_WriteFile(path, buffer, (size_t)used < sizeof(buffer) ? (size_t)used : sizeof(buffer) - 1, 0);
}

/* CoD2x's crash handler, stages 1 to 4 (exception_handleCrash): the minimal report,
   the small dump, the full report, then CoD2x's copy for a fault in CoD2x-layer code,
   sent at once, or the report receiver for any other fault. path gets the report. */
void Win_Cod2xCrashReport(EXCEPTION_POINTERS *exception, const char *reason, int owned, char *path, size_t size)
{
    static EXCEPTION_RECORD abortRecord;
    static CONTEXT abortContext;
    static EXCEPTION_POINTERS abortPointers;
    char pending[MAX_PATH * 2], dump[MAX_PATH * 2] = "";
    char *json, *text = NULL, *saved;
    WRReport *report = (WRReport *)calloc(1, sizeof(*report));
    Win_Cod2xCrashed();
    GetLocalTime(&crashTime);
    Win_Cod2xDiagnosticPath(path, size, "crash", &crashTime, ".txt");
    Win_Cod2xDiagnosticPath(pending, sizeof(pending), "crash", &crashTime, WR_PENDING_SUFFIX);
    if (!exception) {
        /* abort(): reported as CoD2x reports the game runtime's abort, error 10. */
        RtlCaptureContext(&abortContext);
        memset(&abortRecord, 0, sizeof(abortRecord));
        abortRecord.ExceptionCode = WR_CRT_RUNTIME_ERROR;
        abortRecord.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
        abortRecord.ExceptionAddress = (PVOID)(uintptr_t)WR_PC(&abortContext);
        abortRecord.NumberParameters = 2;
        abortRecord.ExceptionInformation[0] = 10;
        abortRecord.ExceptionInformation[1] = (ULONG_PTR)(reason ? reason : "abort()");
        abortPointers.ExceptionRecord = &abortRecord;
        abortPointers.ContextRecord = &abortContext;
        exception = &abortPointers;
    }
    if (exception->ExceptionRecord)
        WR_Minimal(path, exception, GetCurrentThreadId());
    Win_Cod2xDump(exception, GetCurrentThreadId(), 0, "crash", dump, sizeof(dump));
    if (!report) {
        if (!owned)
            Sys_CrashReceive(path, reason);
        return;
    }
    WR_Fill(report, "crash", exception, GetCurrentThreadId(), &crashTime);
    snprintf(report->minidump, sizeof(report->minidump), "%s", dump);
    json = WR_Build(report, &text);
    saved = WR_Crlf(text, json);
    if (saved)
        WR_WriteFile(path, saved, strlen(saved), 0);
    if (owned && json) {
        WR_WriteFile(pending, json, strlen(json), 0);
        if (Cod2x_ReportConsent()) {
            sendError[0] = '\0';
            InterlockedExchange(&sendSpinner, 0);
            InterlockedExchange(&sendState, 1);
            Cod2x_ReportCrash(json, pending, WR_SEND_WAIT_MS, 0, WR_Sent);
        }
    } else if (!owned) {
        Sys_CrashReceive(path, reason);
    }
    free(saved);
    free(text);
    free(json);
    free(report);
}

/* CoD2x's freeze report (exception_reportFreeze): "<exe>.freeze.<time>.txt" with the
   stacks of the game's main and render threads, sent to CoD2x when the main thread
   is stuck in CoD2x-layer code, waiting up to 7 seconds; any other freeze goes to
   the report receiver. At most three a run. */
void Win_Cod2xFreezeReport(const char *reason, DWORD frozenMs)
{
    static volatile LONG reporting;
    static int reported;
    SYSTEMTIME now;
    char path[MAX_PATH * 2], pending[MAX_PATH * 2];
    char *json, *text = NULL, *saved;
    WRReport *report;
    int owned = 0, i;
    if (Win_Cod2xHasCrashed() || reported >= 3 || InterlockedCompareExchange(&reporting, 1, 0) != 0)
        return;
    ++reported;
    GetLocalTime(&now);
    Win_Cod2xDiagnosticPath(path, sizeof(path), "freeze", &now, ".txt");
    Win_Cod2xDiagnosticPath(pending, sizeof(pending), "freeze", &now, WR_PENDING_SUFFIX);
    report = (WRReport *)calloc(1, sizeof(*report));
    if (!report) {
        InterlockedExchange(&reporting, 0);
        return;
    }
    WR_Fill(report, "freeze", NULL, 0, &now);
    report->freezeReason = reason;
    report->frozenMs = frozenMs;
    json = WR_Build(report, &text);
    /* Whose freeze: the first frame of the main thread inside this executable. */
    for (i = 0; i < stackCount; ++i) {
        int f;
        if (strcmp(stacks[i].role, "CoD2 MainThread") || strcmp(stacks[i].method, "StackWalk64"))
            continue;
        for (f = 0; f < stacks[i].count; ++f) {
            uintptr_t base = 0;
            if (WR_Module(stacks[i].frames[f].address, NULL, 0, &base) && base == (uintptr_t)GetModuleHandleA(NULL)) {
                owned = Cod2x_InLayer((const void *)stacks[i].frames[f].address);
                break;
            }
        }
        break;
    }
    saved = WR_Crlf(text, json);
    if (saved)
        WR_WriteFile(path, saved, strlen(saved), 0);
    Cod2x_Log("Freeze report saved to %s", path);
    if (owned && json) {
        WR_WriteFile(pending, json, strlen(json), 0);
        if (Cod2x_ReportConsent())
            Cod2x_ReportCrash(json, pending, WR_FREEZE_SEND_WAIT_MS, WR_FREEZE_SEND_WAIT_MS, NULL);
    } else if (!owned) {
        Sys_CrashReceive(path, reason);
    }
    free(saved);
    free(text);
    free(json);
    free(report);
    InterlockedExchange(&reporting, 0);
}

/* What cod2x_report.c sends besides a crash, in CoD2x's Windows forms. */
size_t Sys_Cod2xSystemInfo(char *out, size_t size)
{
    static WRSystem system;
    Cod2xReportText json = {NULL, 0, 0};
    size_t length;
    WR_Gpu(system.gpu, sizeof(system.gpu));
    Win_Cod2xSystem(&system.windows);
    Cod2x_ReportTextAdd(&json, "{");
    WR_SystemFieldsJson(&json, &system);
    Cod2x_ReportTextAdd(&json, "}");
    length = snprintf(out, size, "%s", json.text ? json.text : "{}");
    free(json.text);
    return length < size ? length : size - 1;
}

/* CoD2x's install id (REGID) and cl_hwid2. */
size_t Sys_Cod2xIdentity(char *out, size_t size)
{
    char regid[64] = "", hwid2[64] = "";
    DWORD type = 0, length = sizeof(regid) - 1;
    Cod2xReportText json = {NULL, 0, 0};
    size_t used;
    if (!Win_Cod2xRegRead("REGID", &type, regid, &length) || type != REG_SZ)
        regid[0] = '\0';
    if (!Cod2x_DvarText("cl_hwid2", hwid2, sizeof(hwid2)))
        hwid2[0] = '\0';
    Cod2x_ReportTextAdd(&json, "\"installId\":");
    Cod2x_ReportTextString(&json, regid);
    Cod2x_ReportTextAdd(&json, ",\"hwid2\":");
    Cod2x_ReportTextString(&json, hwid2);
    Cod2x_ReportTextAdd(&json, ",");
    used = snprintf(out, size, "%s", json.text ? json.text : "\"installId\":\"\",\"hwid2\":\"\",");
    free(json.text);
    return used < size ? used : size - 1;
}

size_t Sys_Cod2xLogLines(char *out, size_t size)
{
    return Cod2x_ConsoleLogs(out, size);
}
#endif
