#include "cod2x_policy.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char cod2x_iwdNames[8192];
static int cod2x_iwdDirty;

static int equaln(const char *a, const char *b, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        if (!a[i] || !b[i])
            return a[i] == b[i];
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return 0;
    }
    return 1;
}

static int equal(const char *a, const char *b)
{
    return strlen(a) == strlen(b) && equaln(a, b, strlen(a));
}

void Cod2x_IwdSystemInfo(const char *names)
{
    cod2x_iwdDirty = 1;
    snprintf(cod2x_iwdNames, sizeof(cod2x_iwdNames), "%s", names ? names : "");
}

int Cod2x_IwdDirty(int clear)
{
    int dirty = cod2x_iwdDirty;
    if (clear) cod2x_iwdDirty = 0;
    return dirty;
}

const char *Cod2x_IwdNames(void)
{
    return cod2x_iwdNames;
}

int Cod2x_IwdStock(const char *name, const char *base, int version)
{
    char expected[256];
    int i;
    const char *local;
    for (i = 0; i < 25; ++i) {
        snprintf(expected, sizeof(expected), "%s/iw_%02d", base, i);
        if (equal(name, expected))
            return 1;
    }
    if (version >= 5) {
        snprintf(expected, sizeof(expected), "%s/iw_CoD2x_01", base);
        if (equaln(name, expected, strlen(expected)))
            return 1;
    }
    snprintf(expected, sizeof(expected), "%s/localized_", base);
    if (!equaln(name, expected, strlen(expected)))
        return 0;
    local = name + strlen(expected);
    for (i = 0; i < 25; ++i) {
        const char *p;
        snprintf(expected, sizeof(expected), "_iw%02d", i);
        for (p = local; *p; ++p)
            if (equaln(p, expected, strlen(expected)))
                return 1;
    }
    return 0;
}

static int latest(const char *file, const char *prefix, const char *const *files, size_t count)
{
    size_t i, n = strlen(prefix);
    unsigned long version;
    if (!equaln(file, prefix, n) || !isdigit((unsigned char)file[n]))
        return 0;
    version = strtoul(file + n, NULL, 10);
    for (i = 0; i < count; ++i)
        if (equaln(files[i], prefix, n) && isdigit((unsigned char)files[i][n]) &&
            strtoul(files[i] + n, NULL, 10) > version)
            return 0;
    return 1;
}

int Cod2x_IwdAllowed(const char *folder, const char *file, const char *const *files,
                    size_t count, const Cod2xIwdSelection *s)
{
    char name[256];
    size_t n = strlen(file);
    const char *p;
    if (s->dedicated)
        return 1;
    if (n >= sizeof(name) - 1)
        return 0;
    snprintf(name, sizeof(name), "/%s", file);
    if (n > 4 && equal(file + n - 4, ".iwd"))
        name[n - 3] = '\0';
    if (Cod2x_IwdStock(name, "", s->connecting && !s->listen ? s->version : 6))
        return 1;
    if (s->connecting && !s->listen) {
        if (s->demo && equal(folder, "movie"))
            return 1;
        p = s->names ? s->names : "";
        while (*p) {
            const char *start;
            size_t len;
            while (*p && isspace((unsigned char)*p)) ++p;
            start = p;
            while (*p && !isspace((unsigned char)*p)) ++p;
            len = (size_t)(p - start);
            if (len && equaln(file, start, len))
                return 1;
        }
    }
    if (s->listen && (latest(file, "zpam", files, count) ||
                      latest(file, "zpam_maps_v", files, count)))
        return 1;
    return s->game && *s->game && equal(folder, s->game);
}

const char *Cod2x_ConfigGame(const char *game, const char *qpath)
{
    size_t n = strlen(qpath);
    if (equal(qpath, "config_mp.cfg") ||
        (equaln(qpath, "players/", 8) && n >= 14 && equal(qpath + n - 14, "/config_mp.cfg")))
        return "main";
    if (game && equal(game, "movie") && !(n >= 4 && equal(qpath + n - 4, ".iwd")))
        return "main";
    return game;
}

/* Separate from Cbuf: separators are still handled by the engine command buffer.
   URLs retain // after a colon; comments retain their ordinary meaning. */
int Cod2x_Tokenize(const char *p, int limit, char **argv, char *out, size_t capacity)
{
    int argc = 0;
    size_t used = 0;
    if (!p || !capacity)
        return 0;
    if (limit <= 0 || limit > 512)
        limit = 512;
    while (*p) {
        const char *start;
        int quoted;
        while (*p && (unsigned char)*p <= ' ') ++p;
        if (!*p || (p[0] == '/' && p[1] == '/')) break;
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) ++p;
            if (*p) p += 2;
            continue;
        }
        argv[argc++] = out + used;
        if (argc == limit) {
            size_t n = strlen(p) + 1;
            if (n > capacity - used) return -1;
            memcpy(out + used, p, n);
            return argc;
        }
        quoted = *p == '"';
        if (quoted) ++p;
        start = p;
        while (*p) {
            char c = *p;
            if (quoted && c == '\\' && p[1] == '"') {
                c = '"';
                ++p;
            } else if (c == '"' || (!quoted && ((unsigned char)c <= ' ' ||
                       (c == '/' && (p[1] == '*' || (p[1] == '/' &&
                        (p == start || p[-1] != ':'))))))) {
                break;
            }
            if (used + 1 >= capacity) return -1;
            out[used++] = c;
            ++p;
        }
        if (used == capacity) return -1;
        out[used++] = '\0';
        if (quoted && *p == '"') ++p;
    }
    return argc;
}
