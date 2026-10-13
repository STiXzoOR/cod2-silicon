#ifndef COD2X_HOST_PROTOCOL_H
#define COD2X_HOST_PROTOCOL_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "PC/qcommon/cod2x.h"
#include <stdint.h>
#include <string.h>

extern int Cod2x_ServerGameVersion(void);

static inline int SV_Cod2xHostAdmission(void)
{
    return Cod2x_ServerGameVersion() >= COD2X_REVISION;
}

static inline int SV_Cod2xDiscoveryProtocol(void)
{
    return SV_Cod2xHostAdmission() ? COD2X_PROTOCOL : COD2X_CONNECT_PROTOCOL;
}

static inline int SV_Cod2xHwidGuid(const char *hwid)
{
    uint32_t hash = UINT32_C(2166136261);
    if (!hwid || strlen(hwid) != 32)
        return 0;
    for (int i = 0; i < 32; ++i)
        hash = (hash ^ (unsigned char)hwid[i]) * UINT32_C(16777619);
    if (!hash)
        hash = 1;
    return hash <= INT32_MAX ? (int)hash : -1 - (int)(UINT32_MAX - hash);
}

static inline int SV_Cod2xDigestValid(const char *digest)
{
    if (!digest || strlen(digest) != 32)
        return 0;
    for (int i = 0; i < 32; ++i) {
        unsigned char c = (unsigned char)digest[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return 0;
    }
    return 1;
}
#endif

#endif
