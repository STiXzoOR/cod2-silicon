/* cod2-shadergen: build the OpenGL shader cache from the game's own HLSL.
 *
 * The Windows game compiles materials/shaders/<name>.hlsl with the Direct3D
 * compiler when it loads. This tool does the same work ahead of time for the
 * OpenGL renderer: it reads the HLSL from the game's .iwd archives, applies
 * the OpenGL clip-space edit (glclip.c), compiles vs_main and ps_main to
 * Direct3D 9 bytecode with vkd3d-shader, and translates the bytecode to ARB
 * programs and constant tables (sm2arb.c).
 *
 * usage: cod2-shadergen [--dump DIR] <game "main" folder | file.iwd | HLSL folder> <output folder>
 *        cod2-shadergen --version
 *
 * The output folder receives <name>.vsa/.vc for each vertex shader,
 * <name>.pse/.pc for each pixel shader, and manifest.json with a SHA-256 of
 * every file. Nothing derived from the game is stored anywhere else. */
#include "glclip.h"
#include "sha256.h"
#include "sm2arb.h"

#include <vkd3d_shader.h>
#include <zlib.h>

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TOOL_ID "cod2-shadergen 1"
#define MAX_SOURCE (4u << 20)

static const char SHADER_DIR[] = "materials/shaders/";

typedef struct {
    char *rel;              /* path below materials/shaders/, lower case, forward slashes */
    unsigned char *raw;     /* as shipped */
    size_t raw_size;
    char *text;             /* line ends normalized, OpenGL edit applied */
    size_t size;
} Source;

typedef struct { char *name; unsigned char *data; size_t size; } Asset;

static Source *g_sources;
static size_t g_nsources, g_sources_cap;
static Asset *g_assets;
static size_t g_nassets, g_assets_cap;
static const char *g_dump;

static void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) { fprintf(stderr, "cod2-shadergen: out of memory\n"); exit(1); }
    return p;
}

