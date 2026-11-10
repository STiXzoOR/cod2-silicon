/* CoD2x's error and crash reports (CoD2x shared/error.cpp, shared/exception.cpp,
   shared/logger.cpp), split by where the fault is. Only a fault in CoD2x-layer code,
   the units linked between Cod2x_LayerStart and Cod2x_LayerStop
   (cmake/cod2x-layer.cmake), is reported to CoD2x, in the form its service
   receives. Every other fault goes to the report receiver of crash_handler.h and
   is never sent to CoD2x. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x.h"
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#if defined(_WIN32)
#define CURL_STATICLIB
#endif
#include <curl/curl.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <execinfo.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#endif

#define COD2X_ERROR_URL "https://master.cod2x.me/api/telemetry/cod2x-errors"
#define COD2X_EXCEPTION_URL "https://master.cod2x.me/api/telemetry/cod2x-exceptions"
/* An error report repeats only after a day, unless it was not delivered. */
#define COD2X_ERROR_REPEAT_SECONDS (24 * 60 * 60)
#define COD2X_FATAL_WAIT_MS 3000
#define COD2X_PENDING_SUFFIX ".pending.json"
#define COD2X_PENDING_MAX_AGE_DAYS 14
#define COD2X_PENDING_MAX_SIZE (8 * 1024 * 1024)
#define COD2X_RESEND_MAX_REPORTS 10
#define COD2X_RESEND_WAIT_MS 15000
#define COD2X_LOG_CAPACITY 50
#define COD2X_LOG_LENGTH 512

extern void Cod2x_LayerStart(void);
extern void Cod2x_LayerStop(void);
extern void Sys_InstallCrashHandler(const char *appName, const char *version, const char *buildDate,
                                    const char *cmdline);
/* crash_handler.c */
extern const char *Sys_CrashDirectory(void);
extern const char *Sys_Cod2xReportType(void);
extern size_t Sys_Cod2xSystemInfo(char *out, size_t size);
extern size_t Sys_Cod2xIdentity(char *out, size_t size);
extern size_t Sys_Cod2xLogLines(char *out, size_t size);
extern void Sys_CrashReceive(const char *path, const char *reason);

/* 1 when address is code of the CoD2x layer. If the build did not lay the layer
   out as one run (a CoD2x function outside it, or engine code inside), nothing
   counts as CoD2x's, so nothing is sent to CoD2x by mistake. */
int Cod2x_InLayer(const void *address)
{
    uintptr_t start = (uintptr_t)&Cod2x_LayerStart, stop = (uintptr_t)&Cod2x_LayerStop;
    uintptr_t value = (uintptr_t)address, layer = (uintptr_t)&Cod2x_InLayer,
              engine = (uintptr_t)&Sys_InstallCrashHandler;
    if (start >= stop || layer < start || layer >= stop || (engine >= start && engine < stop))
        return 0;
    return value >= start && value < stop;
}

/* ---- The CoD2x logger: the last lines, carried in every report ---- */
static struct {
    size_t head, count;
    time_t times[COD2X_LOG_CAPACITY];
    char lines[COD2X_LOG_CAPACITY][COD2X_LOG_LENGTH + 1];
} cod2xLog;
static volatile int cod2xLogLock;

/* A crash handler gives up after a few tries rather than wait on a lock. */
static int Cod2x_LogLock(int tries)
{
    while (__sync_lock_test_and_set(&cod2xLogLock, 1)) {
        if (tries >= 0 && tries-- == 0)
            return 0;
#if defined(_WIN32)
        Sleep(0);
#else
        sched_yield();
#endif
    }
    return 1;
}

void Cod2x_Log(const char *format, ...)
{
    char line[COD2X_LOG_LENGTH + 1];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    Cod2x_LogLock(-1);
    cod2xLog.times[cod2xLog.head] = time(NULL);
    memcpy(cod2xLog.lines[cod2xLog.head], line, sizeof(line));
    cod2xLog.head = (cod2xLog.head + 1) % COD2X_LOG_CAPACITY;
    if (cod2xLog.count < COD2X_LOG_CAPACITY)
        ++cod2xLog.count;
    __sync_lock_release(&cod2xLogLock);
}

