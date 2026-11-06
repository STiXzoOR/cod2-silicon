#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "browser_source.h"

clientStatic_t cls;
serverStatusRequest_t cl_serverStatusList[16];
static ping_t pings[16];
void *imp_cls = &cls, *imp_cl_pinglist = pings;
static char output[16384];
static const char *protocolOverride = "";
static va_info_t vaInfo;
static int failures;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #x); ++failures; } } while (0)

void Com_Printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    size_t used = strlen(output);
    vsnprintf(output + used, sizeof(output) - used, fmt, ap);
    va_end(ap);
}
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_Error(int level, const char *fmt, ...) { (void)level; (void)fmt; abort(); }
void Com_PumpMessageLoop(void) { }
const char *Dvar_GetString(const char *name) { assert(!strcmp(name, "debug_protocol")); return protocolOverride; }
const char *NET_AdrToString(netadr_t address) { (void)address; return "203.0.113.7:28960"; }
int Sys_Milliseconds(void) { return 12345; }
void *Sys_GetValue(int key) { assert(key == 1); return &vaInfo; }
qboolean NET_CompareAdr(netadr_t a, netadr_t b)
{ return a.type == b.type && !memcmp(a.ip, b.ip, 4) && a.port == b.port; }
int NET_CompareAdrSigned(netadr_t *a, netadr_t *b)
{ int c = memcmp(a->ip, b->ip, 4); return c ? c : (int)a->port - b->port; }

