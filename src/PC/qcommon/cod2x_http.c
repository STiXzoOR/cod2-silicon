#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_http.h"
#include "cod2x_http_parse.h"
#include <ctype.h>
#include <limits.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#include <bcrypt.h>
#define strncasecmp _strnicmp
#else
#include <strings.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define COD2X_HTTP_CURL 1
#include <curl/curl.h>

/* ---- threads, lock, random ---------------------------------------------- */

#ifdef _WIN32
static SRWLOCK http_lock = SRWLOCK_INIT;
void Cod2x_Lock(void) { AcquireSRWLockExclusive(&http_lock); }
void Cod2x_Unlock(void) { ReleaseSRWLockExclusive(&http_lock); }

typedef struct {
    void (*run)(void *);
    void *argument;
} HttpThread;

static unsigned __stdcall Http_ThreadMain(void *pointer)
{
    HttpThread thread = *(HttpThread *)pointer;
    free(pointer);
    thread.run(thread.argument);
    return 0;
}

int Cod2x_ThreadStart(void (*run)(void *argument), void *argument)
{
    HttpThread *thread = malloc(sizeof(*thread));
    uintptr_t handle;
    if (!thread)
        return 0;
    thread->run = run;
    thread->argument = argument;
    handle = _beginthreadex(NULL, 0, Http_ThreadMain, thread, 0, NULL);
    if (!handle) {
        free(thread);
        return 0;
    }
    CloseHandle((HANDLE)handle);
    return 1;
}

void Cod2x_Random(void *out, size_t length)
{
    if (BCryptGenRandom(NULL, out, (ULONG)length, BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        for (size_t i = 0; i < length; ++i)
            ((unsigned char *)out)[i] = (unsigned char)(GetTickCount() * 2654435761u >> (i % 24));
    }
}
#else
static pthread_mutex_t http_lock = PTHREAD_MUTEX_INITIALIZER;
void Cod2x_Lock(void) { pthread_mutex_lock(&http_lock); }
void Cod2x_Unlock(void) { pthread_mutex_unlock(&http_lock); }

typedef struct {
    void (*run)(void *);
    void *argument;
} HttpThread;

static void *Http_ThreadMain(void *pointer)
{
    HttpThread thread = *(HttpThread *)pointer;
    free(pointer);
    thread.run(thread.argument);
    return NULL;
}

int Cod2x_ThreadStart(void (*run)(void *argument), void *argument)
{
    HttpThread *thread = malloc(sizeof(*thread));
    pthread_t handle;
    pthread_attr_t attributes;
    int started;
    if (!thread)
        return 0;
    thread->run = run;
    thread->argument = argument;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    started = pthread_create(&handle, &attributes, Http_ThreadMain, thread) == 0;
    pthread_attr_destroy(&attributes);
    if (!started)
        free(thread);
    return started;
}

void Cod2x_Random(void *out, size_t length)
{
#ifdef __APPLE__
    arc4random_buf(out, length);
#else
    int file = open("/dev/urandom", O_RDONLY);
    size_t done = 0;
    if (file >= 0) {
        while (done < length) {
            ssize_t got = read(file, (char *)out + done, length - done);
            if (got <= 0)
                break;
            done += (size_t)got;
        }
        close(file);
    }
    for (; done < length; ++done)
        ((unsigned char *)out)[done] = (unsigned char)(rand() >> 3);
#endif
}
#endif

/* ---- shared folders --------------------------------------------------- */

#ifdef _WIN32
intptr_t Cod2x_LockFolder(const char *folder, const char *name)
{
    /* A mutex name can't hold backslashes: FNV-1a of the folder, case folded. */
    uint32_t hash = 2166136261u;
    char mutexName[96];
    HANDLE mutex;
    DWORD wait;
    for (const char *p = folder; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c - 'A' + 'a');
        hash = (hash ^ c) * 16777619u;
    }
    snprintf(mutexName, sizeof(mutexName), "Global\\CoD2x_%s_%08x", name, (unsigned int)hash);
    mutex = CreateMutexA(NULL, FALSE, mutexName);
    if (!mutex)
        return -1;
    wait = WaitForSingleObject(mutex, 0);
    if (wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED) {
        CloseHandle(mutex);
        return -1;
    }
    return (intptr_t)mutex;
}

void Cod2x_UnlockFolder(intptr_t lock)
{
    ReleaseMutex((HANDLE)lock);
    CloseHandle((HANDLE)lock);
}

int Cod2x_ReplaceFile(const char *from, const char *to)
{
    return MoveFileExA(from, to, MOVEFILE_REPLACE_EXISTING) != 0;
}
#else
intptr_t Cod2x_LockFolder(const char *folder, const char *name)
{
    int file = open(folder, O_RDONLY);
    (void)name; /* each feature locks its own folder */
    if (file < 0)
        return -1;
    if (flock(file, LOCK_EX | LOCK_NB) != 0) {
        close(file);
        return -1;
    }
    return file;
}

void Cod2x_UnlockFolder(intptr_t lock)
{
    close((int)lock);
}

int Cod2x_ReplaceFile(const char *from, const char *to)
{
    return rename(from, to) == 0;
}
#endif

/* ---- SHA-256 (FIPS 180-4) ------------------------------------------------ */

