/* Mac frame contracts through complete production frame and dispatch bodies. */
#include "common_types.h"
#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static level_locals_t level;
static struct bgs_t level_bgs;
static struct bgs_t *bgs;
static void *imp_bgs = &bgs;
static scr_const_t scr_const;
static gentity_t g_entities[1024];
static gclient_t clients[4];
static entityHandler_t entityHandlers[20];
static serverStatic_t svs;
static dvar_t oldVoting, listEntity, dumpAnims, broadcastLimit;
static const dvar_t *g_oldVoting = &oldVoting, *g_listEntity = &listEntity;
static const dvar_t *g_dumpAnims = &dumpAnims, *sv_playerBroadcastLimit = &broadcastLimit;
#define SCR_CONST() (&scr_const)
static int failures, calls[256], callCount, threads, notified[256], notifyCount;
static int initCount, hudCount, endCount, scoreCount, dumpCount, thinkCount;
static int animationRemaining, animationDisable, pendingOther, errorExpected;
static char printed[16384];
static tagInfo_t tag;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); ++failures; } } while (0)
static void record(int n) { assert(callCount < 256); calls[callCount++] = n; }
void Com_Memcpy(void *d, const void *s, int n) { memcpy(d, s, n); }
void Com_Error(int code, const char *fmt, ...)
{
    CHECK(code == 1 && !strcmp(fmt, "\x15NULL ent->think"));
    exit(errorExpected && !failures ? 0 : 3);
}
void Com_Printf(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    size_t len = strlen(printed);
    vsnprintf(printed + len, sizeof(printed) - len, fmt, ap);
    va_end(ap);
}
void SV_DObjInitServerTime(gentity_t *ent, float dt)
{
    CHECK(ent->r.inuse && bgs == &level_bgs && fabsf(dt - 0.05f) < 0.00001f);
    ++initCount;
}
int G_DObjUpdateServerTime(gentity_t *ent, int notify)
{
    CHECK(ent->r.inuse && !(ent->flags & 0x2000) && notify == 1);
    if (animationRemaining > 0) { --animationRemaining; record(30); return 1; }
    return 0;
}
void Scr_RunCurrentThreads(void)
{
    ++threads;
    if (animationDisable && callCount && calls[callCount - 1] == 30) g_entities[0].flags |= 0x2000;
}
void Scr_IncTime(void) { record(31); }
void Scr_AddEntity(gentity_t *ent) { pendingOther = (int)(ent - g_entities); }
void Scr_Notify(gentity_t *ent, unsigned short event, unsigned int count)
{
    CHECK(event == 42 && count == 1 && pendingOther == 3);
    notified[notifyCount++] = (int)(ent - g_entities);
}
void G_FreeEntity(gentity_t *ent) { record(10); ent->r.inuse = 0; }
void SV_UnlinkEntity(gentity_t *ent) { CHECK(!ent->unlinkAfterEvent); record(11); }
void G_RunMissile(gentity_t *ent) { (void)ent; record(12); }
void G_RunItem(gentity_t *ent) { (void)ent; record(13); }
void G_RunCorpse(gentity_t *ent) { (void)ent; record(14); }
void G_RunMover(gentity_t *ent) { (void)ent; record(15); }
void G_RunClient(gentity_t *ent) { (void)ent; record(16); }
void G_GeneralLink(gentity_t *ent) { (void)ent; record(17); }
tagInfo_t *COD2_TagInfoFromHandle(int handle) { if (!handle) return NULL; CHECK(handle == 1); return &tag; }
void HudElem_UpdateClient(gclient_t *client, int number, int which)
{
    CHECK(client == &clients[number] && which == 3); ++hudCount;
}
void ClientEndFrame(gentity_t *ent) { CHECK(ent->client == &clients[ent->s.number]); ++endCount; }
void CheckTeamStatus(void) { record(32); }
void CheckVote(void) { record(33); }
void DeathmatchScoreboardMessage(gentity_t *ent) { CHECK(ent == &g_entities[0]); ++scoreCount; }
const char *SL_ConvertToString(unsigned int id) { (void)id; return "fixture"; }
void Dvar_SetBool(const dvar_t *dvar, qboolean value) { CHECK(dvar == g_listEntity && !value); listEntity.current.enabled = value; }
void SaveRegisteredWeapons(void) { record(34); }
void SaveRegisteredItems(void) { record(35); }
void SV_DObjDisplayAnim(gentity_t *ent) { CHECK(ent == &level.gentities[dumpAnims.current.integer]); ++dumpCount; }
static void Think(gentity_t *ent) { CHECK(ent->nextthink == 0); ++thinkCount; record(18); }

