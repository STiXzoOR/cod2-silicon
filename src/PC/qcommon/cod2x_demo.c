#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_demo.h"
#include <curl/curl.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static CURLM *demo_multi;
static CURL *demo_easy;
static FILE *demo_file;
static struct curl_slist *demo_headers;
static char demo_marker[4096];
static Cod2xDemoProgress demo_progress;
static uint64_t demo_started, demo_nextScan;
static int demo_curlInitialized;
static struct DemoDirectory {
    char *path;
    struct DemoDirectory *next;
} *demo_directories;
static struct DemoAttempt {
    char *marker;
    unsigned attempts;
    struct DemoAttempt *next;
} *demo_attempts;

static uint64_t Cod2x_DemoMilliseconds(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000 + (uint64_t)time.tv_nsec / 1000000;
}

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
    for (i = 0; i < count; ++i) {
        unsigned char c = (unsigned char)requested[i];
        name[i] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') ? (char)c : '_';
    }
    memcpy(name + count, ending, endLength + 1);
    return strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
}

int Cod2x_DemoHTTPS(const char *url)
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
        valid = !strcmp(scheme, "https") && *host;
    curl_free(scheme);
    curl_free(host);
    curl_free(user);
    curl_url_cleanup(parsed);
    return valid;
}

int Cod2x_DemoUploadURL(char *url, size_t capacity, const char *base, const char *name)
{
    size_t baseLength, nameLength;
    if (!url || !name || !Cod2x_DemoHTTPS(base))
        return 0;
    baseLength = strlen(base);
    nameLength = strlen(name);
    if (baseLength >= capacity || nameLength >= capacity - baseLength)
        return 0;
    memcpy(url, base, baseLength);
    memcpy(url + baseLength, name, nameLength + 1);
    return Cod2x_DemoHTTPS(url);
}

int Cod2x_DemoUploadSucceeded(long status)
{
    /* CoD2x src/mss32/demo.cpp:406: conflict means the demo already exists. */
    return status == 200 || status == 201 || status == 409;
}

static struct DemoAttempt *Cod2x_DemoAttemptFor(const char *marker)
{
    struct DemoAttempt *attempt;
    for (attempt = demo_attempts; attempt; attempt = attempt->next)
        if (!strcmp(attempt->marker, marker))
            return attempt;
    attempt = calloc(1, sizeof(*attempt));
    if (!attempt)
        return NULL;
    attempt->marker = strdup(marker);
    if (!attempt->marker) {
        free(attempt);
        return NULL;
    }
    attempt->next = demo_attempts;
    demo_attempts = attempt;
    return attempt;
}

static int Cod2x_DemoMarkerInDirectory(const char *directory, char marker[4096], char file[4096])
{
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    size_t length;
    int result = 0;
    if (!directory || !(dir = opendir(directory)))
        return 0;
    while ((entry = readdir(dir)) != NULL) {
        struct DemoAttempt *attempt;
        length = strlen(entry->d_name);
        if (length <= 12 || strcmp(entry->d_name + length - 12, ".dm_1.upload"))
            continue;
        if (snprintf(marker, 4096, "%s/%s", directory, entry->d_name) >= 4096)
            continue;
        if (lstat(marker, &st) || !S_ISREG(st.st_mode))
            continue;
        attempt = Cod2x_DemoAttemptFor(marker);
        if (!attempt || attempt->attempts >= 3)
            continue;
        memcpy(file, marker, strlen(marker) - 7);
        file[strlen(marker) - 7] = '\0';
        result = 1;
        break;
    }
    closedir(dir);
    return result;
}

static void Cod2x_DemoRememberDirectory(const char *path)
{
    struct DemoDirectory *directory;
    if (!path || !*path)
        return;
    for (directory = demo_directories; directory; directory = directory->next)
        if (!strcmp(directory->path, path))
            return;
    directory = calloc(1, sizeof(*directory));
    if (!directory)
        return;
    directory->path = strdup(path);
    if (!directory->path) {
        free(directory);
        return;
    }
    directory->next = demo_directories;
    demo_directories = directory;
}

static int Cod2x_DemoNextMarker(const char *path, char marker[4096], char file[4096])
{
    struct DemoDirectory *directory;
    if (Cod2x_DemoMarkerInDirectory(path, marker, file))
        return 1;
    for (directory = demo_directories; directory; directory = directory->next)
        if ((!path || strcmp(path, directory->path)) &&
            Cod2x_DemoMarkerInDirectory(directory->path, marker, file))
            return 1;
    return 0;
}

int Cod2x_DemoUploadsPending(const char *directory)
{
    char marker[4096], file[4096];
    Cod2x_DemoRememberDirectory(directory);
    return demo_easy != NULL || Cod2x_DemoNextMarker(directory, marker, file);
}

static FILE *Cod2x_DemoOpen(const char *path)
{
    struct stat st;
    FILE *file;
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0)
        return NULL;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode)) {
        close(fd);
        return NULL;
    }
    file = fdopen(fd, "rb");
    if (!file)
        close(fd);
    return file;
}

