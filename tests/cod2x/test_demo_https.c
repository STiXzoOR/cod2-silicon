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
#define curl_easy_init TestDemoEasyInit
#include "../../src/PC/qcommon/cod2x_demo.c"
#undef curl_easy_init

int main(int argc, char **argv)
{
    int i, success;
    const Cod2xDemoProgress *progress;
    assert(argc == 3);
    success = !strcmp(argv[2], "success");
    for (i = 0; i < 1000 && Cod2x_DemoUploadsPending(argv[1]); ++i) {
        Cod2x_DemoUploadFrame(argv[1], 10, 0);
        usleep(10000);
    }
    assert(i < 1000);
    progress = Cod2x_DemoUploadProgress();
    assert(progress->state == (success ? COD2X_DEMO_UPLOAD_DONE : COD2X_DEMO_UPLOAD_FAILED));
    if (success) {
        assert(progress->httpStatus == 201);
        assert(progress->uploaded == progress->total);
    } else {
        assert(progress->attempts == 3);
    }
    Cod2x_DemoUploadShutdown();
    return 0;
}