typedef struct {
    uint32_t state[8];
    uint64_t length;
    unsigned char block[64];
    size_t used;
} Sha256;

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define SHA_ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void Sha256_Block(Sha256 *sha, const unsigned char *data)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t)data[i * 4] << 24 | (uint32_t)data[i * 4 + 1] << 16 | (uint32_t)data[i * 4 + 2] << 8 |
               data[i * 4 + 3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = SHA_ROR(w[i - 15], 7) ^ SHA_ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = SHA_ROR(w[i - 2], 17) ^ SHA_ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = sha->state[0]; b = sha->state[1]; c = sha->state[2]; d = sha->state[3];
    e = sha->state[4]; f = sha->state[5]; g = sha->state[6]; h = sha->state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t t1 = h + (SHA_ROR(e, 6) ^ SHA_ROR(e, 11) ^ SHA_ROR(e, 25)) + ((e & f) ^ (~e & g)) + sha256_k[i] + w[i];
        uint32_t t2 = (SHA_ROR(a, 2) ^ SHA_ROR(a, 13) ^ SHA_ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    sha->state[0] += a; sha->state[1] += b; sha->state[2] += c; sha->state[3] += d;
    sha->state[4] += e; sha->state[5] += f; sha->state[6] += g; sha->state[7] += h;
}

static void Sha256_Init(Sha256 *sha)
{
    static const uint32_t start[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(sha->state, start, sizeof(start));
    sha->length = 0;
    sha->used = 0;
}

static void Sha256_Update(Sha256 *sha, const unsigned char *data, size_t length)
{
    sha->length += length;
    while (length) {
        size_t take = 64 - sha->used < length ? 64 - sha->used : length;
        memcpy(sha->block + sha->used, data, take);
        sha->used += take;
        data += take;
        length -= take;
        if (sha->used == 64) {
            Sha256_Block(sha, sha->block);
            sha->used = 0;
        }
    }
}

static void Sha256_Final(Sha256 *sha, unsigned char digest[32])
{
    uint64_t bits = sha->length * 8;
    unsigned char pad = 0x80, zero = 0, size[8];
    Sha256_Update(sha, &pad, 1);
    while (sha->used != 56)
        Sha256_Update(sha, &zero, 1);
    for (int i = 0; i < 8; ++i)
        size[i] = (unsigned char)(bits >> (56 - 8 * i));
    Sha256_Update(sha, size, 8);
    for (int i = 0; i < 8; ++i) {
        digest[i * 4] = (unsigned char)(sha->state[i] >> 24);
        digest[i * 4 + 1] = (unsigned char)(sha->state[i] >> 16);
        digest[i * 4 + 2] = (unsigned char)(sha->state[i] >> 8);
        digest[i * 4 + 3] = (unsigned char)sha->state[i];
    }
}

int Cod2x_Sha256File(const char *path, char hex[65])
{
    unsigned char buffer[65536], digest[32];
    Sha256 sha;
    size_t got;
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    Sha256_Init(&sha);
    while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
        Sha256_Update(&sha, buffer, got);
    fclose(file);
    Sha256_Final(&sha, digest);
    for (int i = 0; i < 32; ++i)
        snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return 1;
}

/* ---- requests ------------------------------------------------------------- */

int Cod2x_HttpAvailable(void)
{
    return COD2X_HTTP_CURL;
}

void Cod2x_HttpInit(void)
{
    static int initialized;
    if (!initialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        initialized = 1;
    }
}

typedef struct HttpBuffer {
    char *data;
    size_t length, capacity, limit;
    int tooLarge;
} HttpBuffer;

static int Http_Append(HttpBuffer *buffer, const char *data, size_t length)
{
    if (length > SIZE_MAX - buffer->length - 1 ||
        (buffer->limit && length > buffer->limit - buffer->length)) {
        buffer->tooLarge = 1;
        return 0;
    }
    if (buffer->length + length + 1 > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 1024;
        char *grown;
        while (capacity < buffer->length + length + 1)
            capacity *= 2;
        grown = realloc(buffer->data, capacity);
        if (!grown)
            return 0;
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static size_t Http_Body(char *data, size_t size, size_t count, void *user)
{
    return Http_Append(user, data, size * count) ? size * count : 0;
}

static size_t Http_Headers(char *data, size_t size, size_t count, void *user)
{
    HttpBuffer *headers = user;
    /* A new status line (after an informational response) starts over. */
    if (size * count >= 5 && !memcmp(data, "HTTP/", 5))
        headers->length = 0;
    return Http_Append(headers, data, size * count) ? size * count : 0;
}

static int Http_HasHeader(const char *headers, const char *name)
{
    size_t length = strlen(name);
    const char *line = headers;
    while (line && *line) {
        if (!strncasecmp(line, name, length) && line[length] == ':')
            return 1;
        line = strstr(line, "\r\n");
        if (line)
            line += 2;
    }
    return 0;
}

/* The caller's lines, and curl's own defaults switched off. */
static struct curl_slist *Http_HeaderList(const char *headers, int body)
{
    struct curl_slist *list = NULL;
    const char *line = headers;
    while (line && *line) {
        const char *end = strstr(line, "\r\n");
        size_t length = end ? (size_t)(end - line) : strlen(line);
        if (length) {
            char *copy = malloc(length + 1);
            if (copy) {
                memcpy(copy, line, length);
                copy[length] = '\0';
                list = curl_slist_append(list, copy);
                free(copy);
            }
        }
        line = end ? end + 2 : NULL;
    }
    if (!Http_HasHeader(headers, "Accept"))
        list = curl_slist_append(list, "Accept:");
    if (!Http_HasHeader(headers, "Expect"))
        list = curl_slist_append(list, "Expect:");
    if (body && !Http_HasHeader(headers, "Content-Type"))
        list = curl_slist_append(list, "Content-Type:");
    return list;
}

static void Http_Common(CURL *curl)
{
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
}

/* CoD2x's words for what went wrong, curl's for the rest. */
int Cod2x_HttpValidUrl(const char *url)
{
    const char *p = url;
    if (!p || !isalpha((unsigned char)*p))
        return 0;
    for (++p; isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.'; ++p)
        ;
    if (strncmp(p, "://", 3) || !p[3])
        return 0;
    for (p += 3; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c <= ' ' || c > '~' || c == '"' || c == '`' || c == '<' || c == '>' || c == '\\')
            return 0;
    }
    return 1;
}

/* curl's failures in the words CoD2x's own client uses for them. */
static void Http_Error(CURL *curl, const Cod2xHttpRequest *request, CURLcode code, char *error, size_t size)
{
    curl_off_t connected = 0;
    switch (code) {
    case CURLE_URL_MALFORMAT:
    case CURLE_UNSUPPORTED_PROTOCOL:
        snprintf(error, size, "Invalid URL");
        break;
    case CURLE_COULDNT_RESOLVE_HOST:
        snprintf(error, size, "DNS error");
        break;
    case CURLE_COULDNT_CONNECT:
        snprintf(error, size, "socket error");
        break;
    case CURLE_OPERATION_TIMEDOUT:
        /* The connect timer runs first while it is the shorter one. */
        curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME_T, &connected);
        snprintf(error, size, "%s",
                 !connected && request->connectTimeoutMs > 0 &&
                         (request->timeoutMs <= 0 || request->connectTimeoutMs <= request->timeoutMs)
                     ? "Connection timeout"
                     : "Timeout");
        break;
    case CURLE_GOT_NOTHING:
    case CURLE_RECV_ERROR:
    case CURLE_PARTIAL_FILE:
        snprintf(error, size, "Connection closed unexpectedly");
        break;
    default:
        snprintf(error, size, "%s", curl_easy_strerror(code));
        break;
    }
}

/* Async jobs use a multi handle so cancellation also interrupts an idle socket
   or a request with no timeout. Only its worker touches the curl handles. */
static CURLcode Http_Transfer(CURL *curl, const int *cancel)
{
    CURLM *multi;
    CURLcode result = CURLE_FAILED_INIT;
    int running = 0;
    if (!cancel)
        return curl_easy_perform(curl);
    multi = curl_multi_init();
    if (!multi)
        return result;
    if (curl_multi_add_handle(multi, curl) == CURLM_OK) {
        for (;;) {
            int stop, remaining;
            CURLMsg *message;
            Cod2x_Lock();
            stop = *cancel;
            Cod2x_Unlock();
            if (stop) {
                result = CURLE_ABORTED_BY_CALLBACK;
                break;
            }
            if (curl_multi_perform(multi, &running) != CURLM_OK)
                break;
            if (!running) {
                while ((message = curl_multi_info_read(multi, &remaining)) != NULL)
                    if (message->msg == CURLMSG_DONE)
                        result = message->data.result;
                break;
            }
            if (curl_multi_poll(multi, NULL, 0, 20, NULL) != CURLM_OK)
                break;
        }
        curl_multi_remove_handle(multi, curl);
    }
    curl_multi_cleanup(multi);
    return result;
}

static int Http_PerformBounded(const Cod2xHttpRequest *request, Cod2xHttpResponse *response, const int *cancel);

static int Http_Perform(const Cod2xHttpRequest *request, Cod2xHttpResponse *response, const int *cancel)
{
    if (request->maxReceive)
        return Http_PerformBounded(request, response, cancel);
    HttpBuffer body = {0}, headers = {0};
    struct curl_slist *list;
    const char *method = request->method ? request->method : "GET";
    int post = !strcmp(method, "POST");
    int hasBody = post || request->bodyLength > 0;
    char length[64];
    long status = 0;
    CURLcode code;
    CURL *curl;

    memset(response, 0, sizeof(*response));
    curl = Cod2x_HttpValidUrl(request->url) ? curl_easy_init() : NULL;
    if (!curl) {
        snprintf(response->error, sizeof(response->error), "%s",
                 Cod2x_HttpValidUrl(request->url) ? "Out of memory" : "Invalid URL");
        response->body = calloc(1, 1);
        response->headers = calloc(1, 1);
        return 0;
    }
    body.limit = request->maxBody;
    list = Http_HeaderList(request->headers, hasBody);
    /* CoD2x states the body length on every request, also an empty one. */
    if (!hasBody) {
        snprintf(length, sizeof(length), "Content-Length: 0");
        list = curl_slist_append(list, length);
    }
    Http_Common(curl);
    curl_easy_setopt(curl, CURLOPT_URL, request->url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);
    if (post) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (strcmp(method, "GET") || hasBody) {
        /* Also a GET with a body, which curl would otherwise send as POST. */
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
        if (!strcmp(method, "HEAD"))
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    }
    if (hasBody) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)request->bodyLength);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request->body ? request->body : "");
    }
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)(request->timeoutMs > 0 ? request->timeoutMs : 0));
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                     (long)(request->connectTimeoutMs > 0 ? request->connectTimeoutMs : 0));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Http_Body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, Http_Headers);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers);

    code = Http_Transfer(curl, cancel);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    if (code == CURLE_WRITE_ERROR && (body.tooLarge || headers.tooLarge))
        snprintf(response->error, sizeof(response->error), "Response too large");
    else if (code == CURLE_ABORTED_BY_CALLBACK)
        snprintf(response->error, sizeof(response->error), "Connection closed unexpectedly");
    else if (code != CURLE_OK)
        Http_Error(curl, request, code, response->error, sizeof(response->error));
    else
        response->status = (int)status;
    curl_slist_free_all(list);
    curl_easy_cleanup(curl);

    response->body = body.data ? body.data : calloc(1, 1);
    response->length = body.length;
    response->headers = headers.data ? headers.data : calloc(1, 1);
    if (response->error[0]) {
        response->status = 0;
        return 0;
    }
    return 1;
}

