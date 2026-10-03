#include "cod2x_policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *list[] = {"iw_00.iwd", "iw_CoD2x_01.iwd", "zpam334.iwd", "zpam335.iwd",
        "zpam_maps_v4.8bb28f35.iwd", "zpam_maps_v5.iwd", "foreign.iwd"};
    Cod2xIwdSelection s = {6, 0, 0, 0, 0, "", ""};
    char output[256], *argv[16];
    int n;
    assert(Cod2x_IwdAllowed("main", list[0], list, 7, &s));
    assert(Cod2x_IwdAllowed("main", list[1], list, 7, &s));
    assert(!Cod2x_IwdAllowed("main", list[2], list, 7, &s));
    assert(Cod2x_IwdStock("main/iw_CoD2x_01", "main", 5));
    assert(!Cod2x_IwdStock("main/iw_CoD2x_01", "main", 4));
    assert(!Cod2x_IwdStock("main/iw_25", "main", 6));
    assert(Cod2x_IwdStock("main/localized_english_iw11", "main", 0));
    s.connecting = 1; s.version = 0; s.names = "zpam_maps_v4 zpam334";
    assert(!Cod2x_IwdAllowed("main", list[1], list, 7, &s));
    assert(Cod2x_IwdAllowed("main", list[2], list, 7, &s));
    assert(!Cod2x_IwdAllowed("main", list[3], list, 7, &s));
    assert(Cod2x_IwdAllowed("main", list[4], list, 7, &s));
    s.demo = 1;
    assert(Cod2x_IwdAllowed("movie", list[6], list, 7, &s));
    s.demo = 0; s.listen = 1;
    assert(!Cod2x_IwdAllowed("main", list[2], list, 7, &s));
    assert(Cod2x_IwdAllowed("main", list[3], list, 7, &s));
    assert(!Cod2x_IwdAllowed("main", list[4], list, 7, &s));
    assert(Cod2x_IwdAllowed("main", list[5], list, 7, &s));
    s.listen = 0; s.connecting = 0; s.game = "mods/pam";
    assert(Cod2x_IwdAllowed("mods/pam", list[6], list, 7, &s));
    assert(!Cod2x_IwdAllowed("pam", list[6], list, 7, &s));
    s.dedicated = 1;
    assert(Cod2x_IwdAllowed("main", list[6], list, 7, &s));
    assert(!strcmp(Cod2x_ConfigGame("pam", "players/default/config_mp.cfg"), "main"));
    assert(!strcmp(Cod2x_ConfigGame("movie", "demos/a.dm_1"), "main"));
    assert(!strcmp(Cod2x_ConfigGame("movie", "movie.iwd"), "movie"));
    n = Cod2x_Tokenize("upload https://host/demo // comment", 16, argv, output, sizeof(output));
    assert(n == 2 && !strcmp(argv[1], "https://host/demo"));
    n = Cod2x_Tokenize("a /* block */ \"b \\\"c\\\"\" d", 16, argv, output, sizeof(output));
    assert(n == 3 && !strcmp(argv[1], "b \"c\""));
    n = Cod2x_Tokenize("one two three four", 2, argv, output, sizeof(output));
    assert(n == 2 && !strcmp(argv[1], "two three four"));
    assert(Cod2x_Tokenize("overlong", 16, argv, output, 4) == -1);
    puts("CoD2x IWD/config/tokenizer policy: passed");
    return 0;
}
