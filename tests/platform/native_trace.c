#include "PC/qcommon/port_debug.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv)
{
    const char *value = argc > 1 ? argv[1] : "";
    if (*value) setenv("COD2_NATIVE_TRACE", value, 1);
    else unsetenv("COD2_NATIVE_TRACE");
    COD2_InitNativeTracing();
    assert(COD2_NATIVE_TRACE_ENABLED() == (*value && strcmp(value, "0") != 0));
    int enabled = COD2_NATIVE_TRACE_ENABLED();
    setenv("COD2_NATIVE_TRACE", enabled ? "0" : "1", 1);
    COD2_InitNativeTracing();
    assert(COD2_NATIVE_TRACE_ENABLED() == enabled);
    puts("PASS native trace startup policy and cached hot-path flag");
    return 0;
}