int Cod2x_HttpPerform(const Cod2xHttpRequest *request, Cod2xHttpResponse *response)
{
    return Http_Perform(request, response, NULL);
}

void Cod2x_HttpResponseFree(Cod2xHttpResponse *response)
{
    free(response->body);
    free(response->headers);
    memset(response, 0, sizeof(*response));
}

/* ---- background requests -------------------------------------------------- */

typedef struct HttpJob {
    Cod2xHttpRequest request;
    char *method, *url, *headers;
    void *body;
    Cod2xHttpDone done;
    void *context;
    Cod2xHttpResponse response;
    int cancel;
    struct HttpJob *next;
} HttpJob;

static HttpJob *http_finished;
static HttpJob *http_active;

static char *Http_Copy(const char *s)
{
    char *copy;
    if (!s)
        return NULL;
    copy = malloc(strlen(s) + 1);
    if (copy)
        strcpy(copy, s);
    return copy;
}

static void Http_JobFree(HttpJob *job)
{
    Cod2x_HttpResponseFree(&job->response);
    free(job->method);
    free(job->url);
    free(job->headers);
    free(job->body);
    free(job);
}

static void Http_JobRun(void *argument)
{
    HttpJob *job = argument, **tail;
    Http_Perform(&job->request, &job->response, &job->cancel);
    Cod2x_Lock();
    for (tail = &http_active; *tail && *tail != job; tail = &(*tail)->next)
        ;
    if (*tail)
        *tail = job->next;
    job->next = NULL;
    for (tail = &http_finished; *tail; tail = &(*tail)->next)
        ;
    *tail = job;
    Cod2x_Unlock();
}

