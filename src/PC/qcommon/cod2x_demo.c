#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_demo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void Com_Printf(const char *, ...);
extern void Com_Error(int, const char *, ...);
#define COD2X_DEMO_ERR_FATAL 0

static Cod2xDemoProgress demo_progress;

/* The macOS, Linux and Windows builds link libcurl (Windows a static one, on its
   own TLS). Elsewhere no upload URL is accepted, nothing is queued and quitting
   never waits for an upload. */
#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#define COD2X_DEMO_UPLOADS 1
#else
#define COD2X_DEMO_UPLOADS 0
#endif

#if COD2X_DEMO_UPLOADS
#if defined(_WIN32)
#define CURL_STATICLIB
#endif
#include <curl/curl.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#if defined(_WIN32)
#include <windows.h>
/* CoD2x prints and opens these paths with backslashes. */
#define COD2X_DEMO_SEPARATOR '\\'
#define COD2X_DEMO_OPEN_FLAGS (O_RDONLY | O_BINARY)
#else
#define COD2X_DEMO_SEPARATOR '/'
#define COD2X_DEMO_OPEN_FLAGS (O_RDONLY | O_NOFOLLOW)
#endif

static CURLM *demo_multi;
static CURL *demo_easy;
static FILE *demo_file;
static struct curl_slist *demo_headers;
static char demo_marker[4096], demo_path[4096], demo_response[1024];
static size_t demo_responseLength;
static uint64_t demo_started, demo_hideAt;
static int demo_curlInitialized, demo_errorCount;
/* Markers that failed three times in a row; skipped until the game restarts. */
static struct DemoFailed {
    char *marker;
    struct DemoFailed *next;
} *demo_failed;

static uint64_t Cod2x_DemoMilliseconds(void)
{
#if defined(_WIN32)
    return (uint64_t)GetTickCount64();
#else
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000 + (uint64_t)time.tv_nsec / 1000000;
#endif
}

/* A regular file that is not a link: an upload never follows one. */
static int Cod2x_DemoRegularFile(const char *path, struct stat *st)
{
#if defined(_WIN32)
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return 0;
    return !stat(path, st) && S_ISREG(st->st_mode);
#else
    return !lstat(path, st) && S_ISREG(st->st_mode);
#endif
}
#endif

