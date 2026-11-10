#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "imports.h"
#include "../qcommon/cod2x_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Script functions CoD2x 1.4.6.8 gives mods for talking to web services:
   http_fetch here, the websocket and match calls beside it. Answers arrive
   on later frames and run the script's callbacks. */

extern void Com_Printf(const char *fmt, ...);
extern const char *va(const char *fmt, ...);
extern const char *Dvar_GetString(const char *dvarName);
extern int I_stricmp(const char *s1, const char *s2);
extern unsigned int Scr_GetNumParam(void);
extern const char *Scr_GetString(unsigned int index);
extern int Scr_GetInt(unsigned int index);
extern int Scr_GetFunc(unsigned int index);
extern void Scr_Error(const char *error);
extern void Scr_AddUndefined(void);
extern void Scr_AddString(const char *value);
extern void Scr_AddInt(int value);
extern void Scr_MakeArray(void);
extern void Scr_AddArray(void);
extern scr_thread_t Scr_ExecThread(scr_func_t handle, unsigned int paramcount);
extern void Scr_FreeThread(int handle);
extern int Scr_IsSystemActive(int inst);

/* CoD2x's HTTP client keeps at most this much of an answer. */
#define FETCH_MAX_RECEIVE (3u * 1024u * 1024u)
/* and reads this many header lines of it. */
#define FETCH_MAX_HEADERS 30

/* ---- which scripts a callback belongs to ---------------------------------- */

/* A callback is a place in the compiled scripts, and every map load and
   restart compiles them again. The same map, gametype and program size give
   the same places, so a request made before a map_restart can still answer;
   after any other change its callbacks are not run. A shutdown drops what is
   still on its way, as CoD2x's client is deleted then. */
typedef struct {
    char map[64];
    char gametype[64];
    unsigned int programLength;
    unsigned int generation;
} ScriptScope;

static unsigned int scriptGeneration;

static void Script_ScopeNow(ScriptScope *scope)
{
    snprintf(scope->map, sizeof(scope->map), "%s", Dvar_GetString("mapname"));
    snprintf(scope->gametype, sizeof(scope->gametype), "%s", Dvar_GetString("g_gametype"));
    scope->programLength = (unsigned int)((struct scrCompilePub_t *)imp_scrCompilePub)->programLen;
    scope->generation = scriptGeneration;
}

static int Script_ScopeDropped(const ScriptScope *scope)
{
    return scope->generation != scriptGeneration;
}

static int Script_ScopeCallable(const ScriptScope *scope)
{
    ScriptScope now;
    if (Script_ScopeDropped(scope) || !Scr_IsSystemActive(1))
        return 0;
    Script_ScopeNow(&now);
    return !strcmp(now.map, scope->map) && !strcmp(now.gametype, scope->gametype) &&
           now.programLength == scope->programLength;
}

static void Script_RunCallback(int handle, unsigned int paramcount)
{
    Scr_FreeThread(Scr_ExecThread(handle, paramcount));
}

/* ---- http_fetch ------------------------------------------------------------- */

typedef struct {
    int onDone, onError;
    char *url;
    ScriptScope scope;
} ScriptFetch;

static int fetchPending;

typedef struct {
    const char *name, *value;
    size_t nameLength, valueLength;
} FetchHeader;

static int Fetch_CompareHeaders(const void *a, const void *b)
{
    const FetchHeader *x = a, *y = b;
    size_t length = x->nameLength < y->nameLength ? x->nameLength : y->nameLength;
    int order = memcmp(x->name, y->name, length);
    if (order)
        return order;
    return x->nameLength < y->nameLength ? -1 : x->nameLength > y->nameLength;
}

static char *Fetch_Copy(const char *s, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, s, length);
        copy[length] = '\0';
    }
    return copy;
}

/* The answer's headers as CoD2x hands them over: [name, value, name, value, ...]
   sorted by name (case counts), a name sent twice keeps its last value, and
   only the first 30 lines are read. */
static void Fetch_AddHeaders(const char *raw)
{
    FetchHeader headers[FETCH_MAX_HEADERS];
    int count = 0;
    const char *line = raw;

    while (line && *line) {
        const char *end = strstr(line, "\r\n");
        size_t length = end ? (size_t)(end - line) : strlen(line);
        const char *colon = memchr(line, ':', length);
        if (length >= 5 && !memcmp(line, "HTTP/", 5)) {
            count = 0; /* a new answer after an interim one: only the last counts */
        } else if (colon && colon != line && count < FETCH_MAX_HEADERS) {
            const char *value = colon + 1, *valueEnd = line + length;
            while (value < valueEnd && (*value == ' ' || *value == '\t'))
                ++value;
            while (valueEnd > value && (valueEnd[-1] == ' ' || valueEnd[-1] == '\t'))
                --valueEnd;
            headers[count].name = line;
            headers[count].nameLength = (size_t)(colon - line);
            headers[count].value = value;
            headers[count].valueLength = (size_t)(valueEnd - value);
            ++count;
        }
        line = end ? end + 2 : NULL;
    }
    /* Stable for equal names, so the later one is last and wins below. */
    for (int i = 1; i < count; ++i) {
        FetchHeader item = headers[i];
        int j = i - 1;
        while (j >= 0 && Fetch_CompareHeaders(&headers[j], &item) > 0) {
            headers[j + 1] = headers[j];
            --j;
        }
        headers[j + 1] = item;
    }

    Scr_MakeArray();
    for (int i = 0; i < count; ++i) {
        char *name, *value;
        if (i + 1 < count && !Fetch_CompareHeaders(&headers[i], &headers[i + 1]))
            continue;
        name = Fetch_Copy(headers[i].name, headers[i].nameLength);
        value = Fetch_Copy(headers[i].value, headers[i].valueLength);
        Scr_AddString(name ? name : "");
        Scr_AddArray();
        Scr_AddString(value ? value : "");
        Scr_AddArray();
        free(name);
        free(value);
    }
}

