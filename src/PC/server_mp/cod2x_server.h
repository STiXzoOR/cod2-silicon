#ifndef COD2X_SERVER_H
#define COD2X_SERVER_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* Server behaviour of a CoD2x 1.4.6 host (cod2x_server.c). */
#define COD2X_MASTER_COUNT 3

void SV_Cod2xServerInit(void);
int SV_Cod2xCracked(void);
const netadr_t *SV_Cod2xMasterAddress(int index);
int SV_Cod2xIsMasterAddress(netadr_t from);
#endif

#endif
