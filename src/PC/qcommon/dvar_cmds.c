#include "common_types.h"
#include "imports.h"

extern int Cmd_Argc(void);
extern const char *Cmd_Argv(int arg);
extern void Com_Printf(const char *fmt, ...);
extern void Dvar_AddFlags(void *dvar, int flags);
extern void *Dvar_FindVar(const char *name);
extern const char *Dvar_DisplayableValue(void *dvar);
extern void Dvar_SetCommand(const char *name, const char *value);
extern void Dvar_Reset(void *dvar, int source);
extern void Cmd_AddCommand(const char *name, void (*func)(void));

extern int atoi(const char *s);
extern double atof(const char *s);
extern int I_stricmp(const char *s1, const char *s2);
extern int stricmp(const char *s1, const char *s2);
extern void I_strncat(char *dest, int maxLen, const char *src);
extern void Com_PrintMessage(print_msg_type_t type, const char *msg);
extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern int Com_Filter(const char *filter, const char *name, int casesensitive);
extern void FS_Printf(int f, const char *fmt, ...);
extern void Info_SetValueForKey(char *s, const char *key, const char *value);
extern void Info_SetValueForKey_Big(char *s, const char *key, const char *value);
extern const char *CL_GetUsernameForLocalClient(int controllerIndex);
extern void SV_SetConfigValueForKey(int start, int max, const char *key, const char *value);

extern Bool Dvar_IsValidName(const char *dvarName);
extern const char *Dvar_DisplayableResetValue(const dvar_t *dvar);
extern const char *Dvar_DisplayableLatchedValue(const dvar_t *dvar);
extern Bool Dvar_HasLatchedValue(const dvar_t *dvar);
extern const char *Dvar_IndexStringToEnumString(const dvar_t *dvar, const char *indexString);
extern void Dvar_PrintDomain(int type, DvarLimits domain);
extern void Dvar_SetFloatFromSource(const dvar_t *dvar, float value, DvarSetSource source);
extern void Dvar_SetIntFromSource(const dvar_t *dvar, int value, DvarSetSource source);
extern void Dvar_SetBoolFromSource(const dvar_t *dvar, int value, DvarSetSource source);
extern const dvar_t *Dvar_RegisterBool(const char *name, Bool value, int flags);
extern const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, int flags);
extern const dvar_t *Dvar_RegisterFloat(const char *name, float value, float min, float max, int flags);

static char info1[1024];
static char info2[8192];

void Dvar_ForEach(void (*callback)());
static void Dvar_GetCombinedString(void);
qboolean Dvar_Command(void);
static Bool Dvar_ToggleInternal(void);
void Dvar_Toggle_f(void);
void Dvar_TogglePrint_f(void);
void Dvar_Set_f(void);
static void Dvar_RegisterBool_f(void);
static void Dvar_RegisterInt_f(void);
static void Dvar_RegisterFloat_f(void);
void Dvar_SetU_f(void);
void Dvar_SetS_f(void);
void Dvar_SetA_f(void);
void Dvar_SetFromDvar_f(void);
void Dvar_Reset_f(void);
void Dvar_WriteVariables(fileHandle_t f);
void Dvar_WriteDefaults(fileHandle_t f);
void Dvar_List_f(void);
void Com_DvarDump(print_msg_type_t type);
void Dvar_Dump_f(void);
void SV_SetConfig(int start, int max, int bit);
char *Dvar_InfoString(int bit);
char *Dvar_InfoString_Big(int bit);
void Dvar_AddCommands(void);

void Dvar_ForEach(void (*callback)())
{
    dvar_t *dvar;
    dvar = *(dvar_t **)imp_sortedDvars;
    if (!dvar)
        return;
    do {
        ((void (*)(const char *))callback)(dvar->name);
        dvar = *(dvar_t **)&dvar->next;
    } while (dvar);
}

static void Dvar_GetCombinedString(void)
{

}