int Cod2x_HttpStart(const Cod2xHttpRequest *request, Cod2xHttpDone done, void *context)
{
    HttpJob *job = calloc(1, sizeof(*job));
    if (!job)
        return 0;
    Cod2x_HttpInit();
    job->request = *request;
    job->method = Http_Copy(request->method ? request->method : "GET");
    job->url = Http_Copy(request->url ? request->url : "");
    job->headers = Http_Copy(request->headers);
    if (request->bodyLength) {
        job->body = malloc(request->bodyLength);
        if (job->body)
            memcpy(job->body, request->body, request->bodyLength);
    }
    if (!job->method || !job->url || (request->headers && !job->headers) || (request->bodyLength && !job->body)) {
        Http_JobFree(job);
        return 0;
    }
    job->request.method = job->method;
    job->request.url = job->url;
    job->request.headers = job->headers;
    job->request.body = job->body;
    job->done = done;
    job->context = context;
    Cod2x_Lock();
    job->next = http_active;
    http_active = job;
    if (!Cod2x_ThreadStart(Http_JobRun, job)) {
        http_active = job->next;
        Cod2x_Unlock();
        Http_JobFree(job);
        return 0;
    }
    Cod2x_Unlock();
    return 1;
}

/* Takes the finished jobs (all, or those for done), oldest first. */
static HttpJob *Http_TakeFinished(Cod2xHttpDone done)
{
    HttpJob *taken = NULL, **takenTail = &taken, **link;
    Cod2x_Lock();
    link = &http_finished;
    while (*link) {
        HttpJob *job = *link;
        if (done && job->done != done) {
            link = &job->next;
            continue;
        }
        *link = job->next;
        job->next = NULL;
        *takenTail = job;
        takenTail = &job->next;
    }
    Cod2x_Unlock();
    return taken;
}

static void Http_Deliver(Cod2xHttpDone done)
{
    HttpJob *jobs;
    jobs = Http_TakeFinished(done);
    while (jobs) {
        HttpJob *next = jobs->next;
        if (jobs->done)
            jobs->done(jobs->context, &jobs->response);
        Http_JobFree(jobs);
        jobs = next;
    }
}

void Cod2x_HttpFrame(void)
{
    Http_Deliver(NULL);
}

void Cod2x_HttpFrameFor(Cod2xHttpDone done)
{
    Http_Deliver(done);
}

