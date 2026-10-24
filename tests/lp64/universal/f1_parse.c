/* Synthetic public-parser checks derived from Mac 1.3 control flow. */
#include "common_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char str_002157b8[] = "";
extern ParseThreadInfo g_parse[1];
extern void Com_BeginParseSession(const char *name);
extern void Com_EndParseSession(void);
extern void Com_SetSpaceDelimited(qboolean enabled);
extern const char *Com_Parse(const char **text);
extern const char *Com_GetLastTokenPos(void);
extern void Com_UngetToken(void);

void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...)
{
    (void)code;
    fprintf(stderr, "Unexpected parser error: %s\n", format);
    abort();
}
void I_strncpyz(char *dest, const char *source, int capacity)
{
    snprintf(dest, (size_t)capacity, "%s", source);
}

static int failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static void LongTokens(void)
{
    const int lengths[] = {1023, 1024, 1025, 2048};
    char input[2060];
    for (int mode = 0; mode < 5; ++mode) {
        for (unsigned int n = 0; n < sizeof(lengths) / sizeof(lengths[0]); ++n) {
            int length = lengths[n];
            int prefix = mode == 4 ? 3 : 0;
            int tokenEnd = prefix + length + (mode == 3 ? 4 : 0);
            if (prefix)
                memcpy(input, "1e+", 3);
            memset(input + prefix, mode >= 2 ? '7' : 'a', (size_t)length);
            if (mode == 3)
                memcpy(input + length, "e+23", 4);
            memcpy(input + tokenEnd, " ;", 3);
            Com_BeginParseSession("long-token");
            Com_SetSpaceDelimited(mode == 0);
            parseInfo_t *pi = &g_parse[0].parseInfo[g_parse[0].parseInfoNum];
            pi->lines = 0x11223344;
            const char *cursor = input;
            const char *token = Com_Parse(&cursor);
            CHECK(strlen(token) == 1023);
            CHECK(cursor == input + tokenEnd);
            CHECK(pi->lines == 0x11223344);
            Com_EndParseSession();
        }
    }
}

static void TokenPositions(void)
{
    const char input[] = "alpha beta gamma";
    const char *cursor = input;
    Com_BeginParseSession("positions");
    CHECK(!strcmp(Com_Parse(&cursor), "alpha"));
    CHECK(Com_GetLastTokenPos() == input);
    CHECK(!strcmp(Com_Parse(&cursor), "beta"));
    CHECK(Com_GetLastTokenPos() == input + 6);
    Com_UngetToken();
    CHECK(Com_GetLastTokenPos() == input);
    CHECK(!strcmp(Com_Parse(&cursor), "beta"));
    CHECK(Com_GetLastTokenPos() == input + 6);
    CHECK(!strcmp(Com_Parse(&cursor), "gamma"));
    CHECK(Com_GetLastTokenPos() == input + 11);
    Com_EndParseSession();
}

static void HighBytes(void)
{
    const char input[] = "\200\377foo next";
    for (int mode = 0; mode < 2; ++mode) {
        const char *cursor = input;
        Com_BeginParseSession("high-bytes");
        Com_SetSpaceDelimited(mode);
        CHECK(!strcmp(Com_Parse(&cursor), "foo"));
        CHECK(cursor == input + 5);
        CHECK(!strcmp(Com_Parse(&cursor), "next"));
        Com_EndParseSession();
    }
}

static void Exponents(void)
{
    const char *inputs[] = {"1ex;", "1e+x;", "2E-?;", "1e+23;"};
    const char *expected[] = {"1ex", "1e+x", "2E-?", "1e+23"};
    for (unsigned int i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        const char *cursor = inputs[i];
        Com_BeginParseSession("exponent");
        Com_SetSpaceDelimited(0);
        CHECK(!strcmp(Com_Parse(&cursor), expected[i]));
        CHECK(*cursor == ';');
        CHECK(!strcmp(Com_Parse(&cursor), ";"));
        Com_EndParseSession();
    }
}

static void TerminalExponents(void)
{
    const char *inputs[] = {"1e", "1e+", "1e-"};
    for (unsigned int i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        size_t length = strlen(inputs[i]);
        char *input = malloc(length + 1);
        memcpy(input, inputs[i], length + 1);
        const char *cursor = input;
        Com_BeginParseSession("terminal-exponent");
        Com_SetSpaceDelimited(0);
        CHECK(!strcmp(Com_Parse(&cursor), inputs[i]));
        CHECK(cursor == input + length);
        CHECK(!strcmp(Com_Parse(&cursor), ""));
        CHECK(cursor == NULL);
        Com_EndParseSession();
        free(input);
    }
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    if (!strcmp(argv[1], "long")) LongTokens();
    else if (!strcmp(argv[1], "positions")) TokenPositions();
    else if (!strcmp(argv[1], "highbytes")) HighBytes();
    else if (!strcmp(argv[1], "exponents")) Exponents();
    else if (!strcmp(argv[1], "terminal")) TerminalExponents();
    else return 2;
    if (!failures)
        printf("F1 parser %s: pass\n", argv[1]);
    return failures != 0;
}
