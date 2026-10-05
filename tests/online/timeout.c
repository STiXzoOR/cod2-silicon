#include "common_types.h"
#include <assert.h>
#include <stdio.h>

clientConnection_t clientConnections[1];
clientActive_t clients[1];
clientStatic_t cls;
static LegacyHacks hacks;
LegacyHacks *legacyHacks = &hacks;
static clientActive_t *active = clients;
void *imp_cl = &active;
static int modifiedFlags;
void *imp_dvar_modifiedFlags = &modifiedFlags;
static dvar_t paused, timeout, connectTimeout, ingame;
const dvar_t *cl_paused = &paused, *sv_paused = &paused;
const dvar_t *cl_timeout = &timeout, *cl_connectTimeout = &connectTimeout, *cl_ingame = &ingame;
static int drops;
#if defined(COD2_CODX) && COD2_CODX
void Cod2x_Frame(int connected, int demo) { (void)connected; (void)demo; }
void Cod2x_DemoClientFrame(void) {}
#endif
void Voice_GetLocalVoiceData(ClientVoicePacket_t *p) { (void)p; }
void Voice_Playback(void) {}
void CL_UpdateColor(void) {}
int DL_InProgress(void) { return 0; }
void CL_WWWDownload(void) {}
void CL_CheckForResend(void) {}
void CL_SetCGameTime(void) {}
void CL_SendCmd(void) {}
void Dvar_SetBool(const dvar_t *d, unsigned char value) { ((dvar_t *)d)->current.enabled = value; }
char *Dvar_InfoString(int flags) { (void)flags; return ""; }
char *va(const char *format, ...) { (void)format; return ""; }
void MSG_WriteReliableCommandToBuffer(const char *text, char *buffer, int size)
{ (void)text; (void)buffer; (void)size; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; ++drops; }
#include "timeout_source.h"
int main(void)
{
    hacks.cl_running = 1;
    timeout.current.value = 30;
    connectTimeout.current.value = 30;
    clientConnections[0].state = CA_ACTIVE;
    clientConnections[0].connectTime = 1000;
    clientConnections[0].lastPacketTime = 99000;
    cls.realtime = 100000;
    for (int i = 0; i < 10; ++i)
        CL_Frame(8);
    assert(drops == 0 && clients[0].timeoutcount == 0);
    clientConnections[0].lastPacketTime = 1000;
    for (int i = 0; i < 6; ++i)
        CL_Frame(8);
    assert(drops == 1);
    puts("online: active timeout follows received packets and still detects silence");
    return 0;
}
