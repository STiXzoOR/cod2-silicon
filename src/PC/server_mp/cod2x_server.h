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
/* Match service (cod2x_match.c). */
void SV_Cod2xMatchInit(void);
int SV_Cod2xMatchRunning(void);
void SV_Cod2xMatchStartGameType(void);
void SV_Cod2xMatchBeforeMapChange(int shutdown);
void SV_Cod2xMatchFrame(void);
/* Update check (cod2x_updater.c). */
void SV_Cod2xUpdaterInit(void);
void SV_Cod2xUpdaterCheck(void);
void SV_Cod2xUpdaterFrame(void);
void SV_Cod2xUpdateResponse(netadr_t from);
/* zPAM version choice (cod2x_zpam.c): files are the iwd names listed in
   folder; the ones not to load move to the end, the count to load is returned. */
void SV_Cod2xZpamInit(void);
int SV_Cod2xZpamFilterIwds(const char *folder, char **files, int count);
void SV_Cod2xZpamFrame(void);
/* CoD2x's zPAM manifest from the update check. */
#define COD2X_ZPAM_MAX_FILES 16
#define COD2X_ZPAM_MAX_BLOCKED 64
typedef struct {
    char name[256];
    char url[1024];
    char sha256[72];
    size_t size;
} Cod2xZpamFile;
typedef struct {
    Cod2xZpamFile files[COD2X_ZPAM_MAX_FILES];
    int fileCount;
    char blocked[COD2X_ZPAM_MAX_BLOCKED][256];
    int blockedCount;
} Cod2xZpamManifest;
/* The zPAM this server runs ("zpam409.iwd"), empty when it runs none. */
void SV_Cod2xZpamInstalled(char *out, size_t size);
/* zPAM in main/, no fs_game and sv_updateZpam: the update check is needed. */
int SV_Cod2xZpamUpdateCheckNeeded(void);
/* Main thread: writes the blocked list and installs the rest in the background.
   0 when the blocked list could not be written (a CoD2x update must wait). */
int SV_Cod2xZpamApplyManifest(const Cod2xZpamManifest *manifest);
#endif

#endif
