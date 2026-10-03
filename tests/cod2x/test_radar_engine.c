/* Exercise the production reader's temp-buffer ownership, including empty files. */
#include "../../src/PC/cgame_mp/cg_cod2x_radar.c"
#include <assert.h>

unsigned char cgsArray[sizeof(cgs_t)] __attribute__((aligned(16)));
static dvar_t image;
static int result, allocations, frees;

int FS_ReadFile(const char *name, void **buffer)
{
    assert(strstr(name, "maps/mp/") == name);
    if (result < 0) {
        *buffer = NULL;
        return -1;
    }
    *buffer = calloc(1, (size_t)result + 1);
    assert(*buffer);
    ++allocations;
    return result;
}

void FS_FreeFile(void *buffer)
{
    assert(buffer);
    ++frees;
    free(buffer);
}

void Dvar_SetString(const dvar_t *var, const char *value)
{
    ((dvar_t *)var)->current.string = value;
}
void Dvar_SetInt(const dvar_t *var, int value) { (void)var; (void)value; }
void Dvar_SetFloat(const dvar_t *var, float value) { (void)var; (void)value; }
char *Com_Parse(const char **cursor) { (void)cursor; return ""; }
void Com_Printf(const char *format, ...) { (void)format; }

int main(void)
{
    mapImage = &image;
    snprintf(cgs->mapname, sizeof(cgs->mapname), "maps/mp/missing.d3dbsp");
    result = -1;
    CG_Cod2xRadarLoad();
    assert(allocations == 0 && frees == 0);
    snprintf(cgs->mapname, sizeof(cgs->mapname), "maps/mp/empty.d3dbsp");
    result = 0;
    CG_Cod2xRadarLoad();
    assert(allocations == 1 && frees == 1);
    snprintf(cgs->mapname, sizeof(cgs->mapname), "maps/mp/invalid.d3dbsp");
    result = 8;
    CG_Cod2xRadarLoad();
    assert(allocations == 2 && frees == 2);
    CG_Cod2xRadarLoad();
    assert(allocations == 2 && frees == 2);
    puts("CoD2x production radar reader buffer ownership: pass");
    return 0;
}
