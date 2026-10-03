#include "PC/qcommon/crash_handler.h"
#include <signal.h>

int main(void)
{
    Sys_InstallCrashHandler("macOS platform probe", "test", "native-arm64", "crash probe");
    raise(SIGABRT);
    return 1;
}
