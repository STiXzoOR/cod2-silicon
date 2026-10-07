#include "dvar_services.h"
#include "registrations_source.h"
int main(void)
{
    Dvar_Init();
    RegisterCorrections();
    const char *names[] = {"r_lodBias", "r_lightTweakAmbient", "r_lightTweakDiffuseFraction", "r_lightTweakSunLight",
        "r_lightTweakAmbientColor", "r_lightTweakSunColor", "r_lightTweakSunDiffuseColor", "r_lightTweakSunDirection"};
    for (int i = 0; i < 8; ++i) {
        const dvar_t *var = Dvar_FindVar(names[i]);
        CHECK(var && (var->flags & 0x80));
        CHECK(var && !(var->flags & 1));
        if (var) {
            DvarValue before = var->current;
            float vector[3];
            if (var->type == 3)
                memcpy(vector, var->current.vector, sizeof(vector));
            const char *value = var->type == 8 ? "0 0 0 1" : var->type == 3 ? "1 2 3" : i == 0 ? "-1" : "2";
            Dvar_SetCommand(names[i], value);
            CHECK(!memcmp(&var->current, &before, sizeof(before)));
            if (var->type == 3)
                CHECK(!memcmp(var->current.vector, vector, sizeof(vector)));
        }
    }
    const dvar_t *packets = Dvar_FindVar("cl_maxpackets");
    CHECK(packets && packets->domain.integer.min == 10);
#if defined(COD2_CODX) && COD2_CODX
    CHECK(packets && packets->reset.integer == 125 && packets->domain.integer.max == 125);
    CHECK(Dvar_FindVar("r_lodScale")->flags == 0x2001 && Dvar_FindVar("r_lodScale")->domain.value.min == 0);
#else
    CHECK(packets && packets->reset.integer == 30 && packets->domain.integer.max == 100);
    CHECK(Dvar_FindVar("r_lodScale")->flags == 0x2080 && Dvar_FindVar("r_lodScale")->domain.value.min == 1);
#endif
    Dvar_SetCommand("cl_maxpackets", "10");
    CHECK(packets && packets->current.integer == 10);
    const dvar_t *language = Dvar_FindVar("loc_language");
    CHECK(language && language->domain.integer.max == 14);
    const dvar_t *height = Dvar_FindVar("cl_profileTextHeight");
    CHECK(height && height->type == 5 && height->reset.integer == 16 && height->domain.integer.min == 1 && height->domain.integer.max == 100 && height->flags == 0x1000);
    const dvar_t *motd = Dvar_FindVar("motd");
    CHECK(motd && motd->type == 7 && !strcmp(motd->reset.string, "") && motd->flags == 0x1000);
    const char *owned[] = {
        "activeAction", "cl_anglespeedkey", "cl_avidemo", "cl_bypassMouseInput",
        "cl_connectTimeout", "cl_forceavidemo", "cl_freezeDemo", "cl_noprint",
        "cl_serverStatusResendTime", "cl_showSend", "cl_showServerCommands", "cl_showTimeDelta",
        "cl_showmouserate", "cl_shownet", "cl_shownuments", "cl_talking",
        "cl_timeout", "fx_cull", "fx_sort", "nextdemo"
    };
    for (unsigned i = 0; i < sizeof(owned) / sizeof(owned[0]); ++i) {
        const dvar_t *var = Dvar_FindVar(owned[i]);
        CHECK(var && (var->flags & 0x1000));
    }
    Dvar_Shutdown();
    /* A previous engine registration without owner bits must yield its name
       to the client registration, retaining the existing typed values. */
    Dvar_Init();
    Dvar_RegisterBool("CL_NOPRINT", 1, 0);
    Dvar_RegisterInt("CL_SHOWNET", 1, -2, 4, 0);
    Dvar_RegisterFloat("CL_TIMEOUT", 100, 0, 3600, 0);
    Dvar_RegisterString("ACTIVEACTION", "fixture", 0);
    RegisterCorrections();
    CHECK(!strcmp(Dvar_FindVar("cl_noprint")->name, "cl_noprint"));
    CHECK(!strcmp(Dvar_FindVar("cl_shownet")->name, "cl_shownet"));
    CHECK(!strcmp(Dvar_FindVar("cl_timeout")->name, "cl_timeout"));
    CHECK(!strcmp(Dvar_FindVar("activeAction")->name, "activeAction"));
    CHECK(Dvar_FindVar("cl_noprint")->current.enabled == 1);
    CHECK(Dvar_FindVar("cl_shownet")->current.integer == 1);
    CHECK(Dvar_FindVar("cl_timeout")->current.value == 100);
    CHECK(!strcmp(Dvar_FindVar("activeAction")->current.string, "fixture"));
    Dvar_Shutdown();
    /* Config text can create archived strings before engine registration. */
    Dvar_Init();
    Dvar_SetCommand("cl_maxpackets", "10");
    Dvar_SetCommand("loc_language", "14");
    Dvar_SetCommand("r_lodBias", "-1");
    Dvar_AddFlags(Dvar_FindVar("r_lodBias"), 1);
    Dvar_SetCommand("r_lightTweakAmbient", "2");
    RegisterCorrections();
    CHECK(Dvar_FindVar("cl_maxpackets")->current.integer == 10);
    CHECK(Dvar_FindVar("loc_language")->current.integer == 14);
    CHECK(Dvar_FindVar("r_lodBias")->current.value == 0 && (Dvar_FindVar("r_lodBias")->flags & 0x80));
    CHECK(Dvar_FindVar("r_lightTweakAmbient")->current.value == 0.1f);
    Dvar_Shutdown();
    FINISH("registrations");
}
