#define TEST_WWW_LIFECYCLE 1
#include "download_fixture.h"

void CL_BeginDownload(const char *localName, const char *remoteName)
{
    assert(!strcmp(localName, "main/map.iwd") && !strcmp(remoteName, "main/remote.iwd"));
    event("ask-server");
}
#include "www_download_source.h"

static const unsigned char body[] = {'P', 'K', 0, 1, 255, 7, 0, 9};
static char staged[512], installed[512];

static void prepare(int flags)
{
    memset(&cls, 0, sizeof(cls)); memset(&connection, 0, sizeof(connection)); memset(&hacks, 0, sizeof(hacks));
    reads = nextDownloads = copies = removals = cancels = errors = beginFails = motd = renameFails = 0;
    directActive = directCancelled = directFinished = 0;
    commands[0] = events[0] = errorText[0] = queued[0] = renamed[0] = 0;
    redirectFlags = flags;
    strcpy(cls.downloadName, "main/map.iwd"); strcpy(cls.downloadTempName, "stage/map.iwd.tmp");
    strcpy(connection.downloadName, "main/map.iwd"); strcpy(connection.downloadTempName, "stage/map.iwd.tmp");
    strcpy(hacks.cl_downloadName, "downloading");
    FILE *file = fopen(staged, "wb");
    assert(file && fwrite(body, 1, sizeof(body), file) == sizeof(body) && !fclose(file));
}

static void parse(void)
{
    msg_t message = {0};
    CL_ParseWWWDownload(&message);
    assert(!strcmp(cls.originalDownloadName, "main/map.iwd"));
    assert(!strcmp(remote, "https://example.test/main/map.iwd") && !strcmp(local, staged));
    assert(!strcmp(commands, redirectFlags ? "wwwdl ack\nwwwdl bbl8r\n" : "wwwdl ack\n"));
    commands[0] = events[0] = 0;
}

static void completed(const char *expected)
{
    assert(!strcmp(events, expected));
    assert(!cls.wwwDlInProgress && nextDownloads == 1 && !errors);
    assert(!cls.downloadName[0] && !cls.downloadTempName[0]);
    assert(!connection.downloadName[0] && !connection.downloadTempName[0] && !hacks.cl_downloadName[0]);
    assert(!strcmp(renamed, installed) && access(staged, F_OK));
    unsigned char actual[sizeof(body) + 1]; FILE *file = fopen(installed, "rb");
    assert(file && fread(actual, 1, sizeof(actual), file) == sizeof(body) && !fclose(file));
    assert(!memcmp(body, actual, sizeof(body)) && !unlink(installed));
    printf("completion: %s\n", events);
}

