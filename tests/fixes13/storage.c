#include "common_types.h"
#include <assert.h>
#include <string.h>
static int g_largeLocalPos;
#include "storage_source.h"
int main(void)
{
    clientStatic_t cls;
    serverStatic_t svs;
    assert(sizeof(serverInfo_t) == 148);
    assert(offsetof(serverInfo_t, requestCount) == 34);
    assert(offsetof(serverInfo_t, hostName) == 42);
    assert(sizeof(challenge_t) == 116);
    assert(sizeof(svs.challenges[0].PBguid) == 33);
    assert(sizeof(svs.challenges[0].clientPBguid) == 33);
    assert(offsetof(serverStatic_t, sv_lastTimeMasterServerCommunicated) > offsetof(serverStatic_t, authorizeAddress));
    assert(sizeof(cls.globalServers) == 20000 * 148);
    assert(sizeof(clientStatic_t) == 3000008);
    assert(sizeof(serverStatic_t) == 119096); /* Native pointers/alignment. */
    assert(sizeof(g_largeLocalBuf) == 0x100000);
    LargeLocal allocations[8];
    for (int i = 0; i < 8; ++i) {
        LargeLocal_LargeLocal(&allocations[i], 0x20000);
        memset(LargeLocal_GetBuf(&allocations[i]), i, 0x20000);
    }
    assert(g_largeLocalBuf[0xfffff] == 7);
    for (int i = 7; i >= 0; --i) ZN10LargeLocalD1Ev(&allocations[i]);
    assert(g_largeLocalPos == 0);
    return 0;
}
