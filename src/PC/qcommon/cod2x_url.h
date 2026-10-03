#ifndef COD2X_URL_H
#define COD2X_URL_H
#include <stddef.h>

typedef struct {
    char address[256];
    char password[128];
    int hasPassword;
} Cod2xURL;

int Cod2x_ParseURL(const char *url, Cod2xURL *parsed);
int Cod2x_URLCommands(const Cod2xURL *parsed, char *commands, size_t capacity);
#endif
