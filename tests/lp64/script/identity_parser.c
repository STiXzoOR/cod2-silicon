/* Parse real synthetic GSC player calls through the retained scanner/parser. */
#include "PC/script/scr_yacc.c"
#include "PC/script/scr_parsetree.c"
#include <assert.h>
#include <ctype.h>

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

static const char script[] = "+main() { self getIp(); self getHWID(); self getCDKeyHash(); "
    "self getAuthorizationStatus(); self getViewOrigin(); self getStance(); self isUsingTurret(); }\n";
static size_t inputPosition, allocationCount;
static void *allocations[512];
static char interned[32][64];
static unsigned int internCount;

int Scr_ScanFile(char *buf, int maximum)
{
    size_t remaining=sizeof(script)-1-inputPosition;
    size_t count=remaining<(size_t)maximum ? remaining : (size_t)maximum;
    memcpy(buf,script+inputPosition,count); inputPosition+=count;
    return (int)count;
}

void *Hunk_AllocateTempMemoryHighInternal(int size)
{
    assert(allocationCount<512);
    void *node=calloc(1,(size_t)size); assert(node);
    allocations[allocationCount++]=node;
    return node;
}

unsigned int SL_GetString_(const char *s, unsigned int user, int type)
{
    (void)user; (void)type;
    for(unsigned int i=0;i<internCount;++i) if(!strcmp(interned[i],s)) return i+1;
    assert(internCount<32); snprintf(interned[internCount],64,"%s",s);
    return ++internCount;
}

unsigned int SL_GetStringOfLen(const char *s,unsigned int user,unsigned int length,int type)
{
    assert(length==strlen(s)+1);
    return SL_GetString_(s,user,type);
}

unsigned int SL_ConvertToLowercase(unsigned int value,unsigned int user,int type)
{
    (void)user; (void)type;
    assert(value>0 && value<=internCount);
    for(char *p=interned[value-1];*p;++p) *p=(char)tolower((unsigned char)*p);
    return value;
}

void CompileError(unsigned int sourcePosition,const char *message,...)
{
    fprintf(stderr,"GSC parse error at %u: %s\n",sourcePosition,message); abort();
}

int main(void)
{
    sval_t result;
    ScriptParse(&result,1);
    assert(yynerrs==0 && result.node);
    static const char *methods[]={"getip","gethwid","getcdkeyhash","getauthorizationstatus","getvieworigin","getstance","isusingturret"};
    for(int i=0;i<7;++i) {
        int found=0;
        for(unsigned int j=0;j<internCount;++j) found |= !strcmp(methods[i],interned[j]);
        assert(found);
    }
    for(size_t i=0;i<allocationCount;++i) free(allocations[i]);
    puts("WS76 real GSC parser: seven player calls passed");
}
