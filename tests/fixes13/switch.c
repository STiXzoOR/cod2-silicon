#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
static scrCompileGlob_t compiler;
void *imp_scrCompileGlob = &compiler;
#define SCRCG (&compiler)
static int errors;
static unsigned int errorPos;
void CompileError(unsigned int pos, const char *fmt, ...)
{
    (void)fmt; ++errors; errorPos = pos;
}
#include "switch_source.h"
int main(void)
{
    CaseStatementInfo cases[2] = {0};
    unsigned int table[2][2] = {{0, 12}, {0, 24}};
    cases[0].name = 0; cases[0].sourcePos = 10; cases[0].next = &cases[1];
    cases[1].name = 0; cases[1].sourcePos = 20;
    compiler.currentCaseStatement = cases;
    check_cases(table, 2);
    assert(errors == 1 && errorPos == 10);
    errors = 0; table[0][0] = table[1][0] = cases[0].name = cases[1].name = 123;
    check_cases(table, 2); assert(errors == 1);
    errors = 0; table[0][0] = cases[0].name = 0; table[1][0] = cases[1].name = 123;
    check_cases(table, 2); assert(errors == 0);
    return 0;
}
