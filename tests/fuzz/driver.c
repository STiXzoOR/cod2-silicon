/* Coverage-guided mutation driver for libFuzzer-style targets.
 *
 * Apple clang compiles SanitizerCoverage instrumentation but ships no libFuzzer
 * runtime. Link this file with a target that defines LLVMFuzzerTestOneInput and
 * was compiled with -fsanitize-coverage=inline-8bit-counters,trace-cmp; the
 * driver reads the counters and comparison operands itself. Compile the driver
 * without coverage instrumentation and with the same sanitizers as the target.
 *
 *   driver [options] DIR...    fuzz, seeding the corpus from each DIR
 *   driver [options] FILE...   run each FILE once (regression mode)
 *   driver -minimize_crash=1 FILE
 *
 * Options use libFuzzer spelling: -runs=N -max_total_time=S -seed=N -max_len=N
 * -timeout=S (CPU seconds) -artifact_prefix=PATH -dict=FILE -corpus_out=DIR -print_every=S.
 * A crash, sanitizer report or timeout writes the input to the artifact prefix
 * (default crashes/) and exits non-zero. Seeds are recorded in every run log.
 */
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
void __sanitizer_set_death_callback(void (*callback)(void));
void __sanitizer_print_stack_trace(void);

/* Mach-O cannot leave a weak reference undefined; targets override this. */
__attribute__((weak)) int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void)argc;
    (void)argv;
    return 0;
}
const char *__asan_default_options(void)
{
    return "handle_abort=1:handle_sigill=1:abort_on_error=0:detect_leaks=0:"
           "allocator_may_return_null=0:symbolize=1:detect_stack_use_after_return=0";
}

const char *__ubsan_default_options(void)
{
    return "halt_on_error=1:print_stacktrace=1:symbolize=1";
}

/* ---- options ---- */

static long opt_runs = -1;
static long opt_max_total_time = 0;
static unsigned long long opt_seed = 0;
static int opt_seed_given = 0;
static size_t opt_max_len = 4096;
static int opt_timeout = 10;
static const char *opt_artifact_prefix = "crashes/";
static const char *opt_dict = NULL;
static const char *opt_corpus_out = NULL;
static int opt_minimize = 0;
static int opt_print_every = 10;

/* ---- deterministic random numbers (splitmix64) ---- */

static uint64_t rng_state;

static uint64_t Rand64(void)
{
    uint64_t z = (rng_state += 0x9e3779b97f4a7c15ull);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
    return z ^ (z >> 31);
}

static size_t RandBelow(size_t n)
{
    return n ? (size_t)(Rand64() % n) : 0;
}

static int RandBool(void)
{
    return (int)(Rand64() & 1);
}

/* ---- coverage: inline 8-bit counters ---- */

#define MAX_REGIONS 64
static struct {
    uint8_t *start;
    uint8_t *stop;
} regions[MAX_REGIONS];
static int num_regions;
static uint8_t *seen_buckets[MAX_REGIONS];
static size_t total_counters;
static size_t total_features;

void __sanitizer_cov_8bit_counters_init(uint8_t *start, uint8_t *stop)
{
    int i;

    if (start == stop)
        return;
    for (i = 0; i < num_regions; ++i) {
        if (regions[i].start == start)
            return;
    }
    if (num_regions == MAX_REGIONS)
        return;
    regions[num_regions].start = start;
    regions[num_regions].stop = stop;
    seen_buckets[num_regions] = calloc((size_t)(stop - start), 1);
    total_counters += (size_t)(stop - start);
    ++num_regions;
}

void __sanitizer_cov_pcs_init(const uintptr_t *start, const uintptr_t *stop)
{
    (void)start;
    (void)stop;
}

static uint8_t CounterBucket(uint8_t count)
{
    if (count >= 128)
        return 0x80;
    if (count >= 32)
        return 0x40;
    if (count >= 16)
        return 0x20;
    if (count >= 8)
        return 0x10;
    if (count >= 4)
        return 0x08;
    if (count == 3)
        return 0x04;
    if (count == 2)
        return 0x02;
    return 0x01;
}

static void ClearCounters(void)
{
    int r;

    for (r = 0; r < num_regions; ++r)
        memset(regions[r].start, 0, (size_t)(regions[r].stop - regions[r].start));
}

/* Returns the number of new (counter, bucket) features and records them. */
static size_t CollectFeatures(int record)
{
    size_t found = 0;
    int r;

    for (r = 0; r < num_regions; ++r) {
        uint8_t *p = regions[r].start;
        uint8_t *end = regions[r].stop;
        uint8_t *seen = seen_buckets[r];

        while (p < end) {
            if (((uintptr_t)p & 7) == 0 && p + 8 <= end) {
                uint64_t word;
                memcpy(&word, p, sizeof(word));
                if (!word) {
                    p += 8;
                    continue;
                }
            }
            if (*p) {
                size_t index = (size_t)(p - regions[r].start);
                uint8_t bucket = CounterBucket(*p);
                if (!(seen[index] & bucket)) {
                    ++found;
                    if (record)
                        seen[index] |= bucket;
                }
            }
            ++p;
        }
    }
    if (record)
        total_features += found;
    return found;
}

