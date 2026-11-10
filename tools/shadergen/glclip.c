/* OpenGL clip-space edit for the game's HLSL.
 *
 * lib/globals.hlsl gains OpenGLworldViewProjectionMatrix, declared right
 * after worldViewProjectionMatrix so register allocation keeps the order the
 * engine's constant upload expects. lib/transform.hlsl gains two helpers.
 * Each vs_main then writes its output position in one of four ways:
 *
 *   direct         position = Transform_ObjectToClip( E ) and nothing reads it
 *                  afterwards: use the OpenGL matrix on E.
 *   split          later code reads the clip position (depth feather, float
 *                  depth): those reads keep the Direct3D position and the
 *                  output uses the OpenGL matrix on E.
 *   inverse        the position is built or modified some other way: map the
 *                  final Direct3D position back to object space and forward
 *                  with the OpenGL matrix just before the last return.
 *   ifdef-inverse  inverse under one macro, direct otherwise.
 */
#include "glclip.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char ANCHOR[] = "float4x4\tworldViewProjectionMatrix : register( c0 );";
static const char GLOBAL[] = "\nfloat4x4\tOpenGLworldViewProjectionMatrix;";
static const char HELPERS[] =
    "\n\n"
    "float4 Transform_ObjectToClipGL( float4 objectSpacePosition )\n"
    "{\n"
    "\treturn mul( objectSpacePosition, OpenGLworldViewProjectionMatrix );\n"
    "}\n"
    "\n\n"
    "float4 Transform_D3DClipToGLClip( float4 clipSpacePosition )\n"
    "{\n"
    "\treturn mul( mul( clipSpacePosition, inverseWorldViewProjectionMatrix ), OpenGLworldViewProjectionMatrix );\n"
    "}\n";

static const char *const SPLIT[] = {
    "lib/z_feather.hlsl", "lib/float_z_build.hlsl", "lib/float_z_build_alphatest.hlsl", "lib/distortion.hlsl", NULL };
static const char *const INVERSE[] = { "lib/particle_cloud.hlsl", NULL };
static const char IFDEF_INVERSE_FILE[] = "lib/vertex_color_simple.hlsl";
static const char IFDEF_INVERSE_MACRO[] = "OFFSET_VERTEX_TO_EYE";
static const char SKIP_FILE[] = "null.hlsl";

typedef struct { char *p; size_t n, cap; int failed; } Buf;

static void put(Buf *b, const char *s, size_t n)
{
    if (b->failed) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = (b->n + n + 1) * 2;
        char *p = realloc(b->p, cap);
        if (!p) { b->failed = 1; return; }
        b->p = p;
        b->cap = cap;
    }
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = 0;
}

static void puts_(Buf *b, const char *s) { put(b, s, strlen(s)); }

/* Character classes as a text-mode regular expression sees Latin-1. */
static int is_space(unsigned char c)
{
    return c == ' ' || (c >= '\t' && c <= '\r') || (c >= 0x1c && c <= 0x1f) || c == 0x85 || c == 0xa0;
}

static int is_word(unsigned char c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') return 1;
    if (c < 0xaa) return 0;
    if (c == 0xaa || c == 0xb2 || c == 0xb3 || c == 0xb5 || c == 0xb9 || c == 0xba || (c >= 0xbc && c <= 0xbe)) return 1;
    return c >= 0xc0 && c != 0xd7 && c != 0xf7;
}

static int same_path(const char *a, const char *b)
{
    for (;; ++a, ++b) {
        unsigned char x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
        if (x != y) return 0;
        if (!x) return 1;
    }
}

static int listed(const char *const *list, const char *rel)
{
    for (; *list; ++list) if (same_path(*list, rel)) return 1;
    return 0;
}

static int fail(char *error, size_t error_size, const char *rel, const char *what)
{
    if (error && error_size) snprintf(error, error_size, "%s: %s", rel, what);
    return 0;
}

/* "vs_main ( ... ) {" at a word start; the body runs to the matching brace. */
static int function_body(const char *t, size_t n, const char *name, size_t *start, size_t *end, int *unbalanced)
{
    size_t len = strlen(name);
    *unbalanced = 0;
    for (size_t i = 0; i + len <= n; ++i) {
        size_t j;
        int depth = 1;
        if (memcmp(t + i, name, len) || (i && is_word((unsigned char)t[i - 1]))) continue;
        j = i + len;
        while (j < n && is_space((unsigned char)t[j])) ++j;
        if (j == n || t[j] != '(') continue;
        ++j;
        while (j < n && t[j] != ')') ++j;
        if (j == n) continue;
        ++j;
        while (j < n && is_space((unsigned char)t[j])) ++j;
        if (j == n || t[j] != '{') continue;
        *start = ++j;
        while (depth) {
            if (j == n) { *unbalanced = 1; return 0; }
            depth += t[j] == '{';
            depth -= t[j] == '}';
            ++j;
        }
        *end = j - 1;
        return 1;
    }
    return 0;
}

