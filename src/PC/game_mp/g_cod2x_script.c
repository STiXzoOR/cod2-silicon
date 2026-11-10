#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "imports.h"
#include "g_cod2x_script.h"
#include "../qcommon/cod2x_http.h"
#include "../qcommon/cod2x_websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Script functions CoD2x 1.4.6.8 gives mods for talking to web services:
   http_fetch and the websocket calls here, the match calls in cod2x_match.c. Answers arrive
   on later frames and run the script's callbacks. */

extern void Com_Printf(const char *fmt, ...);
extern void Com_DPrintf(const char *fmt, ...);
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
extern void Scr_AddBool(int value);
extern void Scr_MakeArray(void);
extern void Scr_AddArray(void);
extern scr_thread_t Scr_ExecThread(scr_func_t handle, unsigned int paramcount);
extern void Scr_FreeThread(int handle);
extern int Scr_IsSystemActive(int inst);
extern level_locals_t level;

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
static unsigned int scriptGeneration;

void GScr_Cod2xScopeNow(Cod2xScriptScope *scope)
{
    snprintf(scope->map, sizeof(scope->map), "%s", Dvar_GetString("mapname"));
    snprintf(scope->gametype, sizeof(scope->gametype), "%s", Dvar_GetString("g_gametype"));
    scope->programLength = (unsigned int)((struct scrCompilePub_t *)imp_scrCompilePub)->programLen;
    scope->generation = scriptGeneration;
}

static int Script_ScopeDropped(const Cod2xScriptScope *scope)
{
    return scope->generation != scriptGeneration;
}

int GScr_Cod2xScopeCallable(const Cod2xScriptScope *scope)
{
    Cod2xScriptScope now;
    if (Script_ScopeDropped(scope) || !Scr_IsSystemActive(1))
        return 0;
    GScr_Cod2xScopeNow(&now);
    return !strcmp(now.map, scope->map) && !strcmp(now.gametype, scope->gametype) &&
           now.programLength == scope->programLength;
}

void GScr_Cod2xRunCallback(int handle, unsigned int paramcount)
{
    Scr_FreeThread(Scr_ExecThread(handle, paramcount));
}

/* ---- http_fetch ------------------------------------------------------------- */