/* ---- comparison operands (table of recent compares) ---- */

#define TORC_SIZE 512
static struct {
    uint64_t a, b;
    uint8_t width;
} torc[TORC_SIZE];
static unsigned torc_next;
static volatile int running_target;

static inline void TorcAdd(uint64_t a, uint64_t b, uint8_t width)
{
    unsigned slot;

    if (!running_target || a == b)
        return;
    slot = (unsigned)((a * 0x9e3779b1u) ^ b ^ width) % TORC_SIZE;
    if ((torc_next++ & 3) == 0)
        slot = torc_next % TORC_SIZE;
    torc[slot].a = a;
    torc[slot].b = b;
    torc[slot].width = width;
}

void __sanitizer_cov_trace_cmp1(uint8_t a, uint8_t b) { TorcAdd(a, b, 1); }
void __sanitizer_cov_trace_cmp2(uint16_t a, uint16_t b) { TorcAdd(a, b, 2); }
void __sanitizer_cov_trace_cmp4(uint32_t a, uint32_t b) { TorcAdd(a, b, 4); }
void __sanitizer_cov_trace_cmp8(uint64_t a, uint64_t b) { TorcAdd(a, b, 8); }
void __sanitizer_cov_trace_const_cmp1(uint8_t a, uint8_t b) { TorcAdd(a, b, 1); }
void __sanitizer_cov_trace_const_cmp2(uint16_t a, uint16_t b) { TorcAdd(a, b, 2); }
void __sanitizer_cov_trace_const_cmp4(uint32_t a, uint32_t b) { TorcAdd(a, b, 4); }
void __sanitizer_cov_trace_const_cmp8(uint64_t a, uint64_t b) { TorcAdd(a, b, 8); }

void __sanitizer_cov_trace_switch(uint64_t value, uint64_t *cases)
{
    uint64_t n = cases[0];
    uint64_t bits = cases[1];
    uint64_t i;

    for (i = 0; i < n && i < 16; ++i)
        TorcAdd(value, cases[2 + i], (uint8_t)(bits / 8 ? bits / 8 : 1));
}

void __sanitizer_cov_trace_div4(uint32_t value) { (void)value; }
void __sanitizer_cov_trace_div8(uint64_t value) { (void)value; }
void __sanitizer_cov_trace_gep(uintptr_t index) { (void)index; }

/* ---- dictionary: manual entries plus strings seen in libc compares ---- */

#define DICT_MAX 1024
#define DICT_WORD 64
typedef struct {
    uint8_t data[DICT_WORD];
    uint8_t size;
} word_t;
static word_t dict[DICT_MAX];
static int dict_count;
static word_t auto_dict[256];
static unsigned auto_dict_next;

static void AutoDictAdd(const void *data, size_t size)
{
    word_t *w;

    if (!running_target || size < 2 || size > DICT_WORD)
        return;
    w = &auto_dict[auto_dict_next++ % 256];
    memcpy(w->data, data, size);
    w->size = (uint8_t)size;
}

static void StringHook(const char *a, const char *b, size_t limit)
{
    size_t la, lb;

    if (!running_target || !a || !b)
        return;
    la = strnlen(a, limit < DICT_WORD ? limit : DICT_WORD);
    lb = strnlen(b, limit < DICT_WORD ? limit : DICT_WORD);
    AutoDictAdd(a, la);
    AutoDictAdd(b, lb);
}

void __sanitizer_weak_hook_strcmp(void *pc, const char *a, const char *b, int result)
{
    (void)pc;
    if (result)
        StringHook(a, b, DICT_WORD);
}

void __sanitizer_weak_hook_strcasecmp(void *pc, const char *a, const char *b, int result)
{
    (void)pc;
    if (result)
        StringHook(a, b, DICT_WORD);
}

void __sanitizer_weak_hook_strncmp(void *pc, const char *a, const char *b, size_t n, int result)
{
    (void)pc;
    if (result)
        StringHook(a, b, n);
}

void __sanitizer_weak_hook_strncasecmp(void *pc, const char *a, const char *b, size_t n, int result)
{
    (void)pc;
    if (result)
        StringHook(a, b, n);
}

void __sanitizer_weak_hook_memcmp(void *pc, const void *a, const void *b, size_t n, int result)
{
    (void)pc;
    if (result && n >= 2 && n <= DICT_WORD) {
        AutoDictAdd(a, n);
        AutoDictAdd(b, n);
    }
}

