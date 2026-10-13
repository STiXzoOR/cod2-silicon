#include "common_types.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

clientStatic_t cls;
sharedUiInfo_t sharedUiInfo;
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
#include "browser_maps_source.h"
int main(void)
{
    int failures = 0;
    sharedUiInfo.mapCount = 2;
    sharedUiInfo.mapList[0].mapLoadName = "mp_a";
    sharedUiInfo.mapList[0].mapName = "Zulu";
    sharedUiInfo.mapList[1].mapLoadName = "mp_z";
    sharedUiInfo.mapList[1].mapName = "Alpha";
    cls.numglobalservers = 2;
    for (int source = 0; source < 3; ++source) {
        serverInfo_t *servers = source == 1 ? cls.globalServers : source == 2 ? cls.favoriteServers : cls.localServers;
        strcpy(servers[0].mapName, "MP_A");
        strcpy(servers[1].mapName, "mp_z");
        strcpy(servers[0].hostName, "Zulu host");
        strcpy(servers[1].hostName, "Alpha host");
        servers[0].ping = 12; servers[1].ping = 35;
        for (int reverse = 0; reverse < 2; ++reverse) {
            int result = LAN_CompareServers(source, 3, reverse, 0, 1);
            if (reverse ? result >= 0 : result <= 0) {
                fprintf(stderr, "source %d direction %d did not use map display order\n", source, reverse);
                ++failures;
            }
        }
        assert(LAN_CompareServers(source, 2, 0, 0, 1) > 0);
        assert(LAN_CompareServers(source, 2, 1, 0, 1) < 0);
        assert(LAN_CompareServers(source, 3, 0, -1, 1) == 0);
        assert(LAN_CompareServers(source, 3, 0, 0, source == 1 ? 2 : 128) == 0);
        sharedUiInfo.mapList[0].mapName = "Same";
        sharedUiInfo.mapList[1].mapName = "sAME";
        for (int reverse = 0; reverse < 2; ++reverse) {
            if (LAN_CompareServers(source, 3, reverse, 0, 1) >= 0) {
                fprintf(stderr, "source %d direction %d lost display-name ping tie\n", source, reverse);
                ++failures;
            }
        }
        sharedUiInfo.mapList[0].mapName = "Zulu";
        sharedUiInfo.mapList[1].mapName = "Alpha";
    }
    assert(!strcmp(UI_GetMapDisplayName("MP_A"), "Zulu"));
    assert(!strcmp(UI_GetMapDisplayName("custom_unknown"), "custom_unknown"));
    printf("browser map display ordering: %d failures\n", failures);
    return failures != 0;
}
