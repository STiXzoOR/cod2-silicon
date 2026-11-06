#if defined(COD2_X64)
#include "PC/qcommon/port_debug.h"
#endif
#include "common_types.h"
#include "imports.h"
#include <stddef.h>
#include <stdio.h>

COD2_ASSERT_FIELD(client_t, reliableAcknowledge,     0x20810);
COD2_ASSERT_FIELD(client_t, messageAcknowledge,      0x20818);
COD2_ASSERT_FIELD(client_t, challenge,               0x20820);
COD2_ASSERT_FIELD(client_t, reliableCommandInfo,     0x40c);
COD2_ASSERT_FIELD(client_t, lastClientCommandString, 0x20844);
COD2_ASSERT_FIELD(client_t, name,                    0x20c48);
COD2_ASSERT_FIELD(client_t, state,                   0x0);

#define CLIENT_RELIABLEACK_OFF 0x20810
#define CLIENT_MSGACK_OFF 0x20818
#define CLIENT_CHALLENGE_OFF 0x20820
#define CLIENT_RELCMDINFO_OFF 0x40c
#define CLIENT_LASTCLIENTCMDSTR_OFF 0x20844
#define CLIENT_NAME_OFF 0x20c48
#define CLIENT_STATE_OFF 0x0
#define CLIENT_NETCHAN_OFF ((int)offsetof(client_t, netchan))
#define CLIENT_NETCHAN_PPROF_OFF ((int)(offsetof(client_t, netchan) + offsetof(netchan_t, pProf)))
#define CLIENT_SERVERID_OFF ((int)offsetof(client_t, serverId))
#define CLIENT_STRIDE ((int)sizeof(client_t))
#define SVSCMD_SIZE 1032

extern byte svs_ptr[];
extern byte sv_ptr[];
extern byte *net_profile_dvar;

#define SVS_CLIENTS_OFF ((int)offsetof(serverStatic_t, clients))
#define SVS_POOBPROF_OFF ((int)offsetof(serverStatic_t, pOOBProf))

extern Bool Netchan_TransmitNextFragment(netchan_t *chan);
extern Bool Netchan_Transmit(netchan_t *chan, int length, const byte *data);
#if defined(COD2_X64)
extern serverStatic_t svs;
extern const dvar_t *sv_maxclients;
extern const dvar_t *net_profile;
extern void NetProf_PrepProfiling(netProfileInfo_t **prof);
#else
extern void NetProf_PrepProfiling(netProfileInfo_t *prof);
#endif
extern void NetProf_AddPacket(netProfileStream_t *stream, int iLength, qboolean bFragment);
extern void NetProf_UpdateStatistics(netProfileStream_t *stream);
extern int Com_sprintf(char *dest, int size, const char *fmt, ...);
extern void Com_Printf(const char *fmt, ...);
extern void CL_DrawString(int x, int y, const char *str, int color, int size);

void SV_Netchan_Decode(client_t *client, byte *data, int size);
Bool SV_Netchan_TransmitNextFragment(netchan_t *chan);
Bool SV_Netchan_Transmit(client_t *client, int length, byte *data);
void SV_Netchan_AddOOBProfilePacket(int iLength);
void SV_Netchan_PrintProfileStats(qboolean bPrintToConsole);

static float _fminf(float a, float b)
{
    return (a < b) ? a : b;
}

static float _fmaxf(float a, float b)
{
    return (a > b) ? a : b;
}

void SV_Netchan_Decode(client_t *client, byte *data, int size)
{
    int reliableAcknowledge;
    const char *string;
    byte key;
    int index;
    int i;

    reliableAcknowledge = client->reliableAcknowledge;
    string = client->reliableCommandInfo[reliableAcknowledge & 0x7f].cmd;

    key = (byte)client->serverId;
    key ^= (byte)client->challenge;
    key ^= (byte)client->messageAcknowledge;

    index = 0;
    for (i = 0; i < size; i++) {
        byte ch;

        if (string[index] == '\0') {
            ch = (unsigned char)string[0];
            index = 1;
        } else {
            ch = (unsigned char)string[index];
            index++;
        }

        key ^= (byte)(ch << (i & 1));

        data[i] ^= key;
    }
}

Bool SV_Netchan_TransmitNextFragment(netchan_t *chan)
{
    return Netchan_TransmitNextFragment(chan);
}