typedef struct {
    size_t start, end;         /* the whole assignment statement, through ';' */
    size_t var, var_len;       /* the output variable */
    size_t expr, expr_len;     /* the right-hand side, trimmed */
} Assign;

/* First "<identifier>.position = <expression>;" in the body. */
static int find_position(const char *b, size_t n, Assign *a)
{
    static const char member[] = ".position";
    size_t mlen = sizeof member - 1;
    for (size_t i = 0; i < n; ++i) {
        size_t k, p, g, q;
        if (!is_word((unsigned char)b[i]) || (i && is_word((unsigned char)b[i - 1]))) continue;
        k = i;
        while (k < n && is_word((unsigned char)b[k])) ++k;
        if (n - k < mlen || memcmp(b + k, member, mlen)) continue;
        p = k + mlen;
        while (p < n && is_space((unsigned char)b[p])) ++p;
        if (p == n || b[p] != '=') continue;
        g = ++p;
        while (g < n && is_space((unsigned char)b[g])) ++g;
        q = g;
        while (q < n && b[q] != ';') ++q;
        if (q == n || q == g) continue;
        while (g < q && is_space((unsigned char)b[g])) ++g;
        a->start = i; a->end = q + 1;
        a->var = i; a->var_len = k - i;
        a->expr = g; a->expr_len = q - g;
        while (a->expr_len && is_space((unsigned char)b[a->expr + a->expr_len - 1])) --a->expr_len;
        return 1;
    }
    return 0;
}

/* The expression must be exactly Transform_ObjectToClip( E ), E on one line. */
static int clip_argument(const char *e, size_t n, size_t *arg, size_t *arg_len)
{
    static const char call[] = "Transform_ObjectToClip(";
    size_t clen = sizeof call - 1, a, z;
    if (n <= clen || memcmp(e, call, clen) || e[n - 1] != ')') return 0;
    a = clen; z = n - 1;
    while (a < z && is_space((unsigned char)e[a])) ++a;
    while (z > a && is_space((unsigned char)e[z - 1])) --z;
    if (a == z || memchr(e + a, '\n', z - a)) return 0;
    *arg = a; *arg_len = z - a;
    return 1;
}

/* Start of the last "return" keyword in the body. */
static int last_return(const char *b, size_t n, size_t *at)
{
    int found = 0;
    for (size_t i = 0; i + 6 <= n; ++i) {
        if (memcmp(b + i, "return", 6) || (i && is_word((unsigned char)b[i - 1]))) continue;
        if (i + 6 < n && is_word((unsigned char)b[i + 6])) continue;
        *at = i;
        found = 1;
    }
    return found;
}

