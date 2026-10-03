#ifndef COD2X_NATIVE_H
#define COD2X_NATIVE_H
void Cod2xNative_Init(void);
void Cod2xNative_Frame(void);
void Cod2xNative_Heartbeat(void);
void Cod2xNative_Shutdown(void);
void Cod2xNativeURL_Install(void);
void Cod2xNativeURL_SetupPaths(void);
void Cod2xNativeURL_Shutdown(void);
int Cod2xNativeURL_Queue(const char *url);
void Cod2xNativeURL_Frame(void);
void Sys_CrashSetDirectory(const char *path);
void Sys_CrashFreezeReport(void *context);
#endif
