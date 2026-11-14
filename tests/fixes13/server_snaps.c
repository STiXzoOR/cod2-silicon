#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static client_t client;
static dvar_t dedicated;
static const dvar_t *dedicatedPointer = &dedicated;
void *imp_com_dedicated = &dedicatedPointer;
static const char *requested;
static int lan;
static const char *download = "1";
char *Info_ValueForKey(const char *info, const char *key)
{
    assert(info == client.userinfo);
    if (!strcmp(key, "name")) return "fixture";
    if (!strcmp(key, "rate")) return "25000";
    if (!strcmp(key, "snaps")) return (char *)requested;
    if (!strcmp(key, "cl_voice")) return "1";
    if (!strcmp(key, "cl_wwwDownload")) return (char *)download;
    assert(!"unexpected userinfo key");
    return "";
}
qboolean Sys_IsLANAddress(netadr_t address) { (void)address; return lan; }
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
#include "server_snaps_source.h"
int main(void)
{
    static const struct { const char *value; int stock, codx; } cases[] = {
        {"", 50, 50}, {"-1", 1000, 1000}, {"0", 1000, 1000},
        {"1", 1000, 1000}, {"20", 50, 50}, {"30", 33, 33},
        {"31", 33, 32}, {"39", 33, 25}, {"40", 33, 25},
        {"41", 33, 25}, {"125", 33, 25}
    };
    int failures = 0;
    for (lan = 0; lan <= 1; ++lan) {
        for (int mode = 0; mode <= 2; mode += 2) {
            dedicated.current.integer = mode;
            for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                memset(&client, 0, sizeof(client));
                requested = cases[i].value;
                SV_UserinfoChanged(&client);
#if defined(COD2_CODX) && COD2_CODX
                int expected = cases[i].codx;
#else
                int expected = cases[i].stock;
#endif
                if (client.snapshotMsec != expected) {
                    fprintf(stderr, "snaps %s -> %d ms, expected %d\n", requested, client.snapshotMsec, expected);
                    ++failures;
                }
                assert(!strcmp(client.name, "fixture"));
                assert(client.rate == (lan && mode != 2 ? 99999 : 25000));
                assert(client.sendVoice);
#if COD2_IS_PATCH_13
                assert(client.wwwOk);
#endif
            }
        }
    }
#if COD2_IS_PATCH_13
    const char *downloadCases[] = {"", "0", "-1", "1", "2"};
    for (unsigned i = 0; i < sizeof(downloadCases) / sizeof(downloadCases[0]); ++i) {
        download = downloadCases[i];
        SV_UserinfoChanged(&client);
#if defined(COD2_CODX) && COD2_CODX
        int expected = atoi(download) > 0;
#else
        int expected = atoi(download) != 0;
#endif
        if (client.wwwOk != expected) { fprintf(stderr, "download %s: got %d expected %d\n", download, client.wwwOk, expected); ++failures; }
    }
#endif
    printf("server snapshot intervals: %d failures\n", failures);
    return failures != 0;
}
