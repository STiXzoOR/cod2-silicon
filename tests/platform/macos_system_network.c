/* Apple-only native probes; no game data or blob definitions required. */
#include "common_types.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

/* Include the production network path so the probe can inspect its bound socket. */
#include "PC/win32/win_net.c"
#include <fcntl.h>

static dvar_t test_dvars[16];
static int test_dvar_count;
static int failures;

void Com_Printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

void Com_Error(int code, const char *fmt, ...)
{
    (void)code;
    (void)fmt;
    abort();
}

static dvar_t *test_dvar(const char *name)
{
    int i;
    for (i = 0; i < test_dvar_count; i++) {
        if (!strcmp(test_dvars[i].name, name))
            return &test_dvars[i];
    }
    test_dvars[test_dvar_count].name = name;
    return &test_dvars[test_dvar_count++];
}

const dvar_t *Dvar_RegisterBool(const char *name, qboolean value, int flags)
{
    dvar_t *d = test_dvar(name);
    (void)flags;
    d->current.enabled = value;
    return d;
}

const dvar_t *Dvar_RegisterString(const char *name, const char *value, int flags)
{
    dvar_t *d = test_dvar(name);
    (void)flags;
    d->current.string = !strcmp(name, "net_ip") ? "127.0.0.1" : value;
    return d;
}

const dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, int flags)
{
    dvar_t *d = test_dvar(name);
    (void)min;
    (void)max;
    (void)flags;
    d->current.integer = !strcmp(name, "net_port") ? 0 : value;
    return d;
}

void Dvar_SetInt(const dvar_t *d, int value)
{
    ((dvar_t *)d)->current.integer = value;
}

qboolean I_isdigit(int c) { return isdigit(c); }
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
const char *NET_AdrToString(netadr_t a) { (void)a; return "test-peer"; }

extern char *Sys_DefaultHomePath(void);
extern int Sys_Milliseconds(void);
extern DWORD timeGetTime(void);
extern BOOL QueryPerformanceFrequency(void *frequency);
extern BOOL QueryPerformanceCounter(void *counter);

#ifdef DEDICATED
/* Minimal engine services required by Sys_GetEvent; packet IO remains production code. */
byte sys_packetReceived[16384];
void *Z_MallocInternal(int size) { return malloc((size_t)size); }
void Z_FreeInternal(void *ptr) { free(ptr); }
char *Sys_ConsoleInput(void) { return NULL; }
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, (size_t)size, "%s", src); }
qboolean NET_GetPacket(netadr_t *from, msg_t *message) { return Sys_GetPacket(from, message); }
void MSG_Init(msg_t *message, byte *data, int length)
{
    memset(message, 0, sizeof(*message));
    message->data = data;
    message->maxsize = length;
}
extern sysEvent_t Sys_GetEvent(void);
#endif

static void test_check(int result, const char *name)
{
    printf("%s: %s\n", result ? "PASS" : "FAIL", name);
    if (!result)
        failures++;
}

int main(int argc, char **argv)
{
    netadr_t adr, peer;
    struct sockaddr_in bound;
    socklen_t boundlen = sizeof(bound);
    const char payload[] = "native-arm64-udp";
    byte incoming[128];
    msg_t message;
    int i, before;
    long long frequency, counter0, counter1;
    char expected[1024];
    char *home;

    memset(&adr, 0xa5, sizeof(adr));
    test_check(Sys_StringToAdr("127.0.0.1", &adr), "numeric IPv4 resolution");
    test_check(adr.ip[0] == 127 && adr.ip[1] == 0 && adr.ip[2] == 0 && adr.ip[3] == 1,
          "IPv4 address bytes");
    test_check(adr.ipx[0] == 0xa5 && adr.ipx[1] == 0xa5 && adr.ipx[2] == 0xa5 && adr.ipx[3] == 0xa5,
          "IPv4 conversion preserves adjacent storage");
    test_check(sizeof(sockaddr_gen) == sizeof(struct sockaddr_in), "native IPv4 socket layout");

    if (argc > 1 && !strcmp(argv[1], "--closed-stdin"))
        close(STDIN_FILENO);
    NET_Init();
    test_check(ip_socket >= 0 && !getsockname(ip_socket, (struct sockaddr *)&bound, &boundlen),
          "engine opens nonblocking loopback UDP socket");
    if (ip_socket >= 0) {
        adr.port = bound.sin_port;
        Sys_SendPacket(sizeof(payload), payload, adr);
        memset(&message, 0, sizeof(message));
        message.data = incoming;
        message.maxsize = sizeof(incoming);
        memset(&peer, 0xa5, sizeof(peer));
        for (i = 0; i < 50 && !Sys_GetPacket(&peer, &message); i++)
            usleep(1000);
        test_check(message.cursize == sizeof(payload) && !memcmp(incoming, payload, sizeof(payload)),
              "engine sends and receives exact UDP payload");
        test_check(peer.type == NA_IP && !memcmp(peer.ip, adr.ip, 4) && peer.port == adr.port,
              "received native IPv4 address and port");
        test_check(peer.ipx[0] == 0xa5 && peer.ipx[3] == 0xa5, "receive preserves adjacent storage");
#ifdef DEDICATED
        Sys_SendPacket(sizeof(payload), payload, adr);
        {
            sysEvent_t event = Sys_GetEvent();
            for (i = 0; i < 50 && event.evType == 0; i++) {
                usleep(1000);
                event = Sys_GetEvent();
            }
            test_check(event.evType == 5 && event.evPtrLength == sizeof(netadr_t) + sizeof(payload),
                       "engine packet event includes complete native address");
            if (event.evPtr) {
                netadr_t *from = (netadr_t *)event.evPtr;
                test_check(from->port == adr.port && !memcmp(from->ip, adr.ip, 4) &&
                           !memcmp((byte *)event.evPtr + sizeof(netadr_t), payload, sizeof(payload)),
                           "engine packet event preserves sender port and payload");
                Z_FreeInternal(event.evPtr);
            }
        }
#endif
        before = Sys_Milliseconds();
        NET_Sleep(20);
        test_check(Sys_Milliseconds() - before >= 15, "engine waits for UDP readiness with native fd_set");
        test_check(!Sys_GetPacket(&peer, &message), "empty socket receive is nonblocking");
        i = ip_socket;
        NET_Config(0);
        test_check(fcntl(i, F_GETFD) == -1 && errno == EBADF, "engine closes UDP descriptor");
    }

    home = Sys_DefaultHomePath();
    {
        const char *userHome = getenv("HOME");
        if (!userHome || !userHome[0])
            userHome = getpwuid(getuid())->pw_dir;
        snprintf(expected, sizeof(expected), "%s/Library/Application Support/CoD2 Silicon", userHome);
    }
    test_check(home && !strcmp(home, expected), "macOS application support home path");
    before = Sys_Milliseconds();
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter0);
    usleep(20000);
    QueryPerformanceCounter(&counter1);
    test_check(Sys_Milliseconds() - before >= 15 && Sys_Milliseconds() - before < 250,
          "engine elapsed milliseconds");
    test_check(counter1 > counter0 && (counter1 - counter0) * 1000 / frequency >= 15 &&
          (counter1 - counter0) * 1000 / frequency < 250, "high resolution counter frequency");
    printf("home=%s clock frequency=%lld timeGetTime=%lu failures=%d\n",
           home ? home : "(null)", frequency, timeGetTime(), failures);
    return failures != 0;
}