void __sanitizer_weak_hook_strstr(void *pc, const char *haystack, const char *needle, char *result)
{
    (void)pc;
    (void)haystack;
    if (!result && needle)
        AutoDictAdd(needle, strnlen(needle, DICT_WORD));
}

static int ParseDictLine(const char *line, word_t *w)
{
    const char *p = strchr(line, '"');
    size_t n = 0;

    if (!p || line[0] == '#')
        return 0;
    for (++p; *p && *p != '"'; ++p) {
        int c = (unsigned char)*p;
        if (c == '\\' && p[1]) {
            ++p;
            if (*p == 'x' && p[1] && p[2]) {
                char hex[3] = { p[1], p[2], 0 };
                c = (int)strtol(hex, NULL, 16);
                p += 2;
            } else {
                c = (unsigned char)*p;
            }
        }
        if (n == DICT_WORD)
            return 0;
        w->data[n++] = (uint8_t)c;
    }
    w->size = (uint8_t)n;
    return n > 0;
}

static void LoadDict(const char *path)
{
    char line[512];
    FILE *f = fopen(path, "r");

    if (!f) {
        fprintf(stderr, "driver: cannot read dictionary %s: %s\n", path, strerror(errno));
        exit(2);
    }
    while (fgets(line, sizeof(line), f) && dict_count < DICT_MAX) {
        if (ParseDictLine(line, &dict[dict_count]))
            ++dict_count;
    }
    fclose(f);
}

/* ---- corpus ---- */

typedef struct {
    uint8_t *data;
    size_t size;
} unit_t;
static unit_t *corpus;
static size_t corpus_count;
static size_t corpus_capacity;

static void CorpusAdd(const uint8_t *data, size_t size)
{
    unit_t *u;

    if (corpus_count == corpus_capacity) {
        corpus_capacity = corpus_capacity ? corpus_capacity * 2 : 256;
        corpus = realloc(corpus, corpus_capacity * sizeof(*corpus));
        if (!corpus)
            abort();
    }
    u = &corpus[corpus_count++];
    u->data = malloc(size ? size : 1);
    if (!u->data)
        abort();
    memcpy(u->data, data, size);
    u->size = size;
}

/* ---- artifacts ---- */

static const uint8_t *current_data;
static size_t current_size;
static int artifact_written;

static void Sha1(const uint8_t *data, size_t size, char out[41])
{
    uint32_t h[5] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0 };
    uint64_t bitlen = (uint64_t)size * 8;
    size_t total = ((size + 8) / 64 + 1) * 64;
    size_t off;
    int i;

    for (off = 0; off < total; off += 64) {
        uint32_t w[80];
        uint32_t a, b, c, d, e;
        for (i = 0; i < 64; ++i) {
            size_t pos = off + (size_t)i;
            uint8_t byte;
            if (pos < size)
                byte = data[pos];
            else if (pos == size)
                byte = 0x80;
            else if (pos >= total - 8)
                byte = (uint8_t)(bitlen >> (8 * (total - 1 - pos)));
            else
                byte = 0;
            if ((i & 3) == 0)
                w[i / 4] = 0;
            w[i / 4] |= (uint32_t)byte << (24 - 8 * (i & 3));
        }
        for (i = 16; i < 80; ++i) {
            uint32_t x = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (x << 1) | (x >> 31);
        }
        a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (i = 0; i < 80; ++i) {
            uint32_t f, k, t;
            if (i < 20)
                f = (b & c) | (~b & d), k = 0x5a827999;
            else if (i < 40)
                f = b ^ c ^ d, k = 0x6ed9eba1;
            else if (i < 60)
                f = (b & c) | (b & d) | (c & d), k = 0x8f1bbcdc;
            else
                f = b ^ c ^ d, k = 0xca62c1d6;
            t = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d, d = c, c = (b << 30) | (b >> 2), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    for (i = 0; i < 5; ++i)
        snprintf(out + 8 * i, 9, "%08x", h[i]);
}

static void MakeParentDir(const char *prefix)
{
    char dir[1024];
    char *slash;

    snprintf(dir, sizeof(dir), "%s", prefix);
    slash = strrchr(dir, '/');
    if (!slash)
        return;
    *slash = '\0';
    for (slash = dir + 1; *slash; ++slash) {
        if (*slash == '/') {
            *slash = '\0';
            mkdir(dir, 0755);
            *slash = '/';
        }
    }
    mkdir(dir, 0755);
}

static void WriteArtifact(const char *kind, const uint8_t *data, size_t size)
{
    char hash[41];
    char path[1200];
    int fd;

    if (artifact_written || !data)
        return;
    artifact_written = 1;
    Sha1(data, size, hash);
    MakeParentDir(opt_artifact_prefix);
    snprintf(path, sizeof(path), "%s%s-%s", opt_artifact_prefix, kind, hash);
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        size_t done = 0;
        while (done < size) {
            ssize_t n = write(fd, data + done, size - done);
            if (n <= 0)
                break;
            done += (size_t)n;
        }
        close(fd);
    }
    fprintf(stderr, "==driver== %s input (%zu bytes) written to %s; seed=%llu\n",
            kind, size, path, opt_seed);
}

