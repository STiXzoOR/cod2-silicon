/* Direct3D 9 shader model 2/3 bytecode to ARB vertex/fragment program text.
 *
 * The output follows the layout the engine's OpenGL path expects: constants
 * come from program.env at their Direct3D register numbers, vertex inputs
 * arrive on the generic attributes the draw code binds, and pixel programs
 * end with the fixed-function fog blend unless the shader is a full-screen
 * or shadow pass. */
#include "sm2arb.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REG 2048

typedef struct Block { struct Block *next; } Block;
typedef struct { char **v; size_t n, cap; } Lines;
typedef struct { unsigned op; uint32_t tok; const uint32_t *args; unsigned nargs; } Ins;
typedef struct { const char *name; unsigned set, reg, cnt, cls, type, elems; } Const;

typedef struct {
    Block *blocks;
    jmp_buf fail;
    char *error;
    size_t error_size;
    const char *name;
    int ps;
    unsigned major;
    Const *consts;
    size_t nconsts;
    unsigned array_base[MAX_REG], array_cnt[MAX_REG];
    Lines defs, pre;
    int nscratch;
    int vsin[MAX_REG];                     /* register -> generic attribute, -1 if undeclared */
    struct { const char *key, *value; } psin[64];
    size_t npsin;
    const char *samplers[MAX_REG];
    unsigned char texout[MAX_REG];
} T;

static void fail(T *t, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (t->error && t->error_size) vsnprintf(t->error, t->error_size, fmt, ap);
    va_end(ap);
    longjmp(t->fail, 1);
}

static void *alloc(T *t, size_t size)
{
    Block *b = malloc(sizeof *b + size);
    if (!b) fail(t, "out of memory");
    b->next = t->blocks;
    t->blocks = b;
    return b + 1;
}

static char *str(T *t, const char *fmt, ...)
{
    va_list ap, copy;
    int n;
    char *s;
    va_start(ap, fmt);
    va_copy(copy, ap);
    n = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    s = alloc(t, (size_t)n + 1);
    vsnprintf(s, (size_t)n + 1, fmt, ap);
    va_end(ap);
    return s;
}

static void add(T *t, Lines *l, const char *line)
{
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap * 2 : 64;
        char **v = alloc(t, cap * sizeof *v);
        if (l->n) memcpy(v, l->v, l->n * sizeof *v);
        l->v = v;
        l->cap = cap;
    }
    l->v[l->n++] = (char *)line;
}

static void extend(T *t, Lines *l, const Lines *from)
{
    for (size_t i = 0; i < from->n; ++i) add(t, l, from->v[i]);
}