static void Dvar_GetCombinedString_impl(char *combined, int first)
{
    int argc;
    int l;
    int last;
    int i;
    int arglen;
    const char *arg;

    argc = Cmd_Argc();
    combined[0] = '\0';
    if (argc <= first)
        return;

    l = 0;
    last = argc - 1;
    for (i = first; i < argc; i++) {
        arg = Cmd_Argv(i);

        arglen = (int)strlen(arg) + 1;
        l += arglen - 1;
        if (l > 0xffd)
            return;
        I_strncat(combined, 1023, arg);
        if (i != last) {
            I_strncat(combined, 1023, " ");
        }
    }
}

qboolean Dvar_Command(void)
{
    dvar_t *dvar;
    char combined[0x1000];
    const char *resetval;

    dvar = (dvar_t *)Dvar_FindVar(Cmd_Argv(0));
    if (!dvar)
        return 0;

    if (Cmd_Argc() - 1 != 0) {

        Dvar_GetCombinedString_impl(combined, 1);
        Dvar_SetCommand(Cmd_Argv(0), combined);
        return 1;
    }

    resetval = Dvar_DisplayableResetValue(dvar);
    Com_Printf("\"%s\" is: \"%s^7\" default: \"%s^7\"\n",
               dvar->name,
               Dvar_DisplayableValue(dvar),
               resetval);
    if (Dvar_HasLatchedValue(dvar)) {
        Com_Printf("latched: \"%s\"\n", Dvar_DisplayableLatchedValue(dvar));
    }
    Dvar_PrintDomain(dvar->type, dvar->domain);
    return 1;
}

