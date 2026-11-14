#include "dvar_services.h"
#include <assert.h>

static char config[65536];static size_t configLength;static const char *tokens[3];
const char *Dvar_DisplayableLatchedValue(const dvar_t *);
const char *Dvar_DisplayableValue(const dvar_t *);
Bool Dvar_IsValidName(const char *);
static int Cmd_Argc(void) { return 3; }
static char *Cmd_Argv(int i) { return (char *)tokens[i]; }
static void FS_Printf(int file,const char *format,...) { (void)file;va_list ap;va_start(ap,format);int n=vsnprintf(config+configLength,sizeof config-configLength,format,ap);va_end(ap);assert(n>=0&&(size_t)n<sizeof config-configLength);configLength+=n; }


#include "registrations_source.h"
static void persistence(void) {
 init_rune_locale();
 const char *names[]={"fx_sort","sc_enable","r_zFeather","r_dlightLimit","r_depthPrepassModels"};
 const char *defaults[]={"1","1","1","4","0"};const char *values[]={"0","0","0","0","1"};
 Dvar_Init();RegisterCorrections();
#if !COD2_CODX
 char presetNames[5][32]={0},presetValues[5][32]={0};
 for(int i=0;i<5;i++){strcpy(presetNames[i],names[i]);strcpy(presetValues[i],defaults[i]);}
 Com_SetConfigureDvars(5,(char *)presetNames,(char *)presetValues);
#endif
 for(int i=0;i<5;i++){
  const dvar_t *v=Dvar_FindVar(names[i]);CHECK(v);CHECK(!strcmp(Dvar_DisplayableValue(v),defaults[i]));
  Dvar_SetCommand(names[i],values[i]);CHECK(!strcmp(Dvar_DisplayableValue(v),values[i]));
  printf("graphics before-save name=%s value=%s flags=%x\n",names[i],Dvar_DisplayableValue(v),v->flags);
 }
 /* No new archive policy for a configure_mp.cfg `set`, nor loss of existing flags. */
 CHECK(!(Dvar_FindVar("r_multiGpu")->flags&1));CHECK(Dvar_FindVar("r_glow")->flags&1);
 configLength=0;config[0]=0;Dvar_WriteVariables(1);
 char saved[65536];memcpy(saved,config,configLength+1);
 int expectedPersist=1;int written=0;
 for(int i=0;i<5;i++){char search[128];snprintf(search,sizeof search,"seta %s \"%s\"",names[i],values[i]);written+=strstr(saved,search)!=NULL;}
 CHECK(written==(expectedPersist?5:0));printf("graphics written=%d/5\n",written);
 puts("saved-config-begin");fputs(saved,stdout);puts("saved-config-end");Dvar_Shutdown();
 /* Fresh registry loads saved seta commands before typed renderer registration. */
 Dvar_Init();char *line=saved;
 while(*line){char name[256],value[4096];int used=0;CHECK(sscanf(line,"seta %255s \"%4095[^\"]\"%n",name,value,&used)>=1);
  char *end=strchr(line,'\n');if(!end)break;
  char *quote=strchr(line,'"');
  if(quote && quote[1]=='"')value[0]=0;
  tokens[0]="seta";tokens[1]=name;tokens[2]=value;Dvar_SetA_f();line=end+1;
 }
 RegisterCorrections();int restored=0;
 for(int i=0;i<5;i++){const dvar_t *v=Dvar_FindVar(names[i]);const char *value=Dvar_DisplayableValue(v);restored+=!strcmp(value,values[i]);CHECK(!strcmp(value,expectedPersist?values[i]:defaults[i]));printf("graphics reloaded name=%s value=%s flags=%x\n",names[i],value,v->flags);}
 CHECK(restored==(expectedPersist?5:0));printf("graphics restored=%d/5\n",restored);
 /* Re-registration preserves both imported archives and user-selected values. */
 RegisterCorrections();for(int i=0;i<5;i++)CHECK(!strcmp(Dvar_DisplayableValue(Dvar_FindVar(names[i])),expectedPersist?values[i]:defaults[i]));
 Dvar_Shutdown();
}