static char *xstrdup(const char *s)
{
    char *p = xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

static int ends_with(const char *s, const char *suffix)
{
    size_t n = strlen(s), m = strlen(suffix);
    if (n < m) return 0;
    for (size_t i = 0; i < m; ++i) if (lower((unsigned char)s[n - m + i]) != suffix[i]) return 0;
    return 1;
}

/* Lower case, forward slashes, no "." or ".." parts. Returns NULL for a path that leaves the folder. */
static char *clean_path(const char *path, size_t len)
{
    char *out = xmalloc(len + 1);
    size_t n = 0, i = 0;
    while (i < len) {
        size_t start = i, part;
        while (i < len && path[i] != '/' && path[i] != '\\') ++i;
        part = i - start;
        if (i < len) ++i;
        if (!part || (part == 1 && path[start] == '.')) continue;
        if (part == 2 && path[start] == '.' && path[start + 1] == '.') {
            if (!n) { free(out); return NULL; }
            while (n && out[n - 1] != '/') --n;
            if (n) --n;
            continue;
        }
        if (n) out[n++] = '/';
        for (size_t k = 0; k < part; ++k) out[n++] = (char)lower((unsigned char)path[start + k]);
    }
    out[n] = 0;
    return out;
}

static Source *find_source(const char *rel)
{
    for (size_t i = 0; i < g_nsources; ++i) if (!strcmp(g_sources[i].rel, rel)) return &g_sources[i];
    return NULL;
}

/* Takes ownership of rel and data. A later archive replaces an earlier one's file, as in the game. */
static void add_source(char *rel, unsigned char *data, size_t size)
{
    Source *s = find_source(rel);
    if (s) {
        free(s->raw);
        free(rel);
    } else {
        if (g_nsources == g_sources_cap) {
            g_sources_cap = g_sources_cap ? g_sources_cap * 2 : 256;
            g_sources = realloc(g_sources, g_sources_cap * sizeof *g_sources);
            if (!g_sources) { fprintf(stderr, "cod2-shadergen: out of memory\n"); exit(1); }
        }
        s = &g_sources[g_nsources++];
        memset(s, 0, sizeof *s);
        s->rel = rel;
    }
    s->raw = data;
    s->raw_size = size;
}

static unsigned rd16(const unsigned char *p) { return p[0] | (unsigned)p[1] << 8; }
static uint32_t rd32(const unsigned char *p) { return rd16(p) | (uint32_t)rd16(p + 2) << 16; }

static int read_at(FILE *f, uint64_t offset, void *buffer, size_t size)
{
    return !fseeko(f, (off_t)offset, SEEK_SET) && fread(buffer, 1, size, f) == size;
}

/* Read the shader sources out of one .iwd (a zip archive). Returns 0 only for an archive that cannot be parsed. */
static int load_iwd(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char *tail = NULL, *dir = NULL, local[30];
    uint64_t size;
    size_t tail_size, at, dir_size, p = 0;
    uint32_t dir_offset;
    unsigned entries;
    int ok = 0;

    if (!f) return 0;
    if (fseeko(f, 0, SEEK_END)) goto done;
    size = (uint64_t)ftello(f);
    tail_size = size < 65557 ? (size_t)size : 65557;
    if (tail_size < 22) goto done;
    tail = xmalloc(tail_size);
    if (!read_at(f, size - tail_size, tail, tail_size)) goto done;
    for (at = tail_size - 22;; --at) {
        if (rd32(tail + at) == 0x06054b50) break;
        if (!at) goto done;
    }
    entries = rd16(tail + at + 10);
    dir_size = rd32(tail + at + 12);
    dir_offset = rd32(tail + at + 16);
    if ((uint64_t)dir_offset + dir_size > size) goto done;
    dir = xmalloc(dir_size);
    if (!read_at(f, dir_offset, dir, dir_size)) goto done;
    for (unsigned i = 0; i < entries; ++i) {
        const unsigned char *e = dir + p;
        unsigned method, name_len;
        uint32_t packed, unpacked, crc, header;
        const char *name;
        if (dir_size - p < 46 || rd32(e) != 0x02014b50) goto done;
        method = rd16(e + 10); crc = rd32(e + 16); packed = rd32(e + 20); unpacked = rd32(e + 24);
        name_len = rd16(e + 28); header = rd32(e + 42);
        if (dir_size - p - 46 < (size_t)name_len + rd16(e + 30) + rd16(e + 32)) goto done;
        name = (const char *)e + 46;
        p += 46 + (size_t)name_len + rd16(e + 30) + rd16(e + 32);
        if (name_len > sizeof SHADER_DIR - 1 && name_len < 512) {
            char prefix[sizeof SHADER_DIR], *rel;
            unsigned char *packed_data, *data;
            uint64_t offset;
            for (size_t k = 0; k < sizeof SHADER_DIR - 1; ++k) prefix[k] = (char)lower(name[k] == '\\' ? '/' : (unsigned char)name[k]);
            prefix[sizeof SHADER_DIR - 1] = 0;
            if (strcmp(prefix, SHADER_DIR)) continue;
            rel = clean_path(name + sizeof SHADER_DIR - 1, name_len - (sizeof SHADER_DIR - 1));
            if (!rel || !ends_with(rel, ".hlsl") || unpacked > MAX_SOURCE || packed > MAX_SOURCE) { free(rel); continue; }
            if (!read_at(f, header, local, sizeof local) || rd32(local) != 0x04034b50) { free(rel); goto done; }
            offset = (uint64_t)header + 30 + rd16(local + 26) + rd16(local + 28);
            packed_data = xmalloc(packed);
            data = xmalloc(unpacked);
            if (!read_at(f, offset, packed_data, packed)) { free(packed_data); free(data); free(rel); goto done; }
            if (method == 0 && packed == unpacked) {
                memcpy(data, packed_data, unpacked);
            } else if (method == 8) {
                z_stream z;
                int rc;
                memset(&z, 0, sizeof z);
                z.next_in = packed_data; z.avail_in = packed;
                z.next_out = data; z.avail_out = unpacked;
                if (inflateInit2(&z, -MAX_WBITS) != Z_OK) { free(packed_data); free(data); free(rel); goto done; }
                rc = inflate(&z, Z_FINISH);
                inflateEnd(&z);
                if (rc != Z_STREAM_END || z.total_out != unpacked) { free(packed_data); free(data); free(rel); goto done; }
            } else {
                free(packed_data); free(data); free(rel);
                goto done;
            }
            free(packed_data);
            if ((uint32_t)crc32(crc32(0, NULL, 0), data, unpacked) != crc) { free(data); free(rel); goto done; }
            add_source(rel, data, unpacked);
        }
    }
    ok = 1;
done:
    free(tail);
    free(dir);
    fclose(f);
    return ok;
}

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    unsigned char *data;
    long n;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) || (n = ftell(f)) < 0 || (unsigned long)n > MAX_SOURCE || fseek(f, 0, SEEK_SET)) { fclose(f); return NULL; }
    data = xmalloc((size_t)n);
    if (fread(data, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(data); return NULL; }
    fclose(f);
    *size = (size_t)n;
    return data;
}

