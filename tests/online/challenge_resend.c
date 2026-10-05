#include "common_types.h"
#include "PC/qcommon/cdkey_hash.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static clientConnection_t connection;
const clientConnection_t *clc = &connection;
clientStatic_t cls;
static char sampleKey[33];
void *imp_cl_cdkey = sampleKey;
static int modifiedFlags, authorizations, sent;
void *imp_dvar_modifiedFlags = &modifiedFlags;
static dvar_t lanAuthorize;
const dvar_t *net_lanauthorize = &lanAuthorize;
static char packet[256];
char *strupr(char *text)
{
    for (char *p = text; *p; ++p)
        *p = (char)toupper((unsigned char)*p);
    return text;
}
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; assert(0); }
char *va(const char *format, ...)
{
    static char text[256];
    va_list args; va_start(args, format); vsnprintf(text, sizeof(text), format, args); va_end(args);
    return text;
}
int Sys_IsLANAddress(netadr_t adr) { (void)adr; return 0; }
void CL_RequestAuthorization(void) { ++authorizations; }
#if defined(COD2_CODX) && COD2_CODX
void Cod2x_PrepareConnect(void) {}
#endif
Bool NET_OutOfBandPrint(netsrc_t sock, netadr_t adr, const char *text)
{ (void)sock; (void)adr; snprintf(packet, sizeof(packet), "%s", text); return ++sent; }
const char *Dvar_InfoString(int bit) { (void)bit; return ""; }
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
void Info_SetValueForKey(char *s, const char *key, const char *value) { (void)s; (void)key; (void)value; }
Bool NET_OutOfBandData(netsrc_t sock, netadr_t adr, unsigned char *data, int len)
{ (void)sock; (void)adr; (void)data; (void)len; return 1; }
#include "challenge_resend_source.h"
int main(void)
{
    char hash[33];
    /* Bytes after the terminator must not enter the digest (Mac 1.3 stops at strlen). */
    memcpy(sampleKey, "abcd-1234\0EFGH", 14);
    CL_BuildMd5StrFromCDKey(hash);
    assert(!strcmp(hash, "77b51bb0130bd4bbbfc94d0c64bd6fdc"));

    connection.state = CA_CONNECTING;
    connection.connectTime = -99999;
    cls.realtime = 5000;
    CL_CheckForResend();
    assert(sent == 1 && authorizations == 1);
    assert(!strcmp(packet, "getchallenge 0 \"77b51bb0130bd4bbbfc94d0c64bd6fdc\""));
    cls.realtime = 6999;
    CL_CheckForResend();
    assert(sent == 1);
    cls.realtime = 7000;
    CL_CheckForResend();
    assert(sent == 2 && authorizations == 2);
    puts("online: getchallenge carries the quoted 1.3 CD-key digest every 2000 ms");
    return 0;
}
