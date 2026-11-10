#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *Json_Space(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        ++p;
    return p;
}

static int Json_Hex(const char *p, unsigned int *out)
{
    unsigned int value = 0;
    for (int i = 0; i < 4; ++i) {
        char c = p[i];
        value <<= 4;
        if (c >= '0' && c <= '9')
            value |= (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            value |= (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            value |= (unsigned int)(c - 'A' + 10);
        else
            return 0;
    }
    *out = value;
    return 1;
}

static void Json_Put(char *out, size_t size, size_t *used, char c)
{
    if (out && *used + 1 < size)
        out[*used] = c;
    ++*used;
}

/* Decodes the string at p (at its quote) into out; returns the text after it, NULL when broken. */
static const char *Json_String(const char *p, char *out, size_t size, size_t *written)
{
    size_t used = 0;
    int decode = out != NULL; /* only skipping: any valid escape will do */
    if (*p != '"')
        return NULL;
    for (++p; *p != '"'; ++p) {
        unsigned int code;
        if (!*p || (unsigned char)*p < 0x20)
            return NULL;
        if (*p != '\\') {
            Json_Put(out, size, &used, *p);
            continue;
        }
        switch (*++p) {
        /* CoD2x's reader (mongoose) knows no "\/" and reads "\u00XX" as the
           byte XX (game text is Latin-1); a string with a larger \u code or
           "\/" can't be read at all there. */
        case '"': case '\\': Json_Put(out, size, &used, *p); break;
        case '/':
            if (decode)
                return NULL;
            break;
        case 'b': Json_Put(out, size, &used, '\b'); break;
        case 'f': Json_Put(out, size, &used, '\f'); break;
        case 'n': Json_Put(out, size, &used, '\n'); break;
        case 'r': Json_Put(out, size, &used, '\r'); break;
        case 't': Json_Put(out, size, &used, '\t'); break;
        case 'u':
            if (!Json_Hex(p + 1, &code) || (decode && code > 0xff))
                return NULL;
            p += 4;
            Json_Put(out, size, &used, (char)code);
            break;
        default:
            return NULL;
        }
    }
    if (out && size)
        out[used < size ? used : size - 1] = '\0';
    if (written)
        *written = used;
    return p + 1;
}

/* The text after the value at p, NULL when it is broken. */
static const char *Json_Skip(const char *p)
{
    p = Json_Space(p);
    if (*p == '"')
        return Json_String(p, NULL, 0, NULL);
    if (*p == '{' || *p == '[') {
        char close = *p == '{' ? '}' : ']';
        p = Json_Space(p + 1);
        if (*p == close)
            return p + 1;
        for (;;) {
            if (close == '}') {
                p = Json_String(Json_Space(p), NULL, 0, NULL);
                if (!p)
                    return NULL;
                p = Json_Space(p);
                if (*p++ != ':')
                    return NULL;
            }
            p = Json_Skip(p);
            if (!p)
                return NULL;
            p = Json_Space(p);
            if (*p == close)
                return p + 1;
            if (*p++ != ',')
                return NULL;
        }
    }
    if (!strncmp(p, "true", 4))
        return p + 4;
    if (!strncmp(p, "false", 5))
        return p + 5;
    if (!strncmp(p, "null", 4))
        return p + 4;
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        char *end;
        strtod(p, &end);
        return end > p ? end : NULL;
    }
    return NULL;
}

/* The member key of the object at p, or NULL. */
static const char *Json_Member(const char *p, const char *key, size_t keyLength)
{
    p = Json_Space(p);
    if (*p != '{')
        return NULL;
    p = Json_Space(p + 1);
    if (*p == '}')
        return NULL;
    for (;;) {
        char name[256];
        size_t nameLength;
        p = Json_String(Json_Space(p), name, sizeof(name), &nameLength);
        if (!p)
            return NULL;
        p = Json_Space(p);
        if (*p++ != ':')
            return NULL;
        p = Json_Space(p);
        if (nameLength == keyLength && nameLength < sizeof(name) && !memcmp(name, key, keyLength))
            return p;
        p = Json_Skip(p);
        if (!p)
            return NULL;
        p = Json_Space(p);
        if (*p != ',')
            return NULL;
        ++p;
    }
}

static const char *Json_Element(const char *p, long index)
{
    p = Json_Space(p);
    if (*p != '[' || index < 0)
        return NULL;
    p = Json_Space(p + 1);
    if (*p == ']')
        return NULL;
    for (long i = 0;; ++i) {
        p = Json_Space(p);
        if (i == index)
            return p;
        p = Json_Skip(p);
        if (!p)
            return NULL;
        p = Json_Space(p);
        if (*p != ',')
            return NULL;
        ++p;
    }
}

const char *Cod2x_JsonFind(const char *json, const char *path, size_t *length)
{
    const char *p = json ? Json_Space(json) : NULL;
    const char *end;
    if (!p || !path || *path != '$')
        return NULL;
    ++path;
    while (*path && p) {
        if (*path == '.') {
            size_t keyLength = strcspn(path + 1, ".[");
            p = Json_Member(p, path + 1, keyLength);
            path += 1 + keyLength;
        } else if (*path == '[') {
            char *close;
            long index = strtol(path + 1, &close, 10);
            if (*close != ']')
                return NULL;
            p = Json_Element(p, index);
            path = close + 1;
        } else {
            return NULL;
        }
    }
    if (!p)
        return NULL;
    end = Json_Skip(p);
    if (!end)
        return NULL;
    if (length)
        *length = (size_t)(end - p);
    return p;
}

/* A value as text: strings decoded, numbers with no decimals, booleans as words. */
static int Json_ValueText(const char *value, char *out, size_t size)
{
    if (!size)
        return 0;
    if (*value == '"')
        return Json_String(value, out, size, NULL) != NULL;
    if (*value == '-' || (*value >= '0' && *value <= '9')) {
        snprintf(out, size, "%.0f", strtod(value, NULL));
        return 1;
    }
    if (!strncmp(value, "true", 4) || !strncmp(value, "false", 5)) {
        snprintf(out, size, "%s", *value == 't' ? "true" : "false");
        return 1;
    }
    return 0;
}

int Cod2x_JsonString(const char *json, const char *path, char *out, size_t size)
{
    const char *value = Cod2x_JsonFind(json, path, NULL);
    return value && Json_ValueText(value, out, size);
}

int Cod2x_JsonNumber(const char *json, const char *path, double *out)
{
    const char *value = Cod2x_JsonFind(json, path, NULL);
    if (!value || !(*value == '-' || (*value >= '0' && *value <= '9')))
        return 0;
    *out = strtod(value, NULL);
    return 1;
}

int Cod2x_JsonBool(const char *json, const char *path, int *out)
{
    const char *value = Cod2x_JsonFind(json, path, NULL);
    if (!value || (strncmp(value, "true", 4) && strncmp(value, "false", 5)))
        return 0;
    *out = *value == 't';
    return 1;
}

int Cod2x_JsonEach(const char *json, const char *path, Cod2xJsonItem item, void *context)
{
    const char *p = Cod2x_JsonFind(json, path, NULL);
    int object, index = 0;
    if (!p || (*p != '{' && *p != '['))
        return 0;
    object = *p == '{';
    p = Json_Space(p + 1);
    if (*p == (object ? '}' : ']'))
        return 1;
    for (;; ++index) {
        const char *value, *end;
        char key[256];
        if (object) {
            p = Json_String(Json_Space(p), key, sizeof(key), NULL);
            if (!p)
                return 0;
            p = Json_Space(p);
            if (*p++ != ':')
                return 0;
        }
        value = Json_Space(p);
        end = Json_Skip(value);
        if (!end)
            return 0;
        if (object) {
            size_t length = (size_t)(end - value);
            char *text = malloc(length + 1);
            int keep;
            if (!text)
                return 0;
            /* A string CoD2x can't decode comes as its JSON text, quotes and all. */
            if (*value != '"' || !Json_String(value, text, length + 1, NULL)) {
                memcpy(text, value, length);
                text[length] = '\0';
            }
            keep = item(context, index, key, text, strlen(text));
            free(text);
            if (!keep)
                return 1;
        } else if (!item(context, index, NULL, value, (size_t)(end - value))) {
            return 1;
        }
        p = Json_Space(end);
        if (*p == (object ? '}' : ']'))
            return 1;
        if (*p++ != ',')
            return 0;
    }
}

void Cod2x_TextAppend(Cod2xText *text, const char *s, size_t length)
{
    if (text->failed)
        return;
    if (text->length + length + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity : 256;
        char *data;
        while (capacity < text->length + length + 1)
            capacity *= 2;
        data = realloc(text->data, capacity);
        if (!data) {
            text->failed = 1;
            return;
        }
        text->data = data;
        text->capacity = capacity;
    }
    memcpy(text->data + text->length, s, length);
    text->length += length;
    text->data[text->length] = '\0';
}

void Cod2x_TextAppendString(Cod2xText *text, const char *s)
{
    Cod2x_TextAppend(text, s, strlen(s));
}

void Cod2x_TextPrintf(Cod2xText *text, const char *format, ...)
{
    char buffer[1024];
    va_list args;
    int length;
    va_start(args, format);
    length = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length < 0)
        return;
    if ((size_t)length < sizeof(buffer)) {
        Cod2x_TextAppend(text, buffer, (size_t)length);
    } else {
        char *big = malloc((size_t)length + 1);
        if (!big) {
            text->failed = 1;
            return;
        }
        va_start(args, format);
        vsnprintf(big, (size_t)length + 1, format, args);
        va_end(args);
        Cod2x_TextAppend(text, big, (size_t)length);
        free(big);
    }
}

