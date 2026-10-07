/* Synthetic admission feeds the real resolver, compiler emitter and VM call. */
#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

int rand(void) { static int serial=1; return serial++ & 0x7fff; }
void FS_FCloseFile(fileHandle_t f) { (void)f; }
void Z_FreeInternal(void *p) { free(p); }
Bool SV_Loaded(void) { return 0; }
void ClientDisconnect(int n) { (void)n; }
void SV_SetUserinfo(int n, const char *s) { (void)n; (void)s; }
void Z_VirtualFreeInternal(void *p);
void SV_SendServerCommand(client_t *c,svscmd_type t,const char *s,...) { (void)c; (void)t; (void)s; }
static const char *gameDenial;
static int readDuringConnect;
void WS76_ConnectScript(int n);

#include "host_fixture.h"

static int requestedMaxClients;
void Z_VirtualFreeInternal(void *p) { if(p!=clients) free(p); }
void *Z_VirtualAllocInternal(int n) { return calloc(1,(size_t)n); }
void *Hunk_AllocateTempMemoryInternal(int n) { return calloc(1,(size_t)n); }
void Hunk_FreeTempMemory(void *p) { free(p); }
const dvar_t *Dvar_RegisterInt(const char *s,int v,int lo,int hi,int f) { (void)v; (void)lo; (void)hi; (void)f; assert(!strcmp(s,"sv_maxclients")); settings[0].current.integer=requestedMaxClients; return &settings[0]; }
void Dvar_ClearModified(const dvar_t *d) { (void)d; }
void Dvar_SetInt(const dvar_t *d,int v) { ((dvar_t *)d)->current.integer=v; }

server_t sv;
void *imp_sv=&sv;
static dvar_t running;
const dvar_t *com_sv_running=&running;
static int errorEntered;
void *imp_com_errorEntered=&errorEntered;
void SV_SendClientSnapshot(client_t *c) { (void)c; }
void SV_RemoveOperatorCommands(void) {}
void SV_MasterShutdown(void) {}
void SV_ShutdownGameProgs(void) {}
void Com_Memset(void *p,int v,int n) { memset(p,v,(size_t)n); }
void FX_FreeSystem(void) {}
void Dvar_SetBool(dvar_t *d,int v) { d->current.enabled=(Bool)v; }
void Cod2x_ServerShutdown(void) { revision=0; }
unsigned char CL_GetLocalClientActive(int n) { (void)n; return 0; }
void CL_Disconnect(void) {}
void Com_AbortDObj(void) {}
void DObjAbort(void) {}
void XAnimAbort(void) {}
void Scr_Abort(void) {}

gentity_t g_entities[1024];
void *imp_g_entities = g_entities;
static gclient_t game_clients[2];
level_locals_t level;
scr_const_t scr_const;
scrCompilePub_t scrCompilePub;
scrVarPub_t scrVarPub;
scrVmPub_t scrVmPub;
static scrCompileGlob_t compileGlob;
void *imp_scrCompileGlob = &compileGlob;
void *imp_scrCompilePub = &scrCompilePub;
void *const imp_scrVarPub = &scrVarPub;
unsigned char scrMemTreeGlob[0x80330] __attribute__((aligned(16)));
static dvar_t bobMax;
const dvar_t *bg_bobMax = &bobMax;
static BuiltinMethodDef methods[59];
static jmp_buf errorJump;
static int catchError, objectErrors, argumentErrors;
static unsigned int removedValueCount;
static char vmStrings[64][1024];
static unsigned int vmStringCount;
static unsigned int entityClass;
static unsigned int overrideEntityNumber=UINT32_MAX;
static unsigned char program[128] __attribute__((aligned(16)));
static size_t programSize;

BuiltinMethod ScriptEnt_GetMethod(const char **n) { (void)n; return NULL; }
BuiltinMethod HudElem_GetMethod(const char **n) { (void)n; return NULL; }
void Scr_ObjectError(const char *s) { (void)s; ++objectErrors; assert(catchError); longjmp(errorJump, 1); }
void Scr_Error(const char *s) { (void)s; ++argumentErrors; assert(catchError); longjmp(errorJump, 1); }
void Scr_AddBool(int);
void Scr_AddString(const char *);
void Scr_AddVector(const float *);
unsigned int Scr_GetNumParam(void);
void G_GetPlayerViewOrigin(const gentity_t *, vec_t *);