void Cod2x_HttpCancelFor(Cod2xHttpDone done)
{
    /* The worker observes this before its next transfer poll (at most 20 ms).
       Do not join here: curl cleanup may wait for an OS resolver thread even
       after cancelling its transfer. Its response remains owned until the
       worker finishes, and normal main-thread delivery frees the context. */
    Cod2x_Lock();
    for (HttpJob *job = http_active; job; job = job->next)
        if (job->done == done)
            job->cancel = 1;
    Cod2x_Unlock();
    Http_Deliver(done);
}

long long Cod2x_UtcMsec(void)
{
#ifdef _WIN32
    FILETIME now;
    ULARGE_INTEGER value;
    GetSystemTimeAsFileTime(&now);
    value.LowPart = now.dwLowDateTime;
    value.HighPart = now.dwHighDateTime;
    return (long long)(value.QuadPart / 10000ULL - 11644473600000ULL);
#else
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (long long)now.tv_sec * 1000LL + now.tv_nsec / 1000000L;
#endif
}

void Cod2x_Iso8601(long long msec, char *out, size_t size)
{
    time_t seconds = (time_t)(msec / 1000);
    struct tm utc;
#ifdef _WIN32
    gmtime_s(&utc, &seconds);
#else
    gmtime_r(&seconds, &utc);
#endif
    snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
             utc.tm_hour, utc.tm_min, utc.tm_sec, (int)(msec % 1000));
}

void Cod2x_Sleep(int msec)
{
#ifdef _WIN32
    Sleep((DWORD)msec);
#else
    usleep((useconds_t)msec * 1000);
#endif
}

/* ---- raw connections ----------------------------------------------------- */

struct Cod2xSocket {
    CURL *curl;
    CURLM *multi;
    curl_socket_t socket;
};

/* Waits up to msec for the socket to become readable (or writable). */
static void Http_Wait(curl_socket_t socket, int write, int msec)
{
    fd_set set;
    struct timeval wait;
    FD_ZERO(&set);
    FD_SET(socket, &set);
    wait.tv_sec = msec / 1000;
    wait.tv_usec = (msec % 1000) * 1000;
    select((int)socket + 1, write ? NULL : &set, write ? &set : NULL, NULL, &wait);
}

Cod2xSocket *Cod2x_SocketOpen(const char *url, int timeoutMs, char *error, size_t errorSize)
{
    Cod2xHttpRequest timers = {0};
    Cod2xSocket *connection;
    curl_socket_t socket = CURL_SOCKET_BAD;
    CURLcode code;
    CURL *curl;

    error[0] = '\0';
    if (!Cod2x_HttpValidUrl(url)) {
        snprintf(error, errorSize, "Invalid URL");
        return NULL;
    }
    curl = curl_easy_init();
    connection = calloc(1, sizeof(*connection));
    if (!curl || !connection) {
        snprintf(error, errorSize, "Out of memory");
        if (curl)
            curl_easy_cleanup(curl);
        free(connection);
        return NULL;
    }
    Http_Common(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
    if (timeoutMs > 0) {
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, (long)timeoutMs);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeoutMs);
    }
    code = curl_easy_perform(curl);
    if (code == CURLE_OK)
        code = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &socket);
    if (code != CURLE_OK || socket == CURL_SOCKET_BAD) {
        timers.connectTimeoutMs = timeoutMs;
        timers.timeoutMs = timeoutMs;
        Http_Error(curl, &timers, code != CURLE_OK ? code : CURLE_COULDNT_CONNECT, error, errorSize);
        curl_easy_cleanup(curl);
        free(connection);
        return NULL;
    }
    connection->curl = curl;
    connection->socket = socket;
    return connection;
}

int Cod2x_SocketSend(Cod2xSocket *connection, const void *data, size_t length, int timeoutMs)
{
    const char *p = data;
    int waited = 0;
    while (length) {
        size_t sent = 0;
        CURLcode code = curl_easy_send(connection->curl, p, length, &sent);
        if (code == CURLE_AGAIN) {
            if (waited >= timeoutMs)
                return 0;
            Http_Wait(connection->socket, 1, 10);
            waited += 10;
            continue;
        }
        if (code != CURLE_OK)
            return 0;
        p += sent;
        length -= sent;
    }
    return 1;
}

int Cod2x_SocketRecv(Cod2xSocket *connection, void *buffer, size_t size, int waitMs)
{
    for (int pass = 0; pass < 2; ++pass) {
        size_t received = 0;
        CURLcode code = curl_easy_recv(connection->curl, buffer, size, &received);
        if (code == CURLE_OK)
            return received ? (int)received : -1; /* 0 bytes: the other side closed */
        if (code != CURLE_AGAIN)
            return -2;
        /* TLS may hold data the socket no longer shows, so read before waiting. */
        if (!pass)
            Http_Wait(connection->socket, 0, waitMs);
    }
    return 0;
}

void Cod2x_SocketClose(Cod2xSocket *connection)
{
    if (!connection)
        return;
    if (connection->multi && connection->curl)
        curl_multi_remove_handle(connection->multi, connection->curl);
    curl_easy_cleanup(connection->curl);
    if (connection->multi)
        curl_multi_cleanup(connection->multi);
    free(connection);
}

/* ---- bounded script responses -------------------------------------------- */

static long long Http_MonotonicMsec(void)
{
#ifdef _WIN32
    return (long long)GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (long long)now.tv_sec * 1000 + now.tv_nsec / 1000000;
#endif
}

