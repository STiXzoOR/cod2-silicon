#include "common_types.h"
#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>

extern void SV_Netchan_AddOOBProfilePacket(int length);
extern void SV_Netchan_PrintProfileStats(qboolean console);
extern void SV_Netchan_Decode(client_t *client, byte *data, int size);
extern Bool SV_Netchan_Transmit(client_t *client, int length, byte *data);

server_t sv;
serverStatic_t svs;
void *imp_svs = &svs;
__asm__(".globl _svs_ptr\n.set _svs_ptr, _svs\n.globl _sv_ptr\n.set _sv_ptr, _sv\n");
static dvar_t profileDvar;
static dvar_t maxclients;
const dvar_t *net_profile = &profileDvar;
const dvar_t *sv_maxclients = &maxclients;
byte *net_profile_dvar = (byte *)&net_profile;
static int statisticsCalls;
static int namedClients;
static char totals[1024];

void NetProf_PrepProfiling(netProfileInfo_t **profile)
{
    if (!*profile) *profile = calloc(1, sizeof(**profile));
}
void NetProf_AddPacket(netProfileStream_t *stream, int len, qboolean fragment)
{
    stream->iBytesPerSecond += len;
}
void NetProf_UpdateStatistics(netProfileStream_t *stream) { statisticsCalls++; }
int Com_sprintf(char *dest, int size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int result = vsnprintf(dest, size, fmt, args);
    va_end(args);
    if (strstr(dest, "Totals:")) strcpy(totals, dest);
    if (strstr(dest, "native-client")) namedClients++;
    return result;
}
void Com_Printf(const char *fmt, ...) {}
void CL_DrawString(int x, int y, const char *str, int color, int size) {}
Bool Netchan_Transmit(netchan_t *chan, int length, const byte *data) { return 1; }

int main(void)
{
    profileDvar.current.integer = 1;
    maxclients.current.integer = 2;
    svs.clients = calloc(2, sizeof(client_t));
    netProfileInfo_t profiles[2] = {0};
    for (int i = 0; i < 2; i++) {
        profiles[i].send.iSmallestPacket = 9999;
        profiles[i].recieve.iSmallestPacket = 9999;
        svs.clients[i].state = 2;
        strcpy(svs.clients[i].name, "native-client");
        svs.clients[i].netchan.pProf = &profiles[i];
    }
    SV_Netchan_AddOOBProfilePacket(71);
    assert(svs.pOOBProf->send.iBytesPerSecond == 71);
    svs.pOOBProf->send.iLargestPacket = 97;
    svs.pOOBProf->send.iSmallestPacket = 13;
    svs.pOOBProf->recieve.iLargestPacket = 23;
    svs.pOOBProf->recieve.iSmallestPacket = 7;
    SV_Netchan_PrintProfileStats(1);
    assert(statisticsCalls == 6);
    assert(namedClients == 2);
    assert(strstr(totals, "   97|    7|"));

    /* Retail XOR keys use 32-bit challenge/sequence scalars; native pointers do not enter the bytes. */
    byte packet[] = {0, 0, 0, 0, 0, 1, 2, 3, 4, 5};
    const byte expected[] = {0, 0, 0, 0, 0x70, 0xb5, 0xf7, 0x32, 0x74, 0xb1};
    client_t *client = &svs.clients[1];
    client->challenge = 0x1234;
    client->netchan.outgoingSequence = 5;
    strcpy(client->lastClientCommandString, "Ab");
    SV_Netchan_Transmit(client, sizeof(packet), packet);
    assert(!memcmp(packet, expected, sizeof(packet)));
    client->serverId = 0;
    client->messageAcknowledge = 5;
    client->reliableAcknowledge = 3;
    strcpy(client->reliableCommandInfo[3].cmd, "Ab");
    SV_Netchan_Decode(client, packet + 4, 6);
    const byte plain[] = {0, 1, 2, 3, 4, 5};
    assert(!memcmp(packet + 4, plain, sizeof(plain)));
    free(svs.pOOBProf);
    free(svs.clients);
    puts("netchan: native profiles/client names and retail XOR pass");
}
