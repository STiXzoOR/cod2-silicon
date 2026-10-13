/* Reuse the production playback fixture's device, dvar and capture boundaries. */
#define main VoiceInitControlMain
#include "playback_init.c"
#undef main

extern void MSG_Init(msg_t *, byte *, int);
extern void MSG_WriteString(msg_t *, const char *);
extern void MSG_WriteByte(msg_t *, int);
extern void MSG_WriteShort(msg_t *, int);
extern void MSG_WriteData(msg_t *, const void *, int);
extern int MSG_ReadShort(msg_t *);

clientActive_t clients[1];
clientConnection_t clientConnections[1];
static clientActive_t *cl = clients;
void *imp_cl = &cl;
static clientConnection_t *clc = clientConnections;
void *imp_clc = &clc;
serverStatic_t svs;
void *imp_svs = &svs;
gentity_t g_entities[64];
level_locals_t level;
static client_t serverClients[64];
static gclient_s gameClients[64];
static dvar_t globalVoice, echoVoice, deadVoice, serverVoice, maxClients, showSend;
const dvar_t *voice_global = &globalVoice, *voice_localEcho = &echoVoice;
const dvar_t *voice_deadChat = &deadVoice, *sv_voice = &serverVoice;
const dvar_t *sv_maxclients = &maxClients, *cl_showSend = &showSend;
static byte largeBuffer[0x20000], serverWire[0x20000];
static int clientDatagrams, serverBytes, sentFrames[16], sentCount, expectedTalker;

void LargeLocal_LargeLocal(const LargeLocal *local, int size)
{
    (void)local;
    assert(size == sizeof(largeBuffer));
}
void *LargeLocal_GetBuf(const LargeLocal *local) { (void)local; return largeBuffer; }
void ZN10LargeLocalD1Ev(LargeLocal *local) { (void)local; }
void Com_Memset(void *destination, int value, int count) { memset(destination, value, count); }
int OnSameTeam(gentity_t *a, gentity_t *b) { return a->client->sess.cs.team == b->client->sess.cs.team; }
Bool NET_CompareBaseAdr(netadr_t a, netadr_t b) { return !memcmp(a.ip, b.ip, sizeof(a.ip)); }
void SV_VoicePacket(netadr_t from, msg_t *message);
void SV_QueueVoicePacket(int talker, int listener, VoicePacket_t *packet);
Bool SV_ClientHasClientMuted(int listener, int talker);
Bool SV_ClientWantsVoiceData(int listener);

void NET_OutOfBandVoiceData(netsrc_t socket, netadr_t address, byte *bytes, int length)
{
    (void)address;
    assert(length >= 3 && bytes[0] == 'v' && bytes[1] == 0);
    if (socket == NS_CLIENT1) {
        assert(sentCount < 16);
        sentFrames[sentCount++] = bytes[4];
        clientDatagrams++;
        msg_t message = {0};
        message.data = bytes;
        message.cursize = length;
        message.readcount = 2;
        SV_VoicePacket(serverClients[expectedTalker].netchan.remoteAddress, &message);
        assert(message.readcount == length && !message.overflowed);
    } else {
        assert(socket == NS_SERVER && length <= sizeof(serverWire));
        memcpy(serverWire, bytes, length);
        serverBytes = length;
    }
}

#include "relay_source.h"

static void ResetRouting(void)
{
    memset(serverClients, 0, sizeof(serverClients));
    memset(gameClients, 0, sizeof(gameClients));
    memset(g_entities, 0, sizeof(g_entities));
    svs.clients = serverClients;
    maxClients.current.integer = 64;
    serverVoice.current.enabled = voiceEnabled.current.enabled = 1;
    globalVoice.current.enabled = echoVoice.current.enabled = deadVoice.current.enabled = 0;
    level.time = 2000;
    for (int i = 0; i < 3; i++) {
        g_entities[i].s.number = i;
        g_entities[i].r.inuse = 1;
        g_entities[i].client = &gameClients[i];
        gameClients[i].sess.sessionState = SESS_STATE_PLAYING;
        gameClients[i].sess.cs.team = i == 1 ? TEAM_AXIS : TEAM_ALLIES;
        serverClients[i].state = 4;
        serverClients[i].sendVoice = 1;
        *(gentity_t **)&serverClients[i].gentity = &g_entities[i];
        serverClients[i].netchan.remoteAddress.port = 31000 + i;
        serverClients[i].netchan.qport = 200 + i;
    }
    clientDatagrams = sentCount = serverBytes = 0;
    clients[0].voicePacketCount = 0;
    clientConnections[0].state = CA_ACTIVE;
    clientConnections[0].netchan.sock = NS_CLIENT1;
    expectedTalker = 0;
    clientConnections[0].qport = 200;
}