static Bool Dvar_ToggleInternal(void)
{
    int argc;
    const char *dvarName;
    dvar_t *dvar;
    const char *string;
    int argIdx;
    const char *argString;
    const char *enumStr;

    argc = Cmd_Argc();
    if (argc - 1 <= 0) {
        Com_Printf("USAGE: %s <variable> <optional value sequence>\n", Cmd_Argv(0));
        return 0;
    }

    dvarName = Cmd_Argv(1);
    dvar = (dvar_t *)Dvar_FindVar(dvarName);
    if (!dvar) {
        Com_Printf("toggle failed: dvar '%s' not found.\n", dvarName);
        return 0;
    }

    if (argc == 2) {

        if (dvar->type > 8) {
            return 0;
        }
        switch (dvar->type) {
        case 0:
            Dvar_SetBoolFromSource(dvar, !dvar->current.enabled, DVAR_SOURCE_EXTERNAL);
            return 1;

        case 1:
            if (dvar->domain.value.min >= 0.0f && dvar->domain.value.max < 1.0f) {

                goto toggle_float_clamp_min;
            }
            if (dvar->current.value != 0.0f) {
                Dvar_SetFloatFromSource(dvar, 0.0f, DVAR_SOURCE_EXTERNAL);
            } else {
                Dvar_SetFloatFromSource(dvar, 1.0f, DVAR_SOURCE_EXTERNAL);
            }
            return 1;

        toggle_float_clamp_min:
            if (dvar->current.value == dvar->domain.value.min) {
                Dvar_SetFloatFromSource(dvar, dvar->domain.value.max, DVAR_SOURCE_EXTERNAL);
            } else {
                Dvar_SetFloatFromSource(dvar, dvar->domain.value.min, DVAR_SOURCE_EXTERNAL);
            }
            return 1;

        case 5:
            if (dvar->domain.integer.min > 0) {
                goto toggle_int_pos_domain;
            }
            if (dvar->domain.integer.max <= 0) {
                goto toggle_int_clamp;
            }

            if (dvar->current.integer != 0) {
                Dvar_SetIntFromSource(dvar, 0, DVAR_SOURCE_EXTERNAL);
            } else {
                Dvar_SetIntFromSource(dvar, 1, DVAR_SOURCE_EXTERNAL);
            }
            return 1;

        toggle_int_pos_domain:

            if (dvar->current.integer == dvar->domain.integer.max) {
                Dvar_SetIntFromSource(dvar, dvar->domain.integer.min, DVAR_SOURCE_EXTERNAL);
            } else {
                Dvar_SetIntFromSource(dvar, dvar->current.integer + 1, DVAR_SOURCE_EXTERNAL);
            }
            return 1;

        toggle_int_clamp:

            if (dvar->domain.integer.min == 0) {
                return 1;
            }
            {
                int count = dvar->domain.integer.min;
                Dvar_SetIntFromSource(dvar, (dvar->current.integer + 1) % count, DVAR_SOURCE_EXTERNAL);
            }
            return 1;

        case 6:
            if (dvar->domain.enumeration.stringCount == 0) {
                return 1;
            }
            {
                int next = (dvar->current.integer + 1) % dvar->domain.enumeration.stringCount;
                Dvar_SetIntFromSource(dvar, next, DVAR_SOURCE_EXTERNAL);
            }
            return 1;

        case 2:
        case 3:
        case 4:
        case 7:
        case 8:
        default:
            Com_Printf("'toggle' with no arguments makes no sense for dvar '%s'\n", dvar->name);
            return 0;
        }
    }

    string = Dvar_DisplayableValue(dvar);
    argIdx = 2;
    while (1) {
        int nextIdx = argIdx + 1;
        if (Cmd_Argc() <= nextIdx) {

            argString = Cmd_Argv(2);
            if (dvar->type == 6) {
                enumStr = Dvar_IndexStringToEnumString(dvar, argString);
                if (enumStr[0] != '\0')
                    argString = enumStr;
            }
            Dvar_SetCommand(dvarName, argString);
            return 1;
        }
        argString = Cmd_Argv(argIdx);
        if (dvar->type == 6) {
            enumStr = Dvar_IndexStringToEnumString(dvar, argString);
            if (enumStr[0] != '\0')
                argString = enumStr;
        }
        if (stricmp(string, argString) == 0) {

            const char *nextArg = Cmd_Argv(nextIdx);
            if (dvar->type == 6) {
                enumStr = Dvar_IndexStringToEnumString(dvar, nextArg);
                if (enumStr[0] != '\0')
                    nextArg = enumStr;
            }
            Dvar_SetCommand(dvarName, nextArg);
            return 1;
        }
        argIdx = nextIdx;
    }
}

void Dvar_Toggle_f(void)
{
    Dvar_ToggleInternal();
}

void Dvar_TogglePrint_f(void)
{
    const char *dvarName;
    void *dvar;
    if (!Dvar_ToggleInternal())
        return;
    dvarName = Cmd_Argv(1);
    dvar = Dvar_FindVar(dvarName);
    Com_Printf("%s toggled to %s\n", dvarName, Dvar_DisplayableValue(dvar));
}

void Dvar_Set_f(void)
{
    char combined[0x1000];

    if (Cmd_Argc() <= 2) {
        Com_Printf("USAGE: set <variable> <value>\n");
        return;
    }
    if (!Dvar_IsValidName(Cmd_Argv(1))) {
        Com_Printf("invalid variable name: %s\n", Cmd_Argv(1));
        return;
    }
    Dvar_GetCombinedString_impl(combined, 2);
    Dvar_SetCommand(Cmd_Argv(1), combined);
}

