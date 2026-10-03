/* Real lexer, parser and syntax-node constructors, with memory/input seams. */
#include "PC/script/scr_yacc.c"
#include "PC/script/scr_parsetree.c"
#include <assert.h>

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
static scrCompilePub_t compile_pub;
void *imp_scrCompilePub = &compile_pub;

/* Scr_LoadScript supplies a leading '+' to select full-script grammar. */
static const char script[] = "+main() { return; }\n";
static size_t input_pos;
static void *nodes[64];
static size_t node_count;

int Scr_ScanFile(char *buf, int max_size)
{
    size_t remaining = sizeof(script) - 1 - input_pos;
    size_t count = remaining < (size_t)max_size ? remaining : (size_t)max_size;
    memcpy(buf, script + input_pos, count);
    input_pos += count;
    return (int)count;
}

void *Hunk_AllocateTempMemoryHighInternal(int size)
{
    assert(node_count < sizeof(nodes) / sizeof(nodes[0]));
    void *node = calloc(1, (size_t)size);
    assert(node && (uintptr_t)node > UINT32_MAX);
    nodes[node_count++] = node;
    return node;
}

unsigned int SL_GetString_(const char *str, unsigned int user, int type)
{
    (void)user;
    (void)type;
    assert(!strcmp(str, "main"));
    return 1;
}

unsigned int SL_GetStringOfLen(const char *str, unsigned int user, unsigned int len, int type)
{
    assert(len == 5);
    return SL_GetString_(str, user, type);
}

unsigned int SL_ConvertToLowercase(unsigned int value, unsigned int user, int type)
{
    (void)user;
    (void)type;
    assert(value == 1);
    return value;
}

void CompileError(unsigned int sourcePos, const char *msg, ...)
{
    fprintf(stderr, "parse failure at %u, token %#x (%s): %s\n", sourcePos, yychar, yytext, msg);
    abort();
}

int main(void)
{
    sval_t result;
    ScriptParse(&result, 1);
    assert(yynerrs == 0 && result.node);
    intptr_t *root = (intptr_t *)result.node;
    intptr_t *threads = (intptr_t *)root[1];
    intptr_t *sentinel = (intptr_t *)threads[0];
    intptr_t *entry = (intptr_t *)sentinel[1];
    intptr_t *function = (intptr_t *)entry[0];
    assert(function[0] == 0x44 && function[1] == 1);
    assert(function[4] == 0 && function[5] == 17);
    intptr_t *statements = (intptr_t *)function[3];
    intptr_t *statement_sentinel = (intptr_t *)statements[0];
    intptr_t *statement_entry = (intptr_t *)statement_sentinel[1];
    intptr_t *statement = (intptr_t *)statement_entry[0];
    assert(statement[0] == 0x1c && statement[1] == 9);
    for (size_t i = 0; i < node_count; ++i)
        free(nodes[i]);
    puts("parser-native-nodes");
    return 0;
}
