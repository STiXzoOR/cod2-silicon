#include "cod2_feature_config.h"
#include "www_download.h"

#if COD2_FEATURE_WWW_DOWNLOAD

#    include <string.h>
#    include <stdio.h>
#if defined(COD2_X64)
#    include "common_types.h"
extern void *imp_legacyHacks;
int Com_sprintf(char *dest, int size, const char *fmt, ...);
#endif

void Com_Printf(const char *fmt, ...);
void Com_DPrintf(const char *fmt, ...);
#if defined(COD2_X64)
int FS_CreatePath(char *path);
#else
void FS_CreatePath(const char *path);
#endif

#if defined(__APPLE__) && defined(COD2_X64)
/* Native macOS links the SDK libcurl; its prototypes take enum option types. */
#    include <curl/curl.h>
#else
typedef void CURL;
typedef void CURLM;
typedef int CURLcode;
typedef int CURLMcode;

#    define CURLOPTTYPE_LONG 0
#    define CURLOPTTYPE_OBJECTPOINT 10000
#    define CURLOPTTYPE_FUNCTIONPOINT 20000
#    define CURLOPT_URL (CURLOPTTYPE_OBJECTPOINT + 2)
#    define CURLOPT_WRITEDATA (CURLOPTTYPE_OBJECTPOINT + 1)
#    define CURLOPT_WRITEFUNCTION (CURLOPTTYPE_FUNCTIONPOINT + 11)
#    define CURLOPT_FOLLOWLOCATION (CURLOPTTYPE_LONG + 52)
#    define CURLOPT_FAILONERROR (CURLOPTTYPE_LONG + 45)
#    define CURLOPT_NOPROGRESS (CURLOPTTYPE_LONG + 43)
#    define CURLOPT_CONNECTTIMEOUT (CURLOPTTYPE_LONG + 78)
#    define CURLOPT_USERAGENT (CURLOPTTYPE_OBJECTPOINT + 18)
#    define CURLINFO_RESPONSE_CODE 0x200002
#    if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* CoD2x's https download options (curl.h is not included here: its windows.h
   clashes with the engine's own Windows types). */
#        define CURLOPT_HTTPHEADER (CURLOPTTYPE_OBJECTPOINT + 23)
#        define CURLOPT_LOW_SPEED_LIMIT (CURLOPTTYPE_LONG + 19)
#        define CURLOPT_LOW_SPEED_TIME (CURLOPTTYPE_LONG + 20)
#        define CURLOPT_MAXREDIRS (CURLOPTTYPE_LONG + 68)
#        define CURLINFO_CONTENT_LENGTH_DOWNLOAD_T 0x60000f
typedef long long curl_off_t;
struct curl_slist;
extern struct curl_slist *curl_slist_append(struct curl_slist *list, const char *string);
extern void curl_slist_free_all(struct curl_slist *list);
#    endif

#    define CURLE_OK 0
#    define CURLM_OK 0

extern CURL *curl_easy_init(void);
extern CURLcode curl_easy_setopt(CURL *handle, int option, ...);
extern CURLcode curl_easy_getinfo(CURL *handle, int info, ...);
extern void curl_easy_cleanup(CURL *handle);
extern const char *curl_easy_strerror(CURLcode);

extern CURLM *curl_multi_init(void);
extern CURLMcode curl_multi_add_handle(CURLM *multi, CURL *easy);
extern CURLMcode curl_multi_remove_handle(CURLM *multi, CURL *easy);
extern CURLMcode curl_multi_cleanup(CURLM *multi);
extern CURLMcode curl_multi_perform(CURLM *multi, int *running_handles);

typedef struct {
    int msg;
    CURL *easy_handle;
    union {
        void *whatever;
        CURLcode result;
    } data;
} CURLMsg;
#    define CURLMSG_DONE 1
extern CURLMsg *curl_multi_info_read(CURLM *multi, int *msgs_in_queue);
#endif

static CURLM *dl_multi = 0;
static CURL *dl_easy = 0;
static FILE *dl_fp = 0;
static int dl_running = 0;
static int dl_isMotd = 0;
static dlStatus_t dl_status = DL_STATUS_IDLE;
static int dl_debug = 0;
static char dl_localName[1024];

#if defined(COD2_X64)
static unsigned int dl_bytesRead;

static void DL_SetDisplayName(const char *remoteName)
{
    LegacyHacks *ui = *(LegacyHacks **)imp_legacyHacks;
    const char *schemeEnd = strchr(remoteName, ':');
    const char *authority = NULL;
    if (schemeEnd && schemeEnd[1] == '/' && schemeEnd[2] == '/')
        authority = schemeEnd + 3;

    if (authority) {
        const char *authorityEnd = authority + strcspn(authority, "/?#");
        const char *at = memchr(authority, '@', authorityEnd - authority);
        if (at) {
            const char *end = authorityEnd + strcspn(authorityEnd, "#");
            Com_sprintf(ui->cl_downloadName, sizeof(ui->cl_downloadName),
                        "%.*s://*:*%.*s%.*s", (int)(schemeEnd - remoteName), remoteName,
                        (int)(authorityEnd - at), at, (int)(end - authorityEnd), authorityEnd);
            return;
        }
    }
    Com_sprintf(ui->cl_downloadName, sizeof(ui->cl_downloadName), "%s", remoteName);
}

