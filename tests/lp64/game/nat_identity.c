/* Real admission, packet routing, sequence checks, script VM and late auth.
 * Message reads use memcpy at the fixture seam for packed wire alignment;
 * decoding, game/voice consumption and profiling are external services here. */
static dvar_t quietNetwork;
const dvar_t *showpackets = &quietNetwork, *showdrop = &quietNetwork;
const dvar_t *net_showprofile = &quietNetwork;
static int net_iProfilingOn;
static char *netsrcString[2] = {"client", "server"};
static int gamePackets[2], voicePackets[2];
static void *fixtureBgs;
void *imp_bgs = &fixtureBgs;

void NetProf_AddPacket_core(netProfileStream_t *p, int size, qboolean fragmented)
{ (void)p; (void)size; (void)fragmented; }
void MSG_BeginReading(msg_t *msg) { msg->readcount = 0; msg->bit = 0; msg->overflowed = 0; }
int MSG_ReadByte(msg_t *msg)
{
    if (msg->readcount >= msg->cursize) { msg->overflowed = 1; return -1; }
    return msg->data[msg->readcount++];
}
int MSG_ReadShort(msg_t *msg)
{
    short value;
    if (msg->readcount + 2 > msg->cursize) { msg->overflowed = 1; return -1; }
    memcpy(&value, msg->data + msg->readcount, 2); msg->readcount += 2;
    return value;
}
int MSG_ReadLong(msg_t *msg)
{
    int value;
    if (msg->readcount + 4 > msg->cursize) { msg->overflowed = 1; return -1; }
    memcpy(&value, msg->data + msg->readcount, 4); msg->readcount += 4;
    return value;
}
void SV_ResetSkeletonCache(void) {}
void SV_ConnectionlessPacket(netadr_t from, msg_t *msg) { (void)from; (void)msg; abort(); }
void SV_PktEvtDbg(const char *s, int result, int state, int id, int ack)
{ (void)s; (void)result; (void)state; (void)id; (void)ack; }
void SV_Netchan_Decode(client_t *cl, byte *data, int size) { (void)cl; (void)data; (void)size; }
void SV_ExecuteClientMessage(client_t *cl, msg_t *msg)
{ (void)msg; ++gamePackets[cl - clients]; }
void SV_PreGameUserVoice(client_t *cl, msg_t *msg)
{ (void)msg; ++voicePackets[cl - clients]; }
void SV_UserVoice(client_t *cl, msg_t *msg)
{ (void)msg; ++voicePackets[cl - clients]; }
#if defined(COD2_CODX) && COD2_CODX
#include "PC/server_mp/cod2x_udp_operations.h"
int SV_Cod2xUdpAllow(netadr_t from, cod2xUdpOperation_t operation)
{ (void)from; (void)operation; return 1; }
#endif

#include "nat_packet_source.h"

static void nat_send(int voice, netadr_t from, int qport, int sequence)
{
    byte data[16] __attribute__((aligned(16))) = {0};
    short wireQport = (short)qport;
    msg_t msg = {0};
    if (voice) {
        memcpy(data, &wireQport, 2);
        msg.cursize = 2;
    } else {
        memcpy(data, &sequence, 4);
        memcpy(data + 4, &wireQport, 2);
        data[6] = 1; /* server id; zero message/reliable acknowledgments follow */
        msg.cursize = 15;
    }
    msg.data = data; msg.maxsize = sizeof(data);
    if (voice) SV_VoicePacket(from, &msg);
    else SV_PacketEvent(from, &msg);
}

static void nat_reply(netadr_t from, int challenge, const char *reason)
{
    char token[32];
    snprintf(token, sizeof(token), "%d", challenge);
    argc = 5; argv[0] = "ipAuthorize"; argv[1] = token;
    argv[2] = "accept"; argv[3] = reason; argv[4] = "123";
    SV_AuthorizeIpPacket(from);
}