Bool SV_Netchan_Transmit(client_t *client, int length, byte *data)
{
    int outgoingSequence;
    const char *string;
    byte key;
    int index;
    int dataSize;
    byte *encodeData;
    int i;
    netchan_t *netchan;
#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
    static int x64TraceCount;
    byte beforeTrace[16];
    int beforeTraceLen;
#endif

    dataSize = length - 4;
    encodeData = data + 4;

    string = client->lastClientCommandString;

    outgoingSequence = client->netchan.outgoingSequence;
    key = (byte)client->challenge;
    key ^= (byte)outgoingSequence;

    index = 0;
#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
    beforeTraceLen = dataSize < (int)sizeof(beforeTrace) ? dataSize : (int)sizeof(beforeTrace);
    for (i = 0; i < beforeTraceLen; ++i) {
        beforeTrace[i] = encodeData[i];
    }
#endif
    for (i = 0; i < dataSize; i++) {
        byte ch;

        if (string[index] == '\0') {
            ch = (unsigned char)string[0];
            index = 1;
        } else {
            ch = (unsigned char)string[index];
            index++;
        }

        key ^= (byte)(ch << (i & 1));

        encodeData[i] ^= key;
    }

#if defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__)
#if defined(COD2_X64)
    if (COD2_NATIVE_TRACE_ENABLED() && x64TraceCount < 12) {
#else
    if (x64TraceCount < 12) {
#endif
        FILE *f = fopen("x64_netchan_trace.txt", x64TraceCount ? "a" : "w");
        if (f) {
            fprintf(f,
                    "svenc[%d] len=%d dataSize=%d seq=%d challenge=%d keyEnd=%02x string=\"%.48s\" before:",
                    x64TraceCount,
                    length,
                    dataSize,
                    outgoingSequence,
                    client->challenge,
                    key,
                    string);
            for (i = 0; i < beforeTraceLen; ++i) {
                fprintf(f, " %02x", beforeTrace[i]);
            }
            fprintf(f, " after:");
            for (i = 0; i < beforeTraceLen; ++i) {
                fprintf(f, " %02x", encodeData[i]);
            }
            fprintf(f, "\n");
            fclose(f);
        }
        x64TraceCount++;
    }
#endif

    netchan = &client->netchan;
    return Netchan_Transmit(netchan, length, data);
}

void SV_Netchan_AddOOBProfilePacket(int iLength)
{
    netProfileInfo_t *pOOBProf;

    /* +8 was the x86 offset of dvar->current; on x64 the name pointer + alignment move current to
       16, so the hardcoded read got garbage and skipped the disabled-profiling early-out. */
#if defined(COD2_X64)
    if (!net_profile->current.integer)
#else
    if ((*(const dvar_t **)net_profile_dvar)->current.integer == 0)
#endif
        return;

#if defined(COD2_X64)
    NetProf_PrepProfiling(&svs.pOOBProf);
    if (svs.pOOBProf)
        NetProf_AddPacket(&svs.pOOBProf->send, iLength, 0);
#else
    {
        byte *svs = (byte *)imp_svs;
        NetProf_PrepProfiling((netProfileInfo_t *)(svs + SVS_POOBPROF_OFF));
        pOOBProf = *(netProfileInfo_t **)(svs + SVS_POOBPROF_OFF);
        NetProf_AddPacket((netProfileStream_t *)pOOBProf, iLength, 0);
    }
#endif
}

#if defined(COD2_X64)
extern int Dvar_GetInt(const char *name);

static void SV_ProfileLineLocal(const char *line, qboolean console, int *y, int step)
{
    if (console) {
        Com_Printf("%s\n", line);
    } else {
        *y += step;
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(32, *y, line, 0, Dvar_GetInt("cl_profileTextHeight"));
#endif
    }
}

static void SV_ProfileTotalWordsLocal(int largest, int smallest, int words[3])
{
    /* Mac 0x190d5d/0x190d63 passes two doubles to integer conversions.
       Preserve the three i386 words consumed by the format on native ABIs. */
    double largeValue = (double)(float)largest;
    double smallValue = (double)(float)smallest;
    unsigned long long largeBits, smallBits;
    memcpy(&largeBits, &largeValue, sizeof(largeBits));
    memcpy(&smallBits, &smallValue, sizeof(smallBits));
    words[0] = (int)(unsigned int)largeBits;
    words[1] = (int)(unsigned int)(largeBits >> 32);
    words[2] = (int)(unsigned int)smallBits;
}

void SV_Netchan_PrintProfileStats(qboolean bPrintToConsole)
{
    char line[1024];
    int y = 90;
    int bytes[2] = {0, 0};
    int packets[2] = {0, 0};
    int fragments[2] = {0, 0};
    int largest[2] = {0, 0};
    int smallest[2] = {9999, 9999};
    int words[3];
    int i, side;
    netProfileInfo_t *prof;

#if COD2_APPLE_SDK && defined(DEDICATED)
    bPrintToConsole = 1;
#endif
    if (!svs.clients)
        return;

    if (svs.pOOBProf) {
        NetProf_UpdateStatistics(&svs.pOOBProf->send);
        NetProf_UpdateStatistics(&svs.pOOBProf->recieve);
    }
    for (i = 0; i < sv_maxclients->current.integer; ++i) {
        client_t *client = &svs.clients[i];
        if (client->state && client->netchan.pProf) {
            NetProf_UpdateStatistics(&client->netchan.pProf->send);
            NetProf_UpdateStatistics(&client->netchan.pProf->recieve);
        }
    }

    if (bPrintToConsole)
        Com_Printf("\n\n");
    SV_ProfileLineLocal("====================", bPrintToConsole, &y, 0);
    if (bPrintToConsole) {
        Com_Printf("Server Network Profile:\n\n");
        y = 80;
    } else {
        SV_ProfileLineLocal("Server Network Profile:", 0, &y, 10);
        y += 10;
    }
    SV_ProfileLineLocal("                    | Sent To                         | Recieved From          | Total Source Traffic   |", bPrintToConsole, &y, 10);
    SV_ProfileLineLocal("              Source|   bps|  max|  min|frag%|drop%|ak|   bps|  max|  min|frag%|   bps|  max|  min|frag%|", bPrintToConsole, &y, 10);

    prof = svs.pOOBProf;
    for (i = -1; i < sv_maxclients->current.integer; ++i) {
        if (i >= 0) {
            if (!svs.clients[i].state)
                continue;
            prof = svs.clients[i].netchan.pProf;
        }
        if (!prof)
            continue;
        for (side = 0; side < 2; ++side) {
            netProfileStream_t *stream = side ? &prof->recieve : &prof->send;
            bytes[side] += stream->iBytesPerSecond;
            packets[side] += stream->iCountedPackets;
            fragments[side] += stream->iCountedFragments;
            if (stream->iLargestPacket > largest[side])
                largest[side] = stream->iLargestPacket;
            if (stream->iSmallestPacket < smallest[side])
                smallest[side] = stream->iSmallestPacket;
        }
    }
    SV_ProfileTotalWordsLocal(largest[0] > largest[1] ? largest[0] : largest[1],
                              smallest[0] < smallest[1] ? smallest[0] : smallest[1], words);
    Com_sprintf(line, sizeof(line), "              Totals:%6i|%5i|%5i| %3i%%| %3i%%|%2i|%6i|%5i|%5i| %3i%%|%6i|%5i|%5i| %3i%%|",
                bytes[0], largest[0], smallest[0], packets[0] ? fragments[0] * 100 / packets[0] : 0,
                0, 0, bytes[1], largest[1], smallest[1], packets[1] ? fragments[1] * 100 / packets[1] : 0,
                bytes[0] + bytes[1], words[0], words[1], words[2]);
    SV_ProfileLineLocal(line, bPrintToConsole, &y, 10);

    prof = svs.pOOBProf;
    if (prof) {
        netProfileStream_t *send = &prof->send;
        netProfileStream_t *recv = &prof->recieve;
        SV_ProfileTotalWordsLocal(send->iLargestPacket > recv->iLargestPacket ? send->iLargestPacket : recv->iLargestPacket,
                                  send->iSmallestPacket < recv->iSmallestPacket ? send->iSmallestPacket : recv->iSmallestPacket, words);
        Com_sprintf(line, sizeof(line), "  OutOfBand Messages: %5i|%5i|%5i| %3i%%| %3i%%|%2i| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                    send->iBytesPerSecond, send->iLargestPacket, send->iSmallestPacket, send->iFragmentPercentage,
                    0, 0, recv->iBytesPerSecond, recv->iLargestPacket, recv->iSmallestPacket, recv->iFragmentPercentage,
                    send->iBytesPerSecond + recv->iBytesPerSecond, words[0], words[1], words[2]);
    } else {
        Com_sprintf(line, sizeof(line), "  OutOfBand Messages:     0|    0|    0|   - |   - |  |     0|    0|    0|   - |     0|    0|    0|   - |");
    }
    SV_ProfileLineLocal(line, bPrintToConsole, &y, 10);

    for (i = 0; i < sv_maxclients->current.integer; ++i) {
        client_t *client = &svs.clients[i];
        char name[17];
        int first, previous, pending, acknowledged, losses, j;
        if (!client->state)
            continue;
        strncpy(name, client->name, sizeof(name));
        name[16] = '\0';
        prof = client->netchan.pProf;
        if (!prof) {
            Com_sprintf(line, sizeof(line), "#%2i-%16s:     0|    0|    0|   0%%|     0|    0|    0|   0%%|     0|    0|    0|   0%%|", i, name);
        } else {
            netProfileStream_t *send = &prof->send;
            netProfileStream_t *recv = &prof->recieve;
            previous = 0;
            for (first = 0; first < 32; ++first) {
                int sent = client->frames[first].messageSent;
                if (sent <= previous)
                    break;
                previous = sent;
            }
            losses = 0;
            acknowledged = 0;
            pending = 0;
            for (j = 0; j < 32; ++j) {
                if (client->frames[(j + first) & 31].messageAcked > 0) {
                    losses += pending;
                    ++acknowledged;
                    pending = 0;
                } else {
                    ++pending;
                }
            }
            SV_ProfileTotalWordsLocal(send->iLargestPacket > recv->iLargestPacket ? send->iLargestPacket : recv->iLargestPacket,
                                      send->iSmallestPacket < recv->iSmallestPacket ? send->iSmallestPacket : recv->iSmallestPacket, words);
            Com_sprintf(line, sizeof(line), "#%2i-%16s: %5i|%5i|%5i| %3i%%| %3i%%|%2i| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                        i, name, send->iBytesPerSecond, send->iLargestPacket, send->iSmallestPacket, send->iFragmentPercentage,
                        acknowledged ? losses * 100 / (losses + acknowledged) : 100, pending,
                        recv->iBytesPerSecond, recv->iLargestPacket, recv->iSmallestPacket, recv->iFragmentPercentage,
                        send->iBytesPerSecond + recv->iBytesPerSecond, words[0], words[1], words[2]);
        }
        SV_ProfileLineLocal(line, bPrintToConsole, &y, 10);
    }
}
#else
void SV_Netchan_PrintProfileStats(qboolean bPrintToConsole)
{
    char szLine[1024];
    int iYPos;
    int iTotalBPSSent;
    int iTotalPacketsSent;
    int iTotalFragmentsSent;
    int iTotalBPSRecieved;
    int iTotalPacketsRecieved;
    int iTotalFragmentsRecieved;
    int iTotalMaxSent;
    int iTotalMinSent;
    int iTotalMaxRecieved;
    int iTotalMinRecieved;
    int iFragmentTotal;
    netProfileInfo_t *pOOBProf;
    netProfileInfo_t *pProf;
#if defined(COD2_X64)
    client_t *clientBase;
#else
    byte *clientBase;
#endif
    int numClients;
    int totalPackets;
    char szClientName[17];
    int i;

#if COD2_APPLE_SDK && defined(DEDICATED)
    bPrintToConsole = 1;
#endif

#if defined(COD2_X64)
    clientBase = svs.clients;
#else
    clientBase = *(byte **)(svs_ptr + SVS_CLIENTS_OFF);
#endif
    if (clientBase == NULL)
        return;

#if defined(COD2_X64)
    pOOBProf = svs.pOOBProf;
#else
    pOOBProf = *(netProfileInfo_t **)(svs_ptr + SVS_POOBPROF_OFF);
#endif
    if (pOOBProf != NULL) {
        NetProf_UpdateStatistics(&pOOBProf->send);
        NetProf_UpdateStatistics(&pOOBProf->recieve);
    }

#if defined(COD2_X64)
    numClients = sv_maxclients->current.integer;
#else
    numClients = *(int *)((byte *)sv_ptr + 8);
#endif
    for (i = 0; i < numClients; i++) {
#if defined(COD2_X64)
        client_t *cl = &clientBase[i];
#else
        byte *cl = clientBase + (long)i * CLIENT_STRIDE;
#endif
#if defined(COD2_X64)
        if (!cl->state)
#else
        if (*(int *)(cl + CLIENT_STATE_OFF) == 0)
#endif
            continue;
#if defined(COD2_X64)
        pProf = cl->netchan.pProf;
#else
        pProf = *(netProfileInfo_t **)(cl + CLIENT_NETCHAN_PPROF_OFF);
#endif
        if (pProf == NULL)
            continue;
        NetProf_UpdateStatistics(&pProf->send);
        NetProf_UpdateStatistics(&pProf->recieve);
    }

    if (bPrintToConsole) {
        Com_Printf("\n\n");
    }

    Com_sprintf(szLine, 1024, "====================");
    if (bPrintToConsole) {
        Com_Printf("%s\n", szLine);
        iYPos = 0x50;
    } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(0x20, 0x5a, szLine, 0, 8);
#endif
        iYPos = 0x5a;
    }

    Com_sprintf(szLine, 1024, "Server Network Profile:");
    if (bPrintToConsole) {
        Com_Printf("%s\n\n", szLine);
    } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(0x20, iYPos + 0xa, szLine, 0, 8);
#endif
        iYPos += 0x14;
    }

    Com_sprintf(szLine, 1024,
                "                    | Sent To                | Recieved From          | Total Source Traffic   |");
    if (bPrintToConsole) {
        Com_Printf("%s\n", szLine);
    } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(0x20, iYPos + 0xa, szLine, 0, 8);
