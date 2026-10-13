#ifndef COD2X_UDP_OPERATIONS_H
#define COD2X_UDP_OPERATIONS_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX

typedef enum {
    COD2X_UDP_QUERY,
    COD2X_UDP_RCON,
    COD2X_UDP_DISCONNECT
} cod2xUdpOperation_t;

extern const dvar_t *sv_rateLimiter;
extern const dvar_t *sv_rateLimiterRequestCount;
extern const dvar_t *sv_rateLimiterUniqueIPCount;

void SV_Cod2xUdpInit(void);
int SV_Cod2xUdpAllow(netadr_t from, cod2xUdpOperation_t operation);
void SV_Cod2xUdpStats(void);

#endif
#endif
