#include "common_types.h"
#include "www_download.h"
#include "cod2_feature_config.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static clientConnection_t connection;
static clientConnection_t *clc = &connection;
static clientStatic_t cls;
static LegacyHacks hacks;
static LegacyHacks *legacyHacks = &hacks;
static byte *uiBytes = (byte *)&hacks;
static byte **download_ui_ptr = &uiBytes;
static dvar_t developer;
static const dvar_t *com_developer = &developer;
static int reads, nextDownloads;
static dlStatus_t downloadStatus;
static char local[512], remote[512], renamed[512], commands[512];
void Com_Printf(const char *format, ...) { (void)format; }
void Com_DPrintf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
void I_strncpyz(char *dest, const char *src, int size) { snprintf(dest, size, "%s", src); }
int Com_sprintf(char *dest, int size, const char *format, ...)
{
    va_list args; va_start(args, format); int n = vsnprintf(dest, size, format, args); va_end(args);
    return n;
}
char *va(const char *format, ...)
{
    static char text[1024];
    va_list args; va_start(args, format); vsnprintf(text, sizeof(text), format, args); va_end(args);
    return text;
}
void MSG_WriteReliableCommandToBuffer(const char *text, char *out, int size)
{ I_strncpyz(out, text, size); }
void CL_AddReliableCommand(const char *s) { strcat(commands, s); strcat(commands, "\n"); }
char *Dvar_GetString(const char *name) { (void)name; return "/private"; }
char *MSG_ReadString(msg_t *msg) { (void)msg; return "https://example.test/main/map.iwd"; }
int MSG_ReadLong(msg_t *msg) { (void)msg; return reads++ ? 0 : 100; }
void FS_BuildOSPath(const char *base, const char *game, const char *qpath, char *out)
{ snprintf(out, 256, "%s/%s/%s", base, game, qpath); }
int DL_BeginDownload(const char *to, const char *url, int debug)
{ (void)debug; strcpy(local, to); strcpy(remote, url); downloadStatus = DL_STATUS_IN_PROGRESS; return 1; }
void DL_DownloadLoop(void) { }
dlStatus_t DL_GetStatus(void) { return downloadStatus; }
int DL_DLIsMotd(void) { return 0; }
void DL_CancelDownload(void) { }
void Sys_OpenURL(const char *url, int active) { (void)url; (void)active; abort(); }
void Cbuf_ExecuteText(int when, const char *text) { (void)when; (void)text; abort(); }
int FS_ReadFile(const char *path, void **buf) { (void)path; (void)buf; return -1; }
void FS_FreeFile(void *buf) { (void)buf; }
void Dvar_SetStringByName(const char *name, const char *value) { (void)name; (void)value; }
int rename(const char *old, const char *to) { (void)old; strcpy(renamed, to); return 0; }
int remove(const char *path) { (void)path; abort(); }
void FS_CopyFile(char *from, char *to) { (void)from; (void)to; abort(); }
int CL_ClearStaticDownload(void) { return 0; }
void CL_NextDownload(void) { ++nextDownloads; }