int Cod2x_DemoName(char *name, size_t capacity, const char *requested, unsigned suffix)
{
    char ending[24];
    size_t i, count, endLength;
    if (!name || !requested || capacity < 2 || !*requested)
        return 0;
    ending[0] = '\0';
    if (suffix)
        snprintf(ending, sizeof(ending), "_%u", suffix);
    endLength = strlen(ending);
    if (endLength + 1 >= capacity)
        return 0;
    count = strlen(requested);
    if (count > capacity - endLength - 1)
        count = capacity - endLength - 1;
    /* CoD2x replaces the characters Windows forbids in file names (src/mss32/demo.cpp:262);
       control characters go too, so a name can never break a line or a path. */
    for (i = 0; i < count; ++i) {
        unsigned char c = (unsigned char)requested[i];
        name[i] = (c < 32 || c == 127 || strchr("\\/:*?\"<>|", c)) ? '_' : (char)c;
    }
    memcpy(name + count, ending, endLength + 1);
    return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

#if !COD2X_DEMO_UPLOADS
int Cod2x_DemoUploadAllowed(const char *url)
{
    (void)url;
    return 0;
}
#else
int Cod2x_DemoUploadAllowed(const char *url)
{
    CURLU *parsed;
    CURLUcode result;
    char *scheme = NULL, *host = NULL, *user = NULL;
    const unsigned char *p;
    int valid = 0;
    if (!url || !*url)
        return 0;
    for (p = (const unsigned char *)url; *p; ++p)
        if (*p <= 32 || *p == 127)
            return 0;
    parsed = curl_url();
    if (!parsed)
        return 0;
    result = curl_url_set(parsed, CURLUPART_URL, url, 0);
    if (!result && !curl_url_get(parsed, CURLUPART_SCHEME, &scheme, 0) &&
        !curl_url_get(parsed, CURLUPART_HOST, &host, 0) &&
        curl_url_get(parsed, CURLUPART_USER, &user, 0) == CURLUE_NO_USER)
        valid = (!strcmp(scheme, "http") || !strcmp(scheme, "https")) && *host;
    curl_free(scheme);
    curl_free(host);
    curl_free(user);
    curl_url_cleanup(parsed);
    return valid;
}
#endif

int Cod2x_DemoUploadURL(char *url, size_t capacity, const char *base, const char *name)
{
    size_t baseLength, nameLength;
    if (!url || !name || !Cod2x_DemoUploadAllowed(base))
        return 0;
    baseLength = strlen(base);
    nameLength = strlen(name);
    if (baseLength >= capacity || nameLength >= capacity - baseLength)
        return 0;
    memcpy(url, base, baseLength);
    memcpy(url + baseLength, name, nameLength + 1);
    return Cod2x_DemoUploadAllowed(url);
}

int Cod2x_DemoUploadSucceeded(long status)
{
    /* CoD2x src/mss32/demo.cpp:406: conflict means the demo already exists. */
    return status == 200 || status == 201 || status == 409;
}

#if !COD2X_DEMO_UPLOADS
int Cod2x_DemoUploadsPending(const char *directory)
{
    (void)directory;
    return 0;
}

int Cod2x_DemoUploadFrame(const char *directory, int timeoutSeconds, int check)
{
    (void)directory;
    (void)timeoutSeconds;
    (void)check;
    return 0;
}

int Cod2x_DemoUploading(void)
{
    return 0;
}

int Cod2x_DemoUploadShown(void)
{
    return 0;
}

void Cod2x_DemoUploadNewRecording(void)
{
}
#else
static int Cod2x_DemoFailedMarker(const char *marker)
{
    struct DemoFailed *failed;
    for (failed = demo_failed; failed; failed = failed->next)
        if (!strcmp(failed->marker, marker))
            return 1;
    return 0;
}

static void Cod2x_DemoRemoveMarker(void)
{
    if (remove(demo_marker) != 0)
        Com_Error(COD2X_DEMO_ERR_FATAL, "Failed to delete marker file: %s\n", demo_marker);
}

/* CoD2x src/mss32/demo.cpp:87-160: the first .upload marker that hasn't failed. */
static int Cod2x_DemoFind(const char *directoryIn, char marker[4096], char demo[4096])
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char directory[4096], path[4096], *p;
    size_t length;
    if (!directoryIn || strlen(directoryIn) >= sizeof(directory))
        return 0;
    strcpy(directory, directoryIn);
#if defined(_WIN32)
    for (p = directory; *p; ++p)
        if (*p == '/')
            *p = COD2X_DEMO_SEPARATOR;
#else
    (void)p;
#endif
    Com_Printf("Checking for demos to upload in directory: '%s'...\n", directory);
    dir = opendir(directory);
    if (!dir) {
        Com_Printf("Failed to open directory: %s\n", directory);
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        const char *extension;
        if (snprintf(path, sizeof(path), "%s%c%s", directory, COD2X_DEMO_SEPARATOR, entry->d_name) >=
            (int)sizeof(path)) {
            Com_Printf("Path too long, skipping: %s%c%s\n", directory, COD2X_DEMO_SEPARATOR, entry->d_name);
            continue;
        }
        /* A marker that is a link is never followed. */
        if (!Cod2x_DemoRegularFile(path, &st))
            continue;
        extension = strrchr(entry->d_name, '.');
        if (!extension || strcmp(extension, ".upload") || Cod2x_DemoFailedMarker(path))
            continue;
        Com_Printf("Found demo upload marker file: '%s'\n", path);
        length = strlen(path);
        memcpy(marker, path, length + 1);
        memcpy(demo, path, length - 7);
        demo[length - 7] = '\0';
        Com_Printf("Demo file for upload: '%s'\n", demo);
        closedir(dir);
        return 1;
    }
    closedir(dir);
    Com_Printf("No demo upload marker files found.\n");
    return 0;
}

int Cod2x_DemoUploadsPending(const char *directory)
{
    char marker[4096], demo[4096];
    return Cod2x_DemoFind(directory, marker, demo);
}

static size_t Cod2x_DemoRead(char *buffer, size_t size, size_t count, void *context)
{
    FILE *file = context;
    size_t read = fread(buffer, size, count, file);
    return ferror(file) ? CURL_READFUNC_ABORT : read * size;
}

static size_t Cod2x_DemoResponse(char *buffer, size_t size, size_t count, void *context)
{
    size_t length = size * count, room = sizeof(demo_response) - 1 - demo_responseLength;
    (void)context;
    if (length < room)
        room = length;
    memcpy(demo_response + demo_responseLength, buffer, room);
    demo_responseLength += room;
    demo_response[demo_responseLength] = '\0';
    return length;
}

static int Cod2x_DemoProgress(void *context, curl_off_t downloadTotal, curl_off_t downloaded,
                              curl_off_t uploadTotal, curl_off_t uploaded)
{
    uint64_t elapsed = Cod2x_DemoMilliseconds() - demo_started;
    (void)context;
    (void)downloadTotal;
    (void)downloaded;
    (void)uploadTotal;
    demo_progress.uploaded = uploaded > 0 ? (uint64_t)uploaded : 0;
    if (demo_progress.uploaded > demo_progress.total)
        demo_progress.uploaded = demo_progress.total;
    demo_progress.bytesPerSecond = elapsed ? demo_progress.uploaded * 1000 / elapsed : 0;
    return 0;
}