static int Http_Cancelled(const int *cancel)
{
    int stop = 0;
    if (cancel) {
        Cod2x_Lock();
        stop = *cancel;
        Cod2x_Unlock();
    }
    return stop;
}

static int Http_ConnectSocket(void *context, curl_socket_t socket, curlsocktype purpose)
{
    Cod2xSocket *connection = context;
    if (purpose == CURLSOCKTYPE_IPCXN) connection->socket = socket;
    return CURL_SOCKOPT_OK;
}

static int Http_TcpConnected(Cod2xSocket *connection)
{
    unsigned char peer[128];
#ifdef _WIN32
    int length = sizeof(peer);
#else
    socklen_t length = sizeof(peer);
#endif
    return connection->socket != CURL_SOCKET_BAD &&
           getpeername(connection->socket, (struct sockaddr *)peer, &length) == 0;
}

/* Retain the multi handle for the life of a CONNECT_ONLY connection. Removing
   its easy handle before send/recv would discard the connection on some curl
   versions. Polling also lets a map shutdown cancel DNS/connect/TLS work. */
static Cod2xSocket *Http_OpenBounded(const Cod2xHttpRequest *request, const int *cancel,
                                      char *error, size_t errorSize)
{
    Cod2xSocket *connection = calloc(1, sizeof(*connection));
    CURLcode result = CURLE_FAILED_INIT;
    int running, connected = 0;
    long long started = Http_MonotonicMsec();
    if (!connection) goto failed;
    connection->socket = CURL_SOCKET_BAD;
    connection->curl = curl_easy_init();
    connection->multi = curl_multi_init();
    if (!connection->curl || !connection->multi) goto failed;
    Http_Common(connection->curl);
    curl_easy_setopt(connection->curl, CURLOPT_URL, request->url);
    curl_easy_setopt(connection->curl, CURLOPT_PROXY, "");
    curl_easy_setopt(connection->curl, CURLOPT_CONNECT_ONLY, 1L);
    curl_easy_setopt(connection->curl, CURLOPT_SOCKOPTFUNCTION, Http_ConnectSocket);
    curl_easy_setopt(connection->curl, CURLOPT_SOCKOPTDATA, connection);
    curl_easy_setopt(connection->curl, CURLOPT_TIMEOUT_MS,
                     (long)(request->timeoutMs > 0 ? request->timeoutMs : 0));
    /* curl includes TLS in its connect timer and interprets zero as 300s.
       CoD2x stops this timer at TCP connect and zero disables it entirely. */
    curl_easy_setopt(connection->curl, CURLOPT_CONNECTTIMEOUT_MS, LONG_MAX);
    if (curl_multi_add_handle(connection->multi, connection->curl) != CURLM_OK) goto failed;
    for (;;) {
        CURLMsg *message;
        int remaining;
        if (Http_Cancelled(cancel)) {
            result = CURLE_ABORTED_BY_CALLBACK;
            break;
        }
        if (!connected) connected = Http_TcpConnected(connection);
        if (!connected && request->connectTimeoutMs > 0 &&
            Http_MonotonicMsec() - started >= request->connectTimeoutMs) {
            result = CURLE_OPERATION_TIMEDOUT;
            break;
        }
        if (curl_multi_perform(connection->multi, &running) != CURLM_OK) break;
        if (!running) {
            while ((message = curl_multi_info_read(connection->multi, &remaining)) != NULL)
                if (message->msg == CURLMSG_DONE) result = message->data.result;
            break;
        }
        if (curl_multi_poll(connection->multi, NULL, 0, 20, NULL) != CURLM_OK) break;
    }
    if (result == CURLE_OK &&
        curl_easy_getinfo(connection->curl, CURLINFO_ACTIVESOCKET, &connection->socket) == CURLE_OK &&
        connection->socket != CURL_SOCKET_BAD)
        return connection;
    if (result == CURLE_OK) result = CURLE_COULDNT_CONNECT;
failed:
    if (connection && connection->curl) {
        if (result == CURLE_ABORTED_BY_CALLBACK)
            snprintf(error, errorSize, "Connection closed unexpectedly");
        else if (result == CURLE_OPERATION_TIMEDOUT)
            snprintf(error, errorSize, "%s",
                     !connected && request->connectTimeoutMs > 0 &&
                     (request->timeoutMs <= 0 || request->connectTimeoutMs <= request->timeoutMs)
                         ? "Connection timeout" : "Timeout");
        else
            Http_Error(connection->curl, request, result, error, errorSize);
    } else {
        snprintf(error, errorSize, "Out of memory");
    }
    Cod2x_SocketClose(connection);
    return NULL;
}

/* Host excludes credentials and port; the request target keeps the URL's
   literal slash suffix, including a fragment, as the CoD2x client sends it. */
