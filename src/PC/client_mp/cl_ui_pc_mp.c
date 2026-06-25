#include "common_types.h"
#include "imports.h"

extern qboolean NET_StringToAdr(const char *s, netadr_t *a);
extern qboolean NET_CompareAdr(netadr_t a, netadr_t b);
extern const char *NET_AdrToString(netadr_t a);
extern int NET_CompareAdrSigned(netadr_t *a, netadr_t *b);
extern void I_strncpyz(char *dest, const char *src, int destsize);
extern void Com_Memcpy(void *dest, const void *src, int count);
extern int FS_SV_FOpenFileRead(const char *filename, fileHandle_t *fp);
extern fileHandle_t FS_SV_FOpenFileWrite(const char *filename);
extern int FS_Read(void *buffer, int len, fileHandle_t h);
extern int FS_Write(const void *buffer, int len, fileHandle_t h);
extern void FS_FCloseFile(fileHandle_t h);
extern void CL_SortGlobalServers(void);

extern byte *cls_ptr_195ecac;

#define CLS (*(byte **)&cls_ptr_195ecac)

#define MAX_FAVORITESERVERS 128
#define MAX_GLOBALSERVERS 20000

int LAN_AddServer(int source, const char *name, const char *address);
void LAN_RemoveServer(int source, const char *addr);
void LAN_GetServerAddressString(int source, int n, char *buf, int buflen);
void LAN_LoadCachedServers(void);
void LAN_SaveServersToCache(void);

int LAN_AddServer(int source, const char *name, const char *address)
{
    netadr_t adr;
    int i;
    byte *base;
    serverInfo_t *servers;
    int *count;

    if (source != 2) {
        return -1;
    }

    base = CLS;
    if ((((clientStatic_t *)(base))->numfavoriteservers) > 127) {
        return -1;
    }

    if (!NET_StringToAdr(address, &adr)) {
        return -2;
    }

    count = &(((clientStatic_t *)(base))->numfavoriteservers);
    servers = (((clientStatic_t *)(base))->favoriteServers);

    if (*count > 0) {
        for (i = 0; i < *count; i++) {
            if (NET_CompareAdr(servers[i].adr, adr)) {
                if (*count <= i) {
                    break;
                }
                return 0;
            }
        }
    }

    servers[*count].adr = adr;
    I_strncpyz(servers[*count].hostName, name, 32);
    servers[*count].dirty = 1;
    *count += 1;
    return 1;
}

void LAN_RemoveServer(int source, const char *addr)
{
    netadr_t comp;
    int i;
    int j;
    byte *base;
    serverInfo_t *servers;
    int *count;

    if (source != 2) {
        return;
    }

    NET_StringToAdr(addr, &comp);

    base = CLS;
    count = &(((clientStatic_t *)(base))->numfavoriteservers);
    servers = (((clientStatic_t *)(base))->favoriteServers);

    if (*count <= 0) {
        return;
    }

    for (i = 0; i < *count; i++) {
        if (NET_CompareAdr(servers[i].adr, comp)) {

            if (i < *count - 1) {
                for (j = i; j < *count - 1; j++) {
                    Com_Memcpy(&servers[j], &servers[j + 1], sizeof(serverInfo_t));
                }
            }
            *count -= 1;
            return;
        }
    }
}

void LAN_GetServerAddressString(int source, int n, char *buf, int buflen)
{
    byte *base;
    netadr_t adr;

    switch (source) {
    case 0:
        if ((unsigned int)n > 127) {
            break;
        }
        base = CLS;
        adr = (((clientStatic_t *)(base))->localServers)[n].adr;
        I_strncpyz(buf, NET_AdrToString(adr), buflen);
        return;

    case 1:
        if (n < 0) {
            break;
        }
        base = CLS;
        if (n >= (((clientStatic_t *)(base))->numglobalservers)) {
            break;
        }
        adr = (((clientStatic_t *)(base))->globalServers)[n].adr;
        I_strncpyz(buf, NET_AdrToString(adr), buflen);
        return;

    case 2:
        if ((unsigned int)n > 127) {
            break;
        }
        base = CLS;
        adr = (((clientStatic_t *)(base))->favoriteServers)[n].adr;
        I_strncpyz(buf, NET_AdrToString(adr), buflen);
        return;
    }

    *buf = '\0';
}