static void Fetch_Error(ScriptFetch *fetch, const char *error)
{
    if (Script_ScopeDropped(&fetch->scope))
        return;
    if (fetch->onError && Script_ScopeCallable(&fetch->scope)) {
        Scr_AddString(error);
        Script_RunCallback(fetch->onError, 1);
    } else {
        Com_Printf("HTTP error while fetching %s: %s\n", fetch->url, error);
    }
}

static void Fetch_Free(ScriptFetch *fetch)
{
    free(fetch->url);
    free(fetch);
}

static void Fetch_Done(void *context, const Cod2xHttpResponse *response)
{
    ScriptFetch *fetch = context;
    if (!Script_ScopeDropped(&fetch->scope))
        --fetchPending;
    if (response->error[0]) {
        Fetch_Error(fetch, !strcmp(response->error, "Response too large") ? "MG_MAX_RECV_SIZE" : response->error);
    } else if (fetch->onDone && Script_ScopeCallable(&fetch->scope)) {
        /* onDone(status, body, headers) */
        Fetch_AddHeaders(response->headers);
        Scr_AddString(response->body);
        Scr_AddInt(response->status);
        Script_RunCallback(fetch->onDone, 3);
    }
    Fetch_Free(fetch);
}

/* http_fetch(url, method, data, headers, timeout, onDone, onError): headers are
   lines joined by "\r\n"; timeout is in milliseconds for the whole request
   (0 or less: none). Nothing is returned; the answer comes later. */
static void GScr_Cod2xHttpFetch(void)
{
    const char *url, *method, *data, *headers;
    int timeout, onDone, onError;
    ScriptFetch *fetch;
    Cod2xHttpRequest request;

    if (Scr_GetNumParam() != 7) {
        Scr_Error(va("http_fetch: invalid number of parameters, expected 7, got %d\n", Scr_GetNumParam()));
        Scr_AddUndefined();
        return;
    }
    url = Scr_GetString(0);
    method = Scr_GetString(1);
    data = Scr_GetString(2);
    headers = Scr_GetString(3);
    timeout = Scr_GetInt(4);
    onDone = Scr_GetFunc(5);
    onError = Scr_GetFunc(6);

    /* Only now: the checks above end the call with a script error. */
    fetch = calloc(1, sizeof(*fetch));
    if (!fetch)
        return;
    fetch->onDone = onDone;
    fetch->onError = onError;
    fetch->url = Fetch_Copy(url, strlen(url));
    Script_ScopeNow(&fetch->scope);
    ++fetchPending;

    memset(&request, 0, sizeof(request));
    request.method = method;
    request.url = url;
    request.headers = headers;
    request.body = data;
    request.bodyLength = strlen(data);
    request.timeoutMs = timeout > 0 ? timeout : 0;
    request.connectTimeoutMs = timeout > 0 ? 5000 : 0;
    request.maxBody = FETCH_MAX_RECEIVE;

    /* CoD2x answers these at once, from inside the call. */
    if (!fetch->url || !Cod2x_HttpValidUrl(url) || !Cod2x_HttpStart(&request, Fetch_Done, fetch)) {
        --fetchPending;
        Fetch_Error(fetch, fetch->url && !Cod2x_HttpValidUrl(url) ? "Invalid URL" : "Failed to connect");
        Fetch_Free(fetch);
    }
}

/* ---- map changes and shutdown --------------------------------------------- */

/* After the shutdown's own callbacks: answers still on their way get a second
   to arrive (frames no longer run), and the rest are dropped. */
void GScr_Cod2xNetBeforeMapChange(int shutdown)
{
    if (!shutdown)
        return;
    for (int waited = 0; fetchPending > 0 && waited < 1000; waited += 10) {
        Cod2x_HttpFrameFor(Fetch_Done);
        if (fetchPending > 0)
            Cod2x_Sleep(10);
    }
    fetchPending = 0;
    ++scriptGeneration;
}

/* ---- the table -------------------------------------------------------------- */

typedef struct {
    const char *name;
    void (*call)(void);
    int developer;
} ScriptFunction;

static const ScriptFunction scriptFunctions[] = {
    {"http_fetch", GScr_Cod2xHttpFetch, 0},
};

void *GScr_Cod2xGetFunction(const char **name, int *type)
{
    for (size_t i = 0; i < sizeof(scriptFunctions) / sizeof(scriptFunctions[0]); ++i) {
        if (I_stricmp(*name, scriptFunctions[i].name))
            continue;
        *name = scriptFunctions[i].name;
        *type = scriptFunctions[i].developer;
        return (void *)scriptFunctions[i].call;
    }
    return NULL;
}
#endif