static netadr_t address(void)
{
    netadr_t a = { .type = NA_IP, .ip = {203, 0, 113, 7}, .port = 0x2071 };
    return a;
}
static void reset(void)
{
    memset(&cls, 0, sizeof(cls));
    memset(pings, 0, sizeof(pings));
    memset(cl_serverStatusList, 0, sizeof(cl_serverStatusList));
    output[0] = 0;
    protocolOverride = "";
}
static void announce(int protocol, const char *text)
{
    byte bytes[4096];
    int n = snprintf((char *)bytes, sizeof(bytes), "\\protocol\\%d\\hostname\\fixture\\clients\\7\\sv_maxclients\\16\\mapname\\mp_toujane", protocol) + 1;
    memcpy(bytes + n, text, strlen(text) + 1);
    msg_t msg = { .data = bytes, .cursize = n + (int)strlen(text) + 1 };
    CL_ServerInfoPacket(address(), &msg, 450);
}
static void receive_status(const char *settings, const char *players, int print)
{
    byte bytes[4096];
    int n = snprintf((char *)bytes, sizeof(bytes), "%s\n%s\n", settings, players);
    msg_t msg = { .data = bytes, .cursize = n + 1 };
    cl_serverStatusList[4].address = address();
    cl_serverStatusList[4].pending = 1;
    cl_serverStatusList[4].print = print;
    CL_ServerStatusResponse(address(), &msg);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    reset();
    const char *test = argv[1];
    if (!strcmp(test, "lan-source")) {
        for (int source = 1; source <= 2; ++source) {
            reset(); cls.pingUpdateSource = source;
            announce(118, "");
            CHECK(cls.numlocalservers == 0);
        }
        reset(); cls.waitglobalserverresponse = 1;
        announce(118, "");
        CHECK(cls.numlocalservers == 1);
    } else if (!strcmp(test, "lan-reset")) {
        serverInfo_t *s = &cls.localServers[0];
        memset(s, 0x55, sizeof(*s)); s->adr.port = 0;
        announce(118, "");
        CHECK(cls.numlocalservers == 1);
        CHECK(NET_CompareAdr(s->adr, address()));
        CHECK(s->clients == 0 && s->maxClients == 0 && s->consoleDisabled == 0);
        CHECK(s->dirty == 0 && s->ping == -1 && s->requestCount == 0);
        CHECK(s->hostName[0] == 0 && s->mapName[0] == 0 && s->game[0] == 0 && s->gameType[0] == 0);
        CHECK(s->minPing == 0 && s->maxPing == 0 && s->netType == NA_IP);
        CHECK(s->bPassword == 0x55 && s->allowAnonymous == 0x55 && s->punkbuster == 0x55);
        announce(118, "ignored"); CHECK(cls.numlocalservers == 1 && output[0] == 0);
    } else if (!strcmp(test, "announcement-line")) {
        announce(118, "ready\n"); CHECK(!strcmp(output, "203.0.113.7:28960: ready\n"));
        reset(); announce(118, "ready"); CHECK(!strcmp(output, "203.0.113.7:28960: ready\n"));
    } else if (!strcmp(test, "announcement-short")) {
        announce(118, "x"); CHECK(!strcmp(output, "203.0.113.7:28960: x\n"));
    } else if (!strcmp(test, "announcement-limit")) {
        char text[1024]; memset(text, 'x', 1023); text[1023] = 0;
        announce(118, text);
        CHECK(strlen(output) == strlen("203.0.113.7:28960: ") + 1024);
        CHECK(output[strlen(output) - 1] == '\n');
    } else if (!strcmp(test, "ping")) {
        pings[3].adr = address(); pings[3].start = 400;
        cls.pingUpdateSource = 2; cls.waitglobalserverresponse = 1;
        cls.localServers[0].adr = address(); cls.favoriteServers[0].adr = address();
        cls.globalServers[0].adr = address(); cls.numglobalservers = 1;
        announce(118, "ignored");
        CHECK(pings[3].time == 51 && !strcmp(Info_ValueForKey(pings[3].info, "nettype"), "1"));
        CHECK(cls.numlocalservers == 0 && output[0] == 0);
        CHECK(cls.localServers[0].clients == 7 && cls.favoriteServers[0].maxClients == 16);
        CHECK(cls.globalServers[0].ping == 51 && !strcmp(cls.globalServers[0].hostName, "fixture"));
    } else if (!strcmp(test, "protocol")) {
        announce(119, ""); CHECK(cls.numlocalservers == 0);
        announce(120, "");
#if defined(COD2_CODX) && COD2_CODX
        CHECK(cls.numlocalservers == 1);
#else
        CHECK(cls.numlocalservers == 0);
#endif
        reset(); protocolOverride = "117";
        announce(118, ""); CHECK(cls.numlocalservers == 0);
        announce(117, ""); CHECK(cls.numlocalservers == 1);
    } else if (!strcmp(test, "status")) {
        receive_status("\\hostname\\fixture\\mapname\\mp_toujane", "10 50 \"one\"\n-2 0 \"two\"", 1);
        CHECK(!strcmp(cl_serverStatusList[4].string, "\\hostname\\fixture\\mapname\\mp_toujane\\\\10 50 \"one\"\\-2 0 \"two\"\\"));
        CHECK(cl_serverStatusList[4].time == 12345 && !cl_serverStatusList[4].pending && cl_serverStatusList[4].retrieved);
        CHECK(strstr(output, "hostname                fixture\n") != NULL);
        CHECK(strstr(output, "0    10     50    \"one\"\n") != NULL);
        reset(); receive_status("\\hostname\\fixture", "", 0);
        CHECK(!cl_serverStatusList[4].pending && !cl_serverStatusList[4].retrieved && !output[0]);
    } else if (!strcmp(test, "status-key")) {
        receive_status("\\orphan", "", 1);
        CHECK(!strcmp(output, "Server settings:\norphan                  \nPlayers:\nnum: score: ping: name:\n"));
    } else if (!strcmp(test, "status-limit")) {
        char text[1024]; memset(text, 'k', 1023); text[1023] = 0;
        receive_status(text, "", 1);
        CHECK(strlen(output) == strlen("Server settings:\n") + 1023 + strlen("k\n\nPlayers:\nnum: score: ping: name:\n"));
        CHECK(strstr(output, "kk\n\nPlayers:") != NULL);
    } else { fprintf(stderr, "Unknown case: %s\n", test); return 2; }
    printf("browser %s: %d failures\n", test, failures);
    return failures != 0;
}
