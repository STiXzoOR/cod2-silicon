#ifndef COD2X_POLICY_H
#define COD2X_POLICY_H

#include <stddef.h>

typedef struct {
    int version, dedicated, connecting, demo, listen;
    const char *game, *names;
} Cod2xIwdSelection;

int Cod2x_IwdStock(const char *name, const char *base, int version);
int Cod2x_IwdAllowed(const char *folder, const char *file, const char *const *files,
                    size_t count, const Cod2xIwdSelection *selection);
const char *Cod2x_ConfigGame(const char *game, const char *qpath);
int Cod2x_Tokenize(const char *input, int limit, char **argv, char *out, size_t capacity);
void Cod2x_IwdSystemInfo(const char *names);
const char *Cod2x_IwdNames(void);
int Cod2x_IwdDirty(int clear);

#endif