static int by_name(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Names in a folder with the given suffix, sorted without regard to case. */
static char **list_folder(const char *folder, const char *suffix, size_t *count)
{
    DIR *d = opendir(folder);
    struct dirent *e;
    char **names = NULL;
    size_t n = 0, cap = 0;
    *count = 0;
    if (!d) return NULL;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' || !ends_with(e->d_name, suffix)) continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 64;
            names = realloc(names, cap * sizeof *names);
            if (!names) { fprintf(stderr, "cod2-shadergen: out of memory\n"); exit(1); }
        }
        /* Sort key first, real name after it. */
        names[n] = xmalloc(strlen(e->d_name) * 2 + 2);
        for (size_t k = 0; e->d_name[k]; ++k) names[n][k] = (char)lower((unsigned char)e->d_name[k]);
        names[n][strlen(e->d_name)] = 0;
        strcpy(names[n] + strlen(e->d_name) + 1, e->d_name);
        ++n;
    }
    closedir(d);
    if (n) qsort(names, n, sizeof *names, by_name);
    *count = n;
    return names;
}

static void load_hlsl_folder(const char *folder, const char *prefix)
{
    size_t count;
    char **names = list_folder(folder, ".hlsl", &count);
    for (size_t i = 0; i < count; ++i) {
        const char *real = names[i] + strlen(names[i]) + 1;
        char path[2048], rel[600];
        unsigned char *data;
        size_t size;
        snprintf(path, sizeof path, "%s/%s", folder, real);
        snprintf(rel, sizeof rel, "%s%s", prefix, names[i]);
        if ((data = read_file(path, &size))) add_source(xstrdup(rel), data, size);
        free(names[i]);
    }
    free(names);
}

static int load_sources(const char *input)
{
    struct stat st;
    size_t count;
    char **names;
    if (stat(input, &st)) { fprintf(stderr, "cod2-shadergen: cannot read %s\n", input); return 0; }
    if (!S_ISDIR(st.st_mode)) {
        if (!load_iwd(input)) { fprintf(stderr, "cod2-shadergen: %s is not a readable .iwd archive\n", input); return 0; }
        return 1;
    }
    names = list_folder(input, ".iwd", &count);
    if (count) {
        for (size_t i = 0; i < count; ++i) {
            char path[2048];
            snprintf(path, sizeof path, "%s/%s", input, names[i] + strlen(names[i]) + 1);
            if (!load_iwd(path)) fprintf(stderr, "cod2-shadergen: skipping unreadable archive %s\n", path);
            free(names[i]);
        }
        free(names);
        return 1;
    }
    free(names);
    {
        char lib[2048];
        snprintf(lib, sizeof lib, "%s/lib", input);
        load_hlsl_folder(input, "");
        load_hlsl_folder(lib, "lib/");
    }
    return 1;
}

