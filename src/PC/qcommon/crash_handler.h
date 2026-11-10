#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

void Sys_InstallCrashHandler(const char *appName,
                             const char *version,
                             const char *buildDate,
                             const char *cmdline);

void Sys_CrashSetField(const char *name, const char *value);

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* Where the reports that are not CoD2x's go, and nowhere else: crashes and freezes
   outside CoD2x-layer code (path: the cod2x_crash or cod2x_freeze log, reason: the
   signal or exception, or the freeze), and fatal errors raised outside it (path
   NULL, as no file is written for an error; reason: the error text). This
   repository sends nothing for them. A crash calls the receiver from the signal
   handler or exception filter, so it must be safe to call there. */
typedef void (*Sys_ReportReceiver)(const char *path, const char *reason);
void Sys_SetReportReceiver(Sys_ReportReceiver receiver);
void Sys_CrashReceive(const char *path, const char *reason);
#endif

#ifdef __cplusplus
}
#endif

#endif
