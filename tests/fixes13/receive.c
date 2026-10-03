#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static sysEvent_t eventQue[256];
static int eventHead, eventTail;
extern byte sys_packetReceived[];
static int capacity;
void IN_Frame(void) {}
char *Sys_ConsoleInput(void) { return NULL; }
int Sys_Milliseconds(void) { return 123; }
void *Z_MallocInternal(int n) { return malloc(n); }
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
void MSG_Init(msg_t *msg, byte *data, int size)
{
    memset(msg, 0, sizeof(*msg)); msg->data = data; msg->maxsize = size; capacity = size;
}
qboolean NET_GetPacket(netadr_t *adr, msg_t *msg)
{
    assert(msg->maxsize == 0x20000);
    memset(msg->data, 0x5a, 0x20000);
    msg->cursize = 0x20000; msg->readcount = 4; adr->port = 28960;
    return 1;
}
static void Sys_QueEventInternal(int time, sysEventType_t type, int a, int b, int len, void *data)
{
    eventQue[eventHead++] = (sysEvent_t){ time, type, a, b, len, data };
}
#include "receive_source.h"
int main(void)
{
    sysEvent_t ev = Sys_GetEvent();
    assert(capacity == MAX_MSGLEN);
    assert(ev.evType == 5 && ev.evPtrLength == 0x20000 - 4 + sizeof(netadr_t));
    assert(((byte *)ev.evPtr)[ev.evPtrLength - 1] == 0x5a);
    free(ev.evPtr);
    return 0;
}
