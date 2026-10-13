#include "common_types.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static char configString[0x401];
static int itemCount = 16;
void *imp_bg_numItems = &itemCount;
static int registeredItems[16];
static int registrationCount;
static int drops;
static errorParm_t dropCode;
static char dropMessage[128];
static jmp_buf dropTarget;

const char *CL_GetConfigString(int index)
{
    assert(index == 8);
    return configString;
}

void CG_RegisterItemVisuals(int itemNum)
{
    assert(registrationCount < 16);
    registeredItems[registrationCount++] = itemNum;
}

void Com_Error(errorParm_t code, const char *fmt, ...)
{
    va_list args;

    ++drops;
    dropCode = code;
    va_start(args, fmt);
    vsnprintf(dropMessage, sizeof(dropMessage), fmt, args);
    va_end(args);
    /* ERR_DROP leaves the frame through longjmp in the engine. */
    longjmp(dropTarget, 1);
}

#include "items_source.h"

static void reset_config(size_t length)
{
    assert(length < sizeof(configString));
    memset(configString, '0', length);
    configString[length] = '\0';
    registrationCount = 0;
    drops = 0;
    dropMessage[0] = '\0';
}

static void check_valid(size_t length)
{
    static const int expected[] = {1, 3, 4, 6, 10, 11};

    reset_config(length);
    memcpy(configString, "a5c0", 4);
    if (setjmp(dropTarget)) {
        assert(!"valid item mask dropped");
    }
    CG_RegisterItems();
    assert(drops == 0);
    assert(registrationCount == 6);
    assert(memcmp(registeredItems, expected, sizeof(expected)) == 0);
}

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
static void check_rejected(size_t length)
{
    reset_config(length);
    memset(configString, 'f', length);
    if (!setjmp(dropTarget)) {
        CG_RegisterItems();
        assert(!"oversized item mask returned without ERR_DROP");
    }
    assert(drops == 1);
    assert(dropCode == ERR_DROP);
    assert(strcmp(dropMessage, "CG_RegisterItems: overflowed") == 0);
    assert(registrationCount == 0);
}
#endif

int main(int argc, char **argv)
{
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    if (argc == 2 && !strcmp(argv[1], "overflow")) {
        check_rejected(0x109);
        return 0;
    }
#else
    (void)argc;
    (void)argv;
#endif
    check_valid(4);
    check_valid(0x107);
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    check_rejected(0x108);
    check_rejected(0x109);
    check_rejected(0x400);
    puts("PASS CoD2x item masks: valid boundary registers assets; oversized masks drop before registration");
#else
    check_valid(0x108);
    puts("PASS stock item masks: valid registrations and original 0x108 boundary");
#endif
    return 0;
}