int main(void)
{
    char directory[256], mainPath[512], stagePath[512];
    const char *temporary = getenv("TMPDIR");
    snprintf(directory, sizeof(directory), "%s/www-lifecycle.XXXXXX", temporary ? temporary : "/tmp");
    assert(mkdtemp(directory)); snprintf(homePath, sizeof(homePath), "%s", directory);
    snprintf(mainPath, sizeof(mainPath), "%s/main", directory); snprintf(stagePath, sizeof(stagePath), "%s/stage", directory);
    assert(!mkdir(mainPath, 0700) && !mkdir(stagePath, 0700));
    snprintf(staged, sizeof(staged), "%s/map.iwd.tmp", stagePath); snprintf(installed, sizeof(installed), "%s/map.iwd", mainPath);
    realFiles = 1;

    prepare(0); parse();
    downloadStatus = DL_STATUS_IDLE; CL_WWWDownload();
    downloadStatus = DL_STATUS_IN_PROGRESS; CL_WWWDownload();
    assert(!strcmp(events, "poll|poll|") && cls.wwwDlInProgress && !nextDownloads);
    events[0] = 0; downloadStatus = DL_STATUS_DONE; CL_WWWDownload();
    completed("poll|rename|wwwdl done|next|");
    assert(!copies && !removals && !strcmp(commands, "wwwdl done\n"));

    prepare(0); parse(); renameFails = 1; downloadStatus = DL_STATUS_DONE; CL_WWWDownload();
    completed("poll|rename|copy|remove|wwwdl done|next|");
    assert(copies == 1 && removals == 1); /* Inject EXDEV; copy/unlink and bytes are real. */

    prepare(1); parse(); downloadStatus = DL_STATUS_DONE; CL_WWWDownload();
    completed("poll|rename|reconnect|next|");
    assert(!strcmp(queued, "reconnect\n") && !commands[0] && cls.wwwDlDisconnected);

    prepare(0); parse(); downloadStatus = DL_STATUS_FAILED; CL_WWWDownload();
    assert(!strcmp(events, "poll|wwwdl fail|") && !cls.wwwDlInProgress && !errors && !nextDownloads);
    assert(cls.downloadName[0] && !access(staged, F_OK));
    printf("connected failure: %s\n", events); assert(!unlink(staged));

    prepare(1); parse(); downloadStatus = DL_STATUS_FAILED; CL_WWWDownload();
    assert(!strcmp(events, "poll|cancel|error|") && errors == 1 && cancels == 1 && !nextDownloads);
    assert(!cls.wwwDlDisconnected && !cls.wwwDlInProgress && !cls.downloadName[0] && !cls.originalDownloadName[0]);
    assert(strstr(errorText, "Download failure while getting 'https://example.test/main/map.iwd'"));
    printf("disconnected failure: %s\n", events); assert(!unlink(staged));

    prepare(0); parse(); CL_ClearStaticDownload();
    assert(!strcmp(events, "cancel|") && cancels == 1 && !cls.wwwDlInProgress && downloadStatus == DL_STATUS_CANCELLED);
    assert(!cls.downloadTempName[0] && !nextDownloads && access(installed, F_OK));
    printf("cancellation: %s\n", events); assert(!unlink(staged));

    prepare(1); beginFails = 1;
    msg_t message = {0}; CL_ParseWWWDownload(&message);
    assert(!strcmp(events, "wwwdl ack|begin|wwwdl fail|cancel|wwwdl bbl8r|"));
    assert(!cls.wwwDlInProgress && cls.wwwDlDisconnected && cancels == 1);
    printf("start failure: %s\n", events); assert(!unlink(staged));

    prepare(0); parse(); motd = 1; downloadStatus = DL_STATUS_DONE; CL_WWWDownload();
    assert(!strcmp(events, "poll|motd-read|motd-set|motd-free|") && !cls.wwwDlInProgress && !nextDownloads);
    events[0] = 0; cls.wwwDlInProgress = 1; downloadStatus = DL_STATUS_FAILED; CL_WWWDownload();
    assert(!strcmp(events, "poll|") && !cls.wwwDlInProgress && !nextDownloads); assert(!unlink(staged));

#if defined(COD2_CODX) && COD2_CODX
    prepare(0); parse(); directActive = 1; downloadStatus = DL_STATUS_DONE; CL_WWWDownload();
    completed("poll|rename|direct-end|next|");
    assert(!commands[0] && !directActive && directFinished == 1);
    prepare(0); parse(); directActive = 1; downloadStatus = DL_STATUS_FAILED; CL_WWWDownload();
    assert(!strcmp(events, "poll|direct-end|remove|ask-server|") && !commands[0] && !nextDownloads && !cls.wwwDlInProgress);
    assert(access(staged, F_OK) && directFinished == 1); printf("direct fallback: %s\n", events);
    prepare(0); parse(); directActive = 1; CL_ClearStaticDownload();
    assert(!strcmp(events, "direct-cancel|cancel|") && directCancelled == 1 && !directActive);
    printf("direct cancellation: %s\n", events); assert(!unlink(staged));
#endif
    assert(!rmdir(stagePath) && !rmdir(mainPath) && !rmdir(directory));
    puts("online: production WWW lifecycle, real file installation and injected cross-device fallback pass");
    return 0;
}
