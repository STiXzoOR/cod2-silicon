#include "common_types.h"
#include <assert.h>
#include <string.h>
void SV_WWWDownload_f(void);
#include "tables_source.h"
int main(void)
{
    assert(sizeof(ucmds) / sizeof(ucmds[0]) == 13);
    assert(!strcmp(ucmds[9].name, "wwwdl") && ucmds[9].func == SV_WWWDownload_f);
    assert(ucmds[12].name == NULL);
    assert(sizeof(serverStatusDvars) / sizeof(serverStatusDvars[0]) == 24);
    assert(!strcmp(serverStatusDvars[22].name, "sv_punkbuster"));
    assert(serverStatusDvars[23].name == NULL);
    const unsigned char values[20] = { 0xb5,0xbf,0xdf,0xe0,0xe1,0xe4,0xe5,0xe6,0xe7,0xe8,
                                       0xe9,0xec,0xf1,0xf2,0xf3,0xf6,0xf8,0xf9,0xfa,0xfc };
    int found = 0;
    for (int i = 0; keynames_localized[i].name; ++i) {
        int key = keynames_localized[i].keynum;
        if (key >= 0x80 && key <= 0x93) {
            assert((unsigned char)keynames_localized[i].name[0] == values[key - 0x80]);
            assert(keynames_localized[i].name[1] == 0); ++found;
        }
    }
    assert(found == 20);
    assert((unsigned char)frenchNumberKeysMap[0][0] == 0xe0);
    assert((unsigned char)frenchNumberKeysMap[2][0] == 0xe9);
    assert((unsigned char)frenchNumberKeysMap[7][0] == 0xe8);
    assert((unsigned char)frenchNumberKeysMap[9][0] == 0xe7);
    assert(sizeof(cg_shock_dvar_names) / sizeof(cg_shock_dvar_names[0]) == 29);
    assert(!strcmp(cg_shock_dvar_names[4], "cg_shock_sound"));
    assert(!strcmp(cg_shock_dvar_names[28], "cg_shock_mouse_fadeTime"));
    return 0;
}