static void Cod2x_DemoRelease(void)
{
    if (demo_easy) {
        curl_multi_remove_handle(demo_multi, demo_easy);
        curl_easy_cleanup(demo_easy);
        demo_easy = NULL;
    }
    curl_slist_free_all(demo_headers);
    demo_headers = NULL;
    if (demo_file) {
        fclose(demo_file);
        demo_file = NULL;
    }
}

static void Cod2x_DemoFailure(void)
{
    /* CoD2x counts failures in a row and gives a marker up after three. */
    demo_progress.state = COD2X_DEMO_UPLOAD_FAILED;
    demo_hideAt = Cod2x_DemoMilliseconds() + 3000;
    demo_progress.attempts = ++demo_errorCount;
    if (demo_errorCount >= 3) {
        struct DemoFailed *failed = calloc(1, sizeof(*failed));
        demo_errorCount = 0;
        if (failed && (failed->marker = strdup(demo_marker)) != NULL) {
            failed->next = demo_failed;
            demo_failed = failed;
        } else {
            free(failed);
        }
    }
}

static void Cod2x_DemoFinish(CURLcode result, long status)
{
    demo_progress.httpStatus = status;
    if (result != CURLE_OK) {
        const char *error = curl_easy_strerror(result);
        Com_Printf("^1Demo upload error: %s\n", error);
        snprintf(demo_progress.error, sizeof(demo_progress.error), "Error: %s", error);
        Cod2x_DemoFailure();
    } else {
        Com_Printf("Demo upload completed! Status: %d\n", (int)status);
        if (Cod2x_DemoUploadSucceeded(status)) {
            Com_Printf("^2Demo upload successful with status: %d!\n", (int)status);
            Com_Printf("Deleting marker file: %s\n", demo_marker);
            Cod2x_DemoRemoveMarker();
            if (access(demo_marker, F_OK) != -1)
                Com_Error(COD2X_DEMO_ERR_FATAL, "Marker file still exists after deletion: %s\n", demo_marker);
            demo_progress.state = COD2X_DEMO_UPLOAD_DONE;
            demo_progress.uploaded = demo_progress.total;
            demo_hideAt = Cod2x_DemoMilliseconds() + 2000;
        } else {
            Com_Printf("^1Demo upload failed with status: %d. Response: %s\n", (int)status, demo_response);
            snprintf(demo_progress.error, sizeof(demo_progress.error), "Error: %d", (int)status);
            Cod2x_DemoFailure();
        }
    }
    Cod2x_DemoRelease();
}

