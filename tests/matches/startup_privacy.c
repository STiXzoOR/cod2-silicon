/* Match storage exercised after the actual engine startup dvar conversion. */
#include "../fidelity/dvar_services.h"
#include "cod2x_policy.h"
extern void Dvar_SetBoolFromSource(const dvar_t *, unsigned char, DvarSetSource);
extern void Dvar_SetFromStringByName(const char *, const char *);
extern void Dvar_SetString(const dvar_t *, const char *);
extern Bool Dvar_IsValidName(const char *);

static int cmd_argc;
static char *cmd_argv[512];
static char cmd_tokenized[8704];
int com_numConsoleLines;
char *com_consoleLines[32];
#include "startup_source.h"

#define MATCH_REAL_DVARS
#define main summaryFixtureMain
#include "summary.c"
#undef main

int main(int argc, char **argv)
{
    assert(argc == 3);
    int enabled = strcmp(argv[1], "off") != 0;
    char commandLine[64] = "";
    if (strcmp(argv[1], "default"))
        snprintf(commandLine, sizeof(commandLine), "+set cl_matchSummaries %d", enabled);
    home.current.string = argv[2];
    frameCap.current.integer = 333;
    legacy.cl_running = 1;
    Dvar_Init();
    StartupCommandLine(commandLine);
    Com_StartupVariable(NULL);
    if (strcmp(argv[1], "default")) {
        const dvar_t *startup = Dvar_FindVar("cl_matchSummaries");
        CHECK(startup && startup->type == 7 && (startup->flags & 0x4000));
        CHECK(startup && !strcmp(startup->current.string, enabled ? "1" : "0"));
    }
    MatchSummary_Init();
    const dvar_t *choice = Dvar_FindVar("cl_matchSummaries");
    CHECK(choice && choice->type == 0 && choice->current.enabled == enabled);
    CHECK(choice && (choice->flags & 0x40));
    /* Imported set commands and script/server changes cannot alter the choice. */
    Cmd_TokenizeString(enabled ? "set cl_matchSummaries 0" : "set cl_matchSummaries 1");
    Dvar_Set_f();
    Dvar_SetBoolFromSource(choice, !enabled, DVAR_SOURCE_SCRIPT);
    CHECK(choice->current.enabled == enabled);
    /* Even an internal engine update cannot enable collection after opt-out. */
    if (!enabled) Dvar_SetFromStringByName("cl_matchSummaries", "1");
    begin(); frame(3, 3, 333); CL_Disconnect();
    struct stat status;
    char stats[4096];
    snprintf(stats, sizeof(stats), "%s/main/stats", argv[2]);
    CHECK(enabled ? stat(stats, &status) == 0 : stat(stats, &status) != 0 && errno == ENOENT);
    Dvar_Shutdown();
    FINISH("startup privacy");
}
