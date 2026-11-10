#ifndef COD2X_H
#define COD2X_H

#include <stddef.h>

/* Opt in with COD2_FEATURE_CFLAGS=-DCOD2_CODX=1; no engine layout dependencies. */
#define COD2X_PROTOCOL 120
#define COD2X_CONNECT_PROTOCOL 118
#define COD2X_REVISION 6
/* The CoD2x release this layer follows, as CoD2x prints it to players and admins. */
#define COD2X_RELEASE "1.4.6.8"
#if defined(COD2_X64) && COD2_X64
#define COD2X_VERSION "opencod2-native-CoD2x-1.4.6.8"
#else
#define COD2X_VERSION "opencod2-cod2x-1.4.6"
#endif
/* The platform named in CoD2x reports. */
#define COD2X_PLATFORM "macOS-arm64"

int Cod2x_ConnectProtocol(int advertised);
size_t Cod2x_EncodeConnect(char *packet, size_t capacity, const char *userinfo);
int Cod2x_HwidValid(const char *id);
#if defined(COD2_X64) && COD2_X64
/* The machine values CoD2x hashes into cl_hwid2, in the order of its text
   (cod2x_identity.c). The product UUID is counted but not hashed. */
enum {
    COD2X_BIOS_MAKER, COD2X_BIOS_SERIAL, COD2X_CPU_NAME, COD2X_CPU_ID, COD2X_BOARD_MAKER,
    COD2X_BOARD_SERIAL, COD2X_PRODUCT_UUID, COD2X_DISK_MODEL, COD2X_DISK_SERIAL, COD2X_GPU_NAME,
    COD2X_MACHINE_VALUES
};
#define COD2X_MACHINE_TEXT 128
/* failed: 1 if the GPU couldn't be queried. Returns 0 when CoD2x would refuse. */
int Cod2x_HwidFromMachine(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT], int failed, char id[33]);
int Cod2x_HwidShortFromMachine(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT]);
/* cl_hwid from the last successful machine read, 0 before one. */
int Cod2x_ShortHwid(void);
/* Why the last machine read failed, as CoD2x reports it: an HRESULT and a text. */
const char *Cod2x_HwidError(unsigned int *code);
/* Shows CoD2x's HWID error and asks to retry; 0 means exit. Client platforms. */
int Sys_Cod2xHwidRetry(unsigned int code, const char *error);
#else
int Cod2x_HwidFromUUID(const char *uuid, char id[33]);
#endif
int Cod2x_ReadMachineHwid(char id[33]);
int Cod2x_CDKeyHash(const char *key, char hash[33]);
int Cod2x_LimitedFPS(int requested, int limited);

void Cod2x_Init(void);
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* A host runs as a CoD2x 1.4.6 server: g_cod2x is forced to this revision,
   the protocol dvar is 120 and the revision 6 admission rules apply. */
#define COD2X_HOST_REVISION COD2X_REVISION
void Cod2x_ServerInit(int dedicated);
void Cod2x_ServerSpawn(void);
void Cod2x_ServerShutdown(void);
int Cod2x_ServerActive(void);
int Cod2x_ServerGameVersion(void);
int Cod2x_SetClientDvar(const char *name, const char *value);
void Cod2x_ResetClientAnimation(void);
#endif
void Cod2x_PrepareConnect(void);
void Cod2x_Disconnect(void);
void Cod2x_ResetAnimation(void);
void Cod2x_Frame(int active, int demo);
int Cod2x_GameVersion(void);
int Cod2x_Competitive(void);
int Cod2x_FrameFPS(int requested);
const char *Cod2x_Hwid(void);

/* CoD2x's web services (cod2x_protocol.c). Tests point them at a stand-in with
   COD2X_TEST_SERVICE_URL. */
const char *Cod2x_ServiceURL(const char *url, char *buffer, size_t size);
/* Error and crash reports (cod2x_report.c): only faults in CoD2x-layer code go to
   CoD2x; the rest go to the report receiver of crash_handler.h. */
int Cod2x_InLayer(const void *address);
void Cod2x_Log(const char *format, ...);
size_t Cod2x_LoggerRecent(char *out, size_t size);
void Cod2x_ReportInit(void);
/* This build asks first: the launcher sets the variable to "1" only after the
   player has agreed to send CoD2x-layer crash and error reports to CoD2x. */
#define COD2X_REPORT_CONSENT_ENV "COD2_SILICON_COD2X_REPORTS"
/* Whether reports may go to CoD2x. CoD2x sends them without asking, and so does
   this build. A build that asks the player first defines COD2X_REPORT_CONSENT_ENV
   as the name of an environment variable: reports then go only when it is "1"
   (its launcher sets it once the player has agreed); otherwise a CoD2x-layer fault
   leaves only its log. */
int Cod2x_ReportConsent(void);
void Cod2x_ReportFatal(const char *message, const void *site);
void Cod2x_ReportError(const char *kind, const char *message);
/* POSTs JSON to one of CoD2x's services on a thread of its own, as CoD2x's client
   does; done runs on that thread with the HTTP status and the reply (ok 0: the
   request failed, error says why). */
typedef void (*Cod2xPostDone)(int ok, long status, const char *reply, const char *error, void *user);
void Cod2x_PostJson(const char *url, const char *json, int timeout, Cod2xPostDone done, void *user);
typedef void (*Cod2xCrashSent)(int delivered, const char *error);
void Cod2x_ReportCrash(const char *json, const char *pending, int timeout, int wait, Cod2xCrashSent sent);
/* A GET on its own thread, with the headers CoD2x's client sends (Host,
   Content-Length: 0); done turns 1 when it has finished. 0 when it cannot start. */
typedef struct {
    char *url;
    volatile int done;
    int failed;
    long status;
    char error[256];
    char *body;
    size_t length;
} Cod2xGet;
int Cod2x_GetStart(Cod2xGet *get, int timeout);
void Cod2x_ConsoleMirror(const char *text, size_t length);
size_t Cod2x_ConsoleLogs(char *out, size_t size);
/* JSON text that grows as it is written. */
typedef struct {
    char *text;
    size_t length, size;
} Cod2xReportText;
void Cod2x_TextAdd(Cod2xReportText *text, const char *format, ...);
void Cod2x_TextString(Cod2xReportText *text, const char *value);
void Cod2x_TextLines(Cod2xReportText *text, const char *field, const char *lines);
/* What CoD2x's update request says about this machine (CoD2x updater.cpp, system.cpp):
   the install's random id, the identity before its last change, what changed and
   how often, and the system text. Cleared once an answer came. */
typedef struct {
    char regid[33], previous[33], changed[1024], system[256];
    int changes;
} Cod2xMachineHistory;
void Sys_Cod2xMachineHistory(Cod2xMachineHistory *out);
void Sys_Cod2xMachineHistoryClear(void);
/* A dvar's value as CoD2x's report writes it; 0 when there is no such dvar
   (cod2x_runtime.c). */
int Cod2x_DvarText(const char *name, char *value, size_t size);
int Cod2x_DvarTextNamed(const char *name, char *value, size_t size);
int Cod2x_ClientState(void);

#endif
