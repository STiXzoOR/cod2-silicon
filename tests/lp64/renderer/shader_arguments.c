#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"

static const char *tokens[8];
static int tokenIndex;
const char *Com_Parse(const char **text) { (void)text; return tokens[tokenIndex++]; }
void Com_UngetToken(void) { --tokenIndex; }
int Com_MatchToken(const char **text, const char *match, int lineBreaks)
{ (void)lineBreaks; return !strcmp(Com_Parse(text), match); }
int Com_ParseInt(const char **text) { return atoi(Com_Parse(text)); }
void Com_ScriptWarning(const char *format, ...) { (void)format; }
#include "PC/gfx_d3d/r_material_load_obj.c"

_Static_assert(offsetof(MaterialShaderArgument, u) == 8, "STABS argument union4 widens/aligned");
_Static_assert(sizeof(MaterialShaderArgument) == 16, "STABS shader argument8");

int main(void)
{
    const char *text = "unused";
    MaterialShaderArgument arg = {0};
    unsigned short typeInfo[6] = {3, 3, 0, 0, 4, 0};
    MaterialCodeConstantRouting routing = {0, 3, (const byte *)typeInfo};
    CodeConstantSource sources[] = {{"test", 7, 0, 0, 0}, {0}};
    tokens[0] = "."; tokens[1] = "test";
    arg.type = 1; arg.dest = 19;
    assert(Material_ParseCodeConstantSource_r_impl(&text, (byte *)&routing, 0, sources, (byte *)&arg));
    assert(arg.type == 1 && arg.dest == 19 && arg.u.codeConst.index == 7);
    assert(arg.u.codeConst.rowCount == 3 && arg.u.codeConst.firstRow == 0);
    sources[0].source = 187;
    tokenIndex = 0; tokens[2] = ";";
    assert(Material_ParseCodeConstantSource_r_impl(&text, (byte *)&routing, 0, sources, (byte *)&arg));
    assert(arg.u.codeConst.index == (187 ^ 2) && arg.u.codeConst.rowCount == 4);
    tokenIndex = 0; tokens[2] = "["; tokens[3] = "2"; tokens[4] = "]";
    assert(Material_ParseCodeConstantSource_r_impl(&text, (byte *)&routing, 0, sources, (byte *)&arg));
    assert(arg.u.codeConst.firstRow == 2 && arg.u.codeConst.rowCount == 1);
    assert(s_passOptionsDx7[4].valueOffset == offsetof(MaterialPassDx7, fogToBlack));
    assert(s_textureFuncsDx7[20].argCount == 3 && s_textureFuncsDx7[20].enumerant == 22);
    puts("renderer shader argument union, routing pointers and Dx7 tables: passed");
}
