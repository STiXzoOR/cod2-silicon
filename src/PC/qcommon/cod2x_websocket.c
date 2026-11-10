#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_websocket.h"
#include "cod2x_http.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

/* RFC 6455 over a raw connection from cod2x_http.c, following what CoD2x's
   client (mongoose) does: the handshake lines in its order, no check of
   Sec-WebSocket-Accept, any 101 answer opens, frames up to 3 MB, fragments
   joined, pings answered, a close answered and the connection dropped. A
   failed read or write after connecting ends the connection without an
   error, as there. */

#define WS_MAX_RECEIVE (3u * 1024u * 1024u)
#define WS_POLL_MSEC 20
#define WS_SEND_MSEC 5000

enum { WS_CONTINUE = 0, WS_TEXT = 1, WS_BINARY = 2, WS_CLOSE = 8, WS_PING = 9, WS_PONG = 10 };

typedef struct WsEvent {
    Cod2xWebSocketEvent event;
    struct WsEvent *next;
} WsEvent;

typedef struct WsOutgoing {
    char *text;
    struct WsOutgoing *next;
} WsOutgoing;

struct Cod2xWebSocket {
    /* Set before the thread starts. */
    char *url, *headers;
    unsigned int reconnectMs, pingMs, pongMs;
    /* Shared, under Cod2x_Lock. */
    int disconnect, abandoned, finished;
    WsEvent *events, **eventsTail;
    WsOutgoing *outgoing, **outgoingTail;
};

static unsigned long long Ws_Now(void)
{
#ifdef _WIN32
    return (unsigned long long)GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (unsigned long long)now.tv_sec * 1000ULL + (unsigned long long)now.tv_nsec / 1000000ULL;
#endif
}

static char *Ws_Copy(const char *s, size_t length)
{
    char *copy = malloc(length + 1);
    if (copy) {
        memcpy(copy, s, length);
        copy[length] = '\0';
    }
    return copy;
}

static void Ws_Push(Cod2xWebSocket *ws, Cod2xWebSocketEventType type, const char *text, size_t length, int remote,
                    int full)
{
    WsEvent *item = calloc(1, sizeof(*item));
    if (!item)
        return;
    item->event.type = type;
    item->event.text = text ? Ws_Copy(text, length) : NULL;
    item->event.closedByRemote = remote;
    item->event.fullyDisconnected = full;
    Cod2x_Lock();
    *ws->eventsTail = item;
    ws->eventsTail = &item->next;
    Cod2x_Unlock();
}

static void Ws_PushError(Cod2xWebSocket *ws, const char *error)
{
    Ws_Push(ws, COD2X_WS_ERROR, error, strlen(error), 0, 0);
}

static int Ws_Disconnecting(Cod2xWebSocket *ws)
{
    int disconnect;
    Cod2x_Lock();
    disconnect = ws->disconnect;
    Cod2x_Unlock();
    return disconnect;
}

/* Waits msec, or less when a close is asked for. */
static void Ws_Wait(Cod2xWebSocket *ws, unsigned int msec)
{
    unsigned long long end = Ws_Now() + msec;
    while (!Ws_Disconnecting(ws) && Ws_Now() < end)
        Cod2x_Sleep(10);
}

static void Ws_FreeAll(Cod2xWebSocket *ws)
{
    while (ws->events) {
        WsEvent *next = ws->events->next;
        free(ws->events->event.text);
        free(ws->events);
        ws->events = next;
    }
    while (ws->outgoing) {
        WsOutgoing *next = ws->outgoing->next;
        free(ws->outgoing->text);
        free(ws->outgoing);
        ws->outgoing = next;
    }
    free(ws->url);
    free(ws->headers);
    free(ws);
}