#endif
        iYPos += 0xa;
    }

    Com_sprintf(szLine, 1024,
                "              Source|   bps|  max|  min|frag%%|   bps|  max|  min|frag%%|   bps|  max|  min|frag%%|");
    if (bPrintToConsole) {
        Com_Printf("%s\n", szLine);
    } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(0x20, iYPos + 0xa, szLine, 0, 8);
#endif
        iYPos += 0xa;
    }

#if defined(COD2_X64)
    pOOBProf = svs.pOOBProf;
#else
    pOOBProf = *(netProfileInfo_t **)(svs_ptr + SVS_POOBPROF_OFF);
#endif
    if (pOOBProf != NULL) {
        iTotalBPSSent = pOOBProf->send.iBytesPerSecond;
        iTotalPacketsSent = pOOBProf->send.iCountedPackets;
        iTotalFragmentsSent = pOOBProf->send.iCountedFragments;
        iTotalBPSRecieved = pOOBProf->recieve.iBytesPerSecond;
        iTotalPacketsRecieved = pOOBProf->recieve.iCountedPackets;
        iTotalFragmentsRecieved = pOOBProf->recieve.iCountedFragments;

        iTotalMaxSent = pOOBProf->send.iLargestPacket;
        if (iTotalMaxSent <= 0)
            iTotalMaxSent = 0;

        iTotalMinSent = pOOBProf->send.iSmallestPacket;
        if (iTotalMinSent >= 9999)
            iTotalMinSent = 9999;

        iTotalMaxRecieved = pOOBProf->recieve.iLargestPacket;
        if (iTotalMaxRecieved <= 0)
            iTotalMaxRecieved = 0;

        iTotalMinRecieved = pOOBProf->recieve.iSmallestPacket;
        if (iTotalMinRecieved >= 9999)
            iTotalMinRecieved = 9999;
    } else {
        iTotalBPSSent = 0;
        iTotalBPSRecieved = 0;
        iTotalPacketsSent = 0;
        iTotalFragmentsSent = 0;
        iTotalPacketsRecieved = 0;
        iTotalFragmentsRecieved = 0;
        iTotalMaxSent = 0;
        iTotalMinSent = 9999;
        iTotalMaxRecieved = 0;
        iTotalMinRecieved = 9999;
    }