void LAN_LoadCachedServers(void)
{
    fileHandle_t fileIn;
    int version;
    int size;
    int success;
    byte *base;

    if (!FS_SV_FOpenFileRead("servercache.dat", &fileIn)) {
        base = CLS;
        (((clientStatic_t *)(base))->numglobalservers) = 0;
        (((clientStatic_t *)(base))->numfavoriteservers) = 0;
        return;
    }

    success = 0;

    if (FS_Read(&version, 4, fileIn) != 4) {
        goto done;
    }
    if (version != 1) {
        goto done;
    }

    base = CLS;

    if (FS_Read(&(((clientStatic_t *)(base))->numglobalservers), 4, fileIn) != 4) {
        goto done;
    }
    if ((unsigned int)(((clientStatic_t *)(base))->numglobalservers) > 19999) {
        goto done;
    }

    if (FS_Read(&(((clientStatic_t *)(base))->numfavoriteservers), 4, fileIn) != 4) {
        goto done;
    }
    if ((unsigned int)(((clientStatic_t *)(base))->numfavoriteservers) > 127) {
        goto done;
    }

    if (FS_Read(&size, 4, fileIn) != 4) {
        goto done;
    }
    if (size != 0x29c500) {
        goto done;
    }

    if (FS_Read((((clientStatic_t *)(base))->globalServers), 0x298100, fileIn) != 0x298100) {
        goto done;
    }

    if (FS_Read((((clientStatic_t *)(base))->favoriteServers), 0x4400, fileIn) != 0x4400) {
        goto done;
    }

    success = 1;

done:
    FS_FCloseFile(fileIn);
    if (!success) {
        base = CLS;
        (((clientStatic_t *)(base))->numglobalservers) = 0;
        (((clientStatic_t *)(base))->numfavoriteservers) = 0;
    } else {
        CL_SortGlobalServers();
    }
}

void LAN_SaveServersToCache(void)
{
    fileHandle_t fileOut;
    int version;
    int size;
    byte *base;
    int d;
    serverInfo_t *servers;

    fileOut = FS_SV_FOpenFileWrite("servercache.dat");
    if (!fileOut) {
        return;
    }

    version = 1;
    FS_Write(&version, 4, fileOut);

    base = CLS;
    servers = (((clientStatic_t *)(base))->globalServers);

    d = (((clientStatic_t *)(base))->numglobalservers) - 1;
    while (d >= 0) {
        if (servers[d].requestCount > 2) {

            (((clientStatic_t *)(base))->numglobalservers) -= 1;
            memcpy(&servers[d], &servers[(((clientStatic_t *)(base))->numglobalservers)], sizeof(serverInfo_t));
            d--;
            continue;
        }

        if (d == 0) {
            break;
        }
        if (NET_CompareAdrSigned(&servers[d].adr, &servers[d - 1].adr) == 0) {

            (((clientStatic_t *)(base))->numglobalservers) -= 1;
            memcpy(&servers[d], &servers[(((clientStatic_t *)(base))->numglobalservers)], sizeof(serverInfo_t));
            d--;
            continue;
        }

        d--;
        base = CLS;
        servers = (((clientStatic_t *)(base))->globalServers);
    }

    CL_SortGlobalServers();

    base = CLS;

    FS_Write(&(((clientStatic_t *)(base))->numglobalservers), 4, fileOut);

    FS_Write(&(((clientStatic_t *)(base))->numfavoriteservers), 4, fileOut);

    size = 0x298100;
    FS_Write(&size, 4, fileOut);

    FS_Write((((clientStatic_t *)(base))->globalServers), 0x298100, fileOut);

    FS_Write((((clientStatic_t *)(base))->favoriteServers), 0x4400, fileOut);

    FS_FCloseFile(fileOut);
}
