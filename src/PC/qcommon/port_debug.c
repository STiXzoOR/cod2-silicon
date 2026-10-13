#if defined(COD2_X64)
#include <stdlib.h>

int cod2_native_trace_enabled;

void COD2_InitNativeTracing(void)
{
    static int initialized;
    if (!initialized) {
        const char *value = getenv("COD2_NATIVE_TRACE");
        cod2_native_trace_enabled = value && value[0] && !(value[0] == '0' && !value[1]);
        initialized = 1;
    }
}
#endif