#endif
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
/* CoD2x downloading.cpp: a file from an https server must start with "PK", so a host
   that answers with a web page and status 200 fails the download. */
static int dl_needsZip, dl_expectZip;
static struct curl_slist *dl_headers;
extern int I_strnicmp(const char *s0, const char *s1, size_t n);

/* Called before DL_BeginDownload for a zPAM download from CoD2x's site, which gets
   the https rules even when a test sends it to a plain http stand-in. */
void DL_Cod2xExpectZip(void)
{
    dl_expectZip = 1;
}

static int DL_Cod2xZipStart(const char *path)
{
    unsigned char start[2];
    FILE *file = fopen(path, "rb");
    size_t read = file ? fread(start, 1, sizeof(start), file) : 0;
    if (file)
        fclose(file);
    return read == 2 && start[0] == 'P' && start[1] == 'K';
}

/* CoD2x's https downloader decides how a finished request ends, printing as it does. */
static void DL_Cod2xFinish(CURLcode result)
{
    LegacyHacks *ui = *(LegacyHacks **)imp_legacyHacks;
    if (result != CURLE_OK) {
        Com_Printf("DL_DownloadLoop: request terminated with failure: %s\n", curl_easy_strerror(result));
        dl_status = DL_STATUS_FAILED;
    } else if (!dl_bytesRead) {
        Com_Printf("DL_DownloadLoop: request terminated with failure: %s\n", "Received an empty file");
        dl_status = DL_STATUS_FAILED;
    } else if (!DL_Cod2xZipStart(dl_localName)) {
        Com_Printf("DL_DownloadLoop: request terminated with failure: %s\n", "Unexpected file content");
        dl_status = DL_STATUS_FAILED;
    } else if (ui->cl_downloadSize > 0 && dl_bytesRead != (unsigned int)ui->cl_downloadSize) {
        /* Without a size from the server or the reply, a cut connection would look complete. */
        Com_Printf("DL_DownloadLoop: size does not match, expected %d bytes\n", ui->cl_downloadSize);
        dl_status = DL_STATUS_FAILED;
    }
}

#endif

static size_t DL_WriteCb(void *ptr, size_t size, size_t nmemb, void *stream)
{
#if defined(COD2_X64)
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream) * size;
    dl_bytesRead += (unsigned int)written;
    (*(LegacyHacks **)imp_legacyHacks)->cl_downloadCount = (int)dl_bytesRead;
    return written;
#else
    return fwrite(ptr, size, nmemb, (FILE *)stream);
#endif
}

static void DL_Teardown(void)
{
    if (dl_multi && dl_easy)
        curl_multi_remove_handle(dl_multi, dl_easy);
    if (dl_easy) {
        curl_easy_cleanup(dl_easy);
        dl_easy = 0;
    }
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    if (dl_headers) {
        curl_slist_free_all(dl_headers);
        dl_headers = NULL;
    }
#endif
    if (dl_fp) {
#if defined(COD2_X64)
        /* Buffered writes can fail only when close flushes them. Do not let
         * the engine rename an incomplete file after a successful transfer. */
        if (fclose(dl_fp) != 0 && dl_status == DL_STATUS_DONE) {
            dl_status = DL_STATUS_FAILED;
            Com_Printf("DL_DownloadLoop: could not finish writing '%s'\n", dl_localName);
        }
#else
        fclose(dl_fp);
#endif
        dl_fp = 0;
    }
    dl_running = 0;
}

void DL_InitDownload(void)
{
    if (dl_multi)
        return;
    dl_multi = curl_multi_init();
    Com_Printf("Client download subsystem initialized\n");
}