static void DeathCallback(void)
{
    WriteArtifact("crash", current_data, current_size);
}

/* ---- timeouts ---- */

/* Measured in process CPU time: on a shared Mac under `taskpolicy -b` a
   starved process can stall for many wall-clock seconds without hanging. */
static volatile double run_started;

static double CpuSeconds(void)
{
    struct rusage usage;

    getrusage(RUSAGE_SELF, &usage);
    return (double)usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6 +
           (double)usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
}

static void AlarmHandler(int sig)
{
    (void)sig;
    if (running_target && CpuSeconds() - run_started > opt_timeout) {
        fprintf(stderr, "==driver== timeout: one input used more than %d s of CPU\n", opt_timeout);
        __sanitizer_print_stack_trace();
        WriteArtifact("timeout", current_data, current_size);
        _exit(70);
    }
}

static void StartWatchdog(void)
{
    struct itimerval timer;
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = AlarmHandler;
    sigaction(SIGALRM, &sa, NULL);
    memset(&timer, 0, sizeof(timer));
    timer.it_interval.tv_sec = 1;
    timer.it_value.tv_sec = 1;
    setitimer(ITIMER_REAL, &timer, NULL);
}

/* ---- running one input ---- */

static unsigned long long total_runs;

static void RunOne(const uint8_t *data, size_t size)
{
    /* A private copy lets ASan see reads past the end of the input. */
    uint8_t *copy = malloc(size ? size : 1);

    if (!copy)
        abort();
    memcpy(copy, data, size);
    current_data = copy;
    current_size = size;
    run_started = CpuSeconds();
    running_target = 1;
    LLVMFuzzerTestOneInput(copy, size);
    running_target = 0;
    run_started = 0;
    current_data = NULL;
    current_size = 0;
    free(copy);
    ++total_runs;
}

/* ---- mutations ---- */

static const int64_t interesting[] = {
    0, 1, -1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 100, 127, 128, 255, 256,
    511, 512, 1000, 1023, 1024, 1300, 1301, 4095, 4096, 8191, 8192, 16383,
    16384, 32767, 32768, 65535, 65536, 0x1ffff, 0x20000, 0x20001, 0x7fffffff,
    -2, -127, -128, -129, -32768, -32769, (int64_t)0x80000000u, (int64_t)0xffffffffu,
    -2147483647 - 1, 0x7fffffffffffffffll,
};

static size_t MutateEraseBytes(uint8_t *d, size_t size, size_t cap)
{
    size_t n, at;

    (void)cap;
    if (size <= 1)
        return size;
    n = 1 + RandBelow(size / 2 ? size / 2 : 1);
    at = RandBelow(size - n + 1);
    memmove(d + at, d + at + n, size - at - n);
    return size - n;
}

static size_t MutateInsertByte(uint8_t *d, size_t size, size_t cap)
{
    size_t at;

    if (size >= cap)
        return size;
    at = RandBelow(size + 1);
    memmove(d + at + 1, d + at, size - at);
    d[at] = (uint8_t)Rand64();
    return size + 1;
}

static size_t MutateInsertRepeated(uint8_t *d, size_t size, size_t cap)
{
    size_t room = cap - size;
    size_t n, at;
    uint8_t value;

    if (!room)
        return size;
    /* Mostly short runs; sometimes long enough to stress length limits. */
    if (RandBelow(16) == 0)
        n = 1 + RandBelow(room);
    else
        n = 1 + RandBelow(room < 128 ? room : 128);
    at = RandBelow(size + 1);
    value = RandBool() ? (uint8_t)(RandBool() ? 0x00 : 0xff) : (uint8_t)Rand64();
    memmove(d + at + n, d + at, size - at);
    memset(d + at, value, n);
    return size + n;
}

static size_t MutateChangeByte(uint8_t *d, size_t size, size_t cap)
{
    (void)cap;
    if (size)
        d[RandBelow(size)] = (uint8_t)Rand64();
    return size;
}

static size_t MutateChangeBit(uint8_t *d, size_t size, size_t cap)
{
    (void)cap;
    if (size)
        d[RandBelow(size)] ^= (uint8_t)(1u << RandBelow(8));
    return size;
}

