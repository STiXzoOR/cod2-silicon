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
    char error[256]; /* "Error: ..." after a failure, as CoD2x shows it */
    uint64_t uploaded, total, bytesPerSecond;
    int state, attempts;
    long httpStatus;
} Cod2xDemoProgress;

int Cod2x_DemoName(char *name, size_t capacity, const char *requested, unsigned suffix);
int Cod2x_DemoHTTPS(const char *url);
int Cod2x_DemoUploadURL(char *url, size_t capacity, const char *base, const char *name);
int Cod2x_DemoUploadSucceeded(long status);
/* Looks for the next .upload marker in directory, printing as CoD2x does. */
int Cod2x_DemoUploadsPending(const char *directory);
/* check starts the next upload when none is running. Returns 1 when an upload ended. */
int Cod2x_DemoUploadFrame(const char *directory, int timeoutSeconds, int check);
int Cod2x_DemoUploading(void);
/* An upload is running, or its result is still on screen (2 s after success, 3 s after a failure). */
int Cod2x_DemoUploadShown(void);
void Cod2x_DemoUploadNewRecording(void);
void Cod2x_DemoUploadShutdown(void);
const Cod2xDemoProgress *Cod2x_DemoUploadProgress(void);

/* Engine client hooks are implemented in cl_main_mp.c. */
int Cod2x_DemoClientQuitRequested(void);
void Cod2x_DemoClientDraw(void);

#endif