static char *Http_RequestBytes(const Cod2xHttpRequest *request, size_t *length)
{
    const char *host = strstr(request->url, "://") + 3;
    const char *uri = strchr(host, '/'), *end = uri ? uri : host + strlen(host);
    const char *method = request->method ? request->method : "GET";
    const char *headers = request->headers ? request->headers : "";
    char *wire;
    size_t capacity;
    int count;
    for (const char *p = host; p < end; ++p)
        if (*p == '@') host = p + 1;
    for (const char *p = host; p < end; ++p) {
        if (*p == '[') {
            while (p < end && *p != ']') ++p;
            if (p == end) break;
        } else if (*p == ':') {
            end = p;
            break;
        }
    }
    if (!uri) uri = "/";
    capacity = strlen(method) + strlen(uri) + (size_t)(end - host) + strlen(headers) + 128;
    if (request->bodyLength > SIZE_MAX - capacity) return NULL;
    capacity += request->bodyLength;
    wire = malloc(capacity);
    if (!wire) return NULL;
    count = snprintf(wire, capacity, "%s %s HTTP/1.1\r\nHost: %.*s\r\n%s%sContent-Length: %zu\r\n\r\n",
                     method, uri, (int)(end - host), host, headers, *headers ? "\r\n" : "",
                     request->bodyLength);
    if (count < 0 || (size_t)count >= capacity) {
        free(wire);
        return NULL;
    }
    if (request->bodyLength) memcpy(wire + count, request->body, request->bodyLength);
    *length = (size_t)count + request->bodyLength;
    return wire;
}

static int Http_PerformBounded(const Cod2xHttpRequest *request, Cod2xHttpResponse *response, const int *cancel)
{
    Cod2xSocket *connection = NULL;
    HttpBuffer received = {0};
    Cod2xHttpParsed parsed;
    char *wire = NULL;
    const char *error = NULL;
    size_t wireLength = 0, sent = 0;
    long long started = Http_MonotonicMsec();
    int closed = 0, success = 0;
    memset(response, 0, sizeof(*response));
    if (!Cod2x_HttpValidUrl(request->url)) {
        error = "Invalid URL";
        goto finish;
    }
    wire = Http_RequestBytes(request, &wireLength);
    if (!wire) {
        error = "Out of memory";
        goto finish;
    }
    connection = Http_OpenBounded(request, cancel, response->error, sizeof(response->error));
    if (!connection) goto finish;
    received.limit = request->maxReceive;
    while (!success) {
        int parsedResult;
        if (Http_Cancelled(cancel)) {
            error = "Connection closed unexpectedly";
            break;
        }
        if (request->timeoutMs > 0 && Http_MonotonicMsec() - started >= request->timeoutMs) {
            error = "Timeout";
            break;
        }
        if (sent < wireLength) {
            size_t count = 0;
            CURLcode code = curl_easy_send(connection->curl, wire + sent, wireLength - sent, &count);
            if (code != CURLE_OK && code != CURLE_AGAIN) {
                error = "Connection closed unexpectedly";
                break;
            }
            sent += count;
            if (code == CURLE_AGAIN) Http_Wait(connection->socket, 1, 10);
        }
        parsedResult = Cod2x_HttpParse(received.data, received.length, closed, &parsed, &error);
        if (parsedResult < 0) break;
        if (parsedResult > 0) {
            if (request->maxBody && parsed.bodyLength > request->maxBody) {
                error = "Response too large";
                break;
            }
            response->headers = malloc(parsed.headerLength + 1);
            response->body = malloc(parsed.bodyLength + 1);
            if (!response->headers || !response->body) {
                error = "Out of memory";
                break;
            }
            memcpy(response->headers, received.data, parsed.headerLength);
            response->headers[parsed.headerLength] = '\0';
            memcpy(response->body, received.data + parsed.headerLength, parsed.bodyLength);
            response->body[parsed.bodyLength] = '\0';
            response->length = parsed.bodyLength;
            response->status = parsed.status;
            success = 1;
        } else {
            char buffer[16384];
            size_t room = request->maxReceive - received.length;
            int count;
            if (closed) break;
            if (!room) {
                error = "Response too large";
                break;
            }
            if (room > sizeof(buffer)) room = sizeof(buffer);
            count = Cod2x_SocketRecv(connection, buffer, room, 10);
            if (count > 0) {
                if (!Http_Append(&received, buffer, (size_t)count)) {
                    error = received.tooLarge ? "Response too large" : "Out of memory";
                    break;
                }
            } else if (count < 0) {
                closed = 1;
            }
        }
    }
finish:
    Cod2x_SocketClose(connection);
    free(wire);
    free(received.data);
    if (!success) {
        if (error) snprintf(response->error, sizeof(response->error), "%s", error);
        free(response->body);
        free(response->headers);
        response->body = response->headers = NULL;
        response->status = 0;
        response->length = 0;
    }
    if (!response->body) response->body = calloc(1, 1);
    if (!response->headers) response->headers = calloc(1, 1);
    return success;
}

/* ---- file downloads ------------------------------------------------------- */

typedef struct {
    FILE *file;
    CURL *curl;
    Cod2xHttpProgress *progress;
    int badStatus, writeFailed, chunked;
} HttpDownloadState;

static size_t Http_DownloadHeaders(char *data, size_t size, size_t count, void *user)
{
    HttpDownloadState *state = user;
    size_t length = size * count;
    long status = 0;
    curl_easy_getinfo(state->curl, CURLINFO_RESPONSE_CODE, &status);
    if (status == 200 && length > 18 && !strncasecmp(data, "Transfer-Encoding:", 18)) {
        const char *value = data + 18, *end = data + length;
        while (value < end && isspace((unsigned char)*value))
            ++value;
        while (end > value && isspace((unsigned char)end[-1]))
            --end;
        if (end - value == 7 && !strncasecmp(value, "chunked", 7)) {
            state->chunked = 1;
            return 0;
        }
    }
    return length;
}

