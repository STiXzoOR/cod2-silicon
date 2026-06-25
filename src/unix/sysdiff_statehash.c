#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common_types.h"

extern struct serverStatic_t svs;
extern level_locals_t        level;
extern gentity_t            *g_entities;

static uint32_t sd_tab[256];
static int      sd_ready;

static uint32_t sd_crc(uint32_t crc, const void *buf, int len)
{
    if (!sd_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            sd_tab[i] = c;
        }
        sd_ready = 1;
    }
    const unsigned char *p = (const unsigned char *)buf;
    crc = ~crc;
    while (len-- > 0)
        crc = sd_tab[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

void Sys_StateHashFrame(void)
{
    static FILE *fp;
    static int   on = -1;
    static int   frame;

    if (on < 0) {
        const char *path = getenv("SYSDIFF_STATEHASH");
        on = (path && *path) ? 1 : 0;
        if (on) { fp = fopen(path, "w"); if (!fp) on = 0; }
    }
    if (!on) return;

    uint32_t h = sd_crc(0, &svs.time, sizeof svs.time);
    int n = level.num_entities;
    if (g_entities && n > 0 && n <= 4096)
        for (int i = 0; i < n; i++)
            h = sd_crc(h, &g_entities[i].s, sizeof(entityState_t));

    fprintf(fp, "%d %d %08x\n", frame++, svs.time, h);
    fflush(fp);
}
