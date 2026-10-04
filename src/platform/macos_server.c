#if defined(__APPLE__) && defined(COD2_X64) && defined(DEDICATED)
#include "common_types.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Com_Quit_f(void);

/* Shared HUD alignment vocabulary normally resides in the client UI unit. */
const char str_002b3f60[] = "fullscreen";

static volatile sig_atomic_t quitRequested;

static void MacServer_RequestQuit(int signalNumber)
{
    quitRequested = signalNumber;
}

void MacServer_InstallSignals(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = MacServer_RequestQuit;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGTERM, &action, NULL) || sigaction(SIGINT, &action, NULL)) {
        perror("CoD2 server signal setup");
        exit(1);
    }
}

void MacServer_CheckQuit(void)
{
    if (quitRequested)
        Com_Quit_f();
}
#endif
