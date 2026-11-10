#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_server.h"
#include <string.h>

extern void Com_Printf(const char *fmt, ...);
extern const char *NET_AdrToString(netadr_t a);
extern qboolean NET_StringToAdr(const char *s, netadr_t *a);
extern qboolean NET_CompareBaseAdr(netadr_t a, netadr_t b);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);

/* Masters as on a CoD2x server: Activision and CoD2x fixed, a third for the admin. */
static const dvar_t *sv_cod2xMasters[COD2X_MASTER_COUNT];
static netadr_t sv_cod2xMasterAddresses[COD2X_MASTER_COUNT];
static const dvar_t *sv_cracked;

void SV_Cod2xServerInit(void)
{
    sv_cod2xMasters[0] = Dvar_RegisterString("sv_master1", "cod2master.activision.com", 0x1040);
    sv_cod2xMasters[1] = Dvar_RegisterString("sv_master2", "master.cod2x.me", 0x1040);
    sv_cod2xMasters[2] = Dvar_RegisterString("sv_master3", "", 0x1000);
    sv_cracked = Dvar_RegisterBool("sv_cracked", 0, 0x1000);
}

int SV_Cod2xCracked(void)
{
    return sv_cracked && sv_cracked->current.enabled;
}

/* Resolved once, and again after the dvar changes. A failed lookup leaves the
   address bad until then. Every master uses port 20710, as on CoD2x servers,
   even when the name carries another port. */
const netadr_t *SV_Cod2xMasterAddress(int index)
{
    dvar_t *master = (dvar_t *)sv_cod2xMasters[index];
    netadr_t *address = &sv_cod2xMasterAddresses[index];
    if (address->type == NA_BOT || master->modified) {
        master->modified = 0;
        Com_Printf("Resolving master server %s\n", master->current.string);
        if (!NET_StringToAdr(master->current.string, address)) {
            Com_Printf("Couldn't resolve address: %s\n", master->current.string);
        } else {
            if (strcmp(master->current.string, ":") && master->current.string[0])
                address->port = (unsigned short)((20710 >> 8) | (20710 << 8));
            Com_Printf("%s resolved to %s\n", master->current.string, NET_AdrToString(*address));
        }
    }
    return address;
}

int SV_Cod2xIsMasterAddress(netadr_t from)
{
    for (int i = 0; i < COD2X_MASTER_COUNT; ++i) {
        if (NET_CompareBaseAdr(from, *SV_Cod2xMasterAddress(i)))
            return 1;
    }
    return 0;
}
#endif
