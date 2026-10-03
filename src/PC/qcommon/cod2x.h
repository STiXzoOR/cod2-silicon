#ifndef COD2X_H
#define COD2X_H

#include <stddef.h>

/* Opt in with COD2_FEATURE_CFLAGS=-DCOD2_CODX=1; no engine layout dependencies. */
#define COD2X_PROTOCOL 120
#define COD2X_CONNECT_PROTOCOL 118
#define COD2X_REVISION 6
#define COD2X_VERSION "opencod2-cod2x-1.4.6"

int Cod2x_ConnectProtocol(int advertised);
size_t Cod2x_EncodeConnect(char *packet, size_t capacity, const char *userinfo);
int Cod2x_HwidValid(const char *id);
int Cod2x_HwidFromUUID(const char *uuid, char id[33]);
int Cod2x_ReadMachineHwid(char id[33]);
int Cod2x_CDKeyHash(const char *key, char hash[33]);
int Cod2x_LimitedFPS(int requested, int limited);

void Cod2x_Init(void);
void Cod2x_PrepareConnect(void);
void Cod2x_Disconnect(void);
void Cod2x_ResetAnimation(void);
void Cod2x_Frame(int active, int demo);
int Cod2x_GameVersion(void);
int Cod2x_Competitive(void);
int Cod2x_FrameFPS(int requested);
const char *Cod2x_Hwid(void);

#endif
