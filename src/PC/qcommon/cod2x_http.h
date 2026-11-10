#ifndef COD2X_HTTP_H
#define COD2X_HTTP_H

#include <stddef.h>
#include <stdint.h>

/* HTTP(S) for the CoD2x services (cod2x_http.c), on libcurl. Certificates
   and host names are always verified. Requests send only Host, the caller's
   header lines and Content-Length (0 without a body), as CoD2x's client does:
   no User-Agent, Accept, Expect or Content-Type of curl's own. Nothing here runs without libcurl
   (Cod2x_HttpAvailable() is 0 then and every request fails). */

typedef struct {
    const char *method;          /* "GET", "POST", ...; NULL is GET */
    const char *url;
    const char *headers;         /* header lines, each ending in "\r\n", or NULL */
    const void *body;            /* sent when bodyLength > 0, or always for POST */
    size_t bodyLength;
    int timeoutMs;               /* the whole request; 0 waits for ever */
    int connectTimeoutMs;        /* until connected; 0 waits for ever */
    size_t maxBody;              /* longer responses fail with "Response too large"; 0 = any */
} Cod2xHttpRequest;

typedef struct {
    int status;                  /* HTTP status, 0 when no response arrived */
    char *body;                  /* always NUL-terminated (empty when none) */
    size_t length;
    char *headers;               /* raw response header lines, NUL-terminated */
    char error[256];             /* empty when a response arrived, whatever its status */
} Cod2xHttpResponse;

typedef void (*Cod2xHttpDone)(void *context, const Cod2xHttpResponse *response);

typedef struct {
    volatile size_t downloaded;  /* bytes written so far */
    volatile size_t total;       /* Content-Length, 0 when not known */
    volatile int cancel;         /* set to stop a running download */
} Cod2xHttpProgress;

int Cod2x_HttpAvailable(void);
/* CoD2x's URL check: a scheme, "://", something after it, and no spaces,
   control characters, quotes, backticks, angle brackets or backslashes.
   Requests that fail it end with "Invalid URL". */
int Cod2x_HttpValidUrl(const char *url);
/* Once from the main thread before the first request (curl's global setup). */
void Cod2x_HttpInit(void);

/* Blocking; for worker threads. Returns 1 when a response arrived. */
int Cod2x_HttpPerform(const Cod2xHttpRequest *request, Cod2xHttpResponse *response);
void Cod2x_HttpResponseFree(Cod2xHttpResponse *response);

/* Runs the request on a worker thread (the request is copied). done is called
   from Cod2x_HttpFrame() on the main thread; the response is freed after it.
   Returns 0 when the request could not be started (done is not called). */
int Cod2x_HttpStart(const Cod2xHttpRequest *request, Cod2xHttpDone done, void *context);
/* Main thread, every frame: calls done for the finished requests. */
void Cod2x_HttpFrame(void);
/* The same for the requests started with done only (e.g. while shutting down). */
void Cod2x_HttpFrameFor(Cod2xHttpDone done);
/* Waits msec milliseconds. */
void Cod2x_Sleep(int msec);
/* Milliseconds since 1970 (UTC), and one as "2026-11-10T19:08:23.512Z". */
long long Cod2x_UtcMsec(void);
void Cod2x_Iso8601(long long msec, char *out, size_t size);

/* Blocking download to path, as CoD2x downloads files: GET with
   "User-Agent: CoD2x" and "Connection: close", up to 5 redirects, status 200
   only, 30 s without data is "Timeout", the file must start with magic (NULL
   = anything) and must not be empty. Returns 1 when done; otherwise error
   holds CoD2x's text and the file may be left partly written. */
int Cod2x_HttpDownload(const char *url, const char *path, const char *magic, Cod2xHttpProgress *progress,
                       char *error, size_t errorSize);

/* A raw connection to an http:// or https:// address (TLS checked as above),
   for protocols of their own such as WebSocket. Blocking; for worker threads.
   Open gives NULL and an error text on failure. Send sends all or returns 0.
   Recv returns the bytes read, 0 when none came within waitMs, -1 when the
   other side closed and -2 on an error. */
typedef struct Cod2xSocket Cod2xSocket;
Cod2xSocket *Cod2x_SocketOpen(const char *url, int timeoutMs, char *error, size_t errorSize);
int Cod2x_SocketSend(Cod2xSocket *connection, const void *data, size_t length, int timeoutMs);
int Cod2x_SocketRecv(Cod2xSocket *connection, void *buffer, size_t size, int waitMs);
void Cod2x_SocketClose(Cod2xSocket *connection);

/* SHA-256 of a file as lowercase hex; 0 when it can't be read. */
int Cod2x_Sha256File(const char *path, char hex[65]);

/* Runs run(argument) on a new detached thread; 0 when it could not start. */
int Cod2x_ThreadStart(void (*run)(void *argument), void *argument);
/* Random bytes from the system. */
void Cod2x_Random(void *out, size_t length);
/* A process-wide lock for the small amounts of state the workers share. */
void Cod2x_Lock(void);
void Cod2x_Unlock(void);
/* Locks folder for this process against other CoD2x servers sharing it (an
   flock on the folder, or on Windows a named mutex per name and folder, as
   CoD2x does). Does not wait: -1 when someone else holds it. The system
   releases it when the process ends. */
intptr_t Cod2x_LockFolder(const char *folder, const char *name);
void Cod2x_UnlockFolder(intptr_t lock);
/* Moves from over to, replacing it in one step. */
int Cod2x_ReplaceFile(const char *from, const char *to);

#endif
