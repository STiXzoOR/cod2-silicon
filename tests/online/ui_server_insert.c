#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct sharedUiInfo_t sharedUiInfo;
static dvar_t source = { .current.integer = 1 };
const dvar_t *ui_netSource = &source;
int LAN_CompareServers(int netSource, int sortKey, int sortDir, int a, int b)
{
    assert(netSource == 1 && sortKey == 2);
    return sortDir ? b - a : a - b;
}
#include "ui_server_insert_source.h"

int main(void)
{
    int servers[] = { 50, 20, 70, 40, 80, 60, 10, 30, 40 };
    for (int direction = 0; direction < 2; ++direction) {
        memset(&sharedUiInfo, 0, sizeof(sharedUiInfo));
        sharedUiInfo.serverStatus.currentServer = -1;
        sharedUiInfo.serverStatus.sortKey = 2;
        sharedUiInfo.serverStatus.sortDir = direction;
        for (int i = 0; i < 9; ++i) {
            UI_BinaryInsertServer(servers[i]);
            assert(sharedUiInfo.serverStatus.numDisplayServers == i + 1);
            for (int j = 1; j <= i; ++j)
                assert(LAN_CompareServers(1, 2, direction,
                    sharedUiInfo.serverStatus.displayServers[j - 1],
                    sharedUiInfo.serverStatus.displayServers[j]) <= 0);
        }
    }
    puts("online: first server, both sort orders, ties and insertion progress passed");
}
