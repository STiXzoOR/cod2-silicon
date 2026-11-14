#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Info_SetValueForKey_Big(char *, const char *, const char *);
void Com_Printf(const char *format, ...) { (void)format; }
void Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }

extern void Info_SetValueForKey_BigSized(char *, size_t, const char *, const char *);
static void CheckBoundary(void)
{
    const int lengths[] = {1024, 1025, 8191, 8192, 8193};
    for (unsigned i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        struct { char text[8193]; unsigned long guard; } data = {"\\a\\x", 0xabcdef01UL};
        char value[8192];
        memset(value, 'x', lengths[i] - 7);
        value[lengths[i] - 7] = 0;
        Info_SetValueForKey_BigSized(data.text, sizeof(data.text), "b", value);
        assert(strlen(data.text) == (unsigned)(lengths[i] <= 8192 ? lengths[i] : 4));
        assert(data.guard == 0xabcdef01UL);
        if (lengths[i] == 8192) {
            /* Upstream still rejects an already-full input before changing it. */
            Info_SetValueForKey_BigSized(data.text, sizeof(data.text), "a", "y");
            assert(strlen(data.text) == 8192 && data.text[3] == 'x');
        }
    }
    struct { char text[8192]; unsigned long guard; } legacy = {"\\a\\x", 0xabcdef01UL};
    char value[8192];
    memset(value, 'x', 8185); value[8185] = 0;
    Info_SetValueForKey_Big(legacy.text, "b", value);
    assert(strlen(legacy.text) == 4 && legacy.guard == 0xabcdef01UL);
    for (size_t capacity = 0; capacity < 8; ++capacity) {
        unsigned char small[8]; memset(small, 0x5a, sizeof(small)); small[0] = 0;
        Info_SetValueForKey_BigSized((char *)small, capacity, "a", "xxx");
        assert(small[capacity] == (capacity ? 0x5a : 0));
        assert(capacity >= 7 ? !strcmp((char *)small, "\\a\\xxx") : !small[0]);
    }
    unsigned char unterminated[8]; memset(unterminated, 'x', sizeof(unterminated));
    Info_SetValueForKey_BigSized((char *)unterminated, sizeof(unterminated), "a", "y");
    for (unsigned i = 0; i < sizeof(unterminated); ++i) assert(unterminated[i] == 'x');
}

int main(void)
{
    CheckBoundary();
    struct { char text[8192]; unsigned long guard; } data = {{0}, 0xabcdef01UL};
    char value[8000], prior[8192];
    memset(value, 'x', sizeof(value) - 1); value[sizeof(value) - 1] = 0;
    Info_SetValueForKey_Big(data.text, "sv_iwdNames", value);
    assert(strlen(data.text) == strlen(value) + strlen("\\sv_iwdNames\\"));
    strcpy(prior, data.text);
    Info_SetValueForKey_Big(data.text, "extra", value);
    assert(!strcmp(prior, data.text));
    assert(data.guard == 0xabcdef01UL);
    Info_SetValueForKey_Big(data.text, "sv_iwdNames", "small");
    assert(!strcmp(data.text, "\\sv_iwdNames\\small"));
    puts("CoD2x actual big info strings: large IWD list, bound and replacement passed");
    return 0;
}
