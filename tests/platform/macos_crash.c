#include "PC/qcommon/crash_handler.h"
#include <signal.h>

int main(int argc, char **argv)
{
    Sys_InstallCrashHandler("macOS platform probe", "test", "native-arm64", "crash probe");
    (void)argv;
    raise(argc > 1 ? SIGTRAP : SIGABRT);
    return 1;
}
