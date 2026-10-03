#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_url.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static int HexDigit(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int SafeAddress(const char *address)
{
    const char *colon = strchr(address, ':');
    size_t hostLength = colon ? (size_t)(colon - address) : strlen(address);
    if (!hostLength || hostLength > 253)
        return 0;
    for (size_t i = 0; i < hostLength; ++i) {
        unsigned char c = address[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '.'))
            return 0;
    }
    if (colon) {
        unsigned int port = 0;
        if (!colon[1]) return 0;
        for (const char *p = colon + 1; *p; ++p) {
            if (*p < '0' || *p > '9') return 0;
            port = port * 10 + (unsigned int)(*p - '0');
            if (port > 65535) return 0;
        }
        if (!port) return 0;
    }
    return 1;
}

static int SafePassword(const char *password)
{
    for (const unsigned char *p = (const unsigned char *)password; *p; ++p) {
        if (*p < 0x20 || *p >= 0x7f || *p == '"' || *p == ';' || *p == '\\')
            return 0;
    }
    return 1;
}

static int ReadToken(char **cursor, char *token, size_t capacity)
{
    char *p = *cursor;
    size_t length = 0;
    int quoted;
    while (*p == ' ') ++p;
    quoted = *p == '"';
    if (quoted) ++p;
    while (*p && (quoted ? *p != '"' : *p != ' ')) {
        if (length + 1 >= capacity) return 0;
        token[length++] = *p++;
    }
    if (quoted) {
        if (*p++ != '"' || (*p && *p != ' ')) return 0;
    }
    token[length] = 0;
    *cursor = p;
    return length != 0 || quoted;
}

int Cod2x_ParseURL(const char *url, Cod2xURL *parsed)
{
    char decoded[1024];
    char *cursor;
    Cod2xURL result = {0};
    size_t length = 0;
    int connected = 0;
    if (!url || !parsed || strncasecmp(url, "cod2x://", 8))
        return 0;
    for (const unsigned char *p = (const unsigned char *)url + 8; *p; ++p) {
        unsigned char c = *p;
        if (c == '%') {
            int high, low;
            if (!p[1] || !p[2] || (high = HexDigit(p[1])) < 0 || (low = HexDigit(p[2])) < 0)
                return 0;
            c = (unsigned char)((high << 4) | low);
            p += 2;
        } else if (c == '+') {
            c = ' ';
        }
        if (c < 0x20 || c >= 0x7f || c == ';' || c == '\\' || length + 1 >= sizeof(decoded))
            return 0;
        decoded[length++] = c;
    }
    if (length && decoded[length - 1] == '/') --length;
    decoded[length] = 0;
    cursor = decoded;
    while (*cursor) {
        char command[32], value[256];
        char *name;
        while (*cursor == ' ') ++cursor;
        if (!*cursor) break;
        if (!ReadToken(&cursor, command, sizeof(command)) || !ReadToken(&cursor, value, sizeof(value)))
            return 0;
        name = command + (command[0] == '+');
        if (!strcmp(name, "connect")) {
            if (connected || !SafeAddress(value)) return 0;
            strcpy(result.address, value);
            connected = 1;
        } else if (!strcmp(name, "password")) {
            if (result.hasPassword || strlen(value) >= sizeof(result.password) || !SafePassword(value)) return 0;
            strcpy(result.password, value);
            result.hasPassword = 1;
        } else {
            return 0;
        }
    }
    if (!connected) return 0;
    *parsed = result;
    return 1;
}

int Cod2x_URLCommands(const Cod2xURL *parsed, char *commands, size_t capacity)
{
    int length;
    if (!parsed || !commands || !capacity ||
        !memchr(parsed->address, 0, sizeof(parsed->address)) ||
        !memchr(parsed->password, 0, sizeof(parsed->password)) ||
        !SafeAddress(parsed->address) || !SafePassword(parsed->password))
        return 0;
    length = snprintf(commands, capacity, "password \"%s\"\nconnect %s\n",
                      parsed->hasPassword ? parsed->password : "", parsed->address);
    if (length < 0 || (size_t)length >= capacity) {
        commands[0] = 0;
        return 0;
    }
    return 1;
}
#endif
