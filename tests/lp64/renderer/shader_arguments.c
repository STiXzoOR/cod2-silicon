#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common_types.h"
#include "imports.h"

static const char *tokens[32];
static int tokenIndex;
const char *Com_Parse(const char **text) { (void)text; return tokens[tokenIndex++]; }
void Com_UngetToken(void) { --tokenIndex; }
int Com_MatchToken(const char **text, const char *match, int lineBreaks)
{ (void)lineBreaks; return !strcmp(Com_Parse(text), match); }
int Com_ParseInt(const char **text) { return atoi(Com_Parse(text)); }
void Com_ScriptWarning(const char *format, ...) { (void)format; }
void Com_Printf(const char *format, ...) { (void)format; }
void Com_SetScriptWarningPrefix(const char *prefix) { (void)prefix; }
void Com_SkipRestOfLine(const char **text) { (void)text; }
#include "PC/gfx_d3d/r_material_load_obj.c"

/* A native ARB fallback has no D3DX reflection table. */
const CodeConstantSource s_codeConsts[] = {{"materialColor",155,0,0,0},{0}};
const CodeSamplerSource s_codeSamplers[] = {{"feedback",1,0,0,0},{0}};
const CodeConstantSource s_defaultCodeConsts[] = {{0}};
const CodeSamplerSource s_defaultCodeSamplers[] = {{0}};
static byte constantTable[32];
static void *GetConstantTable(void *object) { (void)object; return constantTable; }
static void ReleaseConstantTable(void *object) { (void)object; }
static void *constantVtable[]={NULL,NULL,(void *)ReleaseConstantTable,(void *)GetConstantTable};
static void **constantObject=constantVtable;
HRESULT D3DXGetShaderConstantTable(const void *program,void **table)
{ (void)program;*table=&constantObject;return 0; }
void *Material_Alloc(int size) { return calloc(1,size); }
const char *Material_RegisterString(const char *s) { return s; }
const float *Material_RegisterLiteral(const float *v) { return v; }
const char *R_ErrorDescription(HRESULT hr) { (void)hr;return "fixture"; }
float Com_ParseFloat(const char **text) { return atof(Com_Parse(text)); }

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
    tokenIndex=0;
    const char *fallback[]={"{","colorMapSampler","=","sampler",".","feedback",";","materialColor","=","constant",".","materialColor",";","}"};
    memcpy(tokens,fallback,sizeof(fallback));
    MaterialShader shader={0};
    unsigned short flags=0,count=0;
    MaterialShaderArgument *arguments=NULL;
    assert(Material_SetPassShaderArguments_impl(&text,(const byte *)&shader,&flags,&count,&arguments));
    assert(count==2 && arguments[0].type==3 && arguments[0].u.codeSampler==1);
    assert(arguments[1].type==1 && arguments[1].dest==0 && arguments[1].u.codeConst.index==155 && arguments[1].u.codeConst.rowCount==1);
    free(arguments);
    puts("renderer shader argument union, routing pointers and Dx7 tables: passed");
}
