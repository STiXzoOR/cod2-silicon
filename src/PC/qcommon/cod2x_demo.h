#ifndef COD2X_DEMO_H
#define COD2X_DEMO_H

#include <stddef.h>
#include <stdint.h>

enum {
    COD2X_DEMO_UPLOAD_IDLE,
    COD2X_DEMO_UPLOAD_ACTIVE,
    COD2X_DEMO_UPLOAD_DONE,
    COD2X_DEMO_UPLOAD_FAILED
};

typedef struct {
    char name[256];
    uint64_t uploaded, total, bytesPerSecond;
    int state, attempts;
    long httpStatus;
} Cod2xDemoProgress;

int Cod2x_DemoName(char *name, size_t capacity, const char *requested, unsigned suffix);
int Cod2x_DemoHTTPS(const char *url);
int Cod2x_DemoUploadURL(char *url, size_t capacity, const char *base, const char *name);
int Cod2x_DemoUploadSucceeded(long status);
int Cod2x_DemoUploadsPending(const char *directory);
void Cod2x_DemoUploadFrame(const char *directory, int timeoutSeconds, int recording);
void Cod2x_DemoUploadShutdown(void);
const Cod2xDemoProgress *Cod2x_DemoUploadProgress(void);

/* Engine client hooks are implemented in cl_main_mp.c. */
int Cod2x_DemoClientQuitRequested(void);

#endif
