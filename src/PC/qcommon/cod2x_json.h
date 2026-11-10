#ifndef COD2X_JSON_H
#define COD2X_JSON_H

#include <stddef.h>

/* Small JSON reader and writer for the CoD2x services (cod2x_json.c).
   Paths are "$" (the whole text), "$.key", "$.key.inner", "$.list[2].name". */

/* The value at path and its length in text, or NULL when there is none. */
const char *Cod2x_JsonFind(const char *json, const char *path, size_t *length);
/* A string value, decoded as CoD2x decodes it ("\u00XX" is the byte XX; a
   string with "\/" or a larger "\u" code can't be read); a number is written
   with no decimals and a boolean as "true" or "false". Returns 0 when there
   is none or it can't be read. */
int Cod2x_JsonString(const char *json, const char *path, char *out, size_t size);
int Cod2x_JsonNumber(const char *json, const char *path, double *out);
int Cod2x_JsonBool(const char *json, const char *path, int *out);
/* Calls item for each member of the object at path (key, value as text as
   Cod2x_JsonString gives it), or for each element of the array at path
   (key NULL; value is the raw JSON text of length valueLength). Stops when
   item returns 0. Returns 0 when there is no such object or array. */
typedef int (*Cod2xJsonItem)(void *context, int index, const char *key, const char *value, size_t valueLength);
int Cod2x_JsonEach(const char *json, const char *path, Cod2xJsonItem item, void *context);

/* A growing text buffer for building requests. */
typedef struct {
    char *data;
    size_t length, capacity;
    int failed;
} Cod2xText;

void Cod2x_TextAppend(Cod2xText *text, const char *s, size_t length);
void Cod2x_TextAppendString(Cod2xText *text, const char *s);
void Cod2x_TextPrintf(Cod2xText *text, const char *format, ...);
/* s escaped for a JSON string: quotes, backslashes and control characters,
   valid UTF-8 kept, any other byte as \u00XX (game text is Latin-1). */
void Cod2x_TextAppendJson(Cod2xText *text, const char *s, size_t length);
void Cod2x_TextFree(Cod2xText *text);

#endif