static void make_folder(const char *path)
{
    char buffer[2048];
    size_t n = strlen(path);
    if (n >= sizeof buffer) return;
    memcpy(buffer, path, n + 1);
    for (size_t i = 1; i < n; ++i)
        if (buffer[i] == '/') { buffer[i] = 0; mkdir(buffer, 0755); buffer[i] = '/'; }
    mkdir(buffer, 0755);
}

static int write_file(const char *folder, const char *name, const void *data, size_t size)
{
    char path[2048];
    FILE *f;
    int ok;
    if (snprintf(path, sizeof path, "%s/%s", folder, name) >= (int)sizeof path) return 0;
    if (!(f = fopen(path, "wb"))) return 0;
    ok = fwrite(data, 1, size, f) == size;
    return !fclose(f) && ok;
}

static void dump(const char *kind, const char *name, const void *data, size_t size)
{
    char folder[2048], path[2048], *slash;
    if (!g_dump) return;
    snprintf(path, sizeof path, "%s/%s/%s", g_dump, kind, name);
    snprintf(folder, sizeof folder, "%s", path);
    if ((slash = strrchr(folder, '/'))) *slash = 0;
    make_folder(folder);
    write_file(folder, strrchr(path, '/') + 1, data, size);
}

/* Includes resolve as in the game's compiler callback: next to the including file, then the shader folder, then lib/. */
static int open_include(const char *filename, bool local, const char *parent_data, void *context,
                        struct vkd3d_shader_code *out)
{
    const Source *parent = context;
    char path[1024], *clean;
    const char *slash;
    (void)local;
    for (size_t i = 0; i < g_nsources; ++i)
        if (g_sources[i].text == parent_data) { parent = &g_sources[i]; break; }
    slash = parent ? strrchr(parent->rel, '/') : NULL;
    for (int attempt = 0; attempt < 3; ++attempt) {
        const Source *s;
        if (attempt == 0) snprintf(path, sizeof path, "%.*s/%s", slash ? (int)(slash - parent->rel) : 0, parent ? parent->rel : "", filename);
        else if (attempt == 1) snprintf(path, sizeof path, "%s", filename);
        else snprintf(path, sizeof path, "lib/%s", filename);
        if (!(clean = clean_path(path, strlen(path)))) continue;
        s = find_source(clean);
        free(clean);
        if (s && s->text) {
            out->code = s->text;
            out->size = s->size;
            return VKD3D_OK;
        }
    }
    return VKD3D_ERROR;
}

static void close_include(const struct vkd3d_shader_code *code, void *context)
{
    (void)code; (void)context;
}

/* 1: compiled. 0: the file has no such entry point. -1: a real error, already reported. */
static int compile(const Source *source, const char *entry, const char *const *profiles, struct vkd3d_shader_code *code)
{
    static const struct vkd3d_shader_macro macros[] = { { "PC", "" } };
    static const struct vkd3d_shader_compile_option options[] = {
        { VKD3D_SHADER_COMPILE_OPTION_BACKWARD_COMPATIBILITY, VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_ALLOW_HALF_GLOBALS },
    };
    char *messages = NULL;
    int missing = 0;
    for (; *profiles; ++profiles) {
        struct vkd3d_shader_preprocess_info preprocess = {
            .type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO,
            .macros = macros, .macro_count = 1,
            .pfn_open_include = open_include, .pfn_close_include = close_include,
            .include_context = (void *)source,
        };
        struct vkd3d_shader_hlsl_source_info hlsl = {
            .type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO, .next = &preprocess,
            .entry_point = entry, .profile = *profiles,
        };
        struct vkd3d_shader_compile_info info = {
            .type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO, .next = &hlsl,
            .source = { source->text, source->size }, .source_type = VKD3D_SHADER_SOURCE_HLSL,
            .target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE,
            .options = options, .option_count = 1,
            .log_level = VKD3D_SHADER_LOG_ERROR, .source_name = source->rel,
        };
        vkd3d_shader_free_messages(messages);
        messages = NULL;
        memset(code, 0, sizeof *code);
        if (vkd3d_shader_compile(&info, code, &messages) == VKD3D_OK) {
            vkd3d_shader_free_messages(messages);
            return 1;
        }
        missing = messages && strstr(messages, "E5005") != NULL;
    }
    if (!missing) fprintf(stderr, "cod2-shadergen: %s %s failed to compile:\n%s\n", source->rel, entry, messages ? messages : "(no message)");
    vkd3d_shader_free_messages(messages);
    return missing ? 0 : -1;
}

