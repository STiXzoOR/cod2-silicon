#include "download_fixture.h"
#include "www_download_source.h"
int main(void)
{
    msg_t message = {0};
    strcpy(cls.downloadName, "main/map.iwd");
    strcpy(cls.downloadTempName, "main/map.iwd.tmp");
    CL_ParseWWWDownload(&message);
    assert(!strcmp(cls.originalDownloadName, "main/map.iwd"));
    assert(!strcmp(remote, "https://example.test/main/map.iwd"));
    assert(!strcmp(local, "/private/main/map.iwd.tmp"));
    assert(cls.wwwDlInProgress && !strcmp(commands, "wwwdl ack\n"));
    downloadStatus = DL_STATUS_DONE;
    CL_WWWDownload();
    assert(!strcmp(renamed, "/private/main/map.iwd"));
    assert(!cls.wwwDlInProgress && nextDownloads == 1);
    assert(!strcmp(commands, "wwwdl ack\nwwwdl done\n"));
    puts("online: HTTP redirect completion installs the IWD under its original local name");
    return 0;
}
