/* Spawn parser error and storage contracts through the full production TU. */
#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern qboolean G_ParseSpawnVars(SpawnVar *);
extern qboolean G_SpawnStringInternal(SpawnVar *, const char *, const char *, const char **);
extern unsigned int G_NewString(const char *);
extern char *vtos(const vec_t *);
static SpawnVar spawn;
static const char *tokens[140];
static int tokenCount, tokenIndex;
static char diagnostic[512], interned[16384], largeToken[1024];
static jmp_buf errorJump;
qboolean SV_GetEntityToken(char *out, int size)
{
    assert(size == 1024);
    if (tokenIndex == tokenCount) return 0;
    assert(strlen(tokens[tokenIndex]) < (size_t)size);
    strcpy(out, tokens[tokenIndex++]); return 1;
}
void Com_Error(int code, const char *format, ...)
{
    assert(code == 1);
    va_list args; va_start(args, format);
    vsnprintf(diagnostic, sizeof(diagnostic), format, args); va_end(args);
    longjmp(errorJump, 1);
}
unsigned int SL_GetString(const char *s, int user)
{ assert(user == 0); snprintf(interned, sizeof(interned), "%s", s); return 123; }
int Com_sprintf(char *out, int size, const char *format, ...)
{
    va_list args; va_start(args, format);
    int result = vsnprintf(out, size, format, args); va_end(args); return result;
}
static void ParserError(const char *kind)
{
    const char *expected;
    tokens[0] = "{"; tokenCount = 1;
    if (!strcmp(kind, "opening")) {
        tokens[0] = "bad";
        expected = "\x15" "G_ParseSpawnVars: found bad when expecting {";
    } else if (!strcmp(kind, "eof-key") || !strcmp(kind, "eof-value")) {
        if (!strcmp(kind, "eof-value")) tokens[tokenCount++] = "key";
        expected = "\x15" "G_ParseSpawnVars: EOF without closing brace";
    } else if (!strcmp(kind, "closing")) {
        tokens[tokenCount++] = "key"; tokens[tokenCount++] = "}";
        expected = "\x15" "G_ParseSpawnVars: closing brace without data";
    } else if (!strcmp(kind, "vars")) {
        for (int i = 0; i < 65; ++i) { tokens[tokenCount++] = "key"; tokens[tokenCount++] = "value"; }
        expected = "\x15" "G_ParseSpawnVars: MAX_SPAWN_VARS";
    } else {
        memset(largeToken, 'a', 1023); largeToken[1023] = 0;
        tokens[tokenCount++] = largeToken; tokens[tokenCount++] = "x";
        tokens[tokenCount++] = !strcmp(kind, "chars-key") ? largeToken : "y";
        tokens[tokenCount++] = largeToken;
        expected = "\x15" "G_AddSpawnVarToken: MAX_SPAWN_VARS";
    }
    if (!setjmp(errorJump)) { G_ParseSpawnVars(&spawn); abort(); }
    assert(!strcmp(diagnostic, expected) && !spawn.spawnVarsValid);
}
static void Controls(void)
{
    tokens[0] = "{"; tokens[1] = "Name"; tokens[2] = "first";
    tokens[3] = "Name"; tokens[4] = "second"; tokens[5] = "}"; tokenCount = 6;
    assert(G_ParseSpawnVars(&spawn) && spawn.spawnVarsValid && spawn.numSpawnVars == 2);
    assert(spawn.numSpawnVarChars == 23);
    const char *value;
    assert(G_SpawnStringInternal(&spawn, "Name", "default", &value) && !strcmp(value, "first"));
    assert(!G_SpawnStringInternal(&spawn, "name", "default", &value) && !strcmp(value, "default"));
    assert(!G_ParseSpawnVars(&spawn) && !spawn.spawnVarsValid && spawn.numSpawnVars == 0);
    assert(G_NewString("a\\nb\\qc") == 123 && !strcmp(interned, "a\nb\\c"));
    char *saved[8];
    for (int i = 0; i < 8; ++i) saved[i] = vtos((vec3_t){i + 0.9f, -1.9f, 2});
    for (int i = 0; i < 8; ++i) { char expected[32]; snprintf(expected, 32, "(%i -1 2)", i); assert(!strcmp(saved[i], expected)); }
    assert(vtos((vec3_t){8, 0, 0}) == saved[0] && !strcmp(saved[0], "(8 0 0)"));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "controls")) Controls(); else ParserError(argv[1]);
    printf("load utils %s: pass\n", argv[1]); return 0;
}