/* Newest first, each line with its age against the newest: "<seconds>s: text". */
size_t Cod2x_LoggerRecent(char *out, size_t size)
{
    size_t used = 0, n, index;
    time_t newest;
    if (!size)
        return 0;
    out[0] = '\0';
    if (!Cod2x_LogLock(1000))
        return 0;
    index = (cod2xLog.head + COD2X_LOG_CAPACITY - 1) % COD2X_LOG_CAPACITY;
    newest = cod2xLog.times[index];
    for (n = 0; n < cod2xLog.count; ++n) {
        int written = snprintf(out + used, size - used, "%lds: %s\n", (long)(newest - cod2xLog.times[index]),
                               cod2xLog.lines[index]);
        if (written < 0 || (size_t)written >= size - used)
            break;
        used += (size_t)written;
        index = (index + COD2X_LOG_CAPACITY - 1) % COD2X_LOG_CAPACITY;
    }
    __sync_lock_release(&cod2xLogLock);
    return used;
}

/* ---- JSON text ---- */
typedef struct {
    char *text;
    size_t length, size;
} Cod2xText;

static void Cod2x_TextAdd(Cod2xText *text, const char *format, ...)
{
    va_list args;
    int needed;
    va_start(args, format);
    needed = vsnprintf(NULL, 0, format, args);
    va_end(args);
    if (needed < 0)
        return;
    if (text->length + (size_t)needed + 1 > text->size) {
        size_t size = (text->size ? text->size : 4096);
        char *grown;
        while (text->length + (size_t)needed + 1 > size)
            size *= 2;
        grown = (char *)realloc(text->text, size);
        if (!grown)
            return;
        text->text = grown;
        text->size = size;
    }
    va_start(args, format);
    vsnprintf(text->text + text->length, text->size - text->length, format, args);
    va_end(args);
    text->length += (size_t)needed;
}

static void Cod2x_TextString(Cod2xText *text, const char *value)
{
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    Cod2x_TextAdd(text, "\"");
    for (; *p; ++p) {
        if (*p == '"' || *p == '\\')
            Cod2x_TextAdd(text, "\\%c", *p);
        else if (*p == '\n')
            Cod2x_TextAdd(text, "\\n");
        else if (*p == '\r')
            Cod2x_TextAdd(text, "\\r");
        else if (*p == '\t')
            Cod2x_TextAdd(text, "\\t");
        else if (*p < 0x20)
            Cod2x_TextAdd(text, "\\u%04x", *p);
        else
            Cod2x_TextAdd(text, "%c", *p);
    }
    Cod2x_TextAdd(text, "\"");
}

/* "field":["line",...], from text split at line ends; empty lines are left out. */
static void Cod2x_TextLines(Cod2xText *text, const char *field, const char *lines)
{
    const char *p = lines ? lines : "";
    int first = 1;
    Cod2x_TextAdd(text, "\"%s\":[", field);
    while (*p) {
        const char *start = p;
        char line[1024];
        size_t length;
        while (*p && *p != '\n' && *p != '\r')
            ++p;
        length = (size_t)(p - start);
        if (length) {
            if (length >= sizeof(line))
                length = sizeof(line) - 1;
            memcpy(line, start, length);
            line[length] = '\0';
            Cod2x_TextAdd(text, first ? "" : ",");
            Cod2x_TextString(text, line);
            first = 0;
        }
        while (*p == '\n' || *p == '\r')
            ++p;
    }
    Cod2x_TextAdd(text, "]");
}