#if !WS76_IDENTITY_IMPLEMENTED && defined(COD2_CODX) && COD2_CODX
const char *SV_Cod2xClientHwid(int n) { (void)n; return ""; }
const char *SV_Cod2xClientDigest(int n) { (void)n; return ""; }
const char *SV_Cod2xClientAuthorization(int n) { (void)n; return ""; }
void SV_Cod2xClearIdentities(void) {}
#endif

#include "identity_shutdown_source.h"

char *TempMalloc(int n) { char *p=(char *)program+programSize; programSize+=(size_t)n; assert(programSize<=sizeof(program)); return p; }
char *TempMallocAlign(int n) { return TempMalloc(n); }
void TempMemorySetPos(char *p) { programSize=(size_t)(p-(char *)program); }
void AddOpcodePos(unsigned int p, int t) { (void)p; (void)t; }
void RemoveOpcodePos(void) {}
static unsigned int EmitValue(VariableCompileValue *v) { (void)v; abort(); }
void CompileError(unsigned int p, const char *s, ...) { (void)p; (void)s; abort(); }
void RemoveRefToValue(int type, VariableUnion u) { (void)type; (void)u; ++removedValueCount; }
void RemoveRefToObject(unsigned int id) { (void)id; }
int GetVarType(unsigned int id) { assert(id==1 || id==2); return 21; }
unsigned int Scr_GetEntityIdRef(unsigned int id) { return (entityClass<<16) | (overrideEntityNumber==UINT32_MAX ? id-1 : overrideEntityNumber); }
unsigned int SL_GetString(const char *s, unsigned int user) { (void)user; assert(vmStringCount<64); snprintf(vmStrings[vmStringCount],1024,"%s",s); return vmStringCount++; }
const float *Scr_AllocVector(const float *v) { float *p=(float *)(scrMemTreeGlob+16); memcpy(p,v,3*sizeof(float)); return p; }
float BG_GetBobCycle(const playerState_t *p) { (void)p; return 0.5f; }
float BG_GetSpeed(const playerState_t *p, int t) { (void)p; (void)t; return 20; }
float BG_GetVerticalBobFactor(const playerState_t *p,float c,float s,float m) { (void)p; (void)c; (void)s; (void)m; return 2; }
float BG_GetHorizontalBobFactor(const playerState_t *p,float c,float s,float m) { (void)p; (void)c; (void)s; (void)m; return 3; }
void AngleVectors(const float *a,float *f,float *r,float *u) { (void)a; (void)f; (void)u; r[0]=0; r[1]=1; r[2]=0; }
void AddLeanToPosition(float *p,float yaw,float lean,float right,float up) { (void)yaw; (void)right; (void)up; p[0]+=lean; }
int G_DObjGetWorldTagPos(gentity_t *e,unsigned short tag,float *p) { assert(e==&g_entities[9]); (void)tag; p[0]=7; p[1]=8; p[2]=9; return 1; }

#include "script_identity_boundary.h"
#include "player_identity_source.h"

static void init_game(void)
{
    memset(g_entities,0,sizeof(g_entities)); memset(game_clients,0,sizeof(game_clients));
    for(int i=0;i<2;++i) { g_entities[i].client=&game_clients[i]; g_entities[i].s.number=i; }
    level.clients=game_clients;
}

static VariableValue invoke(const char *name, int slot, unsigned int params)
{
    int type=77;
    const char *canonical=name;
    BuiltinMethod method=Scr_GetMethod(&canonical,&type);
    VariableValue empty={0}, values[8]={{0}};
    if (!method) { Verify(0,name); return empty; }
    Verify(type==0,"extension method is a regular builtin");
    memset(&scrCompilePub,0,sizeof(scrCompilePub)); memset(&compileGlob,0,sizeof(compileGlob));
    memset(&scrVmPub,0,sizeof(scrVmPub)); program[0]=0x0f; programSize=1; vmStringCount=0; /* preceding GetSelf object opcode */
    removedValueCount=0;
    for(unsigned int i=0;i<params;++i) {
        values[i+1].type=6; values[i+1].u.intValue=100+(int)i;
    }
    if(params>=3) {
        values[2].type=5; values[2].u.floatValue=12.5f;
        values[3].type=2; values[3].u.stringValue=SL_GetString("ignored argument",0);
    }
    EmitBuiltinMethodOpcode((int)params,17,(intptr_t)method);
    Verify(program[1]==0x45+params,"real compiler emits the builtin method opcode");
    Verify(scrCompilePub.func_table_size==1 && scrCompilePub.func_table[0]==(intptr_t)method,"compiler retains resolved full method pointer");
    VariableValue *top=&values[params+1];
    top->type=1; top->u.pointerValue=(unsigned int)slot+1;
    scrVmPub.top=top; scrVmPub.maxstack=values+7;
    const char *pos=(const char *)program+2;
    VM_CandidateCallBuiltinMethod(&pos,&top,params);
    Verify(pos==(const char *)program+4,"real VM consumes emitted method operand");
    Verify(scrVmPub.outparamcount==0 && scrVmPub.inparamcount==0,"real VM clears method parameters");
    Verify(top==values+1 && scrVmPub.top==top,"real VM replaces arguments with one return value");
    Verify(removedValueCount==params,"real VM releases every ignored argument value");
    return *top;
}

