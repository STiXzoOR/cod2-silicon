#ifndef COD2X_SERVER_H
#define COD2X_SERVER_H

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* Server behaviour of a CoD2x 1.4.6 host (cod2x_server.c). */
#define COD2X_MASTER_COUNT 3

void SV_Cod2xServerInit(void);
void SV_Cod2xServerSpawned(void);
int SV_Cod2xCracked(void);
const netadr_t *SV_Cod2xMasterAddress(int index);
int SV_Cod2xIsMasterAddress(netadr_t from);
void SV_Cod2xMasterHeartbeat(const char *name);
void SV_Cod2xIpResponse(netadr_t from);
/* Removes colour and style codes as CoD2x does: "^1" and "^^11" (up to as many
   digits as carets), and "$1", "#1", "@1", "$*", "$-", "@+", "@=", "@-", "#_",
   "#-" and their doubled forms ("$$11", "$$**"). A prefix not followed by a
   code stays text. out may be text; at most size - 1 characters are written. */
void SV_Cod2xStripTextCodes(const char *text, char *out, size_t size);
/* showPacketStrings: text is the packet after its connectionless marker,
   size the whole packet. */
void SV_Cod2xShowPacket(int sent, netadr_t address, const void *text, int length, int size);
/* zPAM version choice (cod2x_zpam.c): files are the iwd names listed in
   folder; the ones not to load move to the end, the count to load is returned. */
void SV_Cod2xZpamInit(void);
int SV_Cod2xZpamFilterIwds(const char *folder, char **files, int count);
void SV_Cod2xZpamFrame(void);
#endif

#endif
