#ifndef COD2X_HTTP_PARSE_H
#define COD2X_HTTP_PARSE_H

#include <stddef.h>

typedef struct {
    size_t headerLength, bodyLength;
    int status;
} Cod2xHttpParsed;

/* A complete buffered response, including chunk framing, stays resident until
   it is delivered. Returns 1 for a message, 0 for incomplete input, -1 for an
   error. Only a complete chunked body is compacted in place. */
int Cod2x_HttpParse(char *data, size_t length, int closed, Cod2xHttpParsed *parsed,
                     const char **error);

#endif