static void string_value(const char *method,int slot,const char *want)
{
    VariableValue v=invoke(method,slot,0);
    if(v.type!=2) { Verify(0,"GSC method produces VM string"); return; }
    Verify(!strcmp(vmStrings[v.u.stringValue],want),method);
}

void WS76_ConnectScript(int n)
{
#if defined(COD2_CODX) && COD2_CODX
    if(readDuringConnect) {
        string_value("getHWID",n,hwid);
        string_value("getCDKeyHash",n,digest);
        string_value("getAuthorizationStatus",n,"KEY_IS_GOOD");
    }
#else
    (void)n;
#endif
}

static void connect_remote(netadr_t address,const char *id,const char *hash,const char *status,const char *reason)
{
    argc=3; argv[0]="getchallenge"; argv[1]="0"; argv[2]=hash;
    SV_GetChallenge(address);
    int challenge=0;
    for(int i=0;i<1024;++i) if(NET_CompareAdr(svs.challenges[i].adr,address)) { challenge=svs.challenges[i].challenge; break; }
    if(status) {
        char token[32]; snprintf(token,sizeof(token),"%d",challenge);
        argc=5; argv[0]="ipAuthorize"; argv[1]=token; argv[2]=status; argv[3]=reason; argv[4]="123";
        SV_AuthorizeIpPacket(svs.authorizeAddress);
    }
    char info[512]; snprintf(info,sizeof(info),"\\protocol\\118\\protocol_cod2x\\6\\cl_hwid2\\%s\\challenge\\%d\\qport\\20\\name\\fixture",id,challenge);
    argc=2; argv[0]="connect"; argv[1]=info; SV_DirectConnect(address);
}