static void add_asset(const char *stem, const char *extension, const void *data, size_t size)
{
    Asset *a;
    if (g_nassets == g_assets_cap) {
        g_assets_cap = g_assets_cap ? g_assets_cap * 2 : 1024;
        g_assets = realloc(g_assets, g_assets_cap * sizeof *g_assets);
        if (!g_assets) { fprintf(stderr, "cod2-shadergen: out of memory\n"); exit(1); }
    }
    a = &g_assets[g_nassets++];
    a->name = xmalloc(strlen(stem) + strlen(extension) + 1);
    sprintf(a->name, "%s%s", stem, extension);
    a->data = xmalloc(size);
    memcpy(a->data, data, size);
    a->size = size;
}

static int build(const Source *source, const char *stem, const char *entry, const char *const *profiles,
                 const char *program_extension, const char *table_extension)
{
    struct vkd3d_shader_code code;
    Sm2ArbProgram program;
    char error[256], name[600];
    int rc = compile(source, entry, profiles, &code);
    if (rc <= 0) return rc == 0;
    snprintf(name, sizeof name, "%s%s.bin", stem, program_extension);
    dump("bytecode", name, code.code, code.size);
    if (!Sm2Arb_Translate(code.code, code.size, stem, &program, error, sizeof error)) {
        fprintf(stderr, "cod2-shadergen: %s %s cannot be translated: %s\n", source->rel, entry, error);
        vkd3d_shader_free_shader_code(&code);
        return 0;
    }
    vkd3d_shader_free_shader_code(&code);
    add_asset(stem, program_extension, program.text, program.text_size);
    add_asset(stem, table_extension, program.table, program.table_size);
    Sm2Arb_Free(&program);
    return 1;
}

static int by_rel(const void *a, const void *b)
{
    return strcmp(((const Source *)a)->rel, ((const Source *)b)->rel);
}

static int by_asset(const void *a, const void *b)
{
    return strcmp(((const Asset *)a)->name, ((const Asset *)b)->name);
}