static void Dvar_RegisterBool_f(void)
{
    const char *dvarName;
    Bool value;
    dvar_t *existing;

    if (Cmd_Argc() != 3) {
        Com_Printf("USAGE: %s <name> <default>\n", Cmd_Argv(0));
        return;
    }

    dvarName = Cmd_Argv(1);
    value = (atoi(Cmd_Argv(2)) != 0) ? 1 : 0;

    existing = (dvar_t *)Dvar_FindVar(dvarName);
    if (existing) {
        if (existing->type == 7) {
            if (!(existing->flags & 0x40)) {
                Com_Printf("dvar '%s' is not a boolean dvar\n", existing->name);
                return;
            }
        } else if (existing->type != 0) {
            Com_Printf("dvar '%s' is not a boolean dvar\n", existing->name);
            return;
        }

        if (existing->type == 7) {
            if (!(existing->flags & 0x40))
                goto print_not_bool;
        }
    }

    Dvar_RegisterBool(dvarName, value, 0x4000);
    return;

print_not_bool:
    Com_Printf("dvar '%s' is not a boolean dvar\n", existing->name);
}

static void Dvar_RegisterInt_f(void)
{
    const char *dvarName;
    int value, min, max;
    dvar_t *existing;

    if (Cmd_Argc() != 5) {
        Com_Printf("USAGE: %s <name> <default> <min> <max>\n", Cmd_Argv(0));
        return;
    }

    dvarName = Cmd_Argv(1);
    value = atoi(Cmd_Argv(2));
    min = atoi(Cmd_Argv(3));
    max = atoi(Cmd_Argv(4));

    if (min > max) {
        Com_Printf("dvar %s: min %i should not be greater than max %i\n", dvarName, min, max);
        return;
    }

    existing = (dvar_t *)Dvar_FindVar(dvarName);
    if (existing) {
        if (existing->type == 7) {
            if (!(existing->flags & 0x40))
                goto print_not_int;
        } else {

            unsigned char t = existing->type;
            t -= 5;
            if (t > 1) {
                goto print_not_int;
            }

        }
        goto do_register_int;
    }

do_register_int:
    Dvar_RegisterInt(dvarName, value, min, max, 0x4000);
    return;

print_not_int:
    Com_Printf("dvar '%s' is not an integer dvar\n", existing->name);
}

static void Dvar_RegisterFloat_f(void)
{
    const char *dvarName;
    float value, min, max;
    dvar_t *existing;

    if (Cmd_Argc() != 5) {
        Com_Printf("USAGE: %s <name> <default> <min> <max>\n", Cmd_Argv(0));
        return;
    }

    dvarName = Cmd_Argv(1);
    value = (float)atof(Cmd_Argv(2));
    min = (float)atof(Cmd_Argv(3));
    max = (float)atof(Cmd_Argv(4));

    if (min > max) {
        Com_Printf("dvar %s: min %g should not be greater than max %g\n", dvarName, (double)min, (double)max);
        return;
    }

    existing = (dvar_t *)Dvar_FindVar(dvarName);
    if (existing) {
        if (existing->type == 7) {
            if (!(existing->flags & 0x40))
                goto print_not_float;
        } else if (existing->type - 1 != 0) {
            goto print_not_float;
        }

        goto do_register_float;
    }

do_register_float:
    Dvar_RegisterFloat(dvarName, value, min, max, 0x4000);
    return;

print_not_float:
    Com_Printf("dvar '%s' is not an integer dvar\n", existing->name);
}

void Dvar_SetU_f(void)
{
    void *dvar;
    if (Cmd_Argc() <= 2) {
        Com_Printf("USAGE: setu <variable> <value>\n");
        return;
    }
    Dvar_Set_f();
    dvar = Dvar_FindVar(Cmd_Argv(1));
    if (dvar)
        Dvar_AddFlags(dvar, 2);
}

void Dvar_SetS_f(void)
{
    void *dvar;
    if (Cmd_Argc() <= 2) {
        Com_Printf("USAGE: sets <variable> <value>\n");
        return;
    }
    Dvar_Set_f();
    dvar = Dvar_FindVar(Cmd_Argv(1));
    if (dvar)
        Dvar_AddFlags(dvar, 4);
}

void Dvar_SetA_f(void)
{
    void *dvar;
    if (Cmd_Argc() <= 2) {
        Com_Printf("USAGE: seta <variable> <value>\n");
        return;
    }
    Dvar_Set_f();
    dvar = Dvar_FindVar(Cmd_Argv(1));
    if (dvar)
        Dvar_AddFlags(dvar, 1);
}