static void nat_admit(netadr_t from, int qport, const char *id, const char *hash, const char *reason)
{
    argc = 3; argv[0] = "getchallenge"; argv[1] = "0"; argv[2] = hash;
    SV_GetChallenge(from);
    int slot = -1;
    for (int i = 0; i < 1024; ++i)
        if (NET_CompareAdr(svs.challenges[i].adr, from)) { slot = i; break; }
    assert(slot >= 0);
    int challenge = svs.challenges[slot].challenge;
    if (reason) nat_reply(svs.authorizeAddress, challenge, reason);
    else { svs.time += 6000; SV_GetChallenge(from); } /* actual timeout admission */
    char info[512];
    snprintf(info, sizeof(info), "\\protocol\\118\\protocol_cod2x\\6\\cl_hwid2\\%s\\challenge\\%d\\qport\\%d\\name\\nat-fixture", id, challenge, qport);
    argc = 2; argv[0] = "connect"; argv[1] = info;
    SV_DirectConnect(from);
}

static void nat_values(int slot, const char *hash, const char *reason, const char *id)
{
#if defined(COD2_CODX) && COD2_CODX
    string_value("getCDKeyHash", slot, hash);
    string_value("getAuthorizationStatus", slot, reason);
    string_value("getHWID", slot, id);
#else
    (void)slot; (void)hash; (void)reason; (void)id;
    static const char *names[] = {"getCDKeyHash", "getAuthorizationStatus", "getHWID"};
    for (unsigned int i = 0; i < 3; ++i) {
        const char *name = names[i]; int dev;
        Verify(!Scr_GetMethod(&name, &dev), "stock has no CoD2x identity methods");
    }
#endif
}

