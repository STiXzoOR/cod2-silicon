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

#endif