#if defined(COD2_X64)
    numClients = sv_maxclients->current.integer;
#else
    numClients = *(int *)((byte *)sv_ptr + 8);
#endif
    for (i = 0; i < numClients; i++) {
#if defined(COD2_X64)
        client_t *cl = &clientBase[i];
#else
        byte *cl = clientBase + (long)i * CLIENT_STRIDE;
#endif
#if defined(COD2_X64)
        if (!cl->state)
#else
        if (*(int *)(cl + CLIENT_STATE_OFF) == 0)
#endif
            continue;
#if defined(COD2_X64)
        pProf = cl->netchan.pProf;
#else
        pProf = *(netProfileInfo_t **)(cl + CLIENT_NETCHAN_PPROF_OFF);
#endif
        if (pProf == NULL)
            continue;

        iTotalBPSSent += pProf->send.iBytesPerSecond;
        iTotalPacketsSent += pProf->send.iCountedPackets;
        iTotalFragmentsSent += pProf->send.iCountedFragments;
        iTotalBPSRecieved += pProf->recieve.iBytesPerSecond;
        iTotalPacketsRecieved += pProf->recieve.iCountedPackets;
        iTotalFragmentsRecieved += pProf->recieve.iCountedFragments;

        if (pProf->send.iLargestPacket > iTotalMaxSent)
            iTotalMaxSent = pProf->send.iLargestPacket;
        if (pProf->send.iSmallestPacket < iTotalMinSent)
            iTotalMinSent = pProf->send.iSmallestPacket;
        if (pProf->recieve.iLargestPacket > iTotalMaxRecieved)
            iTotalMaxRecieved = pProf->recieve.iLargestPacket;
        if (pProf->recieve.iSmallestPacket < iTotalMinRecieved)
            iTotalMinRecieved = pProf->recieve.iSmallestPacket;
    }

    totalPackets = iTotalPacketsRecieved + iTotalPacketsSent;
    if (totalPackets > 0 && (iTotalFragmentsSent + iTotalFragmentsRecieved) > 0) {
        iFragmentTotal = (iTotalFragmentsSent + iTotalFragmentsRecieved) * 100 / totalPackets;
    } else {
        iFragmentTotal = 0;
    }

    Com_sprintf(szLine, 1024,
                "              Totals:%6i|%5i|%5i| %3i%%|%6i|%5i|%5i| %3i%%|%6i|%5i|%5i| %3i%%|",
                iTotalBPSSent, iTotalMaxSent, iTotalMinSent, 0,
                iTotalBPSRecieved, iTotalMaxRecieved, iTotalMinRecieved, 0,
                iTotalBPSSent + iTotalBPSRecieved,
#if defined(COD2_X64)
                (iTotalMaxSent > iTotalMaxRecieved ? iTotalMaxSent : iTotalMaxRecieved),
#else
                (double)_fmaxf((float)iTotalMaxSent, (float)iTotalMaxRecieved),
#endif
#if defined(COD2_X64)
                (iTotalMinSent < iTotalMinRecieved ? iTotalMinSent : iTotalMinRecieved),
#else
                (double)_fminf((float)iTotalMinSent, (float)iTotalMinRecieved),
#endif
                iFragmentTotal);
    if (bPrintToConsole) {
        Com_Printf("%s\n", szLine);
    } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(0x20, iYPos + 0xa, szLine, 0, 8);
