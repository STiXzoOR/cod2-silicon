#ifndef COD2X_CLIENT_IDENTITY_H
#define COD2X_CLIENT_IDENTITY_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* Native sidecars keep connection provenance out of the shared client layouts. */
const char *SV_Cod2xClientHwid(int clientNum);
const char *SV_Cod2xClientDigest(int clientNum);
const char *SV_Cod2xClientAuthorization(int clientNum);
void SV_Cod2xClientIp(int clientNum, unsigned char ip[4]);
void SV_Cod2xClearIdentities(void);
#endif

#endif
