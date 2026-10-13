#!/usr/bin/env python3
"""Data-free oracle using production Cbuf/Cmd, dvar command and key-bind code.

Only platform, dvar storage and filesystem dispatch are fixture substitutes.
No game binary, copied reference code, window or registered app is required.
"""
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
scratch = pathlib.Path(sys.argv[1])


def functions(path, names):
    source = (root / path).read_text()
    extracted = []
    for name in names:
        pattern = r"^(?:static inline __attribute__\(\(always_inline\)\)\s*|static )?(?:void|int|Bool|qboolean|char \*)\s*" + name + r"\([^;]*?\)\n\{.*?^\}"
        match = re.search(pattern, source, re.M | re.S)
        if not match:
            raise SystemExit("Cannot extract production function: " + name)
        extracted.append(match[0])
    return "\n".join(extracted)


preamble = r'''
#include <assert.h>
#define DVAR_COMBINED_STRING_SIZE 4096
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
typedef int Bool;
typedef int qboolean;
typedef int fileHandle_t;
typedef unsigned char byte;
typedef void (*xcommand_t)(void);
typedef struct { byte *data; int maxsize; int cmdsize; } cmd_t;
typedef byte cmd_text_buf_t[1][65536];
typedef struct cmd_function_s { intptr_t next; const char *name; xcommand_t function; } cmd_function_t;
typedef struct {
    const char *name; int type; int flags;
    intptr_t next;
    union { const char *string; int integer; int enabled; float value; } current;
    union { struct { int min, max; } integer; struct { float min, max; } value;
            struct { int stringCount; } enumeration; } domain;
} dvar_t;
enum { DVAR_SOURCE_EXTERNAL = 1 };
static int cmd_wait, cmd_argc;
static cmd_t cmd_texts[1], *cmd_text = cmd_texts;
static cmd_text_buf_t cmd_text_buf;
static char *cmd_argv[512], cmd_tokenized[8704];
static cmd_function_t *cmd_functions;
static int competitive;
static int Cod2x_Competitive(void) { return competitive; }
static void Cmd_ExecuteString(const char *text);
static int Cmd_Argc(void);
static char *Cmd_Argv(int);
static void Cbuf_ExecuteInternal(void);
static void Com_Printf(const char *format, ...) { (void)format; }
static int Com_sprintf(char *dest, int count, const char *format, ...) {
    va_list args; va_start(args, format); int result = vsnprintf(dest, count, format, args); va_end(args); return result;
}
static void I_strncpyz(char *dest, const char *src, int count) { snprintf(dest, count, "%s", src); }
static void I_strncat(char *dest, int count, const char *src) {
    size_t n = strlen(dest); if (n < (size_t)count - 1) strncat(dest, src, count - n - 1);
}
static int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
static int stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
static void *Z_MallocInternal(int count) { return calloc(1, count); }
static char *CopyStringInternal(const char *in) { return strdup(in); }
static char var_values[512][4096];
static dvar_t vars[512];
static int var_count;
static dvar_t *Dvar_FindVar(const char *name) {
    for (int i = 0; i < var_count; ++i) if (!strcasecmp(name, vars[i].name)) return &vars[i];
    return NULL;
}
static const char *Dvar_DisplayableValue(const dvar_t *var) { return var->current.string; }
static const char *Dvar_DisplayableLatchedValue(const dvar_t *var) { return var->current.string; }
static const dvar_t *sortedDvars;
static char written_config[8192];
static void FS_Printf(fileHandle_t file, const char *format, ...) {
    (void)file; size_t n = strlen(written_config); va_list args; va_start(args, format);
    vsnprintf(written_config + n, sizeof(written_config) - n, format, args); va_end(args);
}
static const char *Dvar_IndexStringToEnumString(const dvar_t *var, const char *value) { (void)var; return value; }
static void Dvar_SetCommand(const char *name, const char *value) {
    dvar_t *var = Dvar_FindVar(name);
    if (!var) { assert(var_count < 512); var = &vars[var_count++]; var->name = strdup(name); var->type = 7; }
    char *storage = var_values[var - vars]; snprintf(storage, 4096, "%s", value); var->current.string = storage;
}
static Bool Dvar_IsValidName(const char *name) { return *name != 0; }
static void Dvar_AddFlags(const dvar_t *var, int flags) { ((dvar_t *)var)->flags |= flags; }
static void Dvar_SetIntFromSource(const dvar_t *var, int value, int source) { (void)var; (void)value; (void)source; abort(); }
static void Dvar_SetBoolFromSource(const dvar_t *var, int value, int source) { (void)var; (void)value; (void)source; abort(); }
static void Dvar_SetFloatFromSource(const dvar_t *var, float value, int source) { (void)var; (void)value; (void)source; abort(); }
static const dvar_t *com_sv_running;
static void *imp_legacyHacks;
typedef struct { int cl_running; } LegacyHacks;
static qboolean CL_GameCommand(void) { return 0; }
static qboolean SV_GameCommand(void) { return 0; }
static void CL_ForwardCommandToServer(const char *text) { fprintf(stderr, "UNEXPECTED: %s\n", text); abort(); }
static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb"); assert(file); fseek(file, 0, SEEK_END); long n = ftell(file); rewind(file);
    char *bytes = calloc(n + 1, 1); assert(fread(bytes, 1, n, file) == (size_t)n); fclose(file); return bytes;
}
static int FS_ReadFile(const char *path, void **bytes) { *bytes = read_file(path); return (int)strlen(*bytes); }
static void FS_FreeFile(void *bytes) { free(bytes); }
static void Com_DefaultExtension(char *path, int count, const char *ext) { if (!strchr(path, '.')) I_strncat(path, count, ext); }
typedef struct { char va_string[2][1024]; int index; } va_info_t;
static va_info_t formatter;
static jmp_buf formatter_error;
static int expecting_formatter_error;
static void *Sys_GetValue(int key) { assert(key == 1); return &formatter; }
static void Com_Error(int code, const char *message, ...) {
    assert(expecting_formatter_error && code == 1 && !strcmp(message, "va string too long"));
    longjmp(formatter_error, 1);
}
static struct { const char *binding; } keys[256];
static char *Key_KeynumToString(int key, int translate) { (void)key; (void)translate; return "key"; }
static void Cmd_List_f(void) { abort(); }
'''
cmd = functions("src/PC/qcommon/cmd.c", ["Cbuf_Init", "Cbuf_AddTextInternal", "Cbuf_AddText", "Cbuf_InsertText",
    "Cbuf_ExecuteInternal", "Cbuf_Execute", "Cmd_Wait_f", "Cmd_Exec_f", "Cmd_Vstr_f", "Cmd_Argc", "Cmd_Argv",
    "Cmd_TokenizeStringInternal", "Cmd_AddCommand", "Cmd_ExecuteString", "Cmd_Init"])