/* A frame from the client: final, masked. */
static int Ws_SendFrame(Cod2xSocket *connection, int opcode, const void *data, size_t length)
{
    unsigned char header[14], *frame;
    size_t used = 0;
    unsigned char mask[4];
    int ok;

    header[used++] = (unsigned char)(0x80 | opcode);
    if (length < 126) {
        header[used++] = (unsigned char)(0x80 | length);
    } else if (length < 65536) {
        header[used++] = 0x80 | 126;
        header[used++] = (unsigned char)(length >> 8);
        header[used++] = (unsigned char)length;
    } else {
        header[used++] = 0x80 | 127;
        for (int shift = 56; shift >= 0; shift -= 8)
            header[used++] = (unsigned char)((unsigned long long)length >> shift);
    }
    Cod2x_Random(mask, sizeof(mask));
    memcpy(header + used, mask, 4);
    used += 4;
    frame = malloc(used + length);
    if (!frame)
        return 0;
    memcpy(frame, header, used);
    for (size_t i = 0; i < length; ++i)
        frame[used + i] = ((const unsigned char *)data)[i] ^ mask[i & 3];
    ok = Cod2x_SocketSend(connection, frame, used + length, WS_SEND_MSEC);
    free(frame);
    return ok;
}

/* "ws://host:port/path" as http(s) for curl, plus the Host and path CoD2x sends. */
static void Ws_SplitUrl(const char *url, char *http, size_t httpSize, char *host, size_t hostSize, const char **path)
{
    const char *rest = strstr(url, "://"), *start, *end;
    size_t schemeLength = rest ? (size_t)(rest - url) : 0;
    const char *scheme = "http";
    if ((schemeLength == 3 && !strncmp(url, "wss", 3)) || (schemeLength == 5 && !strncmp(url, "https", 5)))
        scheme = "https";
    snprintf(http, httpSize, "%s%s", scheme, rest ? rest : "");
    start = rest ? rest + 3 : url;
    *path = strchr(start, '/');
    end = *path ? *path : start + strlen(start);
    {
        const char *at = memchr(start, '@', (size_t)(end - start));
        if (at)
            start = at + 1;
    }
    if (*start == '[') {
        const char *close = memchr(start, ']', (size_t)(end - start));
        if (close)
            end = close + 1;
    } else {
        const char *colon = memchr(start, ':', (size_t)(end - start));
        if (colon)
            end = colon;
    }
    snprintf(host, hostSize, "%.*s", (int)(end - start), start);
    if (!*path)
        *path = "/";
}

static void Ws_Base64(const unsigned char *data, size_t length, char *out)
{
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < length; i += 3) {
        unsigned int value = (unsigned int)data[i] << 16;
        if (i + 1 < length)
            value |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < length)
            value |= data[i + 2];
        out[o++] = digits[(value >> 18) & 63];
        out[o++] = digits[(value >> 12) & 63];
        out[o++] = i + 1 < length ? digits[(value >> 6) & 63] : '=';
        out[o++] = i + 2 < length ? digits[value & 63] : '=';
    }
    out[o] = '\0';
}

typedef struct {
    unsigned char *data;
    size_t length, capacity;
} WsBuffer;

