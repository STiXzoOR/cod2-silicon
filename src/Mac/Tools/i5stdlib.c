#include <ctype.h>
#include <string.h>
#include <strings.h>
#include "common_types.h"
#include "imports.h"

int stricmp(const char *str1, const char *str2)
{
    return strcasecmp(str1, str2);
}

int strnicmp(const char *str1, const char *str2, size_t n)
{
    return strncasecmp(str1, str2, n);
}

int strcmpi(const char *str1, const char *str2)
{
    return strcasecmp(str1, str2);
}

char *strlwr(char *str)
{

    char *s = str;
    while (*s) {
        *s = tolower(*s);
        s++;
    }
    return str;
}

char *strupr(char *str)
{
    char *s = str;
    while (*s) {
        *s = toupper(*s);
        s++;
    }
    return str;
}

char *itoa(int val, char *str, int radix)
{
    char buf[34];
    int i = 0;
    int isNegative = 0;
    unsigned int uval;

    if (val < 0 && radix == 10) {
        isNegative = 1;
        uval = (unsigned int)(-val);
    } else {
        uval = (unsigned int)val;
    }

    do {
        int digit = uval % radix;
        buf[i++] = (digit > 9) ? ('A' + digit - 10) : ('0' + digit);
        uval /= radix;
    } while (uval > 0);

    if (isNegative) {
        buf[i++] = '-';
    }

    buf[i] = '\0';

    {
        int j;
        int len = i;
        for (j = 0; j < len; j++) {
            str[j] = buf[len - 1 - j];
        }
        str[len] = '\0';
    }

    return str;
}
