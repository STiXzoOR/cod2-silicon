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
#if defined(COD2_X64)
    int value = val;
    int negative = value < 0;
    int length = 0;
    if (negative)
        value = (int)(0u - (unsigned int)value);
    do {
        int digit = value % radix;
        str[length++] = (char)(digit > 9 ? digit + 'A' - 10 : digit + '0');
        value /= radix;
    } while (value != 0);
    if (negative)
        str[length++] = '-';
    str[length] = '\0';

    /* The Mac reverses the written C string, including its signed remainders. */
    length = (int)strlen(str);
    for (int first = 0, last = length - 1; first < last; ++first, --last) {
        char digit = str[first];
        str[first] = str[last];
        str[last] = digit;
    }
    return str;
#else
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
#endif
}
