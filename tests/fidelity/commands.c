#include "common_types.h"
#include "check.h"
static int handlerCalls, quickMessageRegistered, popupCount;
static cg_t state;
static cg_t *cg = &state;
static const char *command;
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
const char *CG_Argv(int index) { (void)index; return command; }
void CL_AddCgameCommand(const char *name) { if (!strcmp(name, "mp_QuickMessage")) ++quickMessageRegistered; }
void CL_Popup(const char *name) { CHECK(!strcmp(name, "UIMENU_WM_QUICKMESSAGE")); ++popupCount; }
#include "commands_source.h"
int main(void)
{
    snapshot_t snap = {0}; cg->snap = &snap;
    CG_InitConsoleCommands();
    CHECK(quickMessageRegistered == 1 && CG_IsConsoleCommandName("MP_QUICKMESSAGE"));
    command = "mp_QuickMessage";
    CHECK(CG_ConsoleCommand() && popupCount == 0);
    cg->nextSnap = &snap;
    CHECK(CG_ConsoleCommand() && popupCount == 0);
    snap.ps.pm_flags = 0x800000;
    CHECK(CG_ConsoleCommand() && popupCount == 1 && handlerCalls == 0);
    CHECK(CG_IsConsoleCommandName("mr"));
    FINISH("commands");
}