static size_t Cod2x_DemoRead(char *buffer, size_t size, size_t count, void *context)
{
    FILE *file = context;
    size_t read = fread(buffer, size, count, file);
    return ferror(file) ? CURL_READFUNC_ABORT : read * size;
}

static size_t Cod2x_DemoDiscard(char *buffer, size_t size, size_t count, void *context)
{
    (void)buffer;
    (void)context;
    return size * count;
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

static void Cod2x_DemoFinish(CURLcode result, long status)
{
    struct DemoAttempt *attempt = Cod2x_DemoAttemptFor(demo_marker);
    demo_progress.httpStatus = status;
    if (!result && Cod2x_DemoUploadSucceeded(status)) {
        demo_progress.state = COD2X_DEMO_UPLOAD_DONE;
        demo_progress.uploaded = demo_progress.total;
        if (unlink(demo_marker))
            demo_progress.state = COD2X_DEMO_UPLOAD_FAILED;
    } else {
        demo_progress.state = COD2X_DEMO_UPLOAD_FAILED;
    }
    if (attempt)
        demo_progress.attempts = (int)++attempt->attempts;
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

static void Cod2x_DemoStart(const char *directory, int timeoutSeconds)
{
    char path[4096], url[1024];
    struct stat st;
    FILE *marker;
    CURLMcode added;
    size_t length;
    if (!Cod2x_DemoNextMarker(directory, demo_marker, path))
        return;
    marker = Cod2x_DemoOpen(demo_marker);
    if (!marker)
        return;
    length = fread(url, 1, sizeof(url) - 1, marker);
    url[length] = '\0';
    if (ferror(marker) || !feof(marker))
        url[0] = '\0';
    fclose(marker);
    url[strcspn(url, "\r\n")] = '\0';
    if (!Cod2x_DemoHTTPS(url)) {
        unlink(demo_marker);
        return;
    }
    demo_file = Cod2x_DemoOpen(path);
    if (!demo_file || fstat(fileno(demo_file), &st) || st.st_size <= 0) {
        if (demo_file)
            fclose(demo_file);
        demo_file = NULL;
        unlink(demo_marker);
        return;
    }
    memset(&demo_progress, 0, sizeof(demo_progress));
    snprintf(demo_progress.name, sizeof(demo_progress.name), "%s", strrchr(path, '/') + 1);
    demo_progress.total = (uint64_t)st.st_size;
    demo_progress.state = COD2X_DEMO_UPLOAD_ACTIVE;
    demo_started = Cod2x_DemoMilliseconds();
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
    curl_easy_setopt(demo_easy, CURLOPT_WRITEFUNCTION, Cod2x_DemoDiscard);
    curl_easy_setopt(demo_easy, CURLOPT_XFERINFOFUNCTION, Cod2x_DemoProgress);
    curl_easy_setopt(demo_easy, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(demo_easy, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(demo_easy, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(demo_easy, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(demo_easy, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(demo_easy, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(demo_easy, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(demo_easy, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(demo_easy, CURLOPT_TIMEOUT, (long)timeoutSeconds);
    curl_easy_setopt(demo_easy, CURLOPT_MAX_SEND_SPEED_LARGE, (curl_off_t)(5 * 1024 * 1024));
    added = curl_multi_add_handle(demo_multi, demo_easy);
    if (added != CURLM_OK)
        Cod2x_DemoFinish(CURLE_FAILED_INIT, 0);
}

void Cod2x_DemoUploadFrame(const char *directory, int timeoutSeconds, int recording)
{
    CURLMsg *message;
    int running, messages;
    Cod2x_DemoRememberDirectory(directory);
    if (!demo_easy && !recording && Cod2x_DemoMilliseconds() >= demo_nextScan) {
        demo_nextScan = Cod2x_DemoMilliseconds() + 1000;
        Cod2x_DemoStart(directory, timeoutSeconds);
    }
    if (!demo_easy)
        return;
    if (curl_multi_perform(demo_multi, &running) != CURLM_OK) {
        Cod2x_DemoFinish(CURLE_RECV_ERROR, 0);
        return;
    }
    while ((message = curl_multi_info_read(demo_multi, &messages)) != NULL) {
        if (message->msg == CURLMSG_DONE && message->easy_handle == demo_easy) {
            long status = 0;
            curl_easy_getinfo(demo_easy, CURLINFO_RESPONSE_CODE, &status);
            Cod2x_DemoFinish(message->data.result, status);
            break;
        }
    }
}

const Cod2xDemoProgress *Cod2x_DemoUploadProgress(void)
{
    return &demo_progress;
}

void Cod2x_DemoUploadShutdown(void)
{
    struct DemoAttempt *attempt;
    struct DemoDirectory *directory;
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
    demo_nextScan = 0;
    while ((directory = demo_directories) != NULL) {
        demo_directories = directory->next;
        free(directory->path);
        free(directory);
    }
    while ((attempt = demo_attempts) != NULL) {
        demo_attempts = attempt->next;
        free(attempt->marker);
        free(attempt);
    }
}
#endif