#endif
        iYPos += 0xa;
    }

#if defined(COD2_X64)
    pOOBProf = svs.pOOBProf;
#else
    pOOBProf = *(netProfileInfo_t **)(svs_ptr + SVS_POOBPROF_OFF);
#endif
    if (pOOBProf != NULL) {
        int oobTotalPackets;
        int oobFragPerc;

        oobTotalPackets = pOOBProf->send.iCountedPackets + pOOBProf->recieve.iCountedPackets;
        if (oobTotalPackets > 0 && (pOOBProf->send.iCountedFragments + pOOBProf->recieve.iCountedFragments) > 0) {
            oobFragPerc = (pOOBProf->send.iCountedFragments + pOOBProf->recieve.iCountedFragments) * 100 / oobTotalPackets;
        } else {
            oobFragPerc = 0;
        }

        Com_sprintf(szLine, 1024,
                    "  OutOfBand Messages: %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                    pOOBProf->send.iBytesPerSecond,
                    pOOBProf->send.iLargestPacket,
                    pOOBProf->send.iSmallestPacket,
                    pOOBProf->send.iFragmentPercentage,
                    pOOBProf->recieve.iBytesPerSecond,
                    pOOBProf->recieve.iLargestPacket,
                    pOOBProf->recieve.iSmallestPacket,
                    pOOBProf->recieve.iFragmentPercentage,
                    pOOBProf->send.iBytesPerSecond + pOOBProf->recieve.iBytesPerSecond,
#if defined(COD2_X64)
                    (pOOBProf->send.iLargestPacket > pOOBProf->recieve.iLargestPacket ? pOOBProf->send.iLargestPacket : pOOBProf->recieve.iLargestPacket),
#else
                    (double)_fmaxf((float)pOOBProf->send.iLargestPacket, (float)pOOBProf->recieve.iLargestPacket),
#endif
#if defined(COD2_X64)
                    (pOOBProf->send.iSmallestPacket < pOOBProf->recieve.iSmallestPacket ? pOOBProf->send.iSmallestPacket : pOOBProf->recieve.iSmallestPacket),
#else
                    (double)_fminf((float)pOOBProf->send.iSmallestPacket, (float)pOOBProf->recieve.iSmallestPacket),
#endif
                    oobFragPerc);
    } else {
        Com_sprintf(szLine, 1024,
                    "  OutOfBand Messages:     0|    0|    0|   - |     0|    0|    0|   - |     0|    0|    0|   - |");
    }
    if (bPrintToConsole) {
        Com_Printf("%s\n", szLine);
    } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
        CL_DrawString(0x20, iYPos + 0xa, szLine, 0, 8);
