#include "download_fixture.h"
#include "www_download_begin_source.h"
int main(void)
{
    CL_BeginDownload("main/map.iwd", "main/map.iwd");
    assert(!strcmp(cls.downloadName, "main/map.iwd"));
    assert(!strcmp(cls.downloadTempName, "main/map.iwd.tmp"));
    assert(!strcmp(connection.reliableCommands[1], "download main/map.iwd"));
    puts("online: HTTP redirect state receives the requested local IWD path");
    return 0;
}
