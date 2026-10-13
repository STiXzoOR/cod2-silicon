#!/usr/bin/env python3
"""Private, data-free save/reload oracle using the production parser and writers.

Only platform, dvar/key storage and filesystem output are fixture seams. No game
binary, app home, graphics or network is involved. Pass a private scratch folder.
"""
import pathlib
import re
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[2]
scratch = pathlib.Path(sys.argv[1])
scratch.mkdir(parents=True, exist_ok=True)


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
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#define DVAR_COMBINED_STRING_SIZE 4096
typedef int Bool;
typedef int qboolean;
typedef int fileHandle_t;
typedef unsigned char byte;
typedef void (*xcommand_t)(void);
typedef struct { byte *data; int maxsize; int cmdsize; } cmd_t;
typedef byte cmd_text_buf_t[1][65536];
typedef struct cmd_function_s { intptr_t next; const char *name; xcommand_t function; } cmd_function_t;
typedef struct { const char *name; int type, flags; intptr_t next;
    union { const char *string; int enabled; } current; } dvar_t;
static int cmd_wait, cmd_argc;
static cmd_t cmd_texts[1], *cmd_text = cmd_texts;
static cmd_text_buf_t cmd_text_buf;
static char *cmd_argv[512], cmd_tokenized[8704];
static cmd_function_t *cmd_functions;
void Cmd_ExecuteString(const char *);
int Cmd_Argc(void);
char *Cmd_Argv(int);
static void Cbuf_ExecuteInternal(void);
static void Com_Printf(const char *format, ...) { (void)format; }
static void I_strncat(char *dest, int count, const char *src) {
    size_t n = strlen(dest); if (n < (size_t)count - 1) strncat(dest, src, count - n - 1);
}
static int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
static void *Z_MallocInternal(int n) { return calloc(1, n); }
static char *CopyStringInternal(const char *s) { return strdup(s); }
static dvar_t variable = {"motd", 7, 0, 0, {NULL}};
static char variable_value[4096];
static dvar_t *Dvar_FindVar(const char *name) { return !strcasecmp(name, variable.name) ? &variable : NULL; }
static Bool Dvar_IsValidName(const char *name) { return *name != 0; }
static void Dvar_SetCommand(const char *name, const char *value) {
    assert(!strcmp(name, variable.name)); snprintf(variable_value, sizeof(variable_value), "%s", value); variable.current.string = variable_value;
}
static void Dvar_AddFlags(const dvar_t *var, int flags) { ((dvar_t *)var)->flags |= flags; }
static const char *Dvar_DisplayableLatchedValue(const dvar_t *var) { return var->current.string; }
static const dvar_t *sortedDvars;
static qboolean Dvar_Command(void) { return 0; }
static const dvar_t *com_sv_running;
static void *imp_legacyHacks;
typedef struct { int cl_running; } LegacyHacks;
static qboolean CL_GameCommand(void) { return 0; }
static qboolean SV_GameCommand(void) { return 0; }
static int forwarded;
static void CL_ForwardCommandToServer(const char *text) { (void)text; ++forwarded; }
static struct { const char *binding; } keys[256];
static int modified_flags;
static void *imp_dvar_modifiedFlags = &modified_flags;
static void ReplaceStringInternal(char **value, const char *replacement) { free(*value); *value = strdup(replacement); }
static int Key_StringToKeynum(const char *key) { return !strcmp(key, "k") ? 107 : -1; }
static const char *Key_KeynumToStringInternal(int key, int translate) { (void)translate; assert(key == 107); return "k"; }
static const char *Key_GetBindingInternal(int key) { return keys[key].binding; }
static void Key_SetBindingInternal(int key, const char *value) {
    free((void *)keys[key].binding); keys[key].binding = strdup(value);
}
static char written_config[16384];
static void FS_Printf(fileHandle_t file, const char *format, ...) {
    (void)file; size_t n = strlen(written_config); va_list args; va_start(args, format);
    vsnprintf(written_config + n, sizeof(written_config) - n, format, args); va_end(args);
}
'''
policy = functions("src/PC/qcommon/cod2x_policy.c", ["Cod2x_Tokenize"])
cmd = functions("src/PC/qcommon/cmd.c", ["Cbuf_Init", "Cbuf_AddTextInternal", "Cbuf_AddText", "Cbuf_ExecuteInternal",
    "Cbuf_Execute", "Cmd_Argc", "Cmd_Argv", "Cmd_TokenizeStringInternal", "Cmd_AddCommand", "Cmd_ExecuteString"])
dvar = functions("src/PC/qcommon/dvar_cmds.c", ["Dvar_GetCombinedString_impl", "Dvar_Set_f", "Dvar_SetA_f", "Dvar_WriteVariables"])
key = functions("src/PC/client_mp/cl_keys_mp.c", ["Key_Bind_f", "Key_Unbindall_f", "Key_WriteBindings"])
main = r'''
static int connections;
static void Connect(void) { assert(!strcmp(Cmd_Argv(1), "1.2.3.4")); ++connections; }
static void load(const char *text) { Cbuf_AddText(text); Cbuf_Execute(); assert(cmd_text->cmdsize == 0); }
static void save(void) { written_config[0] = 0; Key_WriteBindings(1); Dvar_WriteVariables(1); }
int main(void) {
    LegacyHacks hacks = {0}; LegacyHacks *hackptr = &hacks; imp_legacyHacks = &hackptr;
    Cbuf_Init(); Cmd_AddCommand("bind", Key_Bind_f); Cmd_AddCommand("unbindall", Key_Unbindall_f);
    Cmd_AddCommand("seta", Dvar_SetA_f); Cmd_AddCommand("connect", Connect);
    load("bind k say \"a ;connect 1.2.3.4 \"\n");
    assert(connections == 0 && !strcmp(keys[107].binding, "say \"a ;connect 1.2.3.4 \""));
    save(); assert(!strcmp(written_config, "unbindall\nbind k \"say \\\"a ;connect 1.2.3.4 \\\"\"\n"));
    load(written_config); assert(connections == 1);
    puts("PASS: saved binding exposes hidden top-level connect on reload");
    load("bind k say \"hello world\"\n"); save(); load(written_config);
    assert(!strcmp(keys[107].binding, "say \"hello world\""));
    puts("PASS: quoted chat without an exposed separator persists unchanged");
    load("bind k say trailing\\\n"); save(); load(written_config);
    assert(!strcmp(keys[107].binding, "say trailing\""));
    puts("PASS: trailing binding backslash becomes a quote on reload");
    load("unbindall\nseta motd \"hello \\\"world\\\"\"\n"); sortedDvars = &variable;
    assert(!strcmp(variable.current.string, "hello \"world\""));
    save(); assert(!strcmp(written_config, "unbindall\nseta motd \"hello \"world\"\"\n"));
    load(written_config); assert(!strcmp(variable.current.string, "hello  world "));
    puts("PASS: archived inner quotes change the stored value on reload");
    load("seta motd trailing\\\n"); save(); load(written_config);
    assert(!strcmp(variable.current.string, "trailing\""));
    puts("PASS: archived trailing backslash becomes a quote on reload");
    assert(forwarded == 0); return 0;
}
'''
fixture = scratch / "engine-import-persistence.c"
fixture.write_text(preamble + "\n" + policy + "\n" + cmd + "\n" + dvar + "\n" + key + "\n" + main)
binary = scratch / "engine-import-persistence"
for mode, flags in [("stock", []), ("cod2x", ["-DCOD2_CODX=1"])]:
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", "xcrun", "clang", "-target", "arm64-apple-macos13",
        "-DCOD2_X64=1", "-DCOD2_APPLE_SDK=0", *flags, "-Wall", "-Wextra", "-Werror", "-Wno-unused-function",
        "-fsanitize=address,undefined", str(fixture), "-o", str(binary)], check=True)
    subprocess.run(["taskpolicy", "-b", "nice", "-n", "19", str(binary)], check=True)
    print("PASS: " + mode + " production Cbuf/Cmd, Key_WriteBindings and Dvar_WriteVariables (ASan/UBSan)")
