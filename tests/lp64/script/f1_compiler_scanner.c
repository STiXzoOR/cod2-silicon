/* Real scanner refills and token actions; expectations from Mac 1.3 yyparse. */
#include <stdlib.h>
static void *scanner_malloc(size_t size);
#define malloc scanner_malloc
#include "PC/script/scr_yacc.c"
#undef malloc
#include <assert.h>
#include <setjmp.h>

int yychar, yynerrs, yyleng, yy_n_chars, yy_init, yy_start;
int yy_did_buffer_switch_on_eof;
stype_t yylval;
FILE *yyin, *yyout;
char *yytext, *yy_c_buf_p, *yy_last_accepting_cpos;
char yy_hold_char;
yy_state_type yy_last_accepting_state;
YY_BUFFER_STATE yy_current_buffer;
sval_t yaccResult, g_dummyVal;
unsigned int g_out_pos, g_sourcePos;
unsigned char g_parse_user;
char ch_buf[0x4002];
static scrCompilePub_t compilePub;
void *imp_scrCompilePub = &compilePub;
static const char *input;
static size_t inputLength, inputPosition;
static int scanCount, maxRequested;
static jmp_buf errorJump;
static char interned[128];
static int allocationFailure, allocationAttempt;
static void *scanner_malloc(size_t size)
{
    if (allocationFailure && ++allocationAttempt == allocationFailure) return NULL;
    return malloc(size);
}

int Scr_ScanFile(char *buffer, int maximum)
{
    ++scanCount;
    maxRequested = maximum;
    size_t count = inputLength - inputPosition;
    if (count > (size_t)maximum) count = maximum;
    memcpy(buffer, input + inputPosition, count);
    inputPosition += count;
    return (int)count;
}

unsigned int SL_GetStringOfLen(const char *s, unsigned int user, unsigned int len, int type)
{
    (void)user; (void)type;
    assert(len <= sizeof(interned));
    memcpy(interned, s, len);
    return 1;
}
unsigned int SL_GetString_(const char *s, unsigned int user, int type)
{ return SL_GetStringOfLen(s, user, (unsigned int)strlen(s) + 1, type); }
void CompileError(unsigned int pos, const char *format, ...)
{
    assert(pos == 3);
    assert(!strcmp(format, "bad token '%s'"));
    longjmp(errorJump, 1);
}

static void initialize(const char *text, size_t length, int capacity)
{
    input = text; inputLength = length;
    yy_current_buffer = yy_create_buffer(NULL, capacity);
    yyin = NULL; yyout = stdout; yy_start = 3; yy_init = 1;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    char stringBuffer[8192];
    if (!strcmp(argv[1], "scanner-oom-buffer") || !strcmp(argv[1], "scanner-oom-chars")) {
        allocationFailure = !strcmp(argv[1], "scanner-oom-buffer") ? 1 : 2;
        yy_create_buffer(NULL, 64);
        assert(!"failed scanner allocation must exit");
    } else if (!strcmp(argv[1], "scanner-events")) {
        static const char words[] = "notify waittill waittillmatch endon ";
        static const int tokens[] = {0x148, 0x145, 0x146, 0x14e};
        initialize(words, sizeof(words) - 1, 64);
        for (int i = 0; i < 4; ++i) {
            assert(yyparse_yylex(stringBuffer) == tokens[i]);
            assert(!interned[0]);
        }
    } else if (!strcmp(argv[1], "scanner-eof-token")) {
        initialize("last_identifier", 15, 64);
        assert(yyparse_yylex(stringBuffer) == 0x102);
        assert(!strcmp(interned, "last_identifier"));
        assert(yyparse_yylex(stringBuffer) == 0);
    } else if (!strcmp(argv[1], "scanner-last-match")) {
        initialize("", 0, 64);
        memcpy(yy_current_buffer->yy_ch_buf, "abc\0\0", 5);
        yytext = yy_current_buffer->yy_ch_buf;
        yy_c_buf_p = yytext + 4;
        yy_n_chars = 3;
        yy_current_buffer->yy_n_chars = 3;
        assert(yyparse_get_next_buffer(3) == 2);
        assert(yy_n_chars == 3 && yy_current_buffer->yy_buffer_status == 2);
    } else if (!strcmp(argv[1], "scanner-no-fill")) {
        initialize("ignored", 7, 64);
        yytext = yy_current_buffer->yy_ch_buf;
        yy_c_buf_p = yytext + 4;
        yy_current_buffer->yy_fill_buffer = 0;
        yy_current_buffer->yy_buffer_status = 1;
        yy_n_chars = 3;
        assert(yyparse_get_next_buffer(3) == 2);
        assert(scanCount == 0);
    } else if (!strcmp(argv[1], "scanner-capacity")) {
        initialize("abc", 3, 16);
        yytext = yy_current_buffer->yy_ch_buf;
        yy_c_buf_p = yytext + 1;
        yy_current_buffer->yy_buffer_status = 1;
        assert(yyparse_get_next_buffer(0) == 1);
        assert(maxRequested == 15);
    } else if (!strcmp(argv[1], "scanner-nul")) {
        initialize("foo\0bar ", 8, 64);
        assert(yyparse_yylex(stringBuffer) == 0x102);
        if (!setjmp(errorJump)) {
            yyparse_yylex(stringBuffer);
            assert(!"embedded NUL must report bad token");
        }
    } else abort();
    free(yy_current_buffer->yy_ch_buf);
    free(yy_current_buffer);
    puts(argv[1]);
    return 0;
}
