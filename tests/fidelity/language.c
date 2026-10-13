#include "dvar_services.h"
const dvar_t *loc_language, *loc_forceEnglish, *loc_translate, *loc_warnings, *loc_warningsAsErrors, *com_developer;
static qboolean g_currentAsian;
qboolean FS_LanguageHasAssets(int language) { return language == 14; }
const char *SE_LoadLanguage(int forceEnglish) { (void)forceEnglish; return NULL; }
#include "language_source.h"
int main(void)
{
    Dvar_SetCommand("loc_language", "14");
    SEH_InitLanguage();
    CHECK(loc_language->current.integer == 14 && loc_language->domain.integer.max == 14);
    CHECK(!strcmp(SEH_GetLanguageName(14), "czech") && !strcmp(SEH_GetLanguageName(15), "english"));
    int index = -1;
    CHECK(SEH_GetLanguageIndexForName("CZECH", &index) && index == 14);
    SEH_UpdateLanguageInfo();
    CHECK(loc_language->current.integer == 14 && loc_language->domain.integer.max == 14);
    CHECK(SEH_VerifyLanguageSelection(0) == 14);
    Dvar_Shutdown();
    FINISH("language");
}