static size_t MutateShuffle(uint8_t *d, size_t size, size_t cap)
{
    size_t n, at, i;

    (void)cap;
    if (size < 2)
        return size;
    n = 2 + RandBelow(size < 8 ? size - 1 : 7);
    at = RandBelow(size - n + 1);
    for (i = n - 1; i > 0; --i) {
        size_t j = RandBelow(i + 1);
        uint8_t t = d[at + i];
        d[at + i] = d[at + j];
        d[at + j] = t;
    }
    return size;
}

static size_t PutInteger(uint8_t *d, size_t size, size_t cap, size_t at, uint64_t v, int width, int insert)
{
    uint8_t bytes[8];
    int big = RandBool();
    int i;

    for (i = 0; i < width; ++i)
        bytes[big ? width - 1 - i : i] = (uint8_t)(v >> (8 * i));
    if (insert) {
        if (size + (size_t)width > cap)
            return size;
        if (at > size)
            at = size;
        memmove(d + at + width, d + at, size - at);
        memcpy(d + at, bytes, (size_t)width);
        return size + (size_t)width;
    }
    if (size < (size_t)width)
        return size;
    if (at > size - (size_t)width)
        at = size - (size_t)width;
    memcpy(d + at, bytes, (size_t)width);
    return size;
}

static size_t MutateBinaryInteger(uint8_t *d, size_t size, size_t cap)
{
    static const int widths[] = { 1, 2, 4, 8 };
    int width = widths[RandBelow(4)];
    uint64_t v;
    size_t at = RandBelow(size + 1);

    if (RandBool()) {
        v = (uint64_t)interesting[RandBelow(sizeof(interesting) / sizeof(interesting[0]))];
    } else if (size >= (size_t)width && at <= size - (size_t)width) {
        uint64_t old = 0;
        int i;
        for (i = 0; i < width; ++i)
            old |= (uint64_t)d[at + (size_t)i] << (8 * i);
        v = old + (uint64_t)(int64_t)((int)RandBelow(71) - 35);
    } else {
        v = Rand64();
    }
    return PutInteger(d, size, cap, at, v, width, RandBelow(4) == 0);
}

static size_t MutateAsciiInteger(uint8_t *d, size_t size, size_t cap)
{
    char text[32];
    size_t start, end, len, i;
    long long value;

    if (!size)
        return size;
    start = RandBelow(size);
    while (start < size && (d[start] < '0' || d[start] > '9'))
        ++start;
    if (start == size)
        return size;
    end = start;
    value = 0;
    while (end < size && d[end] >= '0' && d[end] <= '9' && end - start < 18) {
        value = value * 10 + (d[end] - '0');
        ++end;
    }
    switch (RandBelow(5)) {
    case 0:
        ++value;
        break;
    case 1:
        --value;
        break;
    case 2:
        value *= 2;
        break;
    case 3:
        value = interesting[RandBelow(sizeof(interesting) / sizeof(interesting[0]))];
        break;
    default:
        value = (long long)RandBelow(1 << 20);
        break;
    }
    len = (size_t)snprintf(text, sizeof(text), "%lld", value);
    if (size - (end - start) + len > cap)
        return size;
    memmove(d + start + len, d + end, size - end);
    for (i = 0; i < len; ++i)
        d[start + i] = (uint8_t)text[i];
    return size - (end - start) + len;
}

static size_t MutateCopyPart(uint8_t *d, size_t size, size_t cap)
{
    size_t n, from, to;
    uint8_t *tmp;

    if (size < 2)
        return size;
    n = 1 + RandBelow(size / 2);
    from = RandBelow(size - n + 1);
    if (RandBool() || size + n > cap) {
        to = RandBelow(size - n + 1);
        memmove(d + to, d + from, n);
        return size;
    }
    tmp = malloc(n);
    if (!tmp)
        return size;
    memcpy(tmp, d + from, n);
    to = RandBelow(size + 1);
    memmove(d + to + n, d + to, size - to);
    memcpy(d + to, tmp, n);
    free(tmp);
    return size + n;
}

static size_t MutateCrossOver(uint8_t *d, size_t size, size_t cap)
{
    const unit_t *other;
    size_t n, from, to;

    if (!corpus_count)
        return size;
    other = &corpus[RandBelow(corpus_count)];
    if (!other->size)
        return size;
    n = 1 + RandBelow(other->size);
    from = RandBelow(other->size - n + 1);
    if (RandBool()) {
        if (n > size)
            n = size;
        if (!n)
            return size;
        to = RandBelow(size - n + 1);
        memcpy(d + to, other->data + from, n);
        return size;
    }
    if (size + n > cap)
        n = cap - size;
    if (!n)
        return size;
    to = RandBelow(size + 1);
    memmove(d + to + n, d + to, size - to);
    memcpy(d + to, other->data + from, n);
    return size + n;
}