/* A top-level string value of a JSON reply ("error" or "message"). */
static int Cod2x_ReplyString(const char *body, const char *key, char *out, size_t size)
{
    char pattern[32];
    const char *p;
    size_t used = 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (!(p = strstr(body, pattern)))
        return 0;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        ++p;
    if (*p++ != ':')
        return 0;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        ++p;
    if (*p++ != '"')
        return 0;
    while (*p && *p != '"' && used + 1 < size) {
        if (*p == '\\' && p[1])
            ++p;
        out[used++] = *p++;
    }
    out[used] = '\0';
    return used > 0;
}

/* ---- Sending ---- */
typedef struct {
    char *json;
    const char *url;
    char pending[1100];
    int exception, timeout;
    volatile int finished;
    volatile int *delivered;
    Cod2xPostDone done;
    void *user;
    char reply[16384];
    size_t replyLength;
} Cod2xPost;

static size_t Cod2x_ReportReply(char *data, size_t size, size_t count, void *user)
{
    Cod2xPost *post = (Cod2xPost *)user;
    size_t length = size * count, room = sizeof(post->reply) - 1 - post->replyLength;
    memcpy(post->reply + post->replyLength, data, length < room ? length : room);
    post->replyLength += length < room ? length : room;
    post->reply[post->replyLength] = '\0';
    return length;
}

/* POSTs JSON as CoD2x's client does (HttpClient::postJson): HTTP/1.1 with Host,
   Content-Type: application/json and Content-Length, nothing else. Returns the
   curl result; status and reply are the server's. */
static CURLcode Cod2x_PostSend(Cod2xPost *post, const char *target, long *status)
{
    char address[512];
    const char *url = Cod2x_ServiceURL(target, address, sizeof(address));
    struct curl_slist *headers = NULL;
    CURL *curl = curl_easy_init();
    CURLcode result;
    *status = 0;
    if (!curl)
        return CURLE_FAILED_INIT;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept:");
    headers = curl_slist_append(headers, "Expect:");
    curl_easy_setopt(curl, CURLOPT_URL, url);
    /* Only https leaves this machine; a test stand-in may be plain http. */
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, url == address ? "http,https" : "https");
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post->json);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post->json));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)post->timeout);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Cod2x_ReportReply);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, post);
    result = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

static void Cod2x_ReportSend(Cod2xPost *post)
{
    long status = 0;
    CURLcode result = Cod2x_PostSend(post, post->exception ? COD2X_EXCEPTION_URL : COD2X_ERROR_URL, &status);
    if (result != CURLE_OK) {
        Cod2x_Log(post->exception ? "[EXCEPTION] Failed to send report: %s" : "[ERROR] Failed to send report: %s",
                  curl_easy_strerror(result));
        return;
    }
    if (!post->exception) {
        if (status >= 200 && status < 300) {
            Cod2x_Log("[ERROR] Report sent successfully (HTTP %d)", (int)status);
            if (post->delivered)
                *post->delivered = 1;
        } else {
            Cod2x_Log("[ERROR] Report rejected by endpoint (HTTP %d)", (int)status);
        }
        return;
    }
    /* Delivered, or refused for good (4xx other than 408 and 429): the pending
       file goes. */
    if ((status >= 200 && status < 300) || (status >= 400 && status < 500 && status != 408 && status != 429))
        remove(post->pending);
    if (status >= 200 && status < 300) {
        Cod2x_Log("[EXCEPTION] Report sent successfully (HTTP %d)", (int)status);
    } else {
        char message[128] = "", error[160];
        if (post->reply[0] == '{' && !Cod2x_ReplyString(post->reply, "error", message, sizeof(message)))
            Cod2x_ReplyString(post->reply, "message", message, sizeof(message));
        snprintf(error, sizeof(error), "HTTP %d%s%s", (int)status, message[0] ? ": " : "", message);
        Cod2x_Log("[EXCEPTION] Report rejected by endpoint: %s", error);
    }
}