static void Cod2x_DemoStart(const char *directory, int timeoutSeconds)
{
    char url[1024];
    struct stat st;
    FILE *marker;
    int fd;
    if (!Cod2x_DemoFind(directory, demo_marker, demo_path))
        return;
    /* Only a regular file is uploaded; a link counts as not readable. */
    if (access(demo_path, R_OK) != 0 || !Cod2x_DemoRegularFile(demo_path, &st)) {
        Com_Printf("Demo file not found or not readable, removing marker file: '%s'\n", demo_marker);
        Cod2x_DemoRemoveMarker();
        return;
    }
    fd = open(demo_path, COD2X_DEMO_OPEN_FLAGS);
    demo_file = fd >= 0 ? fdopen(fd, "rb") : NULL;
    if (!demo_file) {
        if (fd >= 0)
            close(fd);
        Com_Error(COD2X_DEMO_ERR_FATAL, "Failed to open demo file: %s\n", demo_path);
        return;
    }
    if (fstat(fileno(demo_file), &st) || st.st_size <= 0) {
        Com_Printf("Demo file is empty or invalid, deleting marker file: %s\n", demo_marker);
        Cod2x_DemoRelease();
        Cod2x_DemoRemoveMarker();
        return;
    }
    marker = fopen(demo_marker, "r");
    if (!marker) {
        Cod2x_DemoRelease();
        Com_Error(COD2X_DEMO_ERR_FATAL, "Failed to open marker file for reading: %s\n", demo_marker);
        return;
    }
    if (!fgets(url, sizeof(url), marker)) {
        fclose(marker);
        Cod2x_DemoRelease();
        Com_Error(COD2X_DEMO_ERR_FATAL, "Failed to read URL from marker file: %s\n", demo_marker);
        return;
    }
    fclose(marker);
    url[strcspn(url, "\r\n")] = '\0';
    /* http and https as in CoD2x; unlike CoD2x, an https server's certificate and host
       name are checked, and a URL with a user name is refused. */
    if (!Cod2x_DemoUploadAllowed(url)) {
        Com_Printf("Invalid upload URL in marker file, removing marker file: '%s'\n", demo_marker);
        Cod2x_DemoRemoveMarker();
        Cod2x_DemoRelease();
        return;
    }
    memset(&demo_progress, 0, sizeof(demo_progress));
    snprintf(demo_progress.name, sizeof(demo_progress.name), "%s", strrchr(demo_path, COD2X_DEMO_SEPARATOR) + 1);
    demo_progress.total = (uint64_t)st.st_size;
    demo_progress.state = COD2X_DEMO_UPLOAD_ACTIVE;
    demo_progress.attempts = demo_errorCount;
    demo_response[0] = '\0';
    demo_responseLength = 0;
    demo_started = Cod2x_DemoMilliseconds();
    Com_Printf("Starting upload of demo '%s' to URL: '%s'\n", demo_progress.name, url);
    if (!demo_curlInitialized) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            Cod2x_DemoFinish(CURLE_FAILED_INIT, 0);
            return;
        }
        demo_curlInitialized = 1;
    }
    if (!demo_multi)
        demo_multi = curl_multi_init();
    demo_easy = curl_easy_init();
    if (!demo_multi || !demo_easy) {
        Cod2x_DemoFinish(CURLE_FAILED_INIT, 0);
        return;
    }
    demo_headers = curl_slist_append(NULL, "Content-Type: application/octet-stream");
    demo_headers = curl_slist_append(demo_headers, "Transfer-Encoding: chunked");
    curl_easy_setopt(demo_easy, CURLOPT_URL, url);
    curl_easy_setopt(demo_easy, CURLOPT_POST, 1L);
    curl_easy_setopt(demo_easy, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(demo_easy, CURLOPT_READFUNCTION, Cod2x_DemoRead);
    curl_easy_setopt(demo_easy, CURLOPT_READDATA, demo_file);
    curl_easy_setopt(demo_easy, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)st.st_size);
    curl_easy_setopt(demo_easy, CURLOPT_HTTPHEADER, demo_headers);
    curl_easy_setopt(demo_easy, CURLOPT_WRITEFUNCTION, Cod2x_DemoResponse);
    curl_easy_setopt(demo_easy, CURLOPT_XFERINFOFUNCTION, Cod2x_DemoProgress);
    curl_easy_setopt(demo_easy, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(demo_easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(demo_easy, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(demo_easy, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(demo_easy, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(demo_easy, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(demo_easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(demo_easy, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(demo_easy, CURLOPT_TIMEOUT, (long)timeoutSeconds);
    curl_easy_setopt(demo_easy, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t)(5 * 1024 * 1024));
    if (curl_multi_add_handle(demo_multi, demo_easy) != CURLM_OK)
        Cod2x_DemoFinish(CURLE_FAILED_INIT, 0);
}

int Cod2x_DemoUploadFrame(const char *directory, int timeoutSeconds, int check)
{
    CURLMsg *message;
    int running, messages;
    if (check && !demo_easy)
        Cod2x_DemoStart(directory, timeoutSeconds);
    if (!demo_easy)
        return 0;
    if (curl_multi_perform(demo_multi, &running) != CURLM_OK) {
        Cod2x_DemoFinish(CURLE_RECV_ERROR, 0);
        return 1;
    }
    while ((message = curl_multi_info_read(demo_multi, &messages)) != NULL) {
        if (message->msg == CURLMSG_DONE && message->easy_handle == demo_easy) {
            long status = 0;
            curl_easy_getinfo(demo_easy, CURLINFO_RESPONSE_CODE, &status);
            Cod2x_DemoFinish(message->data.result, status);
            return 1;
        }
    }
    return 0;
}

int Cod2x_DemoUploading(void)
{
    return demo_easy != NULL;
}

int Cod2x_DemoUploadShown(void)
{
    return demo_easy != NULL || Cod2x_DemoMilliseconds() < demo_hideAt;
}

void Cod2x_DemoUploadNewRecording(void)
{
    demo_errorCount = 0;
}
#endif

const Cod2xDemoProgress *Cod2x_DemoUploadProgress(void)
{
    return &demo_progress;
}

#if !COD2X_DEMO_UPLOADS
void Cod2x_DemoUploadShutdown(void)
{
}
#else
void Cod2x_DemoUploadShutdown(void)
{
    struct DemoFailed *failed;
    if (demo_easy)
        Cod2x_DemoFinish(CURLE_ABORTED_BY_CALLBACK, 0);
    if (demo_multi) {
        curl_multi_cleanup(demo_multi);
        demo_multi = NULL;
    }
    if (demo_curlInitialized) {
        curl_global_cleanup();
        demo_curlInitialized = 0;
    }
    demo_hideAt = 0;
    demo_errorCount = 0;
    while ((failed = demo_failed) != NULL) {
        demo_failed = failed->next;
        free(failed->marker);
        free(failed);
    }
}
#endif
#endif
