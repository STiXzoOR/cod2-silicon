/* Mac shutdown ordering and ownership contract through production C. */
#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static level_locals_t level;
static bgs_t level_bgs, *bgs = &level_bgs;
static gentity_t g_entities[1024];
static scr_data_t scriptData;
static void *imp_bgs = &bgs, *imp_g_scr_data = &scriptData;
static uintptr_t treeStorage[72][2], staleStorage[8][2];
static char scriptBuffer[8];
static int active, freeCase, events[256], values[256], count;
enum { LOG=1, CLOSE, ENTITY, HUD, ACTIVE, PERS, SHUTDOWN, MANTLE, GAME_SCRIPTS,
    SCRIPTS, TREE, HUNK, BUFFER };
static void Record(int event, int value) { assert(count < 256); events[count]=event; values[count++]=value; }
static void Com_Printf(const char *fmt, ...) { assert(!strcmp(fmt,"==== ShutdownGame ====\n")); }
static void G_LogPrintf(const char *fmt, ...)
{
    assert(!strcmp(fmt,"ShutdownGame:\n") || !strcmp(fmt,"------------------------------------------------------------\n")); Record(LOG,0);
}
static void FS_FCloseFile(int handle) { Record(CLOSE,handle); }
static void G_FreeEntity(gentity_t *ent)
{
    assert(!bgs && ent->r.inuse); Record(ENTITY,(int)(ent-g_entities)); ent->r.inuse=0;
}
static void HudElem_DestroyAll(void)
{
    assert(!level.num_entities && !level.firstFreeEnt && !level.lastFreeEnt); Record(HUD,0);
}
static int Scr_IsSystemActive(int inst) { assert(inst==1); Record(ACTIVE,0); return active; }
static void SV_FreeClientScriptPers(void) { assert(active && !level.savepersist); Record(PERS,0); }
static void Scr_ShutdownSystem(int inst, int freePers) { assert(inst==1 && freePers==!level.savepersist); Record(SHUTDOWN,freePers); }
static void Mantle_ShutdownAnims(void) { Record(MANTLE,0); }
static void GScr_FreeScripts(void) { Record(GAME_SCRIPTS,0); }
static void Scr_FreeScripts(int inst) { assert(inst==1); Record(SCRIPTS,0); }
static void XAnimFreeTree(XAnimTree *tree, Free_t release)
{
    assert(!release);
    int found=-1;
    for(int i=0;i<72;++i) if(tree==(XAnimTree *)treeStorage[i]) found=i;
    assert(found>=0 && found%3!=1);
    Record(TREE,found);
}
static void Hunk_ClearToMarkLow(int mark)
{
    assert(mark==0);
    for(int i=0;i<64;++i) assert(!level_bgs.clientinfo[i].pXAnimTree);
    for(int i=0;i<8;++i) {
        assert(!scriptData.playerCorpseInfo[i].tree);
        /* The corpse owner is distinct from the stale animation view. */
        assert(scriptData.playerCorpseInfo[i].ci.pXAnimTree==(XAnimTree *)staleStorage[i]);
    }
    Record(HUNK,0);
}
static void Z_FreeInternal(void *buffer) { assert(buffer==scriptBuffer); Record(BUFFER,0); }
#include "shutdown_body.inc"
int main(int argc, char **argv)
{
    assert(argc==2);
    active = strstr(argv[1],"active")!=NULL;
    freeCase = !strncmp(argv[1],"free",4);
    level.savepersist = strstr(argv[1],"save")!=NULL;
    int io = !strcmp(argv[1],"io");
    level.logFile=io ? 5 : 0;
    level.openScriptIOFileHandles[0]=io ? 0 : -1;
    level.openScriptIOFileBuffers[0]=io ? scriptBuffer : NULL;
    level.num_entities=5;
    level.firstFreeEnt=&g_entities[12]; level.lastFreeEnt=&g_entities[14];
    g_entities[0].r.inuse=1; g_entities[3].r.inuse=1; g_entities[1022].r.inuse=1;
    for(int i=0;i<64;++i) level_bgs.clientinfo[i].pXAnimTree=i%3==1 ? NULL : (XAnimTree *)treeStorage[i];
    for(int i=0;i<8;++i) { scriptData.playerCorpseInfo[i].tree=(i+64)%3==1 ? NULL : (XAnimTree *)treeStorage[64+i]; scriptData.playerCorpseInfo[i].ci.pXAnimTree=(XAnimTree *)staleStorage[i]; }
    G_ShutdownGame(freeCase);
    int expected[256], expectedValues[256], n=0;
#define EXPECT(e,v) do { expected[n]=(e); expectedValues[n++]=(v); } while(0)
    if(io) { EXPECT(LOG,0); EXPECT(LOG,0); EXPECT(CLOSE,5); }
    EXPECT(ENTITY,0); EXPECT(ENTITY,3); EXPECT(ENTITY,1022); EXPECT(HUD,0); EXPECT(ACTIVE,0);
    if(active&&!level.savepersist) EXPECT(PERS,0);
    EXPECT(SHUTDOWN,!level.savepersist);
    if(freeCase) {
        EXPECT(MANTLE,0); EXPECT(GAME_SCRIPTS,0); EXPECT(SCRIPTS,0);
        for(int i=0;i<72;++i) if(i%3!=1) EXPECT(TREE,i);
        EXPECT(HUNK,0);
    }
    if(io) {EXPECT(BUFFER,0); EXPECT(CLOSE,0);}
    assert(count==n);
    for(int i=0;i<n;++i) {assert(events[i]==expected[i]); assert(values[i]==expectedValues[i]);}
    assert(!bgs && !level.num_entities && !level.openScriptIOFileBuffers[0] && level.openScriptIOFileHandles[0]==-1);
    if(!freeCase) {
        for(int i=0;i<64;++i) assert(level_bgs.clientinfo[i].pXAnimTree==(i%3==1 ? NULL : (XAnimTree *)treeStorage[i]));
        for(int i=0;i<8;++i) assert(scriptData.playerCorpseInfo[i].tree==((i+64)%3==1 ? NULL : (XAnimTree *)treeStorage[64+i]));
    }
    for(int i=0;i<8;++i) assert(scriptData.playerCorpseInfo[i].ci.pXAnimTree==(XAnimTree *)staleStorage[i]);
    puts("game shutdown contract passes"); return 0;
}
