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

    while ((m = curl_multi_info_read(dl_multi, &msgs_in_queue)) != 0) {
        if (m->msg == CURLMSG_DONE) {
            CURLcode res = m->data.result;
            long http_code = 0;
            if (dl_easy)
                curl_easy_getinfo(dl_easy, CURLINFO_RESPONSE_CODE, &http_code);

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
