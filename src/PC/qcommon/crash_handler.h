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

#ifdef __cplusplus
}
#endif

#endif