static int Ws_Append(WsBuffer *buffer, const void *data, size_t length)
{
    if (buffer->length + length > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096;
        unsigned char *grown;
        while (capacity < buffer->length + length)
            capacity *= 2;
        grown = realloc(buffer->data, capacity);
        if (!grown)
            return 0;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    return 1;
}

static void Ws_Consume(WsBuffer *buffer, size_t length)
{
    memmove(buffer->data, buffer->data + length, buffer->length - length);
    buffer->length -= length;
}

/* Connects and shakes hands before the deadline. NULL after an error event. */
static Cod2xSocket *Ws_Open(Cod2xWebSocket *ws, unsigned long long deadline, WsBuffer *received)
{
    char http[1024], host[512], error[256], request[8192];
    unsigned char nonce[16];
    char key[32];
    const char *path;
    Cod2xSocket *connection;
    unsigned long long now = Ws_Now();
    int length;

    if (now >= deadline) {
        Ws_PushError(ws, "Connect timeout");
        return NULL;
    }
    Ws_SplitUrl(ws->url, http, sizeof(http), host, sizeof(host), &path);
    connection = Cod2x_SocketOpen(http, (int)(deadline - now), error, sizeof(error));
    if (!connection) {
        Ws_PushError(ws, !strcmp(error, "Connection timeout") || !strcmp(error, "Timeout") ? "Connect timeout" : error);
        return NULL;
    }
    Cod2x_Random(nonce, sizeof(nonce));
    Ws_Base64(nonce, sizeof(nonce), key);
    length = snprintf(request, sizeof(request),
                      "GET %s HTTP/1.1\r\nUpgrade: websocket\r\nHost: %s\r\nConnection: Upgrade\r\n"
                      "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\n%s\r\n",
                      path, host, key, ws->headers ? ws->headers : "");
    if (length <= 0 || (size_t)length >= sizeof(request) ||
        !Cod2x_SocketSend(connection, request, (size_t)length, WS_SEND_MSEC)) {
        Cod2x_SocketClose(connection);
        return NULL;
    }
    for (;;) {
        unsigned char chunk[4096];
        const unsigned char *end;
        int n;
        if (Ws_Disconnecting(ws)) {
            Cod2x_SocketClose(connection);
            return NULL;
        }
        if (Ws_Now() >= deadline) {
            Ws_PushError(ws, "Connect timeout");
            Cod2x_SocketClose(connection);
            return NULL;
        }
        n = Cod2x_SocketRecv(connection, chunk, sizeof(chunk), WS_POLL_MSEC);
        if (n < 0) {
            Cod2x_SocketClose(connection);
            return NULL;
        }
        if (n > 0 && !Ws_Append(received, chunk, (size_t)n)) {
            Cod2x_SocketClose(connection);
            return NULL;
        }
        end = NULL;
        for (size_t i = 3; i < received->length && !end; ++i)
            if (!memcmp(received->data + i - 3, "\r\n\r\n", 4))
                end = received->data + i + 1;
        if (!end) {
            if (received->length > WS_MAX_RECEIVE) {
                Ws_PushError(ws, "MG_MAX_RECV_SIZE");
                Cod2x_SocketClose(connection);
                return NULL;
            }
            continue;
        }
        if (end - received->data < 15 || memcmp(received->data + 9, "101", 3)) {
            Ws_PushError(ws, "ws handshake error");
            Cod2x_SocketClose(connection);
            return NULL;
        }
        Ws_Consume(received, (size_t)(end - received->data));
        return connection;
    }
}

typedef enum { WS_GOING, WS_EOF, WS_CLOSED, WS_FAILED } WsState;

/* Handles the complete frames in received. */
static WsState Ws_Frames(Cod2xWebSocket *ws, Cod2xSocket *connection, WsBuffer *received, WsBuffer *message,
                         int *messageOp, int *closing, int *waitingPong)
{
    for (;;) {
        const unsigned char *p = received->data;
        size_t header = 2, length;
        int final, opcode, masked;
        unsigned char mask[4] = {0};
        unsigned char *payload;

        if (received->length < 2)
            return WS_GOING;
        final = p[0] & 0x80;
        opcode = p[0] & 0x0f;
        masked = p[1] & 0x80;
        length = p[1] & 0x7f;
        if (length == 126) {
            if (received->length < 4)
                return WS_GOING;
            length = ((size_t)p[2] << 8) | p[3];
            header = 4;
        } else if (length == 127) {
            if (received->length < 10)
                return WS_GOING;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = (length << 8) | p[2 + i];
            header = 10;
        }
        if (masked) {
            if (received->length < header + 4)
                return WS_GOING;
            memcpy(mask, p + header, 4);
            header += 4;
        }
        if (length > WS_MAX_RECEIVE || message->length + length > WS_MAX_RECEIVE) {
            Ws_PushError(ws, "MG_MAX_RECV_SIZE");
            return WS_FAILED;
        }
        if (received->length < header + length)
            return WS_GOING;
        payload = received->data + header;
        if (masked)
            for (size_t i = 0; i < length; ++i)
                payload[i] ^= mask[i & 3];

        switch (opcode) {
        case WS_TEXT:
        case WS_BINARY:
            if (final) {
                if (opcode == WS_TEXT)
                    Ws_Push(ws, COD2X_WS_MESSAGE, (const char *)payload, length, 0, 0);
            } else {
                *messageOp = opcode;
                message->length = 0;
                Ws_Append(message, payload, length);
            }
            break;
        case WS_CONTINUE:
            Ws_Append(message, payload, length);
            if (final) {
                if (*messageOp == WS_TEXT)
                    Ws_Push(ws, COD2X_WS_MESSAGE, (const char *)message->data, message->length, 0, 0);
                message->length = 0;
                *messageOp = 0;
            }
            break;
        case WS_PING:
            if (!Ws_SendFrame(connection, WS_PONG, payload, length))
                return WS_EOF;
            break;
        case WS_PONG:
            *waitingPong = 0;
            break;
        case WS_CLOSE:
            /* Answered with the same payload, and the connection is dropped. */
            Ws_SendFrame(connection, WS_CLOSE, payload, length);
            *closing = 1;
            Ws_Consume(received, header + length);
            return WS_CLOSED;
        default: {
            char error[64];
            snprintf(error, sizeof(error), "unknown WS op %d", opcode);
            Ws_PushError(ws, error);
            return WS_FAILED;
        }
        }
        Ws_Consume(received, header + length);
    }
}

/* One open connection until it ends. */
static void Ws_Session(Cod2xWebSocket *ws, Cod2xSocket *connection, WsBuffer *received)
{
    WsBuffer message = {0};
    int messageOp = 0, closing = 0, waitingPong = 0, disconnect;
    unsigned long long nextPing = Ws_Now() + ws->pingMs, pongDeadline = 0;
    WsState state = WS_GOING;

    Ws_Push(ws, COD2X_WS_OPEN, NULL, 0, 0, 0);
    while (state == WS_GOING) {
        WsOutgoing *outgoing;
        unsigned long long now;
        unsigned char chunk[16384];
        int n;

        Cod2x_Lock();
        outgoing = ws->outgoing;
        ws->outgoing = NULL;
        ws->outgoingTail = &ws->outgoing;
        disconnect = ws->disconnect;
        Cod2x_Unlock();
        while (outgoing) {
            WsOutgoing *next = outgoing->next;
            if (state == WS_GOING && !Ws_SendFrame(connection, WS_TEXT, outgoing->text, strlen(outgoing->text)))
                state = WS_EOF;
            free(outgoing->text);
            free(outgoing);
            outgoing = next;
        }
        if (state != WS_GOING)
            break;
        if (disconnect) {
            Ws_SendFrame(connection, WS_CLOSE, "", 0);
            closing = 1;
            break;
        }
        now = Ws_Now();
        if (ws->pingMs && now >= nextPing) {
            Ws_SendFrame(connection, WS_PING, "", 0);
            nextPing = now + ws->pingMs;
            waitingPong = ws->pongMs > 0;
            pongDeadline = now + ws->pongMs;
        }
        if (waitingPong && ws->pongMs && now >= pongDeadline) {
            Ws_PushError(ws, "ping timeout (no pong received within time limit)");
            closing = 1;
            break;
        }
        n = Cod2x_SocketRecv(connection, chunk, sizeof(chunk), WS_POLL_MSEC);
        if (n < 0) {
            state = WS_EOF;
        } else if (n > 0) {
            if (!Ws_Append(received, chunk, (size_t)n) || received->length > WS_MAX_RECEIVE + 14) {
                Ws_PushError(ws, "MG_MAX_RECV_SIZE");
                state = WS_FAILED;
            } else {
                state = Ws_Frames(ws, connection, received, &message, &messageOp, &closing, &waitingPong);
            }
        }
    }
    Cod2x_SocketClose(connection);
    free(message.data);
    Ws_Push(ws, COD2X_WS_CLOSE, NULL, 0, !closing, Ws_Disconnecting(ws));
}

static void Ws_Run(void *argument)
{
    Cod2xWebSocket *ws = argument;
    int abandoned;
    while (!Ws_Disconnecting(ws)) {
        WsBuffer received = {0};
        unsigned long long start = Ws_Now();
        Cod2xSocket *connection = Ws_Open(ws, start + ws->reconnectMs, &received);
        if (connection) {
            if (Ws_Disconnecting(ws)) {
                /* Asked to close while connecting: no open, no close event. */
                Cod2x_SocketClose(connection);
                free(received.data);
                break;
            }
            Ws_Session(ws, connection, &received);
        }
        free(received.data);
        /* At least a moment, so a zero delay can't spin. */
        Ws_Wait(ws, ws->reconnectMs ? ws->reconnectMs : 50);
    }
    Cod2x_Lock();
    ws->finished = 1;
    abandoned = ws->abandoned;
    Cod2x_Unlock();
    if (abandoned)
        Ws_FreeAll(ws);
}

Cod2xWebSocket *Cod2x_WebSocketConnect(const char *url, const char *headers, unsigned int reconnectMs,
                                       unsigned int pingMs)
{
    Cod2xWebSocket *ws = calloc(1, sizeof(*ws));
    if (!ws)
        return NULL;
    ws->url = Ws_Copy(url, strlen(url));
    ws->headers = Ws_Copy(headers ? headers : "", headers ? strlen(headers) : 0);
    ws->reconnectMs = reconnectMs;
    ws->pingMs = pingMs;
    ws->pongMs = pingMs / 2;
    ws->eventsTail = &ws->events;
    ws->outgoingTail = &ws->outgoing;
    if (!ws->url || !ws->headers || !Cod2x_ThreadStart(Ws_Run, ws)) {
        Ws_FreeAll(ws);
        return NULL;
    }
    return ws;
}

void Cod2x_WebSocketSendText(Cod2xWebSocket *ws, const char *text)
{
    WsOutgoing *item = calloc(1, sizeof(*item));
    if (!item)
        return;
    item->text = Ws_Copy(text, strlen(text));
    if (!item->text) {
        free(item);
        return;
    }
    Cod2x_Lock();
    *ws->outgoingTail = item;
    ws->outgoingTail = &item->next;
    Cod2x_Unlock();
}

void Cod2x_WebSocketClose(Cod2xWebSocket *ws)
{
    Cod2x_Lock();
    ws->disconnect = 1;
    Cod2x_Unlock();
}

int Cod2x_WebSocketNextEvent(Cod2xWebSocket *ws, Cod2xWebSocketEvent *event)
{
    WsEvent *item;
    Cod2x_Lock();
    item = ws->events;
    if (item) {
        ws->events = item->next;
        if (!ws->events)
            ws->eventsTail = &ws->events;
    }
    Cod2x_Unlock();
    if (!item)
        return 0;
    *event = item->event;
    free(item);
    return 1;
}

int Cod2x_WebSocketFinished(Cod2xWebSocket *ws)
{
    int finished;
    Cod2x_Lock();
    finished = ws->finished && !ws->events;
    Cod2x_Unlock();
    return finished;
}

void Cod2x_WebSocketFree(Cod2xWebSocket *ws)
{
    int finished;
    if (!ws)
        return;
    Cod2x_Lock();
    ws->disconnect = 1;
    ws->abandoned = 1;
    finished = ws->finished;
    Cod2x_Unlock();
    if (finished)
        Ws_FreeAll(ws);
}
#endif