#endif
        iYPos += 0xa;
    }

#if defined(COD2_X64)
    numClients = sv_maxclients->current.integer;
#else
    numClients = *(int *)((byte *)sv_ptr + 8);
#endif
    for (i = 0; i < numClients; i++) {
#if defined(COD2_X64)
        client_t *cl = &clientBase[i];
#else
        byte *cl = clientBase + (long)i * CLIENT_STRIDE;
#endif

#if defined(COD2_X64)
        if (!cl->state)
#else
        if (*(int *)(cl + CLIENT_STATE_OFF) == 0)
#endif
            continue;

#if defined(COD2_X64)
        strncpy(szClientName, cl->name, sizeof(szClientName));
#else
        strncpy(szClientName, (const char *)(cl + CLIENT_NAME_OFF), 0x11);
#endif
        szClientName[16] = '\0';

#if defined(COD2_X64)
        pProf = cl->netchan.pProf;
#else
        pProf = *(netProfileInfo_t **)(cl + CLIENT_NETCHAN_PPROF_OFF);
#endif
        if (pProf == NULL) {

            Com_sprintf(szLine, 1024,
                        "#%2i-%16s:     0|    0|    0|   0%%|     0|    0|    0|   0%%|     0|    0|    0|   0%%|",
                        i, szClientName);
        } else {
            int clTotalPackets;
            int clFragPerc;

            clTotalPackets = pProf->send.iCountedPackets + pProf->recieve.iCountedPackets;
            if (clTotalPackets > 0 && (pProf->send.iCountedFragments + pProf->recieve.iCountedFragments) > 0) {
                clFragPerc = (pProf->send.iCountedFragments + pProf->recieve.iCountedFragments) * 100 / clTotalPackets;
            } else {
                clFragPerc = 0;
            }

            Com_sprintf(szLine, 1024,
                        "#%2i-%16s: %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%| %5i|%5i|%5i| %3i%%|",
                        i, szClientName,
                        pProf->send.iBytesPerSecond,
                        pProf->send.iLargestPacket,
                        pProf->send.iSmallestPacket,
                        pProf->send.iFragmentPercentage,
                        pProf->recieve.iBytesPerSecond,
                        pProf->recieve.iLargestPacket,
                        pProf->recieve.iSmallestPacket,
                        pProf->recieve.iFragmentPercentage,
                        pProf->send.iBytesPerSecond + pProf->recieve.iBytesPerSecond,
#if defined(COD2_X64)
                        (pProf->send.iLargestPacket > pProf->recieve.iLargestPacket ? pProf->send.iLargestPacket : pProf->recieve.iLargestPacket),
#else
                        (double)_fmaxf((float)pProf->send.iLargestPacket, (float)pProf->recieve.iLargestPacket),
#endif
#if defined(COD2_X64)
                        (pProf->send.iSmallestPacket < pProf->recieve.iSmallestPacket ? pProf->send.iSmallestPacket : pProf->recieve.iSmallestPacket),
#else
                        (double)_fminf((float)pProf->send.iSmallestPacket, (float)pProf->recieve.iSmallestPacket),
#endif
                        clFragPerc);
        }

        if (bPrintToConsole) {
            Com_Printf("%s\n", szLine);
        } else {
#if !COD2_APPLE_SDK || !defined(DEDICATED)
            CL_DrawString(0x20, iYPos + 0xa, szLine, 0, 8);
#endif
            iYPos += 0xa;
        }
    }
}
#endif