formatter_source = functions("src/PC/universal/q_shared.c", ["va"])
# The original function definitions are external; forward declarations match.
preamble = preamble.replace("static void Cmd_ExecuteString", "void Cmd_ExecuteString").replace("static int Cmd_Argc", "int Cmd_Argc").replace("static char *Cmd_Argv", "char *Cmd_Argv")
policy = functions("src/PC/qcommon/cod2x_policy.c", ["Cod2x_Tokenize"])
dvar = functions("src/PC/qcommon/dvar_cmds.c", ["Dvar_GetCombinedString_impl", "Dvar_Set_f", "Dvar_SetA_f", "Dvar_ToggleInternal", "Dvar_Toggle_f", "Dvar_SetFromDvar_f", "Dvar_WriteVariables"])
# Cmd's dvar dispatch is recorded by a tiny fixture dispatcher, whose joined
# text still goes through the production helper.
dispatch = r'''
static qboolean Dvar_Command(void) {
    dvar_t *var = Dvar_FindVar(Cmd_Argv(0)); if (!var) return 0;
    if (Cmd_Argc() > 1) { char joined[4096]; Dvar_GetCombinedString_impl(joined, 1); Dvar_SetCommand(var->name, joined); }
    return 1;
}
'''
key = functions("src/PC/client_mp/cl_keys_mp.c", ["CL_ExecuteKeyBinding"])
main = r'''
static void Record(void) {
    printf("TRACE"); for (int i = 0; i < Cmd_Argc(); ++i) printf("\t%s", Cmd_Argv(i)); puts("");
}
static void drain(void) { int calls = 0; do { Cbuf_Execute(); assert(++calls < 32); } while (cmd_text->cmdsize || cmd_wait); }
static void execute_file(const char *path) { char *bytes = read_file(path); Cbuf_AddText(bytes); free(bytes); drain(); }
int main(int argc, char **argv) {
    assert(argc == 5); Cbuf_Init(); Cmd_Init();
    Cmd_AddCommand("set", Dvar_Set_f); Cmd_AddCommand("seta", Dvar_SetA_f); Cmd_AddCommand("toggle", Dvar_Toggle_f);
    const char *actions[] = {"say", "+attack", "-attack", "toggleads", "+holdbreath", "quit"};
    for (unsigned i = 0; i < sizeof(actions)/sizeof(actions[0]); ++i) Cmd_AddCommand(actions[i], Record);
    LegacyHacks hacks = {0}; LegacyHacks *hackptr = &hacks; imp_legacyHacks = &hackptr;
    const char *bindings[] = {"+attack", "+attack; say hi", "toggleads; +holdbreath", " +attack", "+attack; // apparent comment; quit"};
    for (unsigned i = 0; i < sizeof(bindings)/sizeof(bindings[0]); ++i) {
        printf("CASE\t%u\n", i); keys[106].binding = bindings[i];
        CL_ExecuteKeyBinding(106, 1, 100); drain(); CL_ExecuteKeyBinding(106, 0, 200); drain();
    }
    puts("CYCLE"); execute_file(argv[1]);
    for (int i = 0; i < 3; ++i) execute_file(argv[2]);
    puts("EXEC"); Cmd_ExecuteString("exec emitted"); drain(); execute_file(argv[2]); execute_file(argv[2]);
    puts("QUOTED"); execute_file(argv[3]); execute_file(argv[4]); execute_file(argv[4]);
    puts("PRESERVED"); char assignment[1200], tail[1010]; memset(tail, 'x', 1009); tail[1009] = 0;
    snprintf(assignment, sizeof(assignment), "seta a \"say %s; vstr safe\"", tail);
    Cmd_ExecuteString(assignment); assert(strlen(Dvar_FindVar("a")->current.string) == 1024);
    Cmd_ExecuteString("seta safe \"say harmless\""); Cmd_ExecuteString("seta sa \"quit\"");
    expecting_formatter_error = 1;
    if (setjmp(formatter_error) == 0) { Cbuf_AddText("vstr a\n"); drain(); assert(!"oversized vstr executed"); }
    cmd_text->cmdsize = 0; cmd_wait = 0; expecting_formatter_error = 0;
    puts("PASS: production formatter rejects preserved 1024-byte helper before leaf dispatch");
    char boundary[1024]; memset(boundary, 'x', sizeof(boundary)); memcpy(boundary, "say ", 4);
    boundary[1022] = 0; Dvar_SetCommand("boundary", boundary);
    const char *formatted = va("%s\n", Dvar_FindVar("boundary")->current.string);
    assert(strlen(formatted) == 1023 && formatted[1022] == '\n');
    boundary[1022] = 'x'; boundary[1023] = 0; Dvar_SetCommand("boundary", boundary);
    expecting_formatter_error = 1;
    if (setjmp(formatter_error) == 0) { Cbuf_AddText("vstr boundary\n"); drain(); assert(!"1023-byte helper executed"); }
    cmd_text->cmdsize = 0; cmd_wait = 0; expecting_formatter_error = 0;
    puts("PASS: production vstr formatter accepts 1022 bytes and rejects 1023");
    Cmd_AddCommand("setfromdvar", Dvar_SetFromDvar_f);
    Cmd_ExecuteString("set source 5"); Cmd_ExecuteString("seta target 0"); Cmd_ExecuteString("setfromdvar target source");
    assert(!strcmp(Dvar_FindVar("target")->current.string, "5")); assert(Dvar_FindVar("target")->flags & 1);
    puts("PASS: production setfromdvar source and archive retention");
    Cmd_ExecuteString("seta script \"say harmless\""); Cmd_ExecuteString("set script \"say \\\"hello world\\\"\"");
    assert(strchr(Dvar_FindVar("script")->current.string, '"'));
    sortedDvars = Dvar_FindVar("script"); Dvar_WriteVariables(1);
    assert(!strcmp(written_config, "seta script \"say \"hello world\"\"\n"));
    // The quoted prefix keeps its trailing space; argument joining adds more.
    Cbuf_AddText(written_config); drain(); assert(!strcmp(Dvar_FindVar("script")->current.string, "say  hello world "));
    puts("PASS: production unescaped archived quote rewrite changes value");
    Dvar_SetCommand("fps", "250"); Cmd_ExecuteString("toggle fps 125 250 333"); assert(!strcmp(Dvar_FindVar("fps")->current.string, "333"));
    Cmd_ExecuteString("toggle fps 125 250 333"); assert(!strcmp(Dvar_FindVar("fps")->current.string, "125"));
    puts("PASS: production toggle value list");
    cmd_wait = 0; competitive = 1; Cmd_ExecuteString("wait 2");
#if defined(COD2_CODX) && COD2_CODX
    assert(cmd_wait == 0);
#else
    assert(cmd_wait == 2);
#endif
    competitive = 0; Cmd_ExecuteString("wait 2"); assert(cmd_wait == 2); cmd_wait = 0;
    puts("PASS: production competitive wait gate");
    return 0;
}
'''
# Function ordering is declaration-only adaptation, never a changed function.
fixture = scratch / "engine-chains.c"
fixture.write_text(preamble + "\nstatic qboolean Dvar_Command(void);\nvoid Dvar_Set_f(void);\n" + formatter_source + "\n" + policy + "\n" + cmd + "\n" + dvar + "\n" + dispatch + "\n" + key + "\n" + main)
binary = scratch / "engine-chains"
expected = """CASE\t0
TRACE\t+attack\t106\t100
TRACE\t-attack\t106\t200
CASE\t1
TRACE\t+attack
TRACE\tsay\thi\t106\t100
TRACE\t-attack
TRACE\tsay\thi\t106\t200
CASE\t2
TRACE\ttoggleads
TRACE\t+holdbreath
CASE\t3
TRACE\t+attack
CASE\t4
TRACE\t+attack
TRACE\tquit\t106\t100
TRACE\t-attack
TRACE\tquit\t106\t200
CYCLE
TRACE\tsay\tone
TRACE\tsay\ttwo
TRACE\tsay\tone
EXEC
TRACE\tsay\tone
TRACE\tsay\ttwo
QUOTED
TRACE\tsay\thello world
TRACE\tsay\tbye
PRESERVED
PASS: production formatter rejects preserved 1024-byte helper before leaf dispatch
PASS: production vstr formatter accepts 1022 bytes and rejects 1023
PASS: production setfromdvar source and archive retention
PASS: production unescaped archived quote rewrite changes value
PASS: production toggle value list
PASS: production competitive wait gate
"""
for mode, mode_flags in [("stock", []), ("codx", ["-DCOD2_CODX=1"])]:
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "xcrun", "clang", "-target", "arm64-apple-macos26",
        "-DCOD2_X64=1", *mode_flags, "-DCOD2_APPLE_SDK=0", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
        "-fsanitize=address,undefined", str(fixture), "-o", str(binary)], check=True)
    output = subprocess.check_output(["taskpolicy", "-b", "nice", "-n", "19", str(binary),
        str(scratch / "emitted.cfg"), str(scratch / "trigger.cfg"), str(scratch / "quoted.cfg"), str(scratch / "quoted-trigger.cfg")], text=True, cwd=scratch)
    if output != expected:
        (scratch / (mode + "-engine-actual.txt")).write_text(output)
        raise SystemExit(mode + ": Production Cbuf/Cmd output differs from independent expected traces: " + repr(output))
    print("PASS: " + mode + " production Cbuf/Cmd/key events and emitted scripts (ASan/UBSan)")
