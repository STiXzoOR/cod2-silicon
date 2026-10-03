#include "common_types.h"
#include <assert.h>

extern void MSG_WriteDeltaEntity(msg_t *, entityState_t *, entityState_t *, qboolean);
extern void MSG_WriteDeltaClient(msg_t *, clientState_t *, clientState_t *, qboolean);
extern void MSG_WriteDeltaArchivedEntity(msg_t *, archivedEntity_t *, archivedEntity_t *, qboolean);
extern int MSG_ReadBits(msg_t *, int);
extern qboolean MSG_ReadDeltaEntity(msg_t *, entityState_t *, entityState_t *, int);
const dvar_t *cl_shownet;
void Com_Printf(const char *fmt, ...) {}

int main(void)
{
    byte buffer[512] = {0};
    msg_t msg = {0};
    msg.data = buffer;
    msg.maxsize = sizeof(buffer);
    entityState_t from = {0}, to = {0};
    to.number = 5;
    MSG_WriteDeltaEntity(&msg, &from, &to, 1);
    /* Retail forced unchanged entity: 10-bit number, remove=0, delta=0. */
    assert(msg.bit == 12 && msg.cursize == 2 && buffer[0] == 5 && buffer[1] == 0);
    memset(buffer, 0, sizeof(buffer));
    msg.bit = msg.cursize = 0;
    from.number = 5;
    MSG_WriteDeltaEntity(&msg, &from, NULL, 1);
    assert(msg.bit == 11 && buffer[0] == 5 && buffer[1] == 4);
    memset(buffer, 0, sizeof(buffer));
    msg.bit = msg.cursize = 0;
    clientState_t client = {0};
    client.clientIndex = 17;
    MSG_WriteDeltaClient(&msg, NULL, &client, 1);
    /* Client adds a presence bit before the six-bit number. */
    assert(msg.bit == 9 && msg.cursize == 2 && buffer[0] == 35 && buffer[1] == 0);
    memset(buffer, 0, sizeof(buffer));
    msg.bit = msg.cursize = 0;
    archivedEntity_t oldEntity = {0}, newEntity = {0};
    newEntity.s.number = 511;
    MSG_WriteDeltaArchivedEntity(&msg, &oldEntity, &newEntity, 1);
    assert(msg.bit == 12 && buffer[0] == 255 && buffer[1] == 1);
    memset(buffer, 0, sizeof(buffer));
    msg.bit = msg.cursize = msg.readcount = 0;
    memset(&from, 0, sizeof(from));
    memset(&to, 0, sizeof(to));
    to.number = 37;
    to.pos.trTime = 12345;
    to.pos.trBase[0] = 12.5f;
    to.pos.trBase[1] = -77.0f;
    to.events[2] = 63;
    to.weapon = 17;
    to.dmgFlags = 0x12345678;
    MSG_WriteDeltaEntity(&msg, &from, &to, 1);
    assert(!msg.overflowed);
    msg.bit = msg.readcount = 0;
    entityState_t decoded = {0};
    int number = MSG_ReadBits(&msg, 10);
    assert(number == 37);
    assert(!MSG_ReadDeltaEntity(&msg, &from, &decoded, number));
    assert(!memcmp(&decoded, &to, sizeof(to)));
    puts("wire: retail entity/remove/client/archive bit fixtures and changed-field round trip pass");
}
