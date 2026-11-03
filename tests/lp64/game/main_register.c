/* Registration metadata and dedicated override through production C. */
#include "common_types.h"
#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <string.h>

static dvar_t value;
static int count, dateCount, dedicatedCount, codxCount;
static const dvar_t *Dvar_RegisterString_mac(const char *name, const char *def, int flags)
{
    ++count;
    if (!strcmp(name,"gamedate")) {assert(!strcmp(def,"Aug 30 2006") && flags==0x1040); ++dateCount;}
    if (!strcmp(name,"gamename")) assert(!strcmp(def,"Call of Duty 2") && flags==0x1044);
    if (!strcmp(name,"g_gametype")) assert(!strcmp(def,"dm") && flags==0x1024);
    if (!strcmp(name,"sv_mapname")) assert(!*def && flags==0x1044);
    return &value;
}
static const dvar_t *Dvar_RegisterBool_mac(const char *name, int def, int flags)
{
    ++count;
    if (!strcmp(name,"sv_cheats")) assert(def==0 && flags==0x1000);
    if (!strcmp(name,"g_antilag")) assert(def==1 && flags==0x1005);
    if (!strcmp(name,"g_shotgun_spread_fix")) {assert(def==1 && flags==0x1080); ++codxCount;}
    return &value;
}
static const dvar_t *Dvar_RegisterInt(const char *name, int def, int min, int max, int flags)
{
    ++count;
    if (!strcmp(name,"dedicated")) {assert(def==0 && min==0 && max==2 && flags==(dedicatedCount ? 0x1040 : 0x1020)); ++dedicatedCount;}
    if (!strcmp(name,"sv_maxclients")) assert(def==20 && min==1 && max==64 && flags==0x1025);
    if (!strcmp(name,"g_speed")) assert(def==190 && min==(-2147483647-1) && max==2147483647 && flags==0x1000);
    if (!strcmp(name,"g_useholdspawndelay")) assert(def==1 && min==0 && max==10 && flags==0x1081);
    if (!strcmp(name,"sv_playerBroadcastLimit")) {assert(def==15 && min==0 && max==64 && flags==0x1000); ++codxCount;}
    return &value;
}
static const dvar_t *Dvar_RegisterFloat(const char *name, float def, float min, float max, int flags)
{
    ++count;
    if (!strcmp(name,"g_gravity")) assert(def==800 && min==1 && max==FLT_MAX && flags==0x1000);
    if (!strcmp(name,"g_knockback")) assert(def==1000 && min==-FLT_MAX && max==FLT_MAX && flags==0x1000);
    if (!strcmp(name,"g_voteAbstainWeight")) assert(def==.5f && min==0 && max==1 && flags==0x1001);
    return &value;
}
static const dvar_t *Dvar_RegisterColor(const char *name, float r, float g, float b, float a, int flags)
{
    ++count;
    assert(flags==0x1100 && g==.5f && a==1);
    if (!strcmp(name,"g_TeamColor_Allies")) assert(r==.5f && b==1);
    else {assert(!strcmp(name,"g_TeamColor_Axis")); assert(r==1 && b==.5f);}
    return &value;
}
#include "register_body.inc"
int main(int argc, char **argv)
{
    assert(argc==2);
    value.current.integer=!strcmp(argv[1],"dedicated");
    G_RegisterDvars_impl();
    assert(dateCount==1 && dedicatedCount==1+value.current.integer);
#if defined(COD2_CODX) && COD2_CODX
    assert(codxCount==2);
#else
    assert(codxCount==0);
#endif
    assert(count==56+value.current.integer+codxCount);
    puts("game dvar registration contract passes");
    return 0;
}