static size_t PutWord(uint8_t *d, size_t size, size_t cap, const word_t *w)
{
    size_t at;

    if (!w->size)
        return size;
    if (RandBool() && size >= w->size) {
        at = RandBelow(size - w->size + 1);
        memcpy(d + at, w->data, w->size);
        return size;
    }
    if (size + w->size > cap)
        return size;
    at = RandBelow(size + 1);
    memmove(d + at + w->size, d + at, size - at);
    memcpy(d + at, w->data, w->size);
    return size + w->size;
}

static size_t MutateDictionary(uint8_t *d, size_t size, size_t cap)
{
    if (dict_count && (RandBool() || !auto_dict_next))
        return PutWord(d, size, cap, &dict[RandBelow((size_t)dict_count)]);
    if (auto_dict_next) {
        size_t n = auto_dict_next < 256 ? auto_dict_next : 256;
        return PutWord(d, size, cap, &auto_dict[RandBelow(n)]);
    }
    return size;
}

/* Replace an operand seen in a comparison with its counterpart. */
static size_t MutateTorc(uint8_t *d, size_t size, size_t cap)
{
    int attempt;

    for (attempt = 0; attempt < 8; ++attempt) {
        unsigned slot = (unsigned)RandBelow(TORC_SIZE);
        uint64_t a = torc[slot].a;
        uint64_t b = torc[slot].b;
        int width = torc[slot].width;
        size_t start, i;
        int big;

        if (!width)
            continue;
        if (RandBool()) {
            uint64_t t = a;
            a = b;
            b = t;
        }
        if (size < (size_t)width) {
            return PutInteger(d, size, cap, 0, b, width, 1);
        }
        big = RandBool();
        start = RandBelow(size);
        for (i = 0; i + (size_t)width <= size; ++i) {
            size_t at = (start + i) % (size - (size_t)width + 1);
            uint64_t v = 0;
            int k;
            for (k = 0; k < width; ++k)
                v |= (uint64_t)d[at + (size_t)(big ? width - 1 - k : k)] << (8 * k);
            if (v == a) {
                for (k = 0; k < width; ++k)
                    d[at + (size_t)(big ? width - 1 - k : k)] = (uint8_t)(b >> (8 * k));
                return size;
            }
        }
        /* Operand absent: insert the wanted value somewhere. */
        return PutInteger(d, size, cap, RandBelow(size + 1), b, width, RandBool());
    }
    return size;
}

typedef size_t (*mutator_t)(uint8_t *, size_t, size_t);
static const mutator_t mutators[] = {
    MutateEraseBytes, MutateInsertByte, MutateInsertRepeated, MutateChangeByte,
    MutateChangeBit, MutateShuffle, MutateBinaryInteger, MutateAsciiInteger,
    MutateCopyPart, MutateCrossOver, MutateDictionary, MutateDictionary, MutateTorc,
    MutateTorc,
};

static size_t Mutate(uint8_t *d, size_t size, size_t cap)
{
    int count = 1 + (int)RandBelow(RandBool() ? 2 : 8);
    int i;

    for (i = 0; i < count; ++i) {
        size_t n = mutators[RandBelow(sizeof(mutators) / sizeof(mutators[0]))](d, size, cap);
        size = n;
    }
    if (!size && cap)
        d[size++] = (uint8_t)Rand64();
    return size;
}

/* ---- files ---- */

static uint8_t *ReadFile(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    uint8_t *data;
    long len;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)len + 1);
    if (!data || fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)len;
    return data;
}