static void nat_case(int voice, int hostRevision)
{
    int initialFailures = failures;
    reset(); init_game(); revision = hostRevision; entityClass = 0;
    memset(gamePackets, 0, sizeof(gamePackets)); memset(voicePackets, 0, sizeof(voicePackets));
#if defined(COD2_CODX) && COD2_CODX
    SV_Cod2xClearIdentities();
#endif
    const char *expectedId = hostRevision ? hwid : "";
    nat_admit(remote, 20, hwid, digest, "FIRST_REPLY");
    Verify(connects == 1 && clients[0].state == 2, "real first client admission");
    nat_values(0, digest, "FIRST_REPLY", expectedId);
    netadr_t translated = remote; translated.port = 200;
    svs.time += 100;
    nat_send(voice, translated, 20, 1);
    Verify(clients[0].netchan.remoteAddress.port == 200, "packet handler repairs translated port");
    Verify(clients[0].lastPacketTime == svs.time, "translated packet reaches admitted client");
    nat_values(0, digest, "FIRST_REPLY", expectedId);
    nat_reply(svs.authorizeAddress, clients[0].challenge, "LATE_REPLY");
    nat_values(0, digest, "LATE_REPLY", expectedId);

    /* Neither a different IP nor an unknown qport can rebind this slot. */
    netadr_t differentIp = translated; ++differentIp.ip[3]; differentIp.port = 201;
    int lastTime = clients[0].lastPacketTime;
    svs.time += 100;
    nat_send(voice, differentIp, 20, 2);
    nat_send(voice, differentIp, 21, 2);
    nat_send(voice, (netadr_t){NA_IP, {8,7,6,5}, 202}, 21, 2);
    Verify(clients[0].netchan.remoteAddress.port == 200 && clients[0].lastPacketTime == lastTime,
           "different IP or qport does not rebind admitted client");
    nat_values(0, digest, "LATE_REPLY", expectedId);

    /* Same base IP, separate qport and challenge: route to its own identity. */
    const char *secondHash = "FFEEDDCCBBAA99887766554433221100";
    const char *secondId = "ZYXWVUTSRQPONMLKJIHGFEDCBA987654";
    netadr_t second = remote; second.port = 300;
    nat_admit(second, 21, secondId, secondHash, "SECOND_REPLY");
    Verify(connects == 2 && clients[1].state == 2, "same IP with another qport admits separate client");
    second.port = 301; nat_send(voice, second, 21, 1);
    Verify(clients[0].netchan.remoteAddress.port == 200 && clients[1].netchan.remoteAddress.port == 301,
           "matching qport rebinds only its own slot");
    nat_values(0, digest, "LATE_REPLY", expectedId);
    nat_values(1, secondHash, "SECOND_REPLY", hostRevision ? secondId : "");
    nat_reply(svs.authorizeAddress, clients[1].challenge, "SECOND_LATE_REPLY");
    nat_values(0, digest, "LATE_REPLY", expectedId);
    nat_values(1, secondHash, "SECOND_LATE_REPLY", hostRevision ? secondId : "");

#if defined(COD2_CODX) && COD2_CODX
    /* Membership still requires the admitted challenge, base IP and qport. */
    ++clients[0].challenge;
    nat_values(0, "", "", "");
    nat_reply(svs.authorizeAddress, clients[0].challenge - 1, "WRONG_CHALLENGE_REPLY");
    --clients[0].challenge;
    nat_values(0, digest, "LATE_REPLY", expectedId);
    ++clients[0].netchan.remoteAddress.ip[3]; nat_values(0, "", "", "");
    --clients[0].netchan.remoteAddress.ip[3];
    ++clients[0].netchan.qport; nat_values(0, "", "", ""); --clients[0].netchan.qport;
#endif
    if (!voice) {
        /* Mac rewrites the port even before rejecting an old sequence. */
        translated.port = 203; lastTime = clients[0].lastPacketTime;
        svs.time += 100; nat_send(0, translated, 20, 1);
        Verify(clients[0].netchan.remoteAddress.port == 203 && clients[0].lastPacketTime == lastTime,
               "rejected sequence retains original port-rewrite order");
        nat_values(0, digest, "LATE_REPLY", expectedId);
    }

    int oldChallenge = clients[0].challenge;
    SV_FreeClient(&clients[0]); nat_values(0, "", "", "");
    clients[0].state = 0;
    netadr_t replacement = remote; replacement.port = 400;
    nat_admit(replacement, 22, secondId, secondHash, NULL);
    Verify(connects == 3 && clients[0].state == 2, "freed slot admits replacement connection");
    nat_values(0, secondHash, "", hostRevision ? secondId : "");
    nat_reply(svs.authorizeAddress, oldChallenge, "STALE_REPLY");
    nat_values(0, secondHash, "", hostRevision ? secondId : "");
    replacement.port = 401; nat_send(voice, replacement, 22, 1);
    nat_values(0, secondHash, "", hostRevision ? secondId : "");
    netadr_t forged = svs.authorizeAddress; ++forged.ip[3];
    nat_reply(forged, clients[0].challenge, "FORGED_REPLY");
    nat_values(0, secondHash, "", hostRevision ? secondId : "");
    nat_reply(svs.authorizeAddress, clients[0].challenge, "TIMEOUT_LATE_REPLY");
    nat_values(0, secondHash, "TIMEOUT_LATE_REPLY", hostRevision ? secondId : "");
    Verify((voice ? voicePackets[0] : gamePackets[0]) == 2 &&
           (voice ? voicePackets[1] : gamePackets[1]) == 1,
           "game/voice handlers dispatch only matching fresh connections");
    printf("%s packets, host revision %d: %d failures\n",
           voice ? "voice" : "game", hostRevision, failures - initialFailures);
}

static void nat_identity_cases(void)
{
    for (int voice = 0; voice <= 1; ++voice) {
        nat_case(voice, 0); /* production host revision stays locked at zero */
#if defined(COD2_CODX) && COD2_CODX
        nat_case(voice, 6); /* synthetic extension admission also retains HWID */
#endif
    }
}