int main(void)
{
    Dvar_Init();
    RegisterCorrections();
    const char *names[] = {"r_lodBias", "r_lightTweakAmbient", "r_lightTweakDiffuseFraction", "r_lightTweakSunLight",
        "r_lightTweakAmbientColor", "r_lightTweakSunColor", "r_lightTweakSunDiffuseColor", "r_lightTweakSunDirection"};
    for (int i = 0; i < 8; ++i) {
        const dvar_t *var = Dvar_FindVar(names[i]);
        CHECK(var && (var->flags & 0x80));
        CHECK(var && !(var->flags & 1));
        if (var) {
            DvarValue before = var->current;
            float vector[3];
            if (var->type == 3)
                memcpy(vector, var->current.vector, sizeof(vector));
            const char *value = var->type == 8 ? "0 0 0 1" : var->type == 3 ? "1 2 3" : i == 0 ? "-1" : "2";
            Dvar_SetCommand(names[i], value);
            CHECK(!memcmp(&var->current, &before, sizeof(before)));
            if (var->type == 3)
                CHECK(!memcmp(var->current.vector, vector, sizeof(vector)));
        }
    }
    const dvar_t *packets = Dvar_FindVar("cl_maxpackets");
    CHECK(packets && packets->domain.integer.min == 10);
#if defined(COD2_CODX) && COD2_CODX
    CHECK(packets && packets->reset.integer == 30 && packets->domain.integer.max == 125);
    CHECK(Dvar_FindVar("r_lodScale")->flags == 0x2001 && Dvar_FindVar("r_lodScale")->domain.value.min == 0);
#else
    CHECK(packets && packets->reset.integer == 30 && packets->domain.integer.max == 100);
    CHECK(Dvar_FindVar("r_lodScale")->flags == 0x2080 && Dvar_FindVar("r_lodScale")->domain.value.min == 1);
#endif
    Dvar_SetCommand("cl_maxpackets", "10");
    CHECK(packets && packets->current.integer == 10);
    const dvar_t *language = Dvar_FindVar("loc_language");
    CHECK(language && language->domain.integer.max == 14);
    const dvar_t *height = Dvar_FindVar("cl_profileTextHeight");
    CHECK(height && height->type == 5 && height->reset.integer == 16 && height->domain.integer.min == 1 && height->domain.integer.max == 100 && height->flags == 0x1000);
    const dvar_t *motd = Dvar_FindVar("motd");
    CHECK(motd && motd->type == 7 && !strcmp(motd->reset.string, "") && motd->flags == 0x1000);
    const char *owned[] = {
        "activeAction", "cl_anglespeedkey", "cl_avidemo", "cl_bypassMouseInput",
        "cl_connectTimeout", "cl_forceavidemo", "cl_freezeDemo", "cl_noprint",
        "cl_serverStatusResendTime", "cl_showSend", "cl_showServerCommands", "cl_showTimeDelta",
        "cl_showmouserate", "cl_shownet", "cl_shownuments", "cl_talking",
        "cl_timeout", "fx_cull", "fx_sort", "nextdemo"
    };
    for (unsigned i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i) {
        const dvar_t *var = Dvar_FindVar(owned[i]);
        CHECK(var && (var->flags & 0x1000));
    }
    Dvar_Shutdown();
    /* A previous engine registration without owner bits must yield its name
       to the client registration, retaining the existing typed values. */
    Dvar_Init();
    Dvar_RegisterBool("CL_NOPRINT", 1, 0);
    Dvar_RegisterInt("CL_SHOWNET", 1, -2, 4, 0);
    Dvar_RegisterFloat("CL_TIMEOUT", 100, 0, 3600, 0);
    Dvar_RegisterString("ACTIVEACTION", "fixture", 0);
    RegisterCorrections();
    CHECK(!strcmp(Dvar_FindVar("cl_noprint")->name, "cl_noprint"));
    CHECK(!strcmp(Dvar_FindVar("cl_shownet")->name, "cl_shownet"));
    CHECK(!strcmp(Dvar_FindVar("cl_timeout")->name, "cl_timeout"));
    CHECK(!strcmp(Dvar_FindVar("activeAction")->name, "activeAction"));
    CHECK(Dvar_FindVar("cl_noprint")->current.enabled == 1);
    CHECK(Dvar_FindVar("cl_shownet")->current.integer == 1);
    CHECK(Dvar_FindVar("cl_timeout")->current.value == 100);
    CHECK(!strcmp(Dvar_FindVar("activeAction")->current.string, "fixture"));
    Dvar_Shutdown();
    /* Config text can create archived strings before engine registration. */
    Dvar_Init();
    Dvar_SetCommand("cl_maxpackets", "10");
    Dvar_SetCommand("loc_language", "14");
    Dvar_SetCommand("r_lodBias", "-1");
    Dvar_AddFlags(Dvar_FindVar("r_lodBias"), 1);
    Dvar_SetCommand("r_lightTweakAmbient", "2");
    RegisterCorrections();
    CHECK(Dvar_FindVar("cl_maxpackets")->current.integer == 10);
    CHECK(Dvar_FindVar("loc_language")->current.integer == 14);
    CHECK(Dvar_FindVar("r_lodBias")->current.value == 0 && (Dvar_FindVar("r_lodBias")->flags & 0x80));
    CHECK(Dvar_FindVar("r_lightTweakAmbient")->current.value == 0.1f);
    Dvar_Shutdown();
    persistence();
    FINISH("registrations");
}