static int CompareNames(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Loads every regular file in a directory, in name order for determinism. */
static void LoadDirectory(const char *dir, void (*use)(const char *, const uint8_t *, size_t))
{
    DIR *d = opendir(dir);
    struct dirent *entry;
    char **names = NULL;
    size_t count = 0, capacity = 0, i;

    if (!d) {
        fprintf(stderr, "driver: cannot open %s: %s\n", dir, strerror(errno));
        exit(2);
    }
    while ((entry = readdir(d))) {
        if (entry->d_name[0] == '.')
            continue;
        if (count == capacity) {
            capacity = capacity ? capacity * 2 : 64;
            names = realloc(names, capacity * sizeof(*names));
        }
        names[count++] = strdup(entry->d_name);
    }
    closedir(d);
    qsort(names, count, sizeof(*names), CompareNames);
    for (i = 0; i < count; ++i) {
        char path[2048];
        struct stat st;
        uint8_t *data;
        size_t size = 0;

        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (stat(path, &st) == 0 && S_ISREG(st.st_mode) && (data = ReadFile(path, &size))) {
            use(path, data, size);
            free(data);
        }
        free(names[i]);
    }
    free(names);
}

static void SaveCorpusUnit(const uint8_t *data, size_t size)
{
    char hash[41];
    char path[2048];
    FILE *f;

    if (!opt_corpus_out)
        return;
    Sha1(data, size, hash);
    snprintf(path, sizeof(path), "%s/%s", opt_corpus_out, hash);
    f = fopen(path, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
}

static void SeedUnit(const char *path, const uint8_t *data, size_t size)
{
    (void)path;
    if (size > opt_max_len)
        size = opt_max_len;
    ClearCounters();
    RunOne(data, size);
    /* Seeds are always kept so crossover can use every hand-made packet. */
    CollectFeatures(1);
    CorpusAdd(data, size);
}

/* ---- modes ---- */

static double Now(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + tv.tv_usec / 1e6;
}

static int Fuzz(int dirc, char **dirv)
{
    double start = Now();
    double last_print = start;
    uint8_t *buf = malloc(opt_max_len + 1);
    int i;

    if (!buf)
        abort();
    for (i = 0; i < dirc; ++i)
        LoadDirectory(dirv[i], SeedUnit);
    if (!corpus_count) {
        uint8_t zero = 0;
        SeedUnit("<empty>", &zero, 1);
    }
    fprintf(stderr, "==driver== seed=%llu corpus=%zu counters=%zu features=%zu max_len=%zu\n",
            opt_seed, corpus_count, total_counters, total_features, opt_max_len);
    while (opt_runs < 0 || (long)total_runs < opt_runs) {
        const unit_t *base;
        size_t size;
        double now;

        /* Prefer recent finds, which usually sit on the coverage frontier. */
        if (corpus_count > 8 && RandBool())
            base = &corpus[corpus_count - 1 - RandBelow(corpus_count / 4 + 1)];
        else
            base = &corpus[RandBelow(corpus_count)];
        size = base->size < opt_max_len ? base->size : opt_max_len;
        memcpy(buf, base->data, size);
        size = Mutate(buf, size, opt_max_len);
        ClearCounters();
        RunOne(buf, size);
        if (CollectFeatures(1)) {
            CorpusAdd(buf, size);
            SaveCorpusUnit(buf, size);
        }
        if ((total_runs & 255) == 0) {
            now = Now();
            if (opt_max_total_time && now - start >= opt_max_total_time)
                break;
            if (now - last_print >= opt_print_every) {
                fprintf(stderr, "#%llu cov: %zu corp: %zu exec/s: %.0f\n", total_runs,
                        total_features, corpus_count, total_runs / (now - start));
                last_print = now;
            }
        }
    }
    fprintf(stderr, "==driver== done: %llu runs in %.0f s, %zu features, corpus %zu, seed=%llu, no crash\n",
            total_runs, Now() - start, total_features, corpus_count, opt_seed);
    free(buf);
    return 0;
}

static int Regress(int filec, char **filev)
{
    int i;

    for (i = 0; i < filec; ++i) {
        size_t size = 0;
        uint8_t *data = ReadFile(filev[i], &size);
        if (!data) {
            fprintf(stderr, "driver: cannot read %s\n", filev[i]);
            return 2;
        }
        fprintf(stderr, "==driver== running %s (%zu bytes)\n", filev[i], size);
        RunOne(data, size);
        free(data);
    }
    fprintf(stderr, "==driver== %d input(s) passed\n", filec);
    return 0;
}

/* Runs an input in a child and returns its sanitizer summary ("" if clean). */
static int ChildSummary(const uint8_t *data, size_t size, char *summary, size_t cap)
{
    char path[1024];
    const char *tmp = getenv("TMPDIR");
    int fd;
    pid_t pid;
    int status = 0;
    FILE *f;
    char line[1024];

    summary[0] = '\0';
    snprintf(path, sizeof(path), "%s/fuzz-minimize-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    fd = mkstemp(path);
    if (fd < 0)
        return -1;
    pid = fork();
    if (pid == 0) {
        dup2(fd, 2);
        artifact_written = 1;
        StartWatchdog();
        RunOne(data, size);
        _exit(0);
    }
    waitpid(pid, &status, 0);
    close(fd);
    f = fopen(path, "r");
    while (f && fgets(line, sizeof(line), f)) {
        if (!strncmp(line, "SUMMARY:", 8) || strstr(line, "==driver== timeout")) {
            char *p, *o = summary;
            /* Addresses and line numbers vary; keep the error kind and function. */
            for (p = line; *p && *p != '\n' && (size_t)(o - summary) < cap - 1; ++p) {
                if (p[0] == '0' && p[1] == 'x') {
                    p += 2;
                    while (*p && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
                        ++p;
                    --p;
                    continue;
                }
                if (*p == ':' && p[1] >= '0' && p[1] <= '9') {
                    while (p[1] == ':' || (p[1] >= '0' && p[1] <= '9'))
                        ++p;
                    continue;
                }
                *o++ = *p;
            }
            *o = '\0';
            break;
        }
    }
    if (f)
        fclose(f);
    unlink(path);
    if (!summary[0] && !(WIFEXITED(status) && WEXITSTATUS(status) == 0))
        snprintf(summary, cap, "abnormal exit status %d", status);
    return 0;
}

static int Minimize(const char *file)
{
    size_t size = 0;
    uint8_t *data = ReadFile(file, &size);
    uint8_t *trial;
    char want[1024], got[1024];
    size_t chunk;
    long attempts = 0;
    long budget = opt_runs > 0 ? opt_runs : 4096;

    if (!data) {
        fprintf(stderr, "driver: cannot read %s\n", file);
        return 2;
    }
    ChildSummary(data, size, want, sizeof(want));
    if (!want[0]) {
        fprintf(stderr, "==driver== %s does not crash; nothing to minimize\n", file);
        return 2;
    }
    fprintf(stderr, "==driver== minimizing %zu bytes: %s\n", size, want);
    trial = malloc(size + 1);
    for (chunk = size / 2; chunk >= 1 && attempts < budget; chunk /= 2) {
        size_t at = 0;
        while (at < size && attempts < budget) {
            size_t n = chunk < size - at ? chunk : size - at;
            memcpy(trial, data, at);
            memcpy(trial + at, data + at + n, size - at - n);
            ++attempts;
            ChildSummary(trial, size - n, got, sizeof(got));
            if (!strcmp(got, want)) {
                memcpy(data, trial, size - n);
                size -= n;
            } else {
                at += n;
            }
        }
        if (chunk == 1)
            break;
    }
    /* Then make the remaining bytes as plain as possible. */
    for (chunk = 0; chunk < size && attempts < budget; ++chunk) {
        static const uint8_t simple[] = { '0', 0, ' ', 'a' };
        size_t k;
        for (k = 0; k < sizeof(simple) && attempts < budget; ++k) {
            uint8_t old = data[chunk];
            if (old == simple[k])
                break;
            data[chunk] = simple[k];
            ++attempts;
            ChildSummary(data, size, got, sizeof(got));
            if (!strcmp(got, want))
                break;
            data[chunk] = old;
        }
    }
    artifact_written = 0;
    WriteArtifact("minimized", data, size);
    fprintf(stderr, "==driver== minimized to %zu bytes after %ld runs\n", size, attempts);
    free(trial);
    free(data);
    return 0;
}

static int ParseOption(const char *arg)
{
    const char *eq = strchr(arg, '=');
    const char *value;

    if (arg[0] != '-' || !eq)
        return 0;
    value = eq + 1;
#define OPTION(name) (!strncmp(arg + 1, name, (size_t)(eq - arg - 1)) && strlen(name) == (size_t)(eq - arg - 1))
    if (OPTION("runs"))
        opt_runs = atol(value);
    else if (OPTION("max_total_time"))
        opt_max_total_time = atol(value);
    else if (OPTION("seed"))
        opt_seed = strtoull(value, NULL, 10), opt_seed_given = 1;
    else if (OPTION("max_len"))
        opt_max_len = (size_t)atol(value);
    else if (OPTION("timeout"))
        opt_timeout = atoi(value);
    else if (OPTION("artifact_prefix"))
        opt_artifact_prefix = value;
    else if (OPTION("dict"))
        opt_dict = value;
    else if (OPTION("corpus_out"))
        opt_corpus_out = value;
    else if (OPTION("minimize_crash"))
        opt_minimize = atoi(value);
    else if (OPTION("print_every"))
        opt_print_every = atoi(value);
    else
        fprintf(stderr, "driver: ignoring unknown option %s\n", arg);
#undef OPTION
    return 1;
}

int main(int argc, char **argv)
{
    char **paths = calloc((size_t)argc, sizeof(*paths));
    int count = 0, dirs = 0, i;

    for (i = 1; i < argc; ++i) {
        if (!ParseOption(argv[i])) {
            struct stat st;
            if (stat(argv[i], &st) != 0) {
                fprintf(stderr, "driver: no such file or directory: %s\n", argv[i]);
                return 2;
            }
            dirs += S_ISDIR(st.st_mode) != 0;
            paths[count++] = argv[i];
        }
    }
    if (dirs && dirs != count) {
        fprintf(stderr, "driver: pass either corpus directories or input files, not both\n");
        return 2;
    }
    if (!opt_seed_given)
        opt_seed = (unsigned long long)time(NULL) ^ ((unsigned long long)getpid() << 16);
    rng_state = opt_seed;
    if (opt_dict)
        LoadDict(opt_dict);
    LLVMFuzzerInitialize(&argc, &argv);
    __sanitizer_set_death_callback(DeathCallback);
    StartWatchdog();
    if (opt_minimize)
        return count == 1 ? Minimize(paths[0]) : 2;
    if (count && !dirs)
        return Regress(count, paths);
    return Fuzz(count, paths);
}