typedef struct {
    int onDone, onError;
    char *url;
    Cod2xScriptScope scope;
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
    if (fetch->onError && GScr_Cod2xScopeCallable(&fetch->scope)) {
        Scr_AddString(error);
        GScr_Cod2xRunCallback(fetch->onError, 1);
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
    } else if (fetch->onDone && GScr_Cod2xScopeCallable(&fetch->scope)) {
        /* onDone(status, body, headers) */
        Fetch_AddHeaders(response->headers);
        Scr_AddString(response->body);
        Scr_AddInt(response->status);
        GScr_Cod2xRunCallback(fetch->onDone, 3);
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
    GScr_Cod2xScopeNow(&fetch->scope);
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

/* ---- websocket_connect, websocket_sendText, websocket_close ---------------- */

#define WEBSOCKET_MAX_CLIENTS 16

typedef struct {
    Cod2xWebSocket *ws;
    int connected; /* opened and not closed yet, as the callbacks have seen it */
    int onConnect, onMessage, onClose, onError;
    Cod2xScriptScope scope;
} ScriptWebSocket;

static ScriptWebSocket webSockets[WEBSOCKET_MAX_CLIENTS];

static void WebSocket_Deliver(int index)
{
    ScriptWebSocket *slot = &webSockets[index];
    Cod2xWebSocketEvent event;
    while (slot->ws && Cod2x_WebSocketNextEvent(slot->ws, &event)) {
        int callable = GScr_Cod2xScopeCallable(&slot->scope);
        switch (event.type) {
        case COD2X_WS_OPEN:
            slot->connected = 1;
            Com_DPrintf("WebSocket client #%d connected.\n", index);
            if (slot->onConnect && callable)
                GScr_Cod2xRunCallback(slot->onConnect, 0);
            break;
        case COD2X_WS_MESSAGE:
            if (slot->onMessage && callable) {
                Scr_AddString(event.text ? event.text : "");
                GScr_Cod2xRunCallback(slot->onMessage, 1);
            }
            break;
        case COD2X_WS_CLOSE:
            slot->connected = 0;
            Com_DPrintf("WebSocket client #%d disconnected, isClosedByRemote: %d, isFullyDisconnected: %d\n", index,
                        event.closedByRemote, event.fullyDisconnected);
            /* onClose(isClosedByRemote, isFullyDisconnected) */
            if (slot->onClose && callable) {
                Scr_AddBool(event.fullyDisconnected);
                Scr_AddBool(event.closedByRemote);
                GScr_Cod2xRunCallback(slot->onClose, 2);
            }
            break;
        case COD2X_WS_ERROR:
            if (slot->onError && callable) {
                Scr_AddString(event.text ? event.text : "");
                GScr_Cod2xRunCallback(slot->onError, 1);
            }
            break;
        }
        free(event.text);
    }
}

static void WebSocket_Free(int index)
{
    Cod2x_WebSocketFree(webSockets[index].ws);
    memset(&webSockets[index], 0, sizeof(webSockets[index]));
}

/* Every frame: the connections' news, and closed ones give their slot back. */
void GScr_Cod2xWebSocketFrame(void)
{
    for (int i = 0; i < WEBSOCKET_MAX_CLIENTS; ++i) {
        if (!webSockets[i].ws)
            continue;
        WebSocket_Deliver(i);
        if (webSockets[i].ws && Cod2x_WebSocketFinished(webSockets[i].ws))
            WebSocket_Free(i);
    }
}

/* websocket_connect(url, headers, onConnect, onMessage, onClose, onError
   [, reconnectDelayMs, pingIntervalMs]): the connection's number (0-15), or
   -1. Both delays count only when both are given (2000 and 15000 otherwise). */
static void GScr_Cod2xWebSocketConnect(void)
{
    unsigned int count = Scr_GetNumParam();
    unsigned int reconnectMs = 2000, pingMs = 15000;
    const char *url, *headers;
    int index = -1, onConnect, onMessage, onClose, onError;
    Cod2xWebSocket *ws;

    if (count < 6) {
        Scr_Error(va("websocket_connect: not enough parameters, expected 6, got %u", count));
        Scr_AddInt(-1);
        return;
    }
    for (int i = 0; i < WEBSOCKET_MAX_CLIENTS && index < 0; ++i)
        if (!webSockets[i].ws)
            index = i;
    if (index < 0) {
        Scr_Error("No free websocket client slots available.");
        Scr_AddInt(-1);
        return;
    }
    url = Scr_GetString(0);
    headers = Scr_GetString(1);
    onConnect = Scr_GetFunc(2);
    onMessage = Scr_GetFunc(3);
    onClose = Scr_GetFunc(4);
    onError = Scr_GetFunc(5);
    if (count >= 8) {
        reconnectMs = (unsigned int)Scr_GetInt(6);
        pingMs = (unsigned int)Scr_GetInt(7);
    }
    ws = Cod2x_WebSocketConnect(url, headers, reconnectMs, pingMs);
    if (!ws) {
        Scr_AddInt(-1);
        return;
    }
    webSockets[index].ws = ws;
    webSockets[index].connected = 0;
    webSockets[index].onConnect = onConnect;
    webSockets[index].onMessage = onMessage;
    webSockets[index].onClose = onClose;
    webSockets[index].onError = onError;
    GScr_Cod2xScopeNow(&webSockets[index].scope);
    Scr_AddInt(index);
}

static int WebSocket_Index(int index)
{
    return index >= 0 && index < WEBSOCKET_MAX_CLIENTS && webSockets[index].ws;
}

/* websocket_sendText(id, message): false when it is not open. */
static void GScr_Cod2xWebSocketSendText(void)
{
    unsigned int count = Scr_GetNumParam();
    int index;
    const char *message;
    if (count < 2) {
        Scr_Error(va("websocket_sendText: not enough parameters, expected 2, got %u", count));
        Scr_AddBool(0);
        return;
    }
    index = Scr_GetInt(0);
    message = Scr_GetString(1);
    if (!WebSocket_Index(index) || !webSockets[index].connected) {
        Scr_AddBool(0);
        return;
    }
    Cod2x_WebSocketSendText(webSockets[index].ws, message);
    Scr_AddBool(1);
}

/* websocket_close(id): closes it for good. */
static void GScr_Cod2xWebSocketClose(void)
{
    int index;
    if (Scr_GetNumParam() < 1) {
        Scr_AddBool(0);
        return;
    }
    index = Scr_GetInt(0);
    if (!WebSocket_Index(index)) {
        Scr_AddBool(0);
        return;
    }
    Cod2x_WebSocketClose(webSockets[index].ws);
    Scr_AddBool(1);
}

/* ---- map changes and shutdown --------------------------------------------- */

/* After the mod's StopGameType callback. Answers to http_fetch still on their
   way get a second on shutdown (frames no longer run). Connections end with a
   full map change or shutdown: they are closed, their close callbacks run
   (on shutdown for up to a second), and then they are forgotten. A round
   restart that keeps the players' data keeps them. */
void GScr_Cod2xNetBeforeMapChange(int shutdown)
{
    int complete = !level.savepersist, any;

    for (int waited = 0; shutdown && fetchPending > 0 && waited < 1000; waited += 10) {
        Cod2x_HttpFrameFor(Fetch_Done);
        if (fetchPending > 0)
            Cod2x_Sleep(10);
    }
    if (!complete && !shutdown)
        return;
    for (int i = 0; i < WEBSOCKET_MAX_CLIENTS; ++i)
        if (webSockets[i].ws)
            Cod2x_WebSocketClose(webSockets[i].ws);
    /* CoD2x lets each close go out right away, and waits on shutdown. */
    for (int waited = 0; waited < (shutdown ? 1000 : 100); waited += 10) {
        any = 0;
        for (int i = 0; i < WEBSOCKET_MAX_CLIENTS; ++i) {
            if (!webSockets[i].ws)
                continue;
            WebSocket_Deliver(i);
            if (!Cod2x_WebSocketFinished(webSockets[i].ws))
                any = 1;
        }
        if (!any)
            break;
        Cod2x_Sleep(10);
    }
    for (int i = 0; i < WEBSOCKET_MAX_CLIENTS; ++i)
        if (webSockets[i].ws)
            WebSocket_Free(i);
}

void GScr_Cod2xDropCallbacks(void)
{
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
    {"websocket_connect", GScr_Cod2xWebSocketConnect, 0},
    {"websocket_sendText", GScr_Cod2xWebSocketSendText, 0},
    {"websocket_close", GScr_Cod2xWebSocketClose, 0},
    {"matchUploadData", GScr_Cod2xMatchUploadData, 0},
    {"matchSetData", GScr_Cod2xMatchSetData, 0},
    {"matchGetData", GScr_Cod2xMatchGetData, 0},
    {"matchRedownloadData", GScr_Cod2xMatchRedownloadData, 0},
    {"matchClearData", GScr_Cod2xMatchClearData, 0},
    {"matchIsActivated", GScr_Cod2xMatchIsActivated, 0},
    {"matchCancel", GScr_Cod2xMatchCancel, 0},
    {"matchFinish", GScr_Cod2xMatchFinish, 0},
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