static int cache_name(const char *stem)
{
    if (!*stem) return 0;
    for (; *stem; ++stem)
        if (!((*stem >= 'a' && *stem <= 'z') || (*stem >= '0' && *stem <= '9') || *stem == '_')) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    static const char *const vs_profiles[] = { "vs_2_0", NULL };
    static const char *const ps_profiles[] = { "ps_2_0", "ps_3_0", NULL };
    const char *input, *output, *compiler = vkd3d_shader_get_version(NULL, NULL);
    unsigned char digest[32];
    char identity[65];
    Sha256 hash;
    FILE *manifest;
    char path[2048];
    int ok = 1;

    if (argc == 2 && !strcmp(argv[1], "--version")) {  /* the manifest's "generator" value */
        printf("%s, %s\n", TOOL_ID, compiler);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "--dump")) { g_dump = argv[2]; argv += 2; argc -= 2; }
    if (argc != 3) {
        fprintf(stderr, "usage: cod2-shadergen [--dump DIR] <game \"main\" folder | file.iwd | HLSL folder> <output folder>\n");
        return 2;
    }
    input = argv[1]; output = argv[2];
    if (!load_sources(input)) return 1;
    if (!g_nsources) { fprintf(stderr, "cod2-shadergen: no %s*.hlsl found in %s\n", SHADER_DIR, input); return 1; }
    qsort(g_sources, g_nsources, sizeof *g_sources, by_rel);

    Sha256_Init(&hash);
    Sha256_Update(&hash, TOOL_ID, sizeof TOOL_ID);
    Sha256_Update(&hash, compiler, strlen(compiler) + 1);
    for (size_t i = 0; i < g_nsources; ++i) {
        Source *s = &g_sources[i];
        unsigned char length[8];
        char error[512], *text;
        for (int k = 0; k < 8; ++k) length[k] = (unsigned char)((uint64_t)s->raw_size >> (k * 8));
        Sha256_Update(&hash, s->rel, strlen(s->rel) + 1);
        Sha256_Update(&hash, length, sizeof length);
        Sha256_Update(&hash, s->raw, s->raw_size);
        text = xmalloc(s->raw_size + 1);
        memcpy(text, s->raw, s->raw_size);
        s->size = GlClip_NormalizeNewlines(text, s->raw_size);
        s->text = GlClip_Patch(s->rel, text, s->size, &s->size, error, sizeof error);
        free(text);
        if (!s->text) { fprintf(stderr, "cod2-shadergen: %s\n", error); return 1; }
        dump("hlsl", s->rel, s->text, s->size);
    }
    Sha256_Final(&hash, digest);
    Sha256_ToHex(digest, identity);

    for (size_t i = 0; i < g_nsources; ++i) {
        const Source *s = &g_sources[i];
        char stem[600];
        if (strchr(s->rel, '/')) continue;
        snprintf(stem, sizeof stem, "%.*s", (int)(strlen(s->rel) - 5), s->rel);
        if (!cache_name(stem)) { fprintf(stderr, "cod2-shadergen: skipping %s: unsupported name\n", s->rel); continue; }
        ok &= build(s, stem, "vs_main", vs_profiles, ".vsa", ".vc");
        ok &= build(s, stem, "ps_main", ps_profiles, ".pse", ".pc");
    }
    if (!ok) return 1;
    if (!g_nassets) { fprintf(stderr, "cod2-shadergen: no shader has a vs_main or ps_main\n"); return 1; }

    qsort(g_assets, g_nassets, sizeof *g_assets, by_asset);
    make_folder(output);
    for (size_t i = 0; i < g_nassets; ++i)
        if (!write_file(output, g_assets[i].name, g_assets[i].data, g_assets[i].size)) {
            fprintf(stderr, "cod2-shadergen: cannot write %s/%s: %s\n", output, g_assets[i].name, strerror(errno));
            return 1;
        }
    snprintf(path, sizeof path, "%s/manifest.json", output);
    if (!(manifest = fopen(path, "wb"))) { fprintf(stderr, "cod2-shadergen: cannot write %s: %s\n", path, strerror(errno)); return 1; }
    fprintf(manifest, "{\n  \"binary_sha256\": \"%s\",\n  \"generator\": \"%s, %s\",\n  \"assets\": {\n", identity, TOOL_ID, compiler);
    for (size_t i = 0; i < g_nassets; ++i) {
        char hex[65];
        Sha256_Hex(g_assets[i].data, g_assets[i].size, hex);
        fprintf(manifest, "    \"%s\": \"%s\"%s\n", g_assets[i].name, hex, i + 1 < g_nassets ? "," : "");
    }
    fprintf(manifest, "  }\n}\n");
    if (fclose(manifest)) { fprintf(stderr, "cod2-shadergen: cannot write %s: %s\n", path, strerror(errno)); return 1; }
    printf("%zu shader files from %zu sources (%s, %s)\n", g_nassets, g_nsources, TOOL_ID, compiler);
    return 0;
}
