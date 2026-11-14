#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_http_parse.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* This parser preserves the response grammar used by the CoD2x services.
   curl supplies the connection and TLS, without imposing its HTTP limits. */
typedef struct {
    const char *text;
    size_t length;
} HttpPart;

static int Http_Equal(HttpPart part, const char *word)
{
    if (part.length != strlen(word))
        return 0;
    for (size_t i = 0; i < part.length; ++i) {
        unsigned char a = (unsigned char)part.text[i], b = (unsigned char)word[i];
        if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
        if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
        if (a != b) return 0;
    }
    return 1;
}

/* Header names and the first two start-line fields accept printable ASCII
   and UTF-8 sequences, including the overlong forms accepted by the peer. */
static size_t Http_Character(const char *p, const char *end)
{
    unsigned char c = (unsigned char)*p;
    size_t count;
    if (c > 32 && c < 127) return 1;
    if ((c & 0xe0) == 0xc0) count = 2;
    else if ((c & 0xf0) == 0xe0) count = 3;
    else if ((c & 0xf8) == 0xf0) count = 4;
    else return 0;
    if ((size_t)(end - p) < count) return 0;
    for (size_t i = 1; i < count; ++i)
        if (((unsigned char)p[i] & 0xc0) != 0x80) return 0;
    return count;
}

static HttpPart Http_Token(const char **cursor, const char *end)
{
    HttpPart part = {*cursor, 0};
    size_t count;
    while (*cursor < end && (count = Http_Character(*cursor, end)) != 0) {
        *cursor += count;
        part.length += count;
    }
    while (*cursor < end && **cursor == ' ') ++*cursor;
    return part;
}

static int Http_Line(const char **cursor, const char *end, HttpPart *part)
{
    const char *p = *cursor;
    part->text = p;
    while (p < end && *p != '\r' && *p != '\n') ++p;
    part->length = (size_t)(p - part->text);
    if (p < end && *p == '\r') ++p;
    if (p == end || *p != '\n') return 0;
    *cursor = p + 1;
    return 1;
}

static int Http_Decimal(HttpPart part, size_t *value)
{
    size_t result = 0;
    if (!part.length) return 0;
    for (size_t i = 0; i < part.length; ++i) {
        unsigned char c = (unsigned char)part.text[i];
        if (c < '0' || c > '9' || result > (SIZE_MAX - (c - '0')) / 10)
            return 0;
        result = result * 10 + (c - '0');
    }
    *value = result;
    return 1;
}

/* First locate a complete sequence of chunks; keep the wire representation
   intact while it is incomplete so the receive ceiling includes framing. */
static int Http_Chunks(char *body, size_t available, size_t *decoded, int compact)
{
    size_t pos = 0, output = 0;
    for (;;) {
        size_t digits = 0, chunk = 0, start;
        if (available - pos < 3) return 0;
        while (pos + digits < available) {
            unsigned char c = (unsigned char)body[pos + digits];
            unsigned int value;
            if (c >= '0' && c <= '9') value = c - '0';
            else if (c >= 'a' && c <= 'f') value = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') value = c - 'A' + 10;
            else break;
            if (++digits > 8) return -1;
            chunk = chunk * 16 + value;
        }
        if (!digits || chunk > INT_MAX || available - pos - digits < 2 ||
            body[pos + digits] != '\r' || body[pos + digits + 1] != '\n')
            return -1;
        start = pos + digits + 2;
        if (available - start < 2 || chunk > available - start - 2) return 0;
        if (body[start + chunk] != '\r' || body[start + chunk + 1] != '\n') return -1;
        if (compact && chunk) memmove(body + output, body + start, chunk);
        output += chunk;
        pos = start + chunk + 2;
        if (!chunk) {
            *decoded = output;
            return 1;
        }
    }
}

int Cod2x_HttpParse(char *data, size_t length, int closed, Cod2xHttpParsed *parsed,
                     const char **error)
{
    const char *cursor, *end;
    HttpPart first, status, reason, contentLength = {0}, transfer = {0};
    size_t headerLength = 0, bodyLength = SIZE_MAX;
    int response, chunked = 0;
    *error = "Connection closed unexpectedly";
    memset(parsed, 0, sizeof(*parsed));
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)data[i];
        if (c < 32 && c != '\r' && c != '\n' && c != '\t') return -1;
        if (c == '\n' && ((i && data[i - 1] == '\n') ||
                         (i > 3 && data[i - 1] == '\r' && data[i - 2] == '\n'))) {
            headerLength = i + 1;
            break;
        }
    }
    if (!headerLength) return 0;
    cursor = data;
    end = data + headerLength;
    first = Http_Token(&cursor, end);
    status = Http_Token(&cursor, end);
    if (!Http_Line(&cursor, end, &reason)) return 0;
    response = first.length > 5 && Http_Equal((HttpPart){first.text, 5}, "HTTP/");
    if (!response && reason.length &&
        (reason.length != 8 || !Http_Equal((HttpPart){reason.text, 5}, "HTTP/") ||
         reason.text[5] < '0' || reason.text[5] > '9' || reason.text[6] != '.' ||
         reason.text[7] < '0' || reason.text[7] > '9')) return -1;
    {
        const char *query = memchr(status.text, '?', status.length);
        if (query) status.length = (size_t)(query - status.text);
    }
    if (!first.length || !status.length) return -1;
    /* Only the first thirty header fields belong to the response map. Later
       lines remain buffered but do not affect framing or validation. */
    for (int field = 0; field < 30; ++field) {
        HttpPart name = {cursor, 0}, value;
        size_t width;
        if (cursor == end) return -1;
        if (*cursor == '\n' || (*cursor == '\r' && cursor + 1 < end && cursor[1] == '\n')) break;
        while (cursor < end && *cursor != ':' && (width = Http_Character(cursor, end)) != 0) {
            cursor += width;
            name.length += width;
        }
        if (!name.length || cursor == end || *cursor != ':') return -1;
        ++cursor;
        while (cursor < end && (*cursor == ' ' || *cursor == '\t')) ++cursor;
        if (!Http_Line(&cursor, end, &value)) return -1;
        while (value.length && (value.text[value.length - 1] == ' ' || value.text[value.length - 1] == '\t'))
            --value.length;
        if (!contentLength.text && Http_Equal(name, "Content-Length")) contentLength = value;
        if (!transfer.text && Http_Equal(name, "Transfer-Encoding")) transfer = value;
    }
    if (contentLength.text && (!Http_Decimal(contentLength, &bodyLength) || bodyLength > SIZE_MAX - headerLength))
        return -1;
    if (!contentLength.text && ((!response && !Http_Equal(first, "POST") && !Http_Equal(first, "PUT")) ||
                                (response && Http_Equal(status, "204"))))
        bodyLength = 0;
    if (closed) bodyLength = length - headerLength;
    if (transfer.text) {
        if (!Http_Equal(transfer, "chunked")) {
            *error = "Invalid Transfer-Encoding";
            return -1;
        }
        chunked = 1;
    }
    if (chunked) {
        int result = Http_Chunks(data + headerLength, length - headerLength, &bodyLength, 0);
        if (result < 0) *error = "Invalid chunk";
        if (result <= 0) return result;
        Http_Chunks(data + headerLength, length - headerLength, &bodyLength, 1);
    } else if (bodyLength > length - headerLength) {
        return 0;
    }
    parsed->headerLength = headerLength;
    parsed->bodyLength = bodyLength;
    parsed->status = atoi(status.text);
    *error = NULL;
    return 1;
}
#endif