#if defined(_WIN32)
static DWORD WINAPI Cod2x_ReportThread(LPVOID data)
#else
static void *Cod2x_ReportThread(void *data)
#endif
{
    Cod2xPost *post = (Cod2xPost *)data;
    if (post->done) {
        long status = 0;
        CURLcode result = Cod2x_PostSend(post, post->url, &status);
        post->done(result == CURLE_OK, status, post->reply, result == CURLE_OK ? "" : curl_easy_strerror(result),
                   post->user);
    } else {
        Cod2x_ReportSend(post);
    }
    free(post->json);
    post->json = NULL;
    /* The waiting side may have gone; whoever comes second frees the post. */
    if (__sync_lock_test_and_set(&post->finished, 1))
        free(post);
#if defined(_WIN32)
    return 0;
#else
    return NULL;
#endif
}

static void Cod2x_Sleep(int milliseconds)
{
#if defined(_WIN32)
    Sleep((DWORD)milliseconds);
#else
    usleep((useconds_t)milliseconds * 1000);
#endif
}

/* Sends on its own thread and waits at most wait ms (0: does not wait). */
static void Cod2x_PostStart(char *json, int timeout, int wait, volatile int *delivered, const char *url,
                            Cod2xPostDone done, void *user)
{
    Cod2xPost *post = (Cod2xPost *)calloc(1, sizeof(*post));
    int waited = 0;
#if !defined(_WIN32)
    pthread_t thread;
    pthread_attr_t attributes;
#endif
    if (!post || !json) {
        free(post);
        free(json);
        return;
    }
    post->json = json;
    post->timeout = timeout;
    post->delivered = delivered;
    post->url = url;
    post->done = done;
    post->user = user;
#if defined(_WIN32)
    {
        HANDLE thread = CreateThread(NULL, 0, Cod2x_ReportThread, post, 0, NULL);
        if (!thread) {
            free(json);
            free(post);
            return;
        }
        CloseHandle(thread);
    }
#else
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&thread, &attributes, Cod2x_ReportThread, post) != 0) {
        pthread_attr_destroy(&attributes);
        free(json);
        free(post);
        return;
    }
    pthread_attr_destroy(&attributes);
#endif
    while (waited < wait && !post->finished) {
        Cod2x_Sleep(10);
        waited += 10;
    }
    if (__sync_lock_test_and_set(&post->finished, 1))
        free(post);
}

static void Cod2x_ReportPost(char *json, int timeout, int wait, volatile int *delivered)
{
    Cod2x_PostStart(json, timeout, wait, delivered, NULL, NULL, NULL);
}

void Cod2x_PostJson(const char *url, const char *json, int timeout, Cod2xPostDone done, void *user)
{
    char *copy = json ? strdup(json) : NULL;
    Cod2x_PostStart(copy, timeout, 0, NULL, url, done, user);
}

/* ---- Error reports (cod2x-errors) ---- */

/* The calling thread's stack, one "  N  0xADDRESS  module+0xOFFSET (symbol)" a line. */
static void Cod2x_CallerStack(char *out, size_t size, int skip)
{
    void *frames[64];
    size_t used = 0;
    int count, i, number = 0;
#if defined(_WIN32)
    count = (int)RtlCaptureStackBackTrace(0, 64, frames, NULL);
#else
    count = backtrace(frames, 64);
#endif
    out[0] = '\0';
    for (i = skip; i < count; ++i) {
        char module[260] = "<unknown>", symbol[256] = "<no symbol>";
        uintptr_t address = (uintptr_t)frames[i], offset = address;
        int written;
#if defined(_WIN32)
        HMODULE handle = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)frames[i], &handle) &&
            handle && GetModuleFileNameA(handle, module, sizeof(module))) {
            const char *name = strrchr(module, '\\');
            if (name)
                memmove(module, name + 1, strlen(name));
            offset = address - (uintptr_t)handle;
        }
#else
        Dl_info info;
        if (dladdr(frames[i], &info) && info.dli_fname) {
            const char *name = strrchr(info.dli_fname, '/');
            snprintf(module, sizeof(module), "%s", name ? name + 1 : info.dli_fname);
            offset = address - (uintptr_t)info.dli_fbase;
            if (info.dli_sname)
                snprintf(symbol, sizeof(symbol), "%s+0x%lx", info.dli_sname,
                         (unsigned long)(address - (uintptr_t)info.dli_saddr));
        }