void Dvar_SetFromDvar_f(void)
{
    void *dvar;
    const char *value;
    if (Cmd_Argc() != 3) {
        Com_Printf("USAGE: setfromdvar <dest_dvar> <source_dvar>\n");
        return;
    }
    dvar = Dvar_FindVar(Cmd_Argv(2));
    if (!dvar) {
        Com_Printf("dvar '%s' doesn't exist\n", Cmd_Argv(2));
        return;
    }
    value = Dvar_DisplayableValue(dvar);
    Dvar_SetCommand(Cmd_Argv(1), value);
}

void Dvar_Reset_f(void)
{
    void *dvar;
    if (Cmd_Argc() != 2) {
        Com_Printf("USAGE: reset <variable>\n");
        return;
    }
    dvar = Dvar_FindVar(Cmd_Argv(1));
    if (dvar)
        Dvar_Reset(dvar, 1);
}

void Dvar_WriteVariables(fileHandle_t f)
{
    dvar_t *dvar;

    dvar = *(dvar_t **)imp_sortedDvars;
    while (dvar) {

        if (I_stricmp(dvar->name, "cl_cdkey") != 0) {

            if (dvar->flags & 1) {
                FS_Printf(f, "seta %s \"%s\"\n",
                          dvar->name,
                          Dvar_DisplayableLatchedValue(dvar));
            }
        }
        dvar = *(dvar_t **)&dvar->next;
    }
}

void Dvar_WriteDefaults(fileHandle_t f)
{
    dvar_t *dvar;

    dvar = *(dvar_t **)imp_sortedDvars;
    while (dvar) {

        if (I_stricmp(dvar->name, "cl_cdkey") != 0) {

            if (!(dvar->flags & 0x40c0)) {
                FS_Printf(f, "set %s \"%s\"\n",
                          dvar->name,
                          Dvar_DisplayableResetValue(dvar));
            }
        }
        dvar = *(dvar_t **)&dvar->next;
    }
}

void Dvar_List_f(void)
{
    const char *match;
    dvar_t *dvar;

    if (Cmd_Argc() - 1 > 0) {
        match = Cmd_Argv(1);
    } else {
        match = NULL;
    }

    dvar = *(dvar_t **)imp_sortedDvars;
    while (dvar) {

        if (match) {
            if (!Com_Filter(match, dvar->name, 0)) {
                dvar = *(dvar_t **)&dvar->next;
                continue;
            }
        }

        if (dvar->flags & 0x404) {
            Com_Printf("S");
        } else {
            Com_Printf(" ");
        }
        if (dvar->flags & 2) {
            Com_Printf("U");
        } else {
            Com_Printf(" ");
        }
        if (dvar->flags & 0x40) {
            Com_Printf("R");
        } else {
            Com_Printf(" ");
        }
        if (dvar->flags & 0x10) {
            Com_Printf("I");
        } else {
            Com_Printf(" ");
        }
        if (dvar->flags & 1) {
            Com_Printf("A");
        } else {
            Com_Printf(" ");
        }
        if (dvar->flags & 0x20) {
            Com_Printf("L");
        } else {
            Com_Printf(" ");
        }
        if ((signed char)(dvar->flags & 0xff) < 0) {
            Com_Printf("C");
        } else {
            Com_Printf(" ");
        }

        Com_Printf(" %s \"%s\"\n", dvar->name, Dvar_DisplayableValue(dvar));

        dvar = *(dvar_t **)&dvar->next;
    }

    Com_Printf("\n%i total dvars\n", *(int *)imp_dvarCount);
}

