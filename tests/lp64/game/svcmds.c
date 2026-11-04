/* Exercise Mac server console commands through production g_svcmds_mp entry points. */
#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern qboolean ConsoleCommand(void);
extern void Svcmd_EntityList_f(void);

level_locals_t level;
gentity_t g_entities[4];

static const char *arg0;
static int argc_value = 1;
static char printed[512], sent[256], executed[64];
static int execWhen, sendType;
static dvar_t dedicatedDvar, banDvar;
const dvar_t *g_dedicated = &dedicatedDvar;
const dvar_t *g_banIPs = &banDvar;

int SV_Cmd_Argc(void) { return argc_value; }
void SV_Cmd_ArgvBuffer(int arg, char *buffer, int length) { strncpy(buffer, arg ? "" : arg0, length); }
void Com_Printf(const char *format, ...)
{
    va_list args;
    size_t used = strlen(printed);
    va_start(args, format);
    vsnprintf(printed + used, sizeof(printed) - used, format, args);
    va_end(args);
}
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
const char *va(const char *format, ...)
{
    static char text[256];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return text;
}
char *ConcatArgs(int start) { static char text[] = "hello"; return text; }
void SV_GameSendServerCommand(int client, svscmd_type type, const char *text)
{
    assert(client == -1);
    sendType = type;
    strncpy(sent, text, sizeof(sent) - 1);
}
void Cbuf_ExecuteText(int when, const char *text)
{
    execWhen = when;
    strncpy(executed, text, sizeof(executed) - 1);
}
const char *SL_ConvertToString(unsigned int value) { return "name"; }

/* Mac 0x1cb589 and 0x1cb673: listip prints g_banIPs through the command buffer; maprotate is not
   a game console command. */
static void list_ip(void)
{
    arg0 = "LISTIP";
    assert(ConsoleCommand() == 1);
    assert(execWhen == 1 && !strcmp(executed, "g_banIPs\n"));
    arg0 = "maprotate";
    executed[0] = 0;
    assert(ConsoleCommand() == 0 && !executed[0]);
}

/* Mac 0x1cb5a1 tests g_dedicated, and 0x2e27c0 tags the text as server chat. */
static void say(void)
{
    arg0 = "say";
    assert(ConsoleCommand() == 0 && !sent[0]);
    dedicatedDvar.current.integer = 1;
    assert(ConsoleCommand() == 1);
    assert(!strcmp(sent, "e \"GAME_SERVER\x15: hello\"") && sendType == SV_CMD_CAN_IGNORE);
}

/* Mac 0x2e2640 and 0x2e2770. */
static void usage(void)
{
    arg0 = "addip";
    assert(ConsoleCommand() == 1);
    assert(!strcmp(printed, "Usage:  addip <ip-mask>\n"));
    printed[0] = 0;
    arg0 = "removeip";
    assert(ConsoleCommand() == 1);
    assert(!strcmp(printed, "Usage:  sv removeip <ip-mask>\n"));
}

/* Mac table 0x33c0a0 has no row for type 2, so a corpse takes the numeric default. */
static void entity_list(void)
{
    level.num_entities = 4;
    g_entities[1].r.inuse = 1;
    g_entities[1].s.eType = 0;
    g_entities[2].r.inuse = 1;
    g_entities[2].s.eType = 2;
    Svcmd_EntityList_f();
    assert(!strcmp(printed, "  1:ET_GENERAL          \n  2:  2                 \n"));
}

int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    if (!strcmp(argv[1], "listip")) list_ip();
    else if (!strcmp(argv[1], "say")) say();
    else if (!strcmp(argv[1], "usage")) usage();
    else if (!strcmp(argv[1], "entitylist")) entity_list();
    else return 2;
    printf("svcmds %s: pass\n", argv[1]);
    return 0;
}