static size_t Http_DownloadBody(char *data, size_t size, size_t count, void *user)
{
    HttpDownloadState *state = user;
    long status = 0;
    curl_off_t total = 0;
    size_t written;
    curl_easy_getinfo(state->curl, CURLINFO_RESPONSE_CODE, &status);
    if (status != 200) {
        state->badStatus = 1;
        return 0;
    }
    written = fwrite(data, 1, size * count, state->file);
    if (state->progress) {
        state->progress->downloaded += written;
        curl_easy_getinfo(state->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &total);
        state->progress->total = total > 0 ? (size_t)total : 0;
    }
    if (written != size * count) {
        state->writeFailed = 1;
        return 0;
    }
    return written;
}

static int Http_DownloadAbort(void *user, curl_off_t downloadTotal, curl_off_t downloaded, curl_off_t uploadTotal,
                              curl_off_t uploaded)
{
    HttpDownloadState *state = user;
    (void)downloadTotal, (void)downloaded, (void)uploadTotal, (void)uploaded;
    return state->progress && state->progress->cancel;
}

static int Http_HasMagic(const char *path, const char *magic)
{
    char start[16] = {0};
    size_t length = strlen(magic), got;
    FILE *file = fopen(path, "rb");
    if (!file)
        return 0;
    got = fread(start, 1, length < sizeof(start) ? length : sizeof(start), file);
    fclose(file);
    return got == length && !memcmp(start, magic, length);
}

int Cod2x_HttpDownload(const char *url, const char *path, const char *magic, Cod2xHttpProgress *progress,
                       char *error, size_t errorSize)
{
    HttpDownloadState state = {0};
    struct curl_slist *list = NULL;
    long status = 0;
    CURLcode code;
    size_t downloaded;
    curl_off_t received = 0;

    error[0] = '\0';
    if (!Cod2x_HttpValidUrl(url)) {
        snprintf(error, errorSize, "Invalid URL");
        return 0;
    }
    state.file = fopen(path, "wb");
    if (!state.file) {
        snprintf(error, errorSize, "Failed to open '%s' for writing", path);
        return 0;
    }
    state.progress = progress;
    if (progress) {
        progress->downloaded = 0;
        progress->total = 0;
    }
    Cod2x_HttpInit();
    state.curl = curl_easy_init();
    if (!state.curl) {
        fclose(state.file);
        snprintf(error, errorSize, "Out of memory");
        return 0;
    }
    list = curl_slist_append(list, "User-Agent: CoD2x");
    list = curl_slist_append(list, "Connection: close");
    list = curl_slist_append(list, "Accept:");
    Http_Common(state.curl);
    curl_easy_setopt(state.curl, CURLOPT_URL, url);
    curl_easy_setopt(state.curl, CURLOPT_HTTPHEADER, list);
    curl_easy_setopt(state.curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(state.curl, CURLOPT_MAXREDIRS, 5L);
    /* No overall limit: a big file takes long; 30 s without data stops it. */
    curl_easy_setopt(state.curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(state.curl, CURLOPT_LOW_SPEED_TIME, 30L);
    curl_easy_setopt(state.curl, CURLOPT_HEADERFUNCTION, Http_DownloadHeaders);
    curl_easy_setopt(state.curl, CURLOPT_HEADERDATA, &state);
    curl_easy_setopt(state.curl, CURLOPT_WRITEFUNCTION, Http_DownloadBody);
    curl_easy_setopt(state.curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(state.curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(state.curl, CURLOPT_XFERINFOFUNCTION, Http_DownloadAbort);
    curl_easy_setopt(state.curl, CURLOPT_XFERINFODATA, &state);
    code = curl_easy_perform(state.curl);
    curl_easy_getinfo(state.curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_getinfo(state.curl, CURLINFO_SIZE_DOWNLOAD_T, &received);
    downloaded = progress ? progress->downloaded : (size_t)received;

    if (fclose(state.file) != 0 && code == CURLE_OK)
        snprintf(error, errorSize, "Failed to write the file");
    if (error[0]) {
        /* already set */
    } else if (state.badStatus || (code == CURLE_OK && status != 200)) {
        snprintf(error, errorSize, "HTTP error %ld", status);
    } else if (state.chunked) {
        snprintf(error, errorSize, "Chunked download is not supported");
    } else if (state.writeFailed) {
        snprintf(error, errorSize, "Failed to write the file");
    } else if (code == CURLE_TOO_MANY_REDIRECTS) {
        snprintf(error, errorSize, "Too many redirects");
    } else if (code == CURLE_ABORTED_BY_CALLBACK) {
        snprintf(error, errorSize, "Cancelled");
    } else if (code == CURLE_OPERATION_TIMEDOUT) {
        snprintf(error, errorSize, "Timeout");
    } else if (code != CURLE_OK) {
        Cod2xHttpRequest timers = {0}; /* only the 30 s stall limit */
        Http_Error(state.curl, &timers, code, error, errorSize);
    } else if (!downloaded) {
        snprintf(error, errorSize, "Received an empty file");
    } else if (magic && !Http_HasMagic(path, magic)) {
        snprintf(error, errorSize, "Unexpected file content");
    }
    curl_slist_free_all(list);
    curl_easy_cleanup(state.curl);
    return !error[0];
}
#endif