#endif
        written = snprintf(out + used, size - used, "  %d  0x%08lx  %s+0x%lx (%s)\n", number++, (unsigned long)address,
                           module, (unsigned long)offset, symbol);
        if (written < 0 || (size_t)written >= size - used)
            break;
        used += (size_t)written;
    }
}

static char *Cod2x_ErrorJson(const char *kind, const char *message, const char *stack)
{
    static char logger[COD2X_LOG_CAPACITY * (COD2X_LOG_LENGTH + 32)];
    char system[4096], identity[512];
    static char logs[64 * 1024];
    Cod2xText text = {NULL, 0, 0};
    Cod2x_LoggerRecent(logger, sizeof(logger));
    Sys_Cod2xSystemInfo(system, sizeof(system));
    Sys_Cod2xIdentity(identity, sizeof(identity));
    Sys_Cod2xLogLines(logs, sizeof(logs));
    Cod2x_TextAdd(&text, "{\"version\":");
    Cod2x_TextString(&text, COD2X_RELEASE);
    Cod2x_TextAdd(&text, ",\"type\":\"%s\",%s\"errorInfo\":{\"kind\":\"%s\",\"message\":", Sys_Cod2xReportType(),
                  identity, kind);
    Cod2x_TextString(&text, message);
    Cod2x_TextAdd(&text, ",");
    Cod2x_TextLines(&text, "stack", stack);
    Cod2x_TextAdd(&text, "},\"systemInfo\":%s,", system);
    Cod2x_TextLines(&text, "loggerLines", logger);
    Cod2x_TextAdd(&text, ",");
    Cod2x_TextLines(&text, "logLines", logs);
    Cod2x_TextAdd(&text, "}");
    return text.text;
}

/* A fatal error (Sys_Error). site: where the error was raised. CoD2x waits up to
   3 seconds for the report before the game goes. */
void Cod2x_ReportFatal(const char *message, const void *site)
{
    char stack[8192];
    if (!Cod2x_InLayer(site)) {
        Sys_CrashReceive(NULL, message);
        return;
    }
    Cod2x_CallerStack(stack, sizeof(stack), 2);
    Cod2x_ReportPost(Cod2x_ErrorJson("fatal", message, stack), 5000, COD2X_FATAL_WAIT_MS, NULL);
}

/* An error CoD2x-layer code reports and the game goes on ("cod2x-update",
   "zpam-update"): the same message again only after a day, unless the earlier
   one was not delivered. */
void Cod2x_ReportError(const char *kind, const char *message)
{
    static struct {
        char message[256];
        time_t sent;
        volatile int delivered;
    } reports[32];
    static int next;
    time_t now = time(NULL);
    int i;
    for (i = 0; i < 32; ++i) {
        if (reports[i].sent && !strncmp(reports[i].message, message, sizeof(reports[i].message) - 1)) {
            if (reports[i].delivered && now - reports[i].sent < COD2X_ERROR_REPEAT_SECONDS)
                return;
            break;
        }
    }
    if (i == 32)
        i = next++ % 32;
    snprintf(reports[i].message, sizeof(reports[i].message), "%s", message);
    reports[i].sent = now;
    reports[i].delivered = 0;
    Cod2x_ReportPost(Cod2x_ErrorJson(kind, message, ""), 5000, 0, &reports[i].delivered);
}

