#include "common_types.h"
#include "imports.h"
#include "bytematch.h"
#include <string.h>
#include <time.h>

extern int toupper(int c);

extern void Com_Error(int code, const char *fmt, ...);

int Com_HashKey(char *string, int maxlen)
{
    int hash, i;

    hash = 0;
    for (i = 0; i < maxlen && string[i] != '\0'; i++) {
        hash += (signed char)string[i] * (i + 119);
    }
    return hash ^ (hash >> 10) ^ (hash >> 20);
}

int Com_RealTime(qtime_t *qtime)
{
    time_t t;
    struct tm *tms;

    t = time(NULL);
    if (qtime) {
        tms = localtime(&t);
        if (tms) {
            qtime->tm_sec = tms->tm_sec;
            qtime->tm_min = tms->tm_min;
            qtime->tm_hour = tms->tm_hour;
            qtime->tm_mday = tms->tm_mday;
            qtime->tm_mon = tms->tm_mon;
            qtime->tm_year = tms->tm_year;
            qtime->tm_wday = tms->tm_wday;
            qtime->tm_yday = tms->tm_yday;
            qtime->tm_isdst = tms->tm_isdst;
        }
    }
    return (int)t;
}

void Com_Memcpy(void *dest, const void *src, int count)
{

    memcpy(dest, src, count);
}

void Com_Memset(void *dest, const int val, int count)
{
    memset(dest, val, count);
}

Bool Com_Filter(const char *filter, const char *name, int casesensitive)
{
    char buf[1024];
    int i, found;
    int namelen, buflen, maxpos;

    while (1) {
        switch (*filter) {
        case '\0':
            return 1;

        case '*':
            filter++;

            i = 0;
            while (*filter && *filter != '*' && *filter != '?') {
                buf[i++] = *filter++;
            }
            buf[i] = '\0';

            if (buf[0] == '\0')
                continue;

            namelen = strlen(name);
            buflen = strlen(buf);
            maxpos = namelen - buflen;
            if (maxpos < 0)
                return 0;

            for (i = 0; i <= maxpos; i++) {
                if (casesensitive) {
                    if (!strncmp(name, buf, buflen))
                        break;
                } else {
                    if (!strncasecmp(name, buf, buflen))
                        break;
                }
                name++;
            }
            if (i > maxpos)
                return 0;
            name += buflen;
            continue;

        case '?':
            filter++;
            name++;
            continue;

        case '[':
            filter++;
            if (*filter == '[') {

                continue;
            }
            found = 0;
            while (*filter) {
                if (found) {

                    if (*filter == ']') {
                        filter++;
                        if (*filter == ']')
                            continue;
                        name++;
                        break;
                    }
                    filter++;
                    continue;
                }
                if (*filter == ']') {

                    if (filter[1] == ']') {

                        return 0;
                    }
                    if (filter[1] == '-')
                        goto check_range;
                    goto check_single;
                }
                if (filter[1] == '-') {
                check_range:;
                    char range_end = filter[2];
                    if (range_end == '\0' || range_end == ']') {
                        goto check_single;
                    }
                    if (casesensitive) {
                        if (*name >= *filter && *name <= range_end)
                            found = 1;
                    } else {
                        if (toupper(*name) >= toupper(*filter) && toupper(*name) <= toupper(range_end))
                            found = 1;
                    }
                    filter += 3;
                    continue;
                }
            check_single:
                if (casesensitive) {
                    if (*filter == *name)
                        found = 1;
                } else {
                    if (toupper(*filter) == toupper(*name))
                        found = 1;
                }
                filter++;
                continue;
            }
            if (found) {
                filter++;
                name++;
                continue;
            }
            return 0;

        default:

            if (casesensitive) {
                if (*filter != *name)
                    return 0;
            } else {
                if (toupper(*filter) != toupper(*name))
                    return 0;
            }
            filter++;
            name++;
            continue;
        }
    }
}

Bool Com_FilterPath(const char *filter, const char *name, int casesensitive)
{
    char new_filter[64];
    char new_name[64];
    int i;

    for (i = 0; i < 63 && filter[i]; i++) {
        if (filter[i] == '\\' || filter[i] == ':')
            new_filter[i] = '/';
        else
            new_filter[i] = filter[i];
    }
    new_filter[i] = '\0';

    for (i = 0; i < 63 && name[i]; i++) {
        if (name[i] == '\\' || name[i] == ':')
            new_name[i] = '/';
        else
            new_name[i] = name[i];
    }
    new_name[i] = '\0';

    return Com_Filter(new_filter, new_name, casesensitive);
}
