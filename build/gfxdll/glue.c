/* Minimal global-array thread-value store for the swap-in renderer DLL. The
 * engine's win_threads.c provides these in the monolith; here the DLL carries
 * its own (its bundled q_shared.c has its own va_info/g_com_error statics that
 * Com_InitThreadData wires into slots 1..3). Lazy-init on first Sys_GetValue so
 * we don't depend on DLL-constructor ordering vs GetRefAPI. */
extern void Com_InitThreadData(int threadContext);
static void *g_threadValues[64];
static int   g_threadDataInit;
void Sys_SetValue(int i, void *d) { if ((unsigned)i < 64) g_threadValues[i] = d; }
void *Sys_GetValue(int i) {
    if (!g_threadDataInit) { g_threadDataInit = 1; Com_InitThreadData(0); }
    return (unsigned)i < 64 ? g_threadValues[i] : 0;
}