int DL_BeginDownload(const char *localName, const char *remoteName, int debug)
{

    if (dl_running)
        DL_Teardown();

    dl_status = DL_STATUS_FAILED;
    dl_debug = debug;
    dl_isMotd = 0;
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    dl_needsZip = dl_expectZip || (remoteName && !I_strnicmp(remoteName, "https://", 8));
    dl_expectZip = 0;
#endif

    if (!localName || !remoteName) {
        Com_DPrintf("Empty download URL or empty local file name\n");
        return 0;
    }

    DL_InitDownload();
    if (!dl_multi)
        return 0;

#if defined(COD2_X64)
    /* The retail MOTD flag is not inferred from the download URL. */
#else
    dl_isMotd = (strstr(remoteName, "motd.txt") != 0);
#endif

    FS_CreatePath(localName);
    {
        size_t n = strlen(localName);
        if (n >= sizeof(dl_localName))
            n = sizeof(dl_localName) - 1;
        memcpy(dl_localName, localName, n);
        dl_localName[n] = '\0';
    }

    dl_fp = fopen(localName, "wb");
    if (!dl_fp) {
        Com_DPrintf("DL_BeginDownload: could not open '%s' for writing\n", localName);
        return 0;
    }

    dl_easy = curl_easy_init();
    if (!dl_easy) {
        fclose(dl_fp);
        dl_fp = 0;
        Com_DPrintf("DL_BeginDownload: curl_easy_init failed\n");
        return 0;
    }

    curl_easy_setopt(dl_easy, CURLOPT_URL, remoteName);
    curl_easy_setopt(dl_easy, CURLOPT_WRITEFUNCTION, DL_WriteCb);
    curl_easy_setopt(dl_easy, CURLOPT_WRITEDATA, dl_fp);
    curl_easy_setopt(dl_easy, CURLOPT_FOLLOWLOCATION, (long)1);
    curl_easy_setopt(dl_easy, CURLOPT_FAILONERROR, (long)1);
    curl_easy_setopt(dl_easy, CURLOPT_NOPROGRESS, (long)1);
    curl_easy_setopt(dl_easy, CURLOPT_CONNECTTIMEOUT, (long)30);
    curl_easy_setopt(dl_easy, CURLOPT_USERAGENT, "CoD2 wwwdl");
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    if (dl_needsZip) {
        /* CoD2x's https downloader sends only Host, "User-Agent: CoD2x" and
           "Connection: close", follows at most 5 redirects and gives up after 30
           seconds without data. Certificates are checked here. */
        curl_easy_setopt(dl_easy, CURLOPT_USERAGENT, "CoD2x");
        dl_headers = curl_slist_append(NULL, "Accept:");
        dl_headers = curl_slist_append(dl_headers, "Connection: close");
        curl_easy_setopt(dl_easy, CURLOPT_HTTPHEADER, dl_headers);
        curl_easy_setopt(dl_easy, CURLOPT_MAXREDIRS, (long)5);
        curl_easy_setopt(dl_easy, CURLOPT_LOW_SPEED_LIMIT, (long)1);
        curl_easy_setopt(dl_easy, CURLOPT_LOW_SPEED_TIME, (long)30);
    }
#endif

    if (curl_multi_add_handle(dl_multi, dl_easy) != CURLM_OK) {
        DL_Teardown();
        Com_DPrintf("DL_BeginDownload: curl_multi_add_handle failed\n");
        return 0;
    }

#if defined(COD2_X64)
    DL_SetDisplayName(remoteName);
    dl_bytesRead = 0;
    if (dl_debug)
        Com_DPrintf("DL_BeginDownload: %s -> %s\n",
                    (*(LegacyHacks **)imp_legacyHacks)->cl_downloadName, localName);
#else
    if (dl_debug)
        Com_DPrintf("DL_BeginDownload: %s -> %s\n", remoteName, localName);
#endif

    dl_running = 1;
    dl_status = DL_STATUS_IN_PROGRESS;
    return 1;
}

void DL_DownloadLoop(void)
{
    int still_running = 0;
    int msgs_in_queue = 0;
    CURLMsg *m;

    if (!dl_running || !dl_multi)
        return;

    if (curl_multi_perform(dl_multi, &still_running) != CURLM_OK) {
        dl_status = DL_STATUS_FAILED;
        DL_Teardown();
        return;
    }
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
    if (dl_needsZip && dl_easy) {
        /* A download the client started learns its size from the reply. */
        curl_off_t length = -1;
        if (curl_easy_getinfo(dl_easy, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &length) == CURLE_OK && length > 0)
            (*(LegacyHacks **)imp_legacyHacks)->cl_downloadSize = (int)length;
    }
#endif

    while ((m = curl_multi_info_read(dl_multi, &msgs_in_queue)) != 0) {
        if (m->msg == CURLMSG_DONE) {
            CURLcode res = m->data.result;
            long http_code = 0;
            if (dl_easy)
                curl_easy_getinfo(dl_easy, CURLINFO_RESPONSE_CODE, &http_code);

#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
            if (dl_needsZip) {
                dl_status = DL_STATUS_DONE;
                DL_Teardown();
                if (dl_status == DL_STATUS_DONE)
                    DL_Cod2xFinish(res);
                return;
            }
#endif
            if (res == CURLE_OK) {
                dl_status = DL_STATUS_DONE;
            } else {
                Com_Printf("DL_DownloadLoop: transfer failed: %s (http %ld)\n",
                           curl_easy_strerror(res), http_code);
                dl_status = DL_STATUS_FAILED;
            }
            DL_Teardown();
            return;
        }
    }

    if (!still_running) {

        dl_status = DL_STATUS_DONE;
        DL_Teardown();
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
        if (dl_status == DL_STATUS_DONE && dl_needsZip)
            DL_Cod2xFinish(CURLE_OK);
#endif
    }
}

void DL_CancelDownload(void)
{
    if (dl_running || dl_easy || dl_fp)
        DL_Teardown();
    dl_status = DL_STATUS_CANCELLED;
    dl_isMotd = 0;
}

int DL_InProgress(void)
{
    return dl_running > 0;
}

int DL_DLIsMotd(void)
{
    return dl_isMotd;
}

dlStatus_t DL_GetStatus(void)
{
    return dl_status;
}

#endif
