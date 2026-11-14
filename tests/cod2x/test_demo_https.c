#include <curl/curl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Only the fixture supplies its ephemeral CA. Production always uses the SDK
   trust store and still verifies both certificate and hostname. */
static CURL *TestDemoEasyInit(void)
{
    CURL *easy = curl_easy_init();
    assert(easy);
    assert(curl_easy_setopt(easy, CURLOPT_CAINFO, getenv("COD2X_TEST_CA")) == CURLE_OK);
    return easy;
}
void Com_Printf(const char *format, ...)
{
    (void)format;
}

void Com_Error(int code, const char *format, ...)
{
    (void)code;
    (void)format;
    abort();
}

#define curl_easy_init TestDemoEasyInit
#include "../../src/PC/qcommon/cod2x_demo.c"
#undef curl_easy_init

int main(int argc, char **argv)
{
    int i, success;
    const Cod2xDemoProgress *progress;
    assert(argc == 3 || argc == 4);
    demo_speedStarted = 1000;
    demo_speedBytes = 1000;
    demo_speedAverage = 0;
    Cod2x_DemoSampleSpeed(1099);
    assert(demo_progress.bytesPerSecond == 0 && demo_speedBytes == 1000);
    Cod2x_DemoSampleSpeed(1100);
    assert(demo_progress.bytesPerSecond == 2000 && demo_speedBytes == 0);
    demo_speedBytes = 2000;
    Cod2x_DemoSampleSpeed(1200);
    assert(demo_progress.bytesPerSecond == 5600);
    Cod2x_DemoSampleSpeed(1300);
    assert(demo_progress.bytesPerSecond == 4480);
    puts("Upload speed samples: 0, 2000, 5600, 4480 bytes/s (100 ms EMA)");
    success = !strcmp(argv[2], "success");
    for (i = 0; i < 1000 && Cod2x_DemoUploadsPending(argv[1]); ++i) {
        Cod2x_DemoUploadFrame(argv[1], !strcmp(argv[2], "timeout") ? 1 : 60, 1);
        usleep(10000);
    }
    assert(i < 1000);
    progress = Cod2x_DemoUploadProgress();
    assert(progress->state == (success ? COD2X_DEMO_UPLOAD_DONE : COD2X_DEMO_UPLOAD_FAILED));
    if (success) {
        assert(progress->httpStatus == (argc == 4 ? atoi(argv[3]) : 201));
        assert(progress->uploaded == progress->total);
    } else {
        assert(progress->attempts == 3);
    }
    Cod2x_DemoUploadShutdown();
    return 0;
}
