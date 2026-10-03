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
#if defined(__APPLE__) && defined(COD2_X64) && defined(COD2_CODX) && COD2_CODX
int Cod2xNativeApp_Arguments(char *buffer, int capacity);
#endif
void Sys_CrashSetDirectory(const char *path);
void Sys_CrashFreezeReport(void *context);
#endif