void Com_DvarDump(print_msg_type_t type)
{
    const char *match;
    dvar_t *var;
    int i;
    char message[0x2000];

    if (Cmd_Argc() - 1 > 0) {
        match = Cmd_Argv(1);
    } else {
        match = NULL;
    }

    if (type == 0) {
        dvar_t *logfile = *(dvar_t **)imp_com_logfile;
        if (!logfile || !logfile->current.integer)
            return;
    }

    Com_PrintMessage(type, "=============================== DVAR DUMP ========================================\n");

    var = *(dvar_t **)imp_sortedDvars;
    i = 0;

    if (!var) {
        goto print_summary;
    }

    while (var) {
        if (match) {
            if (!Com_Filter(match, var->name, 0)) {
                var = *(dvar_t **)&var->next;
                i++;
                continue;
            }
        }

        if (Dvar_HasLatchedValue(var)) {
            Com_sprintf(message, 0x2000, "      %s \"%s\" -- latched \"%s\"\n",
                        var->name,
                        Dvar_DisplayableValue(var),
                        Dvar_DisplayableLatchedValue(var));
        } else {
            Com_sprintf(message, 0x2000, "      %s \"%s\"\n",
                        var->name,
                        Dvar_DisplayableValue(var));
        }
        Com_PrintMessage(type, message);

        var = *(dvar_t **)&var->next;
        i++;
    }

print_summary:
    Com_sprintf(message, 0x2000, "\n%i total dvars\n%i dvar indexes\n",
                *(int *)imp_dvarCount, i);
    Com_PrintMessage(type, message);
    Com_PrintMessage(type, "=============================== END DVAR DUMP =====================================\n");
}

void Dvar_Dump_f(void)
{
    Com_DvarDump(0);
}

void SV_SetConfig(int start, int max, int bit)
{
    dvar_t *dvar;

    dvar = *(dvar_t **)imp_sortedDvars;
    while (dvar) {
        if (dvar->flags & bit) {
            SV_SetConfigValueForKey(start, max,
                                    dvar->name,
                                    Dvar_DisplayableValue(dvar));
        }
        dvar = *(dvar_t **)&dvar->next;
    }
}

char *Dvar_InfoString(int bit)
{
    dvar_t *var;

    info1[0] = '\0';

    var = *(dvar_t **)imp_sortedDvars;
    while (var) {
        if (var->flags & bit) {
            Info_SetValueForKey(info1, var->name, Dvar_DisplayableValue(var));
        }
        var = *(dvar_t **)&var->next;
    }

    if (bit & 2) {
        Info_SetValueForKey(info1, "name", CL_GetUsernameForLocalClient(0));
    }

    return info1;
}

char *Dvar_InfoString_Big(int bit)
{
    dvar_t *var;

    info2[0] = '\0';

    var = *(dvar_t **)imp_sortedDvars;
    while (var) {
        if (var->flags & bit) {
            Info_SetValueForKey_Big(info2, var->name, Dvar_DisplayableValue(var));
        }
        var = *(dvar_t **)&var->next;
    }

    return info2;
}

void Dvar_AddCommands(void)
{
    Cmd_AddCommand("toggle", Dvar_Toggle_f);
    Cmd_AddCommand("togglep", Dvar_TogglePrint_f);
    Cmd_AddCommand("set", Dvar_Set_f);
    Cmd_AddCommand("sets", Dvar_SetS_f);
    Cmd_AddCommand("seta", Dvar_SetA_f);
    Cmd_AddCommand("setfromdvar", Dvar_SetFromDvar_f);
    Cmd_AddCommand("reset", Dvar_Reset_f);
    Cmd_AddCommand("dvarlist", Dvar_List_f);
    Cmd_AddCommand("dvardump", Dvar_Dump_f);
    Cmd_AddCommand("dvar_bool", Dvar_RegisterBool_f);
    Cmd_AddCommand("dvar_int", Dvar_RegisterInt_f);
    Cmd_AddCommand("dvar_float", Dvar_RegisterFloat_f);
    Cmd_AddCommand("setu", Dvar_SetU_f);
}