static void Receive(int listener, const byte *packet, int length, int count)
{
    serverBytes = 0;
    SV_SendClientVoiceData(&serverClients[listener]);
    assert(serverBytes == 3 + count * (length + 2));
    assert(serverWire[2] == count && !serverClients[listener].voicePacketCount);
    for (int i = 0; i < count; i++) {
        const byte *frame = serverWire + 3 + i * (length + 2);
        assert(frame[0] == expectedTalker && frame[1] == length);
        assert(!memcmp(frame + 2, packet, length));
    }
    msg_t message = {0};
    message.data = serverWire;
    message.cursize = serverBytes;
    message.readcount = 2;
    now += 300;
    assert(!Voice_IsClientTalking(expectedTalker));
    CL_VoicePacket(&message);
    assert(message.readcount == serverBytes && !message.overflowed);
    Voice_Playback();
    assert(Voice_IsClientTalking(expectedTalker) && RenderVoice() > 0);
    now += 299;
    assert(Voice_IsClientTalking(expectedTalker));
    now++;
    assert(!Voice_IsClientTalking(expectedTalker));
    while (RenderVoice()) {}
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    assert(argc == 2);
    byte packet[256];
    int length = (int)strlen(argv[1]) / 2;
    assert(length > 0 && length < 256);
    for (int i = 0; i < length; i++) {
        unsigned value;
        assert(sscanf(argv[1] + 2 * i, "%2x", &value) == 1);
        packet[i] = value;
    }
    imp_speex_nb_mode = (void *)&speex_nb_mode;
    Voice_Init();
    assert(g_voice_initialized && g_frame_size == 160);
    ResetRouting();
    clients[0].serverTime = 1000;
    clients[0].voicePacketLastTransmit = 1000;
    for (int i = 0; i < 9; i++)
        assert(Client_SendVoiceData(length, (char *)packet) == length);
    assert(!clientDatagrams && clients[0].voicePacketCount == 9);
    Client_SendVoiceData(length, (char *)packet);
    assert(clientDatagrams == 1 && sentFrames[0] == 10 && !clients[0].voicePacketCount);
    assert(!serverClients[0].voicePacketCount && !serverClients[1].voicePacketCount);
    assert(serverClients[2].voicePacketCount == 10 && gameClients[0].lastVoiceTime == level.time);
    Receive(2, packet, length, 10);
    puts("PASS ten-frame cadence and cross-client team relay -> production Speex/native Render");

    ResetRouting();
    echoVoice.current.enabled = 1;
    clients[0].serverTime = 2000;
    Client_SendVoiceData(length, (char *)packet);
    assert(sentFrames[0] == 1);
    Receive(0, packet, length, 1);
    for (int i = 0; i < 7; i++)
        Client_SendVoiceData(length, (char *)packet);
    clients[0].serverTime = 2199;
    CL_VoiceTransmit();
    assert(sentCount == 1 && clients[0].voicePacketCount == 7);
    clients[0].serverTime = 2200;
    CL_VoiceTransmit();
    assert(sentCount == 2 && sentFrames[1] == 7 && !clients[0].voicePacketCount);
    Receive(0, packet, length, 7);
    puts("PASS one/seven-frame cadence at 199/200 ms; original local echo and 299/300 ms talk timeout");

    ResetRouting();
    gameClients[1].sess.sessionState = SESS_STATE_SPECTATOR;
    VoicePacket_t voicePacket = {0};
    voicePacket.dataSize = length;
    memcpy(voicePacket.data, packet, length);
    G_BroadcastVoice(&g_entities[0], &voicePacket);
    assert(serverClients[1].voicePacketCount == 1 && "spectators must receive other teams' voice with voice_global off");
    Receive(1, packet, length, 1);
    puts("PASS original spectator team bypass -> production decode/playback");

    ResetRouting();
    globalVoice.current.enabled = 1;
    serverClients[1].muteList[0] = 1;
    serverClients[2].sendVoice = 0;
    G_BroadcastVoice(&g_entities[0], &voicePacket);
    for (int i = 0; i < 3; i++) assert(!serverClients[i].voicePacketCount);
    serverClients[1].muteList[0] = 0;
    gameClients[1].sess.sessionState = SESS_STATE_DEAD;
    G_BroadcastVoice(&g_entities[0], &voicePacket);
    assert(!serverClients[1].voicePacketCount);
    deadVoice.current.enabled = 1;
    G_BroadcastVoice(&g_entities[0], &voicePacket);
    assert(serverClients[1].voicePacketCount == 1);
    puts("PASS echo/global/dead chat, server mute and sendVoice gates");
    Voice_Shutdown();
    return 0;
}
