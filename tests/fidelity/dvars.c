#include "dvar_services.h"
static int argc;
static char *args[5];
int Cmd_Argc(void) { return argc; }
char *Cmd_Argv(int index) { return index < argc ? args[index] : ""; }
#include "dvars_source.h"
int main(void)
{
    const char *commands[] = {"dvar_bool", "dvar_int", "dvar_float"};
    void (*handlers[])(void) = {Dvar_RegisterBool_f, Dvar_RegisterInt_f, Dvar_RegisterFloat_f};
    for (int kind = 0; kind < 3; ++kind) {
        Dvar_SetCommand("before", kind == 0 ? "0" : "3");
        args[0] = (char *)commands[kind]; args[1] = "before"; args[2] = "1"; args[3] = "0"; args[4] = "4";
        argc = kind == 0 ? 3 : 5;
        handlers[kind]();
        const dvar_t *var = Dvar_FindVar("before");
        CHECK(var && var->type == (kind == 0 ? 0 : kind == 1 ? 5 : 1));
        if (var && var->type != 7) {
            CHECK(kind == 0 ? var->current.enabled == 0 : kind == 1 ? var->current.integer == 3 : var->current.value == 3);
        }
        Dvar_Shutdown();
        Dvar_RegisterString("owned", "3", 0x1040);
        args[1] = "owned";
        handlers[kind]();
        CHECK(Dvar_FindVar("owned")->type == 7);
        CHECK(Dvar_FindVar("owned")->flags == 0x1040);
        /* The broken handler adds EXTERNAL to a borrowed name. Preserve the
         * failure above while keeping characterization cleanup valid. */
        ((dvar_t *)Dvar_FindVar("owned"))->flags = 0x1040;
        Dvar_Shutdown();
    }
    char longArg[3001], combined[4096]; memset(longArg, 'x', 3000); longArg[3000] = 0;
    argc = 4; args[2] = longArg; args[3] = "tail";
    Dvar_GetCombinedString_impl(combined, 2);
    CHECK(strlen(combined) == 3005 && !strcmp(combined + 3000, " tail"));
    char cap[4094]; memset(cap, 'y', sizeof(cap) - 1); cap[sizeof(cap) - 1] = 0;
    args[2] = cap;
    Dvar_GetCombinedString_impl(combined, 2);
    CHECK(strlen(combined) == 4094 && combined[4093] == ' ');
    FINISH("dvars");
}
