/* Client command tokenisation and info-string helpers (cmd.c, q_shared.c).
 *
 * The input is a NUL-terminated command string. It is tokenised the way
 * SV_Cmd_TokenizeString / Cmd_TokenizeString2 tokenise a client's reliable
 * commands and connectionless packets, and the argument accessors are read
 * back. A second slice drives the userinfo helpers (Info_SetValueForKey,
 * Info_RemoveKey, Info_ValueForKey and the _Big variants) over a 1 KB / 8 KB
 * info string, the same calls SV_DirectConnect and SV_UpdateUserinfo_f reach.
 */
#include "common_types.h"
#include "fuzz.h"

#include <stdarg.h>

extern void Cmd_TokenizeString2(const char *text_in, int max_tokens);
extern void SV_Cmd_TokenizeString(const char *text_in);
extern int SV_Cmd_Argc(void);
extern char *SV_Cmd_Argv(int arg);
extern char *Cmd_Args(int start);
extern void Cmd_ArgsBuffer(char *buffer, int bufferLength);

extern char *Info_ValueForKey(const char *s, const char *key);
extern void Info_SetValueForKey(char *s, const char *key, const char *value);
extern void Info_SetValueForKey_Big(char *s, const char *key, const char *value);
extern void Info_RemoveKey(char *s, const char *key);
extern void Info_RemoveKey_Big(char *s, const char *key);
extern qboolean Info_Validate(const char *s);
extern void Info_NextPair(const char **head, char *key, char *value);

/* Com_GetDecimalDelimiter reads loc_language; nothing on these paths does. */
FUZZ_DVAR(loc_language);

void Com_Printf(const char *fmt, ...)
{
    (void)fmt;
}

void Com_DPrintf(const char *fmt, ...)
{
    (void)fmt;
}

void Com_Error(int code, const char *fmt, ...)
{
    FuzzEngineError(code, fmt);
}

/* The engine's va() lives in q_shared.c and needs Sys_GetValue thread storage;
 * these paths do not call it, but SV_Cmd_Argv etc. are reached through it in
 * production, so provide a trivial buffer. */
void *Sys_GetValue(int index)
{
    static char storage[2][1024];
    static int which;
    (void)index;
    which ^= 1;
    return storage[which];
}

static char *Dup(const uint8_t *data, size_t size)
{
    char *s = malloc(size + 1);
    if (!s)
        abort();
    memcpy(s, data, size);
    s[size] = '\0';
    return s;
}

static void Tokenize(const uint8_t *data, size_t size, int maxTokens)
{
    char *text = Dup(data, size);
    int argc;
    int i;
    char buffer[1024];

    if (maxTokens)
        Cmd_TokenizeString2(text, maxTokens);
    else
        SV_Cmd_TokenizeString(text);

    argc = SV_Cmd_Argc();
    for (i = -1; i <= argc; ++i) {
        char *arg = SV_Cmd_Argv(i);
        /* Every argument must be a NUL-terminated pointer into the token buffer. */
        volatile char sink = arg[0];
        (void)sink;
    }
    Cmd_Args(0);
    if (argc > 1)
        Cmd_Args(1);
    Cmd_ArgsBuffer(buffer, sizeof(buffer));
    free(text);
}

static void Userinfo(FuzzReader *r)
{
    char big[8192];
    char info[1024];
    char key[64];
    char value[256];
    const uint8_t *bytes;
    size_t keyLen, valLen, seedLen;
    int op = FuzzU8(r);

    keyLen = FuzzBytes(r, &bytes, sizeof(key) - 1);
    memcpy(key, bytes, keyLen);
    key[keyLen] = '\0';
    valLen = FuzzBytes(r, &bytes, sizeof(value) - 1);
    memcpy(value, bytes, valLen);
    value[valLen] = '\0';

    seedLen = FuzzBytes(r, &bytes, (op & 1) ? sizeof(big) - 1 : sizeof(info) - 1);
    if (op & 1) {
        memcpy(big, bytes, seedLen);
        big[seedLen] = '\0';
        Info_Validate(big);
        Info_ValueForKey(big, key);
        Info_SetValueForKey_Big(big, key, value);
        Info_RemoveKey_Big(big, key);
        {
            const char *head = big;
            char k[8192], v[8192];
            int steps = 0;
            while (*head && steps++ < 64) {
                const char *before = head;
                Info_NextPair(&head, k, v);
                if (head == before)
                    break;
            }
        }
    } else {
        memcpy(info, bytes, seedLen);
        info[seedLen] = '\0';
        Info_Validate(info);
        Info_ValueForKey(info, key);
        Info_SetValueForKey(info, key, value);
        Info_RemoveKey(info, key);
    }
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzReader reader;
    int mode;

    if (!size)
        return 0;
    mode = data[0] % 3;
    reader.p = data + 1;
    reader.end = data + size;

    switch (mode) {
    case 0:
        Tokenize(reader.p, FuzzLeft(&reader), 0);
        break;
    case 1:
        Tokenize(reader.p, FuzzLeft(&reader), 1 + (data[0] >> 2));
        break;
    default:
        Userinfo(&reader);
        break;
    }
    return 0;
}
