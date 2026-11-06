/* Synthetic contracts independently derived from the Mac 1.3 g_main module. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void CalculateRanks(void);
extern void CheckVote(void);
extern void ExitLevel(void);
level_locals_t level;
static gclient_t clients[8];
static dvar_t maxclients, abstain;
const dvar_t *g_maxclients = &maxclients;
const dvar_t *g_voteAbstainWeight = &abstain;
const dvar_t *g_banIPs;
static int commandCount, messageCount, configCount, queueCount, logCount;
static int queueAllowed, exitMode, sortCount;
static int (*rankCompare)(const void *, const void *);
static char command[128], message[128];

#if MAIN_VOTES_ALLOCATOR
extern void *Hunk_AllocXAnimServer(int size);
static char allocation[16];
static int expectedAllocationSize, allocationCalls;
static void *allocationResult;
void *Hunk_AllocLowInternal(int size)
{
    assert(size == expectedAllocationSize);
    ++allocationCalls;
    return allocationResult;
}
static void Allocator(void)
{
    Alloc_t callback = Hunk_AllocXAnimServer;
    int sizes[] = {0, 1, 4097};
    for (unsigned int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        expectedAllocationSize = sizes[i];
        allocationResult = allocation + i;
        assert(callback(expectedAllocationSize) == allocationResult);
        assert(allocationCalls == (int)i + 1);
    }
    expectedAllocationSize = 32;
    allocationResult = NULL;
    assert(callback(expectedAllocationSize) == NULL && allocationCalls == 4);
}
#endif

char *va(const char *format, ...)
{
    static char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    return buffer;
}
static void BeforeExitMutation(void)
{
    assert(level.teamScores[TEAM_AXIS] == 31);
    assert(level.teamScores[TEAM_ALLIES] == 32);
    assert(clients[0].sess.score == 70 && clients[0].sess.connected == CON_CONNECTED);
}
void Cbuf_ExecuteText(int mode, const char *text)
{
    assert(mode == 2);
    if (exitMode) {
        BeforeExitMutation();
        assert(!strcmp(text, "map_rotate\n"));
    } else {
        assert(level.voteExecuteTime == 0);
    }
    ++commandCount;
    snprintf(command, sizeof(command), "%s", text);
}
qboolean GScr_Cod2xQueueExitLevel(void)
{
    BeforeExitMutation();
    ++queueCount;
    return queueAllowed;
}
void SV_GameSendServerCommand(int client, svscmd_type type, const char *text)
{
    assert(client == -1 && type == 0);
    assert(level.voteTime != 0 && configCount == 0);
    ++messageCount;
    snprintf(message, sizeof(message), "%s", text);
}
void SV_SetConfigstring(const int index, const char *text)
{
    assert(index == 15 && !*text && level.voteTime == 0 && messageCount == 1);
    ++configCount;
}
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vsnprintf(dest, size, format, args);
    va_end(args);
    return result;
}
int FS_Write(const void *buffer, int length, fileHandle_t handle)
{
    assert(exitMode && handle == 5);
    assert(level.teamScores[TEAM_AXIS] == 0 && level.teamScores[TEAM_ALLIES] == 0);
    assert(clients[0].sess.score == 0 && clients[0].sess.connected == CON_CONNECTING);
    assert(!strcmp(buffer, "  0:01 ExitLevel: executed\n"));
    assert(length == (int)strlen(buffer));
    ++logCount;
    return length;
}
/* qsort is a seam so the exact production static comparator can be exercised. */
void qsort(void *base, size_t count, size_t size, int (*compare)(const void *, const void *))
{
    assert(base == level.sortedClients && count == (size_t)level.numConnectedClients);
    assert(size == sizeof(int) && level.bUpdateScoresForIntermission == 0);
    rankCompare = compare;
    ++sortCount;
}
static void Reset(void)
{
    memset(&level, 0, sizeof(level));
    memset(clients, 0, sizeof(clients));
    level.clients = clients;
    level.maxclients = 8;
    level.time = 1000;
    maxclients.current.integer = 4;
    abstain.current.value = 0.5f;
    commandCount = messageCount = configCount = queueCount = logCount = sortCount = 0;
    queueAllowed = 1;
    exitMode = 0;
    rankCompare = NULL;
    command[0] = message[0] = 0;
}
static void RankCounts(void)
{
    Reset();
    clients[0].sess.connected = CON_CONNECTED;
    clients[0].sess.cs.team = TEAM_AXIS;
    clients[1].sess.connected = CON_CONNECTING;
    clients[1].sess.cs.team = TEAM_ALLIES;
    clients[3].sess.connected = CON_CONNECTED;
    clients[3].sess.cs.team = TEAM_SPECTATOR;
    clients[5].sess.connected = CON_CONNECTED;
    clients[5].sess.cs.team = TEAM_FREE;
    level.numConnectedClients = level.numVotingClients = 99;
    CalculateRanks();
    assert(level.numConnectedClients == 4 && level.numVotingClients == 2);
    assert(level.sortedClients[0] == 0 && level.sortedClients[1] == 1);
    assert(level.sortedClients[2] == 3 && level.sortedClients[3] == 5);
    assert(sortCount == 1 && level.bUpdateScoresForIntermission == 1);
    Reset(); level.maxclients = 0;
    CalculateRanks();
    assert(level.numConnectedClients == 0 && level.numVotingClients == 0);
    assert(sortCount == 1 && level.bUpdateScoresForIntermission == 1);
}
static void Pair(int expectedAB, int expectedBA)
{
    int a = 0, b = 1;
    assert(rankCompare(&a, &b) == expectedAB);
    assert(rankCompare(&b, &a) == expectedBA);
}
static void RankComparator(void)
{
    Reset(); CalculateRanks();
    clients[0].sess.connected = clients[1].sess.connected = CON_CONNECTED;
    Pair(0, 0);
    clients[0].sess.score = 10; Pair(-1, 1);
    clients[1].sess.score = 10;
    clients[0].sess.deaths = 4; Pair(1, -1);
    clients[1].sess.deaths = 4; Pair(0, 0);
    clients[0].sess.cs.team = TEAM_SPECTATOR; Pair(1, -1);
    clients[1].sess.cs.team = TEAM_SPECTATOR; Pair(-1, 1);
    int a = 0;
    assert(rankCompare(&a, &a) == 0);
    clients[0].sess.connected = CON_CONNECTING; Pair(1, -1);
    clients[1].sess.connected = CON_CONNECTING; Pair(1, 1);
}
static void BeginVote(void)
{
    Reset();
    level.voteTime = 2000;
    level.numVotingClients = 4;
    strcpy(level.voteString, "synthetic_command");
}
static void Result(int pass)
{
    assert(messageCount == 1 && configCount == 1 && level.voteTime == 0);
    assert(!strcmp(message, pass ? "e \"GAME_VOTEPASSED\"" : "e \"GAME_VOTEFAILED\""));
    assert(level.voteExecuteTime == (pass ? (int)((unsigned)level.time + 3000u) : 0));
}
static void VoteExecute(void)
{
    BeginVote(); level.voteTime = 0; level.voteExecuteTime = 1000;
    CheckVote(); assert(commandCount == 0 && level.voteExecuteTime == 1000);
    level.time = 1001;
    CheckVote(); assert(commandCount == 1 && level.voteExecuteTime == 0);
    assert(!strcmp(command, "synthetic_command\n") && messageCount == 0 && configCount == 0);
    CheckVote(); assert(commandCount == 1);
    /* Execute uses signed relational JL, unlike the vote deadline's JS. */
    BeginVote(); level.voteTime = 0; level.voteExecuteTime = INT_MAX; level.time = INT_MIN;
    CheckVote(); assert(commandCount == 0);
}
static void VoteEarly(void)
{
    BeginVote(); level.voteYes = 2; level.voteNo = 1;
    CheckVote(); assert(messageCount == 0 && configCount == 0 && level.voteTime == 2000);
    level.voteYes = 3; CheckVote(); Result(1);
    BeginVote(); level.voteNo = 2; CheckVote(); Result(0);
    BeginVote(); level.numVotingClients = 5; level.voteNo = 2;
    CheckVote(); assert(messageCount == 0 && level.voteTime == 2000);
    level.voteNo = 3; CheckVote(); Result(0);
}
static void VoteExpiry(void)
{
    BeginVote(); level.time = 2000; level.numVotingClients = 8;
    level.voteYes = 3; level.voteNo = 1;
    CheckVote(); Result(0); /* Strict >: three yes versus one no + ceil(4/2). */
    BeginVote(); level.time = 2000; level.numVotingClients = 8;
    level.voteYes = 4; level.voteNo = 1;
    CheckVote(); Result(1);
    BeginVote(); level.time = 2001; level.voteYes = 2;
    abstain.current.value = 0.50001f;
    CheckVote(); Result(0); /* Ceil the float product before adding no votes. */
    BeginVote(); level.time = 2001; level.voteYes = 2;
    abstain.current.value = 0.5f;
    CheckVote(); Result(1);
}
static void VoteRolloverPending(void)
{
    BeginVote(); level.time = INT_MAX - 100; level.voteTime = INT_MIN + 100;
    CheckVote();
    assert(level.voteTime == INT_MIN + 100 && messageCount == 0 && configCount == 0);
}
static void VoteRolloverExpired(void)
{
    BeginVote(); level.time = INT_MIN + 100; level.voteTime = INT_MAX - 100;
    CheckVote(); Result(0);
}
static void VoteRolloverSchedule(int early)
{
    BeginVote(); level.time = INT_MAX - 100;
    level.voteTime = early ? INT_MAX - 50 : INT_MAX - 200;
    level.voteYes = 3;
    CheckVote(); Result(1);
}
static void ExitSetup(void)
{
    Reset(); exitMode = 1; level.logFile = 5;
    level.teamScores[TEAM_AXIS] = 31; level.teamScores[TEAM_ALLIES] = 32;
    level.teamScores[TEAM_FREE] = 33; level.teamScores[TEAM_SPECTATOR] = 34;
    for (int i = 0; i < 8; ++i) {
        clients[i].sess.score = 70 + i;
        clients[i].sess.deaths = 90 + i;
        clients[i].sess.connected = CON_CONNECTED;
    }
    clients[1].sess.connected = CON_CONNECTING;
    clients[2].sess.connected = CON_DISCONNECTED;
}
static void ExitComplete(void)
{
    ExitSetup(); ExitLevel();
    assert(logCount == 1);
    assert(level.teamScores[TEAM_FREE] == 33 && level.teamScores[TEAM_SPECTATOR] == 34);
    for (int i = 0; i < 8; ++i) {
        assert(clients[i].sess.score == ((i == 0 || i == 3) ? 0 : 70 + i));
        assert(clients[i].sess.deaths == 90 + i);
        assert(clients[i].sess.connected == (i == 2 ? CON_DISCONNECTED : i < 4 ? CON_CONNECTING : CON_CONNECTED));
    }
#if defined(COD2_CODX) && COD2_CODX
    assert(queueCount == 1 && commandCount == 0);
#else
    assert(queueCount == 0 && commandCount == 1);
#endif
}
static void ExitRejected(void)
{
#if defined(COD2_CODX) && COD2_CODX
    ExitSetup(); queueAllowed = 0; ExitLevel();
    BeforeExitMutation();
    assert(logCount == 0 && queueCount == 1 && commandCount == 0);
    assert(clients[3].sess.connected == CON_CONNECTED && clients[3].sess.score == 73);
#else
    ExitComplete();
#endif
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "rank-counts")) RankCounts();
    else if (!strcmp(argv[1], "rank-comparator")) RankComparator();
    else if (!strcmp(argv[1], "vote-execute")) VoteExecute();
    else if (!strcmp(argv[1], "vote-early")) VoteEarly();
    else if (!strcmp(argv[1], "vote-expiry")) VoteExpiry();
    else if (!strcmp(argv[1], "vote-rollover-pending")) VoteRolloverPending();
    else if (!strcmp(argv[1], "vote-rollover-expired")) VoteRolloverExpired();
    else if (!strcmp(argv[1], "vote-rollover-schedule-early")) VoteRolloverSchedule(1);
    else if (!strcmp(argv[1], "vote-rollover-schedule-expiry")) VoteRolloverSchedule(0);
    else if (!strcmp(argv[1], "exit-level")) ExitComplete();
    else if (!strcmp(argv[1], "exit-rejected")) ExitRejected();
#if MAIN_VOTES_ALLOCATOR
    else if (!strcmp(argv[1], "allocator")) Allocator();
#endif
    else abort();
    return 0;
}
