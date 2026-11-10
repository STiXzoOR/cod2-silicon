#ifndef COD2X_WEBSOCKET_H
#define COD2X_WEBSOCKET_H

/* A WebSocket client as CoD2x 1.4.6.8 runs one (cod2x_websocket.c): it
   connects at once, reconnects reconnectMs after an error or a close it did
   not ask for, sends a ping every pingMs (0: never) and gives up on the
   connection when no pong comes within half of that. Only text messages are
   handed over. A thread does the work; the main thread takes the events. */

typedef struct Cod2xWebSocket Cod2xWebSocket;

typedef enum {
    COD2X_WS_OPEN = 1,
    COD2X_WS_MESSAGE,
    COD2X_WS_CLOSE,
    COD2X_WS_ERROR
} Cod2xWebSocketEventType;

typedef struct {
    Cod2xWebSocketEventType type;
    char *text; /* MESSAGE and ERROR; free() it */
    /* CLOSE, with CoD2x's meaning: closedByRemote is set only when the
       connection ended without a close handshake; fullyDisconnected when
       Cod2x_WebSocketClose asked for it (no reconnect follows). */
    int closedByRemote, fullyDisconnected;
} Cod2xWebSocketEvent;

/* url is ws:// or wss:// (http:// and https:// work too); headers are extra
   handshake lines, each ending in "\r\n", sent as given. */
Cod2xWebSocket *Cod2x_WebSocketConnect(const char *url, const char *headers, unsigned int reconnectMs,
                                       unsigned int pingMs);
/* Queued for the connection; the caller checks it is open. */
void Cod2x_WebSocketSendText(Cod2xWebSocket *ws, const char *text);
/* Closes it for good. */
void Cod2x_WebSocketClose(Cod2xWebSocket *ws);
/* Main thread: the next event, 1 when there was one. */
int Cod2x_WebSocketNextEvent(Cod2xWebSocket *ws, Cod2xWebSocketEvent *event);
/* Closed for good and every event taken. */
int Cod2x_WebSocketFinished(Cod2xWebSocket *ws);
/* Closes it if needed and forgets it; no more events. */
void Cod2x_WebSocketFree(Cod2xWebSocket *ws);

#endif
