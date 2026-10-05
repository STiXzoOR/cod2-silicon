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
    /* Deliberately cross a 32-bit address boundary. A fixed address can collide
       with sanitizer shadow or ASLR placement, so reserve 4 GB + 64 KB wherever
       the kernel puts it and open 32 KB around the boundary inside it. */
    const size_t span = (1ULL << 32) + 0x10000;
    byte *area = mmap(NULL, span, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
    assert(area != MAP_FAILED);
    uintptr_t boundary = ((uintptr_t)area + 0xffffffffULL) & ~(uintptr_t)0xffffffffULL;
    if (boundary - (uintptr_t)area < 0x4000)
        boundary += 1ULL << 32;
    assert(mprotect((void *)(boundary - 0x4000), 0x8000, PROT_READ | PROT_WRITE) == 0);
    byte *data = (byte *)(boundary - 0x20);
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
    munmap(area, span);
    puts("online: master response pointer width, duplicates and truncated packet passed");
    return 0;
}
