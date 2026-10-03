#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static clientStatic_t browser;
void *imp_cls = &browser;
void Com_Printf(const char *fmt, ...) { (void)fmt; }
void Com_DPrintf(const char *fmt, ...) { (void)fmt; }
void Com_PumpMessageLoop(void) { }
int NET_CompareAdrSigned(netadr_t *a, netadr_t *b)
{
    int result = memcmp(a->ip, b->ip, sizeof(a->ip));
    return result ? result : (int)a->port - b->port;
}
static int CL_CompareAdrSigned(const void *a, const void *b)
{
    return NET_CompareAdrSigned(&((serverInfo_t *)a)->adr, &((serverInfo_t *)b)->adr);
}
#include "master_response_source.h"
int main(void)
{
    const byte packet[] = {
        255, 255, 255, 255, 'g','e','t','s','e','r','v','e','r','s','R','e','s','p','o','n','s','e',
        '\\', 37, 44, 215, 192, 0x71, 0x20,
        '\\', 37, 187, 138, 61, 0x75, 0x67,
        '\\', 'E', 'O', 'T'
    };
    /* Deliberately cross a 32-bit address boundary in this isolated process. */
    void *area = mmap((void *)0x1ffffc000ULL, 0x8000, PROT_READ | PROT_WRITE,
                     MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(area != MAP_FAILED);
    byte *data = (byte *)0x1ffffffe0ULL;
    memcpy(data, packet, sizeof(packet));
    msg_t msg = { .data = data, .cursize = sizeof(packet) };
    netadr_t from = {0};
    CL_ServersResponsePacket(from, &msg);
    assert(browser.numglobalservers == 2);
    assert(browser.globalServers[0].adr.ip[1] == 44);
    assert(browser.globalServers[0].adr.port == 0x2071);
    CL_ServersResponsePacket(from, &msg);
    assert(browser.numglobalservers == 2);
    msg.cursize = 25; /* incomplete first endpoint */
    CL_ServersResponsePacket(from, &msg);
    assert(browser.numglobalservers == 2);
    munmap(area, 0x8000);
    puts("online: master response pointer width, duplicates and truncated packet passed");
    return 0;
}