/* ---- Crash reports left by the crash handler (cod2x-exceptions) ---- */
static int Cod2x_NameOrder(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Sends the reports a crash left behind, the oldest first, at most ten a start. */
static void Cod2x_ResendPending(const char *directory, const char *prefix, int markResent)
{
    char *names[256];
    int count = 0, sent = 0, i;
    size_t prefixLength = strlen(prefix), suffixLength = strlen(COD2X_PENDING_SUFFIX);
    time_t now = time(NULL);
    struct dirent *entry;
    DIR *folder = opendir(directory);
    if (!folder)
        return;
    while ((entry = readdir(folder)) != NULL && count < 256) {
        size_t length = strlen(entry->d_name);
        if (length >= prefixLength + suffixLength && !strncmp(entry->d_name, prefix, prefixLength) &&
            !strcmp(entry->d_name + length - suffixLength, COD2X_PENDING_SUFFIX))
            names[count++] = strdup(entry->d_name);
    }
    closedir(folder);
    qsort(names, (size_t)count, sizeof(names[0]), Cod2x_NameOrder);
    for (i = 0; i < count; ++i) {
        char path[1100];
        struct stat info;
        FILE *file;
        char *json;
        size_t extra = markResent ? strlen("\"resent\":true,") : 0;
        if (!names[i] || sent >= COD2X_RESEND_MAX_REPORTS) {
            free(names[i]);
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", directory, names[i]);
        if (stat(path, &info) != 0) {
            free(names[i]);
            continue;
        }
        if (now > info.st_mtime && now - info.st_mtime > COD2X_PENDING_MAX_AGE_DAYS * 86400) {
            remove(path);
            free(names[i]);
            continue;
        }
        json = info.st_size > 0 && info.st_size <= COD2X_PENDING_MAX_SIZE
                   ? (char *)malloc((size_t)info.st_size + extra + 1)
                   : NULL;
        file = json ? fopen(path, "rb") : NULL;
        if (!file || fread(json + extra, 1, (size_t)info.st_size, file) != (size_t)info.st_size ||
            json[extra] != '{') {
            if (file)
                fclose(file);
            free(json);
            remove(path);
            free(names[i]);
            continue;
        }
        fclose(file);
        json[extra + (size_t)info.st_size] = '\0';
        if (markResent) {
            json[0] = '{';
            memcpy(json + 1, "\"resent\":true,", extra);
        }
        Cod2x_Log("[EXCEPTION] Sending pending report %s", names[i]);
        {
            Cod2xPost post;
            memset(&post, 0, sizeof(post));
            post.json = json;
            post.exception = 1;
            post.timeout = COD2X_RESEND_WAIT_MS;
            snprintf(post.pending, sizeof(post.pending), "%s", path);
            Cod2x_ReportSend(&post);
        }
        free(json);
        free(names[i]);
        ++sent;
    }
}

#if defined(_WIN32)
static DWORD WINAPI Cod2x_ResendThread(LPVOID unused)
#else
static void *Cod2x_ResendThread(void *unused)
#endif
{
    (void)unused;
    /* Not while the game is still starting. */
    Cod2x_Sleep(10000);
#if defined(_WIN32)
    {
        char exe[MAX_PATH] = "", prefix[MAX_PATH + 1];
        const char *name;
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        name = strrchr(exe, '\\');
        snprintf(prefix, sizeof(prefix), "%s.", name ? name + 1 : exe);
        Cod2x_ResendPending(Sys_CrashDirectory(), prefix, 1);
    }
    return 0;
#else
    Cod2x_ResendPending(Sys_CrashDirectory(), "cod2x_crash.", 0);
    Cod2x_ResendPending(Sys_CrashDirectory(), "cod2x_freeze.", 0);
    return NULL;
#endif
}

void Cod2x_ReportInit(void)
{
    static int started;
#if !defined(_WIN32)
    pthread_t thread;
#endif
    if (started)
        return;
    started = 1;
    curl_global_init(CURL_GLOBAL_DEFAULT);
#if defined(_WIN32)
    {
        HANDLE thread = CreateThread(NULL, 0, Cod2x_ResendThread, NULL, 0, NULL);
        if (thread)
            CloseHandle(thread);
    }
#else
    if (pthread_create(&thread, NULL, Cod2x_ResendThread, NULL) == 0)
        pthread_detach(thread);
#endif
}
#endif