/* Length of a well-formed UTF-8 sequence at s, 0 when it is not one. */
static size_t Json_Utf8(const unsigned char *s, size_t left)
{
    unsigned int code;
    size_t length;
    if (s[0] >= 0xc2 && s[0] <= 0xdf) {
        length = 2;
        code = s[0] & 0x1f;
    } else if (s[0] >= 0xe0 && s[0] <= 0xef) {
        length = 3;
        code = s[0] & 0x0f;
    } else if (s[0] >= 0xf0 && s[0] <= 0xf4) {
        length = 4;
        code = s[0] & 0x07;
    } else {
        return 0;
    }
    if (length > left)
        return 0;
    for (size_t i = 1; i < length; ++i) {
        if ((s[i] & 0xc0) != 0x80)
            return 0;
        code = (code << 6) | (s[i] & 0x3f);
    }
    if ((length == 3 && code < 0x800) || (length == 4 && code < 0x10000) || (code >= 0xd800 && code <= 0xdfff) ||
        code > 0x10ffff)
        return 0;
    return length;
}

void Cod2x_TextAppendJson(Cod2xText *text, const char *s, size_t length)
{
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = p[i];
        size_t sequence;
        char escape[8];
        switch (c) {
        case '"': Cod2x_TextAppend(text, "\\\"", 2); continue;
        case '\\': Cod2x_TextAppend(text, "\\\\", 2); continue;
        case '\b': Cod2x_TextAppend(text, "\\b", 2); continue;
        case '\f': Cod2x_TextAppend(text, "\\f", 2); continue;
        case '\n': Cod2x_TextAppend(text, "\\n", 2); continue;
        case '\r': Cod2x_TextAppend(text, "\\r", 2); continue;
        case '\t': Cod2x_TextAppend(text, "\\t", 2); continue;
        default: break;
        }
        if (c >= 0x20 && c < 0x80) {
            Cod2x_TextAppend(text, (const char *)&p[i], 1);
            continue;
        }
        sequence = c >= 0x80 ? Json_Utf8(p + i, length - i) : 0;
        if (sequence) {
            Cod2x_TextAppend(text, (const char *)&p[i], sequence);
            i += sequence - 1;
            continue;
        }
        snprintf(escape, sizeof(escape), "\\u%04x", (unsigned int)c);
        Cod2x_TextAppend(text, escape, 6);
    }
}

void Cod2x_TextFree(Cod2xText *text)
{
    free(text->data);
    memset(text, 0, sizeof(*text));
}
#endif
