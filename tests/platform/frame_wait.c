#include "macos_system.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint64_t start = MacSystem_Nanoseconds();
    for (int i = 0; i < 50; ++i) {
        uint64_t deadline = MacSystem_Nanoseconds() + 500000;
        MacSystem_WaitUntil(deadline);
        assert(MacSystem_Nanoseconds() >= deadline);
    }
    MacSystem_WaitUntil(start);
    puts("absolute mach wait never returns before its deadline: passed");
    return 0;
}
