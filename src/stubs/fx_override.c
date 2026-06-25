#include "common_types.h"
#include <string.h>

extern void *Hunk_AllocAlignInternal(int size, int align);
extern void *defaultEffect;

void *__wrap_FX_TryRegisterEffect(const char *name)
{

    (void)name;
    return NULL;
}

void *__wrap_FX_RegisterEffect(const char *fileName)
{
    (void)fileName;
    return defaultEffect;
}

extern void Com_Printf(const char *fmt, ...);
extern void __real_R_Error(int level, const char *fmt, ...);

#include <stdarg.h>
#include <stdio.h>

void __wrap_R_Error(int level, const char *fmt, ...)
{

    if (fmt) {

        va_list args;
        char buf[512];
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        if (strstr(buf, "Vertex type") && strstr(buf, "doesn't have")) {

            static int vtypeWarnCount = 0;
            static int vtypeByType[8] = { 0 };
            int vt = 0;
            const char *p = strstr(buf, "Vertex type ");
            if (p)
                vt = p[12] - '0';
            if (vt >= 0 && vt < 8)
                vtypeByType[vt]++;
            vtypeWarnCount++;
            if (vtypeWarnCount <= 10)
                Com_Printf("WARNING: %s", buf);
            else if (vtypeWarnCount == 11)
                Com_Printf("WARNING: Suppressing further vertex type warnings...\n");
            else if (vtypeWarnCount % 10000 == 0)
                Com_Printf("WARNING: vertex type errors so far: t0=%d t1=%d t2=%d t3=%d t4=%d t5=%d\n",
                           vtypeByType[0], vtypeByType[1], vtypeByType[2], vtypeByType[3], vtypeByType[4], vtypeByType[5]);
            return;
        }
    }

    va_list args;
    va_start(args, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    __real_R_Error(level, "%s", buf);
}

float __wrap_FX_CreateDefaultEffect(void)
{

    void *effect = Hunk_AllocAlignInternal(0x68, 4);
    memset(effect, 0, 0x68);
    char *nameBuf = (char *)Hunk_AllocAlignInternal(11, 4);
    strcpy(nameBuf, "default_fx");
    *(char **)effect = nameBuf;
    defaultEffect = effect;
    return 0.0f;
}
