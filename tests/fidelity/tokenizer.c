#include "common_types.h"
#include "cod2x_policy.h"
#include "check.h"
static char cmd_tokenized[8704];
void Com_Printf(const char *format, ...) { (void)format; }
#include "tokenizer_source.h"
static char *argv[512];
static int tokenize(const char *input, int limit)
{
    memset(argv, 0, sizeof(argv));
    return Cmd_TokenizeStringInternal(input, limit, argv, cmd_tokenized);
}
int main(void)
{
    int n = tokenize("bind J \"say \\\"hello\\\"\"", 0);
    CHECK(n == 3 && !strcmp(argv[2], "say \"hello\""));
    n = tokenize("a/*/b \"\" \"c\"d", 0);
    CHECK(n == 5 && !strcmp(argv[0], "a") && !strcmp(argv[1], "b") &&
        !strcmp(argv[2], "") && !strcmp(argv[3], "c") && !strcmp(argv[4], "d"));
    for (int byte = 128; byte < 256; ++byte) {
        char input[] = {'a', (char)byte, 'b', ' ', '"', (char)byte, '"', 0};
        n = tokenize(input, 0);
        CHECK(n == 3 && !strcmp(argv[0], "a") && !strcmp(argv[1], "b") &&
            (unsigned char)argv[2][0] == byte && argv[2][1] == 0);
    }
    n = tokenize("one two   /* comment */ \"three four\"", 3);
    CHECK(n == 3 && !strcmp(argv[2], "  /* comment */ \"three four\""));
    n = tokenize("one \"two\"  three", 3);
    CHECK(n == 3 && !strcmp(argv[2], " three"));
    n = tokenize("\xef\xbb\xbfset name \"\xa7\"", 0);
    CHECK(n == 3 && !strcmp(argv[0], "set") && (unsigned char)argv[2][0] == 0xa7);
    char tokens[1100];
    for (int i = 0; i < 512; ++i) { tokens[2 * i] = 'a'; tokens[2 * i + 1] = ' '; }
    tokens[1023] = 0;
    CHECK(tokenize(tokens, 0) == 512);
    tokens[1023] = ' '; tokens[1024] = 0;
    CHECK(tokenize(tokens, 0) == 0);
    tokens[1024] = 'b'; tokens[1025] = 0;
    CHECK(tokenize(tokens, 0) == 0);
    n = tokenize("upload https://host/demo // comment", 0);
#if defined(COD2_CODX) && COD2_CODX
    CHECK(n == 2 && !strcmp(argv[1], "https://host/demo"));
    CHECK(Cod2x_Tokenize("overlong", 0, argv, cmd_tokenized, 4) == -1);
#else
    CHECK(n == 2 && !strcmp(argv[1], "https:"));
#endif
    FINISH("tokenizer");
}
