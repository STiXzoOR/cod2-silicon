#include "cod2x_demo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_file(const char *path, const char *body)
{
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(body, 1, strlen(body), file) == strlen(body));
    assert(!fclose(file));
}

int main(void)
{
    char name[64], url[1024], directory[] = "/tmp/cod2x-demo.XXXXXX";
    char demo[512], marker[512];
    int i;
    assert(Cod2x_DemoName(name, sizeof(name), "round/one;quit\n", 0));
    assert(!strcmp(name, "round_one_quit_"));
    assert(Cod2x_DemoName(name, sizeof(name), "round/one", 17));
    assert(!strcmp(name, "round_one_17"));
    assert(!Cod2x_DemoName(name, sizeof(name), "", 0));
    assert(!Cod2x_DemoName(name, sizeof(name), "..", 0));
    assert(Cod2x_DemoHTTPS("https://example.test:443/demos/"));
    assert(!Cod2x_DemoHTTPS("http://example.test/demos/"));
    assert(!Cod2x_DemoHTTPS("file:///tmp/example"));
    assert(!Cod2x_DemoHTTPS("https://example.test/a\r\nb"));
    assert(!Cod2x_DemoHTTPS("https://user:pass@example.test/a"));
    assert(Cod2x_DemoUploadURL(url, sizeof(url), "https://example.test/demos/", "round_17"));
    assert(!strcmp(url, "https://example.test/demos/round_17"));
    assert(!Cod2x_DemoUploadURL(url, 12, "https://example.test/demos/", "round_17"));
    assert(Cod2x_DemoUploadSucceeded(200));
    assert(Cod2x_DemoUploadSucceeded(201));
    assert(Cod2x_DemoUploadSucceeded(409));
    assert(!Cod2x_DemoUploadSucceeded(301));
    assert(!Cod2x_DemoUploadSucceeded(500));
    assert(mkdtemp(directory));
    snprintf(demo, sizeof(demo), "%s/round.dm_1", directory);
    snprintf(marker, sizeof(marker), "%s.upload", demo);
    write_file(demo, "owned demo fixture");
    write_file(marker, "http://127.0.0.1:9/round");
    assert(Cod2x_DemoUploadsPending(directory));
    Cod2x_DemoUploadFrame(directory, 10, 0);
    assert(!Cod2x_DemoUploadsPending(directory));
    assert(!access(demo, F_OK));
    assert(access(marker, F_OK));
    write_file(marker, "https://127.0.0.1:9/round");
    for (i = 0; i < 500 && Cod2x_DemoUploadsPending(directory); ++i) {
        Cod2x_DemoUploadFrame(directory, 10, 0);
        usleep(10000);
    }
    assert(i < 500);
    assert(!access(marker, F_OK));
    assert(Cod2x_DemoUploadProgress()->state == COD2X_DEMO_UPLOAD_FAILED);
    assert(Cod2x_DemoUploadProgress()->attempts == 3);
    Cod2x_DemoUploadShutdown();
    assert(Cod2x_DemoUploadsPending(directory));
    Cod2x_DemoUploadShutdown();
    assert(!unlink(marker));
    assert(!unlink(demo));
    assert(!rmdir(directory));
    puts("CoD2x demo policy and persistent upload queue: pass");
    return 0;
}