static char *patch_vs(const char *rel, const char *text, size_t size, size_t *out_size, char *error, size_t error_size)
{
    size_t start, end, ret, arg = 0, arg_len = 0;
    const char *body, *var, *expr;
    size_t body_len;
    int unbalanced, clip;
    Assign m;
    Buf out = {0}, nb = {0};

    if (!function_body(text, size, "vs_main", &start, &end, &unbalanced) || same_path(rel, SKIP_FILE)) {
        if (unbalanced) { fail(error, error_size, rel, "vs_main has unbalanced braces"); return NULL; }
        put(&out, text, size);
        goto finish;
    }
    body = text + start; body_len = end - start;
    if (!find_position(body, body_len, &m)) { fail(error, error_size, rel, "vs_main never assigns .position"); return NULL; }
    var = body + m.var; expr = body + m.expr;
    clip = clip_argument(expr, m.expr_len, &arg, &arg_len);
    if (!last_return(body, body_len, &ret)) { fail(error, error_size, rel, "vs_main has no return"); return NULL; }

#define VAR (int)m.var_len, var
    if (listed(INVERSE, rel)) {
        char line[512];
        put(&nb, body, ret);
        snprintf(line, sizeof line, "%.*s.position = Transform_D3DClipToGLClip( %.*s.position );\n\n\t", VAR, VAR);
        puts_(&nb, line);
        put(&nb, body + ret, body_len - ret);
    } else if (same_path(rel, IFDEF_INVERSE_FILE)) {
        char line[512];
        Buf first = {0};
        if (!clip || m.end > ret) { fail(error, error_size, rel, "position is not Transform_ObjectToClip( E )"); return NULL; }
        put(&first, body, ret);
        snprintf(line, sizeof line, "#ifdef %s\n\t%.*s.position = Transform_D3DClipToGLClip( %.*s.position );\n#endif\n\n\t",
                 IFDEF_INVERSE_MACRO, VAR, VAR);
        puts_(&first, line);
        put(&first, body + ret, body_len - ret);
        if (first.failed) { free(first.p); return NULL; }
        put(&nb, first.p, m.start);
        snprintf(line, sizeof line, "\n#ifdef %s\n\t", IFDEF_INVERSE_MACRO);
        puts_(&nb, line);
        put(&nb, body + m.start, m.end - m.start);
        snprintf(line, sizeof line, "\n#else\n\t%.*s.position = Transform_ObjectToClipGL( ", VAR);
        puts_(&nb, line);
        put(&nb, expr + arg, arg_len);
        puts_(&nb, " );\n#endif\n");
        put(&nb, first.p + m.end, first.n - m.end);
        free(first.p);
    } else {
        char line[512];
        if (!clip) { fail(error, error_size, rel, "position is not Transform_ObjectToClip( E )"); return NULL; }
        put(&nb, body, m.start);
        if (listed(SPLIT, rel)) {
            const char *after = body + m.end;
            size_t after_len = body_len - m.end, vlen = m.var_len + 9, i = 0;
            puts_(&nb, "float4 d3dClipPosition = ");
            put(&nb, expr, m.expr_len);
            snprintf(line, sizeof line, ";\n\t%.*s.position = Transform_ObjectToClipGL( ", VAR);
            puts_(&nb, line);
            put(&nb, expr + arg, arg_len);
            puts_(&nb, " );");
            while (i < after_len) {  /* later reads keep the Direct3D position */
                if (after_len - i >= vlen && !memcmp(after + i, var, m.var_len) && !memcmp(after + i + m.var_len, ".position", 9) &&
                    (i + vlen == after_len || !is_word((unsigned char)after[i + vlen]))) {
                    puts_(&nb, "d3dClipPosition");
                    i += vlen;
                } else {
                    put(&nb, after + i, 1);
                    ++i;
                }
            }
        } else {
            snprintf(line, sizeof line, "%.*s.position = Transform_ObjectToClipGL( ", VAR);
            puts_(&nb, line);
            put(&nb, expr + arg, arg_len);
            puts_(&nb, " );");
            put(&nb, body + m.end, body_len - m.end);
        }
    }
#undef VAR
    put(&out, text, start);
    put(&out, nb.p ? nb.p : "", nb.n);
    put(&out, text + end, size - end);
    if (nb.failed) out.failed = 1;
    free(nb.p);
finish:
    if (out.failed || !out.p) {
        free(out.p);
        if (!out.failed) {  /* an empty file */
            char *empty = calloc(1, 1);
            *out_size = 0;
            return empty;
        }
        fail(error, error_size, rel, "out of memory");
        return NULL;
    }
    *out_size = out.n;
    return out.p;
}

char *GlClip_Patch(const char *rel, const char *text, size_t size, size_t *out_size, char *error, size_t error_size)
{
    Buf out = {0};
    if (error && error_size) error[0] = 0;
    if (same_path(rel, "lib/globals.hlsl")) {
        size_t alen = sizeof ANCHOR - 1, i = 0;
        int found = 0;
        while (i < size) {
            if (size - i >= alen && !memcmp(text + i, ANCHOR, alen)) {
                put(&out, ANCHOR, alen);
                puts_(&out, GLOBAL);
                i += alen;
                found = 1;
            } else {
                put(&out, text + i, 1);
                ++i;
            }
        }
        if (!found) { free(out.p); fail(error, error_size, rel, "worldViewProjectionMatrix declaration not found"); return NULL; }
    } else if (same_path(rel, "lib/transform.hlsl")) {
        put(&out, text, size);
        puts_(&out, HELPERS);
    } else {
        return patch_vs(rel, text, size, out_size, error, error_size);
    }
    if (out.failed || !out.p) { free(out.p); fail(error, error_size, rel, "out of memory"); return NULL; }
    *out_size = out.n;
    return out.p;
}

size_t GlClip_NormalizeNewlines(char *text, size_t size)
{
    size_t n = 0;
    for (size_t i = 0; i < size; ++i) {
        if (text[i] == '\r') {
            text[n++] = '\n';
            if (i + 1 < size && text[i + 1] == '\n') ++i;
        } else {
            text[n++] = text[i];
        }
    }
    return n;
}
