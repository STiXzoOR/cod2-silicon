#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdlib.h>

extern void SV_Startup(void);

static serverStatic_t server;
static dvar_t dedicated;
static dvar_t running;
static dvar_t maxclients;
void *imp_svs = &server;
/* Deliberately use the real import indirection, not a dvar-shaped slot. */
const dvar_t *com_dedicated = &dedicated;
void *imp_com_dedicated = &com_dedicated;
const dvar_t *com_sv_running = &running;
const dvar_t *sv_maxclients;

void Com_Error(int code, const char *fmt, ...)
{
    abort();
}

dvar_t *Dvar_RegisterInt(const char *name, int value, int min, int max, int flags)
{
    maxclients.current.integer = 4;
    return &maxclients;
}

void Dvar_ClearModified(const dvar_t *dvar) {}
void Dvar_SetInt(const dvar_t *dvar, int value)
{
    ((dvar_t *)dvar)->current.integer = value;
}
void Dvar_SetBool(dvar_t *dvar, int value) { dvar->current.enabled = value; }
void *Z_VirtualAllocInternal(int size) { return calloc(1, size); }

int main(void)
{
    for (int mode = 0; mode < 2; mode++) {
        memset(&server, 0, sizeof(server));
        dedicated.current.integer = mode;
        SV_Startup();
        assert(server.initialized && running.current.enabled);
        assert(server.clients);
        server.clients[3].name[0] = 'x';
        assert(server.numSnapshotEntities == 4 * (mode ? 2048 : 256));
        assert(server.numSnapshotClients == 16 * (mode ? 32 : 4));
        free(server.clients);
    }
    puts("server startup: dedicated/listen allocations pass");
    return 0;
}