static uint32_t rd32(const unsigned char *p)
{
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static unsigned rd16(const unsigned char *p)
{
    return p[0] | (unsigned)p[1] << 8;
}

static const char *opname(unsigned op)
{
    switch (op) {
    case 0: return "nop";      case 1: return "mov";     case 2: return "add";    case 3: return "sub";
    case 4: return "mad";      case 5: return "mul";     case 6: return "rcp";    case 7: return "rsq";
    case 8: return "dp3";      case 9: return "dp4";     case 10: return "min";   case 11: return "max";
    case 12: return "slt";     case 13: return "sge";    case 14: return "exp";   case 15: return "log";
    case 16: return "lit";     case 17: return "dst";    case 18: return "lrp";   case 19: return "frc";
    case 20: return "m4x4";    case 21: return "m4x3";   case 22: return "m3x4";  case 23: return "m3x3";
    case 24: return "m3x2";    case 31: return "dcl";    case 32: return "pow";   case 33: return "crs";
    case 34: return "sgn";     case 35: return "abs";    case 36: return "nrm";   case 37: return "sincos";
    case 38: return "rep";     case 39: return "endrep"; case 46: return "mova";  case 47: return "defb";
    case 48: return "defi";    case 65: return "texkill"; case 66: return "texld"; case 78: return "expp";
    case 79: return "logp";    case 81: return "def";    case 88: return "cmp";   case 90: return "dp2add";
    case 95: return "texldl";
    }
    return NULL;
}

static int is(const char *a, const char *b) { return !strcmp(a, b); }

static const char *arbname(T *t, const char *name)
{
    char *s;
    if (is(name, "exp")) return "EX2";
    if (is(name, "expp")) return "EXP";
    if (is(name, "log")) return "LG2";
    if (is(name, "logp")) return "LOG";
    if (is(name, "mova")) return "ARL";
    if (is(name, "crs")) return "XPD";
    s = str(t, "%s", name);
    for (char *p = s; *p; ++p) if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
    return s;
}

static unsigned regtype(uint32_t tok) { return ((tok >> 28) & 7) | ((tok >> 8) & 0x18); }

static const char *reg(T *t, uint32_t tok)
{
    static const char *const rast[] = { "oPos", "oFog", "oPts" };
    unsigned type = regtype(tok), n = tok & 0x7ff;
    if (tok & 0x2000) fail(t, "relative addressing");
    switch (type) {
    case 0: return str(t, "r%u", n);
    case 1: return str(t, "v%u", n);
    case 2:
        if (t->array_cnt[n] > 1) return str(t, "c%u[%u]", t->array_base[n], n - t->array_base[n]);
        return str(t, "c%u", n);
    case 3: return t->ps ? str(t, "t%u", n) : "a0";
    case 4:
        if (n > 2) fail(t, "rasterizer output %u", n);
        return rast[n];
    case 5: return str(t, "oD%u", n);
    case 6:
        if (t->major >= 3 && !t->ps) fail(t, "vs_3_0 outputs");
        t->texout[n] = 1;
        return str(t, "oT%u", n);
    case 8: return str(t, "oC%u", n);
    case 10: return str(t, "s%u", n);
    }
    fail(t, "register type %u", type);
    return NULL;
}

static const char *mask(T *t, uint32_t tok)
{
    unsigned m = (tok >> 16) & 0xf, n = 0;
    char s[6];
    if (m == 0xf) return "";
    s[n++] = '.';
    for (unsigned b = 0; b < 4; ++b) if (m & (1u << b)) s[n++] = "xyzw"[b];
    s[n] = 0;
    return str(t, "%s", s);
}

static const char *swz(T *t, uint32_t tok)
{
    unsigned s = (tok >> 16) & 0xff;
    char c[5];
    for (unsigned k = 0; k < 4; ++k) c[k] = "xyzw"[(s >> (2 * k)) & 3];
    c[4] = 0;
    if (is(c, "xyzw")) return "";
    if (c[0] == c[1] && c[1] == c[2] && c[2] == c[3]) return str(t, ".%c", c[0]);
    return str(t, ".%s", c);
}

static const char *scratch(T *t)
{
    return str(t, "r%d", 15 - ++t->nscratch);
}

static const char *src(T *t, uint32_t tok, int scalar)
{
    unsigned mod = (tok >> 24) & 0xf;
    const char *sw = swz(t, tok), *r;
    if (scalar && strlen(sw) != 2) sw = str(t, ".%c", "xyzw"[(tok >> 22) & 3]);  /* scalar ops read the last component */
    r = str(t, "%s%s", reg(t, tok), sw);
    if (mod == 0) return r;
    if (mod == 1) return str(t, "-%s", r);
    if ((mod == 11 || mod == 12) && t->ps) {
        const char *tmp = scratch(t);
        add(t, &t->pre, str(t, "ABS %s, %s;", tmp, r));
        return mod == 12 ? str(t, "-%s", tmp) : tmp;
    }
    fail(t, "source modifier %u", mod);
    return NULL;
}

static const char *neg(T *t, const char *s)
{
    return s[0] == '-' ? s + 1 : str(t, "-%s", s);
}

/* Select components of a source operand that may already carry a swizzle. */
static const char *swizzle(T *t, const char *s, const char *comps)
{
    const char *dot = strchr(s, '.'), *sw = dot ? dot + 1 : "";
    char full[5], pick[5];
    size_t base = dot ? (size_t)(dot - s) : strlen(s), n = strlen(sw), k;
    if (strchr(sw, '[')) fail(t, "swizzle on array");
    if (n == 1) memset(full, sw[0], 4);
    else if (n == 0) memcpy(full, "xyzw", 4);
    else if (n == 4) memcpy(full, sw, 4);
    else fail(t, "swizzle %s", sw);
    for (k = 0; comps[k]; ++k) pick[k] = full[strchr("xyzw", comps[k]) - "xyzw"];
    if (k > 1) while (k < 4) { pick[k] = pick[k - 1]; ++k; }
    pick[k] = 0;
    return str(t, "%.*s.%s", (int)base, s, pick);
}

/* Direct3D 9's documented vs_2_0 sincos expansion: dst.x = cos, dst.y = sin. */
static void sincos(T *t, const uint32_t *args, unsigned nargs, const char *d, Lines *out)
{
    const char *x = src(t, args[1], 1), *c1, *c2, *tmp, *dot;
    char comps[3];
    size_t n = 0;
    if (t->ps) {  /* ARB_fragment_program has the instruction, with the same component order */
        add(t, out, str(t, "SCS %s, %s;", d, x));
        return;
    }
    if (nargs < 4) fail(t, "sincos without constants");
    c1 = src(t, args[2], 0);
    c2 = src(t, args[3], 0);
    tmp = scratch(t);
    dot = strchr(d, '.');
    add(t, out, str(t, "MUL %s.z, %s, %s;", tmp, x, x));
    add(t, out, str(t, "MAD %s.xy, %s.z, %s, %s;", tmp, tmp, c1, swizzle(t, c1, "wz")));
    add(t, out, str(t, "MAD %s.xy, %s, %s.z, %s;", tmp, tmp, tmp, c2));
    add(t, out, str(t, "MAD %s.xy, %s, %s.z, %s;", tmp, tmp, tmp, swizzle(t, c2, "wz")));
    add(t, out, str(t, "MUL %s.x, %s.x, %s;", tmp, tmp, x));
    add(t, out, str(t, "MUL %s.xy, %s, %s.x;", tmp, tmp, tmp));
    add(t, out, str(t, "ADD %s.xy, %s, %s;", tmp, tmp, tmp));
    add(t, out, str(t, "ADD %s.x, -%s.x, %s;", tmp, tmp, swizzle(t, c2, "z")));
    for (const char *c = "xy"; *c; ++c) if (!dot || strchr(dot + 1, *c)) comps[n++] = *c;
    comps[n] = 0;
    add(t, out, str(t, "MOV %.*s.%s, %s;", dot ? (int)(dot - d) : (int)strlen(d), d, comps, tmp));
}

static void instr(T *t, const char *name, uint32_t tok, const uint32_t *args, unsigned nargs, Lines *body)
{
    Lines out = {0};
    const char *srcs[16], *d, *suffix, *tmp, *op;
    unsigned nsrc = 0;
    int sat, scalar;

    memset(&t->pre, 0, sizeof t->pre);
    t->nscratch = 0;
    if (!nargs) fail(t, "%s without operands", name);
    if (is(name, "texkill")) {
        const char *m = mask(t, args[0]);
        char sw[6] = "";
        if (*m) {  /* a partial mask repeats its last component */
            size_t n = strlen(m + 1);
            sw[0] = '.';
            memcpy(sw + 1, m + 1, n);
            if (n > 1) while (n < 4) { sw[1 + n] = sw[n]; ++n; }
            sw[1 + n] = 0;
        }
        add(t, body, str(t, "KIL %s%s;", reg(t, args[0]), sw));
        return;
    }
    if ((args[0] >> 24) & 0xf) fail(t, "shift");
    d = str(t, "%s%s", reg(t, args[0]), mask(t, args[0]));
    sat = (args[0] >> 20) & 1;
    scalar = is(name, "rcp") || is(name, "rsq") || is(name, "exp") || is(name, "log") || is(name, "expp") ||
             is(name, "logp") || is(name, "pow");
    for (unsigned i = 1; i < nargs; ++i) srcs[nsrc++] = src(t, args[i], scalar);
    suffix = sat ? "_SAT" : "";

    if (is(name, "texld")) {
        static const char *const kinds[] = { "TEX", "TXP", "TXB" };
        unsigned ctl = (tok >> 16) & 0xff, s;
        if (ctl > 2 || nsrc < 2) fail(t, "texld form %u", ctl);
        s = args[2] & 0x7ff;
        if (!t->samplers[s]) fail(t, "undeclared sampler %u", s);
        add(t, &out, str(t, "%s%s %s, %s, texture[ %u ], %s;", kinds[ctl], suffix, d, srcs[0], s, t->samplers[s]));
        goto done;
    }
    if (is(name, "cmp")) {
        if (nsrc < 3) fail(t, "cmp operands");
        tmp = srcs[1]; srcs[1] = srcs[2]; srcs[2] = tmp;
    }
    if (sat && !t->ps && (is(name, "nrm") || is(name, "lrp") || is(name, "dp2add") || is(name, "sincos")))
        fail(t, "vs saturate on %s", name);
    if (is(name, "nrm")) {
        if (nsrc < 1) fail(t, "nrm operands");
        tmp = scratch(t);
        add(t, &out, str(t, "DP3 %s.w, %s, %s;", tmp, srcs[0], srcs[0]));
        add(t, &out, str(t, "RSQ %s.w, %s.w;", tmp, tmp));
        add(t, &out, str(t, "MUL%s %s, %s, %s.w;", suffix, d, srcs[0], tmp));
        goto done;
    }
    if (is(name, "lrp") && !t->ps) {
        if (nsrc < 3) fail(t, "lrp operands");
        tmp = scratch(t);
        add(t, &out, str(t, "ADD %s, %s, %s;", tmp, srcs[1], neg(t, srcs[2])));
        add(t, &out, str(t, "MAD %s, %s, %s, %s;", d, srcs[0], tmp, srcs[2]));
        goto done;
    }
    if (is(name, "dp2add")) {
        if (nsrc < 3) fail(t, "dp2add operands");
        tmp = scratch(t);
        add(t, &out, str(t, "MUL %s.xy, %s, %s;", tmp, srcs[0], srcs[1]));
        add(t, &out, str(t, "ADD %s.x, %s.x, %s.y;", tmp, tmp, tmp));
        add(t, &out, str(t, "ADD%s %s, %s.x, %s;", suffix, d, tmp, srcs[2]));
        goto done;
    }
    if (is(name, "sincos")) {
        if (nargs < 2) fail(t, "sincos operands");
        sincos(t, args, nargs, d, &out);
        goto done;
    }
    if (name[0] == 'm' && name[1] >= '0' && name[1] <= '9') fail(t, "%s", name);
    op = arbname(t, name);
    {
        size_t len = 0;
        char *list;
        for (unsigned i = 0; i < nsrc; ++i) len += strlen(srcs[i]) + 2;
        list = alloc(t, len + 1);
        list[0] = 0;
        for (unsigned i = 0; i < nsrc; ++i) {
            if (i) strcat(list, ", ");
            strcat(list, srcs[i]);
        }
        if (sat && !t->ps) {  /* ARB vertex programs have no _SAT */
            const char *dot = strchr(d, '.');
            const char *tm;
            tmp = scratch(t);
            tm = dot ? str(t, "%s%s", tmp, dot) : tmp;
            add(t, &out, str(t, "%s %s, %s;", op, tm, list));
            add(t, &out, str(t, "MAX %s, %s, 0;", tm, tmp));
            add(t, &out, str(t, "MIN %s, %s, 1;", d, tmp));
        } else {
            add(t, &out, str(t, "%s%s %s, %s;", op, suffix, d, list));
        }
    }
done:
    extend(t, body, &t->pre);
    extend(t, body, &out);
}

static int usage_attrib(T *t, unsigned usage, unsigned idx)
{
    switch (usage) {
    case 0: case 9: return 0;   /* position */
    case 1: return 1;           /* blend weight */
    case 2: return 2;           /* blend indices */
    case 3: return 3;           /* normal */
    case 4: return 4;           /* point size */
    case 5: return 8 + (int)idx;   /* texcoord */
    case 6: return 7;           /* tangent */
    case 7: return 6;           /* binormal */
    case 10: return 5 + (int)idx;  /* color */
    }
    fail(t, "vs input usage %u", usage);
    return 0;
}

static void ps_input(T *t, const char *key, const char *value)
{
    for (size_t i = 0; i < t->npsin; ++i)
        if (is(t->psin[i].key, key)) { t->psin[i].value = value; return; }
    if (t->npsin == sizeof t->psin / sizeof t->psin[0]) fail(t, "too many pixel inputs");
    t->psin[t->npsin].key = key;
    t->psin[t->npsin++].value = value;
}

static void dcl(T *t, const uint32_t *args, unsigned nargs)
{
    uint32_t info, r;
    unsigned type, n, usage, idx;
    if (nargs < 2) fail(t, "dcl operands");
    info = args[0]; r = args[1];
    type = regtype(r); n = r & 0x7ff;
    if (type == 10) {
        switch ((info >> 27) & 0xf) {
        case 2: t->samplers[n] = "2D"; break;
        case 3: t->samplers[n] = "CUBE"; break;
        case 4: t->samplers[n] = "3D"; break;
        default: fail(t, "sampler type %u", (unsigned)((info >> 27) & 0xf));
        }
        return;
    }
    usage = info & 0x1f; idx = (info >> 16) & 0xf;
    if (!t->ps) {
        if (type == 1) t->vsin[n] = usage_attrib(t, usage, idx);
        else if (type == 6) fail(t, "vs_3_0 outputs");
        return;
    }
    if (t->major < 3) {
        if (type == 1) ps_input(t, str(t, "v%u", n), n == 0 ? "fragment.color.primary" : "fragment.color.secondary");
        else if (type == 3) ps_input(t, str(t, "t%u", n), str(t, "fragment.texcoord[ %u ]", n));
        return;
    }
    if (type != 1) fail(t, "ps3 dcl type %u", type);
    if (usage == 5) ps_input(t, str(t, "v%u", n), str(t, "fragment.texcoord[ %u ]", idx));
    else if (usage == 10) ps_input(t, str(t, "v%u", n), idx == 0 ? "fragment.color.primary" : "fragment.color.secondary");
    else if (usage == 11) ps_input(t, str(t, "v%u", n), "fragment.fogcoord");
    else fail(t, "ps input usage %u", usage);
}

static void parse_ctab(T *t, const unsigned char *d, size_t size)
{
    uint32_t count, info;
    if (size < 20) fail(t, "short constant table");
    count = rd32(d + 12); info = rd32(d + 16);
    if (count > 4096 || info > size || (uint64_t)count * 20 > size - info) fail(t, "constant table bounds");
    t->consts = alloc(t, (count ? count : 1) * sizeof *t->consts);
    t->nconsts = count;
    for (uint32_t k = 0; k < count; ++k) {
        const unsigned char *e = d + info + k * 20;
        uint32_t name = rd32(e), tinfo = rd32(e + 12);
        Const *c = &t->consts[k];
        if (name >= size || !memchr(d + name, 0, size - name) || tinfo > size || size - tinfo < 10)
            fail(t, "constant table entry bounds");
        c->name = (const char *)d + name;
        c->set = rd16(e + 4); c->reg = rd16(e + 6); c->cnt = rd16(e + 8);
        c->cls = rd16(d + tinfo); c->type = rd16(d + tinfo + 2); c->elems = rd16(d + tinfo + 8);
        if (c->set == 2)
            for (unsigned r = c->reg; r < c->reg + c->cnt && r < MAX_REG; ++r) {
                t->array_base[r] = c->reg;
                t->array_cnt[r] = c->cnt;
            }
    }
}

static int by_text(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* One PARAM line per float constant, ordered by register name as text. */
static void env_params(T *t, Lines *out)
{
    char **keys = alloc(t, (t->nconsts ? t->nconsts : 1) * sizeof *keys);
    size_t n = 0;
    for (size_t i = 0; i < t->nconsts; ++i) {
        const Const *c = &t->consts[i];
        char *line, *key;
        size_t k;
        if (c->set != 2) continue;
        /* "key\0line": sort on the key, emit the line. A later entry for the same register replaces an earlier one. */
        if (c->cnt == 1) line = str(t, "PARAM c%u = program.env[%u];", c->reg, c->reg);
        else line = str(t, "PARAM c%u[%u] =  { program.env[%u..%u] };", c->reg, c->cnt, c->reg, c->reg + c->cnt - 1);
        key = alloc(t, 16 + strlen(line) + 1);
        snprintf(key, 16, "c%u", c->reg);
        strcpy(key + strlen(key) + 1, line);
        for (k = 0; k < n; ++k) if (is(keys[k], key)) break;
        keys[k] = key;
        if (k == n) ++n;
    }
    qsort(keys, n, sizeof *keys, by_text);
    for (size_t i = 0; i < n; ++i) add(t, out, keys[i] + strlen(keys[i]) + 1);
}

static int no_fog(const char *name)
{
    static const char *const names[] = {
        "glow_apply_sky_bleed", "glow_setup", "glow_apply_bloom", "shadowcookie_blur", "shadowcookie_caster",
        "shadowcookie_display", "shadowcookie_receiver", "zprepass" };
    if (!strncmp(name, "filter_symmetric_", 17) && name[17] >= '1' && name[17] <= '8' && !name[18]) return 1;
    for (size_t i = 0; i < sizeof names / sizeof names[0]; ++i) if (is(name, names[i])) return 1;
    return 0;
}

static const char TEMPS[] = "r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15";
static const char FOG_TAIL[] =
    "MAX r15.x, fragment.fogcoord.x, 0;MIN  r15.x, r15.x, 1;LRP oC0AfterFog.xyz, r15.x, oC0, "
    "state.fog.color;MOV oC0AfterFog.w, oC0.w;END";

static void emit(T *t, const Lines *body, Sm2ArbProgram *out)
{
    Lines l = {0};
    const char *tail = "END";
    size_t size = 0;
    char *p;
    if (t->ps) {
        int sm3 = t->major >= 3, fog = !sm3 && !no_fog(t->name);
        add(t, &l, "!!ARBfp1.0");
        add(t, &l, "OPTION ARB_precision_hint_fastest;");
        if (sm3 || fog) {
            add(t, &l, "OUTPUT oC0AfterFog = result.color;");
            add(t, &l, str(t, "TEMP %s, oC0;", TEMPS));
        } else {
            add(t, &l, "OUTPUT oC0 = result.color;");
            add(t, &l, str(t, "TEMP %s;", TEMPS));
        }
        env_params(t, &l);
        extend(t, &l, &t->defs);
        for (size_t i = 0; i < t->npsin; ++i) add(t, &l, str(t, "ATTRIB %s = %s;", t->psin[i].key, t->psin[i].value));
        extend(t, &l, body);
        if (sm3) tail = "MOV oC0AfterFog, oC0;END";
        else if (fog) tail = FOG_TAIL;
    } else {
        add(t, &l, "!!ARBvp1.0");
        add(t, &l, "OUTPUT oPos = result.position;");
        add(t, &l, "OUTPUT oD0 = result.color;");
        add(t, &l, "OUTPUT oD1 = result.color.secondary;");
        add(t, &l, "OUTPUT oFog = result.fogcoord;");
        for (unsigned n = 0; n < MAX_REG; ++n) if (t->texout[n]) add(t, &l, str(t, "OUTPUT oT%u = result.texcoord[%u];", n, n));
        add(t, &l, str(t, "TEMP %s;", TEMPS));
        add(t, &l, "ADDRESS a0;");
        add(t, &l, "MOV oFog.x, 1;");
        add(t, &l, "PARAM Null = { 0, 0, 0, 1 };");
        for (unsigned n = 0; n < MAX_REG; ++n) if (t->texout[n]) add(t, &l, str(t, "MOV oT%u, Null;", n));
        env_params(t, &l);
        extend(t, &l, &t->defs);
        for (unsigned n = 0; n < MAX_REG; ++n)
            if (t->vsin[n] >= 0) add(t, &l, str(t, "ATTRIB v%u = vertex.attrib[%d];", n, t->vsin[n]));
        extend(t, &l, body);
    }
    for (size_t i = 0; i < l.n; ++i) size += strlen(l.v[i]) + 1;
    size += strlen(tail);
    out->text = malloc(size + 1);
    if (!out->text) fail(t, "out of memory");
    p = out->text;
    for (size_t i = 0; i < l.n; ++i) {
        size_t n = strlen(l.v[i]);
        memcpy(p, l.v[i], n);
        p += n;
        *p++ = '\n';
    }
    strcpy(p, tail);
    out->text_size = size;
}

static void wr16(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); }
static void wr32(unsigned char *p, uint32_t v) { wr16(p, v & 0xffff); wr16(p + 2, v >> 16); }

/* The .vc/.pc layout the engine parses: a D3DXSHADER_CONSTANTTABLE without
 * the creator string, float constants by register and then samplers. */
static void constant_table(T *t, Sm2ArbProgram *out)
{
    size_t count = t->nconsts, size = 28 + count * 36, at;
    const Const **order = alloc(t, (count ? count : 1) * sizeof *order);
    unsigned char *d;
    for (size_t i = 0; i < count; ++i) {  /* stable: samplers last, then by register */
        const Const *c = &t->consts[i];
        size_t k = i;
        while (k && ((order[k - 1]->set == 3) > (c->set == 3) ||
                     ((order[k - 1]->set == 3) == (c->set == 3) && order[k - 1]->reg > c->reg))) {
            order[k] = order[k - 1];
            --k;
        }
        order[k] = c;
        size += strlen(c->name) + 1;
    }
    d = calloc(1, size ? size : 1);
    if (!d) fail(t, "out of memory");
    wr32(d, 28); wr32(d + 12, (uint32_t)count); wr32(d + 16, 28);
    at = 28 + count * 36;
    for (size_t i = 0; i < count; ++i) {
        const Const *c = order[i];
        size_t record = 28 + i * 20, type = 28 + count * 20 + i * 16, n = strlen(c->name) + 1;
        wr32(d + record, (uint32_t)at);
        wr16(d + record + 6, c->reg); wr16(d + record + 8, c->cnt);
        wr32(d + record + 12, (uint32_t)type);
        wr16(d + type, c->cls); wr16(d + type + 2, c->type); wr16(d + type + 8, c->elems);
        memcpy(d + at, c->name, n);
        at += n;
    }
    out->table = d;
    out->table_size = size;
}

static const char *f9(T *t, uint32_t bits)
{
    float f;
    const char *s;
    memcpy(&f, &bits, sizeof f);
    s = str(t, "%.9g", (double)f);
    return is(s, "-0") ? "0" : s;
}

static void translate(T *t, const unsigned char *code, size_t size, Sm2ArbProgram *out)
{
    size_t nwords = size / 4, i = 1, nins = 0;
    uint32_t *words, version;
    const unsigned char *ctab = NULL;
    size_t ctab_size = 0;
    Ins *ins;
    Lines body = {0};
    struct { Lines outer; int count; } stack[8];
    int depth = 0, ints[MAX_REG], have_int[MAX_REG];

    if (nwords < 2) fail(t, "short bytecode");
    words = alloc(t, nwords * sizeof *words);
    for (size_t k = 0; k < nwords; ++k) words[k] = rd32(code + k * 4);
    ins = alloc(t, nwords * sizeof *ins);
    version = words[0];
    while (i < nwords) {
        uint32_t tok = words[i];
        unsigned op = tok & 0xffff, n;
        if (op == 0xffff) break;
        if (op == 0xfffe) {
            n = (tok >> 16) & 0x7fff;
            if (n > nwords - i - 1) fail(t, "comment runs past the end");
            if (n >= 1 && words[i + 1] == 0x42415443) {  /* "CTAB" */
                ctab = code + (i + 2) * 4;
                ctab_size = (size_t)(n - 1) * 4;
            }
            i += 1 + n;
            continue;
        }
        n = (tok >> 24) & 0xf;
        if (n > nwords - i - 1) fail(t, "instruction runs past the end");
        ins[nins].op = op; ins[nins].tok = tok; ins[nins].args = words + i + 1; ins[nins].nargs = n;
        ++nins;
        i += 1 + n;
    }
    t->ps = (version >> 16) == 0xffff;
    t->major = (version >> 8) & 0xff;
    if (ctab) parse_ctab(t, ctab, ctab_size);
    memset(have_int, 0, sizeof have_int);

    for (size_t k = 0; k < nins; ++k) {
        const Ins *in = &ins[k];
        const char *name = opname(in->op);
        if (!name) fail(t, "opcode %u", in->op);
        if (is(name, "dcl")) { dcl(t, in->args, in->nargs); continue; }
        if (is(name, "def")) {
            if (in->nargs < 5) fail(t, "def operands");
            add(t, &t->defs, str(t, "PARAM c%u = { %s, %s, %s, %s };", (unsigned)(in->args[0] & 0x7ff),
                                 f9(t, in->args[1]), f9(t, in->args[2]), f9(t, in->args[3]), f9(t, in->args[4])));
            continue;
        }
        if (is(name, "defi")) {
            if (in->nargs < 5) fail(t, "defi operands");
            ints[in->args[0] & 0x7ff] = (int)in->args[1];
            have_int[in->args[0] & 0x7ff] = 1;
            continue;
        }
        if (is(name, "defb")) fail(t, "defb");
        if (is(name, "rep")) {  /* static loop: unroll */
            unsigned r;
            if (!in->nargs || depth == 8) fail(t, "rep nesting");
            r = in->args[0] & 0x7ff;
            if (!have_int[r] || ints[r] < 0 || ints[r] > 255) fail(t, "rep count");
            stack[depth].outer = body;
            stack[depth++].count = ints[r];
            memset(&body, 0, sizeof body);
            continue;
        }
        if (is(name, "endrep")) {
            Lines inner = body;
            if (!depth) fail(t, "endrep without rep");
            body = stack[--depth].outer;
            for (int c = 0; c < stack[depth].count; ++c) extend(t, &body, &inner);
            continue;
        }
        if (in->tok & 0x10000000) fail(t, "predication");
        instr(t, name, in->tok, in->args, in->nargs, &body);
    }
    emit(t, &body, out);
    constant_table(t, out);
}

int Sm2Arb_Translate(const unsigned char *code, size_t size, const char *name,
                     Sm2ArbProgram *out, char *error, size_t error_size)
{
    T *t = calloc(1, sizeof *t);
    int ok = 0;
    memset(out, 0, sizeof *out);
    if (error && error_size) error[0] = 0;
    if (!t) return 0;
    t->error = error; t->error_size = error_size; t->name = name;
    for (unsigned n = 0; n < MAX_REG; ++n) t->vsin[n] = -1;
    if (!setjmp(t->fail)) {
        translate(t, code, size, out);
        ok = 1;
    } else {
        Sm2Arb_Free(out);
    }
    while (t->blocks) {
        Block *next = t->blocks->next;
        free(t->blocks);
        t->blocks = next;
    }
    free(t);
    return ok;
}

void Sm2Arb_Free(Sm2ArbProgram *program)
{
    free(program->text);
    free(program->table);
    memset(program, 0, sizeof *program);
}