#include "main_frame_source.h"

static void reset(void)
{
    memset(&level, 0, sizeof(level)); memset(&level_bgs, 0, sizeof(level_bgs));
    memset(g_entities, 0, sizeof(g_entities)); memset(clients, 0, sizeof(clients));
    memset(entityHandlers, 0, sizeof(entityHandlers)); memset(&tag, 0, sizeof(tag));
    memset(&oldVoting, 0, sizeof(oldVoting)); memset(&listEntity, 0, sizeof(listEntity));
    memset(&dumpAnims, 0, sizeof(dumpAnims)); memset(&broadcastLimit, 0, sizeof(broadcastLimit));
    level.gentities = g_entities; level.clients = clients; level.time = 1000;
    level.framenum = 9; level.currentEntityThink = -1; dumpAnims.current.integer = -1;
    scr_const.trigger = 42; bgs = NULL; svs.time = 5000;
    callCount = threads = notifyCount = initCount = hudCount = endCount = 0;
    scoreCount = dumpCount = thinkCount = animationRemaining = animationDisable = 0;
    printed[0] = 0;
    entityHandlers[1].think = (fn_think)Think;
    for (int i = 0; i < 4; ++i) { g_entities[i].s.number = i; g_entities[i].client = &clients[i]; }
}
static void frame(void) { (void)G_RunFrame(level.time + 50); }
static void objectives(void)
{
    reset(); level.maxclients = 3;
    for (int i = 0; i < 2; ++i) { g_entities[i].r.inuse = 1; clients[i].sess.cs.team = i + 1; }
    for (int i = 0; i < 16; ++i) {
        level.objectives[i].state = (objectiveState_t)(i == 0 ? 0 : 2);
        level.objectives[i].teamNum = i % 3;
        level.objectives[i].icon = 100 + i;
        level.objectives[i].origin[0] = 11 + i;
    }
    memset(clients[0].ps.objective, 0x5a, sizeof(clients[0].ps.objective));
    memset(clients[1].ps.objective, 0x5a, sizeof(clients[1].ps.objective));
    objective_t before[16]; memcpy(before, clients[0].ps.objective, sizeof(before));
    frame();
    for (int c = 0; c < 2; ++c) for (int i = 0; i < 16; ++i) {
        objective_t *src = &level.objectives[i], *dst = &clients[c].ps.objective[i];
        if (src->state && (!src->teamNum || src->teamNum == c + 1)) CHECK(!memcmp(src, dst, sizeof(*src)));
        else { CHECK(dst->state == 0); CHECK(!memcmp((char *)dst + 4, (char *)&before[i] + 4, sizeof(*dst) - 4)); }
    }
    CHECK(clients[2].ps.objective[1].state == 0 && hudCount == 2 && endCount == 2);
}
static void scoreboard(void)
{
    reset(); level.maxclients = 4; level.bUpdateScoresForIntermission = 1;
    clients[0].sess.connected = CON_CONNECTED; clients[0].ps.pm_type = 5;
    clients[1].sess.connected = CON_CONNECTING; clients[1].ps.pm_type = 5;
    clients[2].sess.connected = CON_CONNECTED; clients[2].ps.pm_type = 0;
    frame(); CHECK(scoreCount == 1 && level.bUpdateScoresForIntermission == 0);
    frame(); CHECK(scoreCount == 1);
    reset(); level.bUpdateScoresForIntermission = 1; frame(); CHECK(!level.bUpdateScoresForIntermission && !scoreCount);
    reset(); level.maxclients = 4; level.bUpdateScoresForIntermission = 1; frame(); CHECK(!level.bUpdateScoresForIntermission && !scoreCount);
}
static void diagnostics(void)
{
    reset(); frame(); CHECK(!printed[0] && !dumpCount);
    reset(); dumpAnims.current.integer = 2; frame(); CHECK(!strcmp(printed, "server:\n") && dumpCount == 1);
}
static void frame_contract(void)
{
    reset(); level.num_entities = 2; level.maxclients = 2;
    g_entities[0].r.inuse = 1; animationRemaining = 2; level.registerWeapons = level.bRegisterItems = 1;
    frame(); CHECK(level.framenum == 10 && level.previousTime == 1000 && level.time == 1050 && level.frametime == 50);
    CHECK(level_bgs.time == 1050 && level_bgs.latestSnapshotTime == 1050 && level_bgs.frametime == 50);
    CHECK(!bgs && level.currentEntityThink == -1 && initCount == 1 && hudCount == 1 && endCount == 1);
    CHECK(threads == 3 && callCount == 7);
    const int expected[] = {30, 30, 31, 16, 32, 34, 35};
    CHECK(!memcmp(calls, expected, sizeof(expected)));
    reset(); level.num_entities = 1; g_entities[0].r.inuse = 1; animationRemaining = 3; animationDisable = 1;
    frame(); CHECK(animationRemaining == 2 && threads == 2);
}
static void triggers(void)
{
    reset(); level.pendingTriggerListSize = 4;
    for (int i = 0; i < 4; ++i) {
        level.pendingTriggerList[i].entnum = (unsigned short)(i == 2 ? 1 : 0);
        level.pendingTriggerList[i].otherEntnum = 3;
        level.pendingTriggerList[i].useCount = 0;
        level.pendingTriggerList[i].otherUseCount = 0;
    }
    level.pendingTriggerList[3].otherUseCount = 1;
    frame(); CHECK(notifyCount == 3 && notified[0] == 0 && notified[1] == 1 && notified[2] == 0);
    CHECK(threads == 2 && !level.currentTriggerListSize && !level.pendingTriggerListSize);
}
static void dispatch(void)
{
    reset(); gentity_t *e = &g_entities[0]; e->client = NULL; e->r.inuse = 1;
    e->processedFrame = level.framenum; G_RunFrameForEntity(e); CHECK(!callCount);
    e->processedFrame = 0; e->flags = 0x800; e->nextthink = 2000; G_RunFrameForEntity(e); CHECK(e->s.eFlags & 0x20);
    e->processedFrame = 0; e->flags = 0; G_RunFrameForEntity(e); CHECK(!(e->s.eFlags & 0x20));
    e->processedFrame = 0; e->s.eFlags = 0x10000; e->s.time2 = 999; G_RunFrameForEntity(e); CHECK(callCount == 1 && calls[0] == 10);
    const int types[] = {4, 3, 2, 6}; const int expected[] = {12, 13, 14, 15};
    for (int i = 0; i < 4; ++i) { reset(); e->client = NULL; e->s.eType = types[i]; G_RunFrameForEntity(e); CHECK(callCount == 1 && calls[0] == expected[i]); }
    reset(); e->client = NULL; e->freeAfterEvent = 1; e->eventTime = 700; G_RunFrameForEntity(e); CHECK(!callCount);
    e->processedFrame = 0; e->eventTime = 699; G_RunFrameForEntity(e); CHECK(callCount == 1 && calls[0] == 10);
    reset(); e->client = NULL; e->unlinkAfterEvent = 1; e->eventTime = 699; G_RunFrameForEntity(e); CHECK(callCount == 1 && calls[0] == 11);
    reset(); e->client = NULL; e->tagInfo = 1; e->s.eType = 3; e->handler = 1; e->nextthink = 1000;
    G_RunFrameForEntity(e); CHECK(callCount == 2 && calls[0] == 17 && calls[1] == 18 && thinkCount == 1);
}
static void think(void)
{
    reset(); gentity_t *e = &g_entities[0]; e->handler = 1;
    e->nextthink = -1; G_RunThink(e); CHECK(e->nextthink == -1 && !thinkCount);
    e->nextthink = 1001; G_RunThink(e); CHECK(e->nextthink == 1001 && !thinkCount);
    e->nextthink = 1000; G_RunThink(e); CHECK(!e->nextthink && thinkCount == 1);
    G_RunThink(e); CHECK(thinkCount == 1);
    e->handler = 0; e->nextthink = 1; errorExpected = 1; G_RunThink(e); CHECK(0);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "objectives")) objectives();
    else if (!strcmp(argv[1], "scoreboard")) scoreboard();
    else if (!strcmp(argv[1], "diagnostics")) diagnostics();
    else if (!strcmp(argv[1], "frame")) frame_contract();
    else if (!strcmp(argv[1], "triggers")) triggers();
    else if (!strcmp(argv[1], "dispatch")) dispatch();
    else if (!strcmp(argv[1], "think")) think();
    else abort();
    return !!failures;
}