static void identity_cases(void)
{
    reset(); init_game(); revision=6; entityClass=0;
    SV_Cod2xClearIdentities();
    connect_remote(remote,hwid,digest,"accept","KEY_IS_GOOD");
    string_value("getIp",0,"8.7.6.5"); string_value("getHWID",0,hwid);
    string_value("getCDKeyHash",0,digest); string_value("getAuthorizationStatus",0,"KEY_IS_GOOD");
    netadr_t second={NA_IP,{192,168,1,2},101};
    connect_remote(second,"ZYXWVUTSRQPONMLKJIHGFEDCBA987654","FFEEDDCCBBAA99887766554433221100",NULL,NULL);
    string_value("getHWID",1,"ZYXWVUTSRQPONMLKJIHGFEDCBA987654");
    string_value("getCDKeyHash",1,"FFEEDDCCBBAA99887766554433221100");
    string_value("getAuthorizationStatus",1,"");
    /* Mutable userinfo must not replace the admitted hardware identity. */
    snprintf(clients[0].userinfo,sizeof(clients[0].userinfo),"\\cl_hwid2\\AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");
    SV_UserinfoChanged(&clients[0]); string_value("GETHWID",0,hwid);
    SV_FreeClientScriptPers(); string_value("getAuthorizationStatus",0,"KEY_IS_GOOD");
    svs.time+=6000;
    connect_remote(remote,hwid,digest,"accept","KEY_IS_GOOD");
    string_value("getCDKeyHash",0,digest); string_value("getAuthorizationStatus",0,"KEY_IS_GOOD");
    SV_FreeClient(&clients[0]);
    string_value("getHWID",0,""); string_value("getCDKeyHash",0,""); string_value("getAuthorizationStatus",0,"");
    clients[0].state=0;
    netadr_t replacement={NA_IP,{8,8,8,8},102};
    connect_remote(replacement,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",NULL,NULL,NULL);
    string_value("getAuthorizationStatus",0,""); string_value("getCDKeyHash",0,"");
    SV_FreeClients();
    string_value("getHWID",1,"");
    SV_Cod2xClearIdentities();
    Verify(SV_Cod2xChallengeDigest(remote,clients[0].challenge)==NULL,"shutdown clears challenge digest");
    string_value("getAuthorizationStatus",0,"");
}

static void provenance_cases(void)
{
    reset(); init_game(); revision=6; entityClass=0; SV_Cod2xClearIdentities();
    connect_remote(remote,hwid,digest,"deny","INVALID_CDKEY");
    Verify(connects==0,"authorization denial prevents client metadata admission");
    string_value("getAuthorizationStatus",0,""); string_value("getCDKeyHash",0,"");
    reset(); init_game(); revision=6; SV_Cod2xClearIdentities();
    argc=3; argv[0]="getchallenge"; argv[1]="0"; argv[2]=digest; SV_GetChallenge(remote);
    svs.time+=6001;
    connect_remote(remote,hwid,digest,NULL,NULL);
    string_value("getCDKeyHash",0,digest); string_value("getAuthorizationStatus",0,"");
    int clientChallenge=clients[0].challenge;
    char token[32]; snprintf(token,sizeof(token),"%d",clientChallenge);
    argc=5; argv[0]="ipAuthorize"; argv[1]=token; argv[2]="accept"; argv[3]="KEY_IS_GOOD"; argv[4]="123";
    netadr_t forged=svs.authorizeAddress; ++forged.ip[3]; SV_AuthorizeIpPacket(forged);
    string_value("getAuthorizationStatus",0,"");
    SV_AuthorizeIpPacket(svs.authorizeAddress);
    string_value("getAuthorizationStatus",0,"KEY_IS_GOOD"); string_value("getHWID",0,hwid);
    /* A reused challenge must not let a late reply cross to an old client. */
    for(int i=0;i<1024;++i) if(svs.challenges[i].challenge==clientChallenge) {
        ++svs.challenges[i].challenge;
        snprintf(token,sizeof(token),"%d",svs.challenges[i].challenge);
        break;
    }
    argv[3]="OTHER_REPLY"; SV_AuthorizeIpPacket(svs.authorizeAddress);
    string_value("getAuthorizationStatus",0,"KEY_IS_GOOD");
    running.current.enabled=1; SV_Shutdown("synthetic shutdown");
    Verify(!svs.clients && !running.current.enabled,"real shutdown frees host state");
    string_value("getHWID",0,""); string_value("getCDKeyHash",0,""); string_value("getAuthorizationStatus",0,"");
    reset(); init_game(); revision=6; SV_Cod2xClearIdentities();
    netadr_t local={NA_LOOPBACK,{127,0,0,1},100};
    connect_remote(local,hwid,digest,NULL,NULL);
    string_value("getIp",0,"127.0.0.1"); string_value("getAuthorizationStatus",0,"");
    string_value("getCDKeyHash",0,digest);
    SV_FreeClient(&clients[0]); clients[0].state=0;
    netadr_t bot={NA_BOT,{0,0,0,0},200};
    connect_remote(bot,"","",NULL,NULL);
    string_value("getIp",0,"0.0.0.0"); string_value("getHWID",0,"");
    string_value("getCDKeyHash",0,""); string_value("getAuthorizationStatus",0,"");
    Verify(!strcmp(SV_Cod2xClientHwid(-1),"") && !strcmp(SV_Cod2xClientAuthorization(64),""),"out-of-range metadata access is empty");
    reset(); init_game(); revision=6; SV_Cod2xClearIdentities();
    connect_remote(remote,hwid,"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",NULL,NULL);
    string_value("getCDKeyHash",0,"XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX");
    string_value("getAuthorizationStatus",0,"");
    Verify(SV_Cod2xChallengeDigest(remote,clients[0].challenge)==NULL,"untrusted supplied hash is visible locally and never forwarded");
    reset(); init_game(); revision=6; SV_Cod2xClearIdentities();
    connect_remote(remote,hwid,digest,"accept","KEY_IS_GOOD");
    requestedMaxClients=3; SV_ChangeMaxClients();
    Verify(svs.clients!=clients && settings[0].current.integer==3,"real maxclients change relocates client allocation");
    string_value("getHWID",0,hwid); string_value("getCDKeyHash",0,digest);
    string_value("getAuthorizationStatus",0,"KEY_IS_GOOD");
    SV_FreeClients(); svs.clients=clients; settings[0].current.integer=2;
    reset(); init_game(); revision=6; SV_Cod2xClearIdentities();
    gameDenial="synthetic mod rejection"; readDuringConnect=1;
    connect_remote(remote,hwid,digest,"accept","KEY_IS_GOOD");
    readDuringConnect=0; gameDenial=NULL;
    Verify(connects==1 && clients[0].state==0,"mod rejection leaves the client slot free");
    string_value("getHWID",0,""); string_value("getCDKeyHash",0,"");
    string_value("getAuthorizationStatus",0,"");
}

static void player_cases(void)
{
    init_game(); entityClass=0;
    game_clients[0].ps.pm_flags=0; string_value("getStance",0,"stand");
    game_clients[0].ps.pm_flags=1; string_value("getStance",0,"prone");
    game_clients[0].ps.pm_flags=3; string_value("getStance",0,"crouch");
    for(int flags=0;flags<=0x300;flags+=0x100) {
        g_entities[0].s.eFlags=flags;
        VariableValue v=invoke("isUsingTurret",0,0);
        Verify(v.type==6 && v.u.intValue==(flags!=0),"VM turret boolean uses authoritative entity flags");
    }
    game_clients[0].ps.origin[0]=10; game_clients[0].ps.origin[1]=20; game_clients[0].ps.origin[2]=30;
    game_clients[0].ps.viewHeightCurrent=40; game_clients[0].ps.leanf=4;
    VariableValue v=invoke("getViewOrigin",0,0);
    if(v.type==4) { const float *p=SCR_VEC_PTR(v.u); Verify(p[0]==14 && p[1]==23 && p[2]==72,"view VM vector includes real height/bob/lean routine"); }
    else Verify(0,"view VM vector type");
    game_clients[0].ps.eFlags=0x100; game_clients[0].ps.viewlocked_entNum=9;
    v=invoke("getViewOrigin",0,0);
    if(v.type==4) { const float *p=SCR_VEC_PTR(v.u); Verify(p[0]==7 && p[1]==8 && p[2]==9,"real view-origin turret tag branch"); }
    const char *n="GETAUTHORIZATIONSTATUS"; int dev=99;
    if(Scr_GetMethod(&n,&dev)) Verify(!strcmp(n,"getAuthorizationStatus"),"case-insensitive lookup canonicalizes extension name");
    const char *legacy="getguid"; Verify(Scr_GetMethod(&legacy,&dev)!=NULL,"original player resolver remains available");
    const char *unknown="notAPlayerMethod"; Verify(Scr_GetMethod(&unknown,&dev)==NULL,"unknown method remains unresolved");
    methods[0]=(BuiltinMethodDef){"getStance",(BuiltinMethod)PlayerCmd_GetGuid,5};
    n="getStance";
    Verify(Scr_GetMethod(&n,&dev)==(BuiltinMethod)PlayerCmd_GetGuid && dev==5,"original general resolver has precedence over extension lookup");
    memset(methods,0,sizeof(methods));
    static const char *names[]={"getIp","getHWID","getCDKeyHash","getAuthorizationStatus","getViewOrigin","getStance","isUsingTurret"};
    for(unsigned int i=0;i<7;++i) {
        const char *name=names[i]; if(!Scr_GetMethod(&name,&dev)) continue;
        catchError=1; objectErrors=argumentErrors=0;
        entityClass=1;
        if(!setjmp(errorJump)) { invoke(names[i],0,0); Verify(0,"nonentity must reject"); }
        Verify(objectErrors==1,"nonentity uses player object error boundary");
        entityClass=0; overrideEntityNumber=65535; objectErrors=0;
        if(!setjmp(errorJump)) { invoke(names[i],0,0); Verify(0,"out-of-range entity must reject"); }
        Verify(objectErrors==1,"out-of-range entity is rejected before player array access");
        overrideEntityNumber=UINT32_MAX;
        entityClass=0; g_entities[0].client=NULL; objectErrors=0;
        if(!setjmp(errorJump)) { invoke(names[i],0,0); Verify(0,"nonplayer must reject"); }
        Verify(objectErrors==1,"nonplayer uses player object error boundary");
        g_entities[0].client=&game_clients[0];
        catchError=0;
    }
}

#if defined(COD2_CODX) && COD2_CODX
static void correlation_cases(void)
{
#if WS76_IDENTITY_IMPLEMENTED
    reset(); init_game(); revision=6; entityClass=0; SV_Cod2xClearIdentities();
    argc=3; argv[0]="getchallenge"; argv[1]="0"; argv[2]=digest; SV_GetChallenge(remote);
    int slot=-1;
    for(int i=0;i<1024;++i) if(NET_CompareAdr(svs.challenges[i].adr,remote)) { slot=i; break; }
    Verify(slot>=0,"digest retry has a real challenge");
    if(slot<0) return;
    int challenge=svs.challenges[slot].challenge;
    char token[32]; snprintf(token,sizeof(token),"%d",challenge);
    argc=5; argv[0]="ipAuthorize"; argv[1]=token; argv[2]="accept"; argv[3]="FIRST_REPLY"; argv[4]="123";
    SV_AuthorizeIpPacket(svs.authorizeAddress);
    Verify(!strcmp(cod2xChallengeIdentities[slot].authorization,"FIRST_REPLY"),"actual first challenge reply is recorded");
    const char *replacement="FFEEDDCCBBAA99887766554433221100";
    argc=3; argv[0]="getchallenge"; argv[1]="0"; argv[2]=replacement; SV_GetChallenge(remote);
    Verify(svs.challenges[slot].challenge==challenge,"unconnected digest retry reuses the challenge value");
    Verify(!cod2xChallengeIdentities[slot].authorization[0],"changed supplied token clears the earlier stored reason");
    Verify(!strcmp(SV_Cod2xChallengeDigest(remote,challenge),replacement),"retry forwards the new strictly validated digest");
    /* The wire reply carries the challenge only; it cannot identify an earlier request's digest. */
    argc=5; argv[0]="ipAuthorize"; argv[1]=token; argv[2]="accept"; argv[3]="EARLIER_REQUEST_REPLY"; argv[4]="123";
    SV_AuthorizeIpPacket(svs.authorizeAddress);
    connect_remote(remote,hwid,replacement,NULL,NULL);
    string_value("getCDKeyHash",0,replacement);
    string_value("getAuthorizationStatus",0,"EARLIER_REQUEST_REPLY");
#else
    Verify(0,"challenge metadata exposes the actual reply correlation limit");
#endif
}
#endif

static void argument_cases(void)
{
    reset(); init_game(); revision=6; entityClass=0; SV_Cod2xClearIdentities();
    connect_remote(remote,hwid,digest,"accept","KEY_IS_GOOD");
    game_clients[0].ps.pm_flags=3; g_entities[0].s.eFlags=0x100;
    game_clients[0].ps.origin[0]=10; game_clients[0].ps.origin[1]=20; game_clients[0].ps.origin[2]=30;
    game_clients[0].ps.viewHeightCurrent=40; game_clients[0].ps.leanf=4;
    static const char *names[]={"getIp","getHWID","getCDKeyHash","getAuthorizationStatus","getViewOrigin","getStance","isUsingTurret"};
    const char *strings[]={"8.7.6.5",hwid,digest,"KEY_IS_GOOD",NULL,"crouch",NULL};
    for(unsigned int i=0;i<7;++i) {
        catchError=1; objectErrors=argumentErrors=0;
        if(setjmp(errorJump)) {
            Verify(0,"reference accessors ignore extra arguments");
        } else {
            VariableValue v=invoke(names[i],0,3);
            if(i==4) {
                Verify(v.type==4,"extra arguments preserve view-origin VM vector type");
                if(v.type==4) { const float *p=SCR_VEC_PTR(v.u); Verify(p[0]==14 && p[1]==23 && p[2]==72,"extra arguments preserve real view origin"); }
            } else if(i==6) {
                Verify(v.type==6 && v.u.intValue==1,"extra arguments preserve turret VM boolean");
            } else {
                Verify(v.type==2,"extra arguments preserve accessor VM string type");
                if(v.type==2) Verify(!strcmp(vmStrings[v.u.stringValue],strings[i]),names[i]);
            }
            Verify(!objectErrors && !argumentErrors,"extra arguments produce no script error");
        }
        catchError=0;
    }
}

int main(void)
{
    WS69_protocol_fixture_main();
#if defined(COD2_CODX) && COD2_CODX
    identity_cases(); provenance_cases(); player_cases(); correlation_cases(); argument_cases();
#else
    static const char *names[]={"getIp","getHWID","getCDKeyHash","getAuthorizationStatus","getViewOrigin","getStance","isUsingTurret"};
    for(int i=0;i<7;++i) { const char *n=names[i]; int dev; Verify(!Scr_GetMethod(&n,&dev),"stock has no extension methods"); }
#endif
    printf("WS76 script identity: %d failures\n",failures);
    return failures!=0;
}
