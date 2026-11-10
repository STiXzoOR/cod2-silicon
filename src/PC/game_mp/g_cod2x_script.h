#ifndef G_COD2X_SCRIPT_H
#define G_COD2X_SCRIPT_H

/* Script callbacks of the CoD2x web calls (g_cod2x_script.c). A callback is
   remembered with the scripts it was made in and runs only while those are
   still the loaded ones (see GScr_Cod2xScopeCallable). */
typedef struct {
    char map[64];
    char gametype[64];
    unsigned int programLength;
    unsigned int generation;
} Cod2xScriptScope;

void GScr_Cod2xScopeNow(Cod2xScriptScope *scope);
int GScr_Cod2xScopeCallable(const Cod2xScriptScope *scope);
/* Runs handle with the paramcount values already added. */
void GScr_Cod2xRunCallback(int handle, unsigned int paramcount);
/* The server shuts down: callbacks still waiting will not run. */
void GScr_Cod2xDropCallbacks(void);
void GScr_Cod2xNetBeforeMapChange(int shutdown);

/* Match calls (cod2x_match.c). */
void GScr_Cod2xMatchUploadData(void);
void GScr_Cod2xMatchSetData(void);
void GScr_Cod2xMatchGetData(void);
void GScr_Cod2xMatchRedownloadData(void);
void GScr_Cod2xMatchClearData(void);
void GScr_Cod2xMatchIsActivated(void);
void GScr_Cod2xMatchCancel(void);
void GScr_Cod2xMatchFinish(void);

#endif
