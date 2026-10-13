#if defined(COD2_X64) && !defined(DEDICATED)
#include "match_summary.h"
#include "common_types.h"
#if defined(COD2_CODX) && COD2_CODX
#include "cod2x.h"
#endif
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

extern const dvar_t *fs_homepath;
extern const dvar_t *com_maxfps;
extern const clientConnection_t *clc;
extern const dvar_t *Dvar_RegisterBool_mac(const char *name, int value, int flags);
extern void Dvar_AddFlags(const dvar_t *dvar, int flags);
extern const char *CL_GetConfigString(int index);
extern char *Info_ValueForKey(const char *info, const char *key);
extern char *NET_AdrToString(netadr_t address);

enum { MATCH_HISTOGRAM_LIMIT = 10000, MATCH_RECORD_LIMIT = 200, MATCH_LINE_LIMIT = 8192 };
static int matchEnabled;
static struct {
    int active;
    uint64_t started, frames, underCap, capFrames, pingSum, pingSamples, packets, lost, snapshots;
    uint64_t histogram[MATCH_HISTOGRAM_LIMIT + 1];
    int frameMax, cap, pingMax;
    char map[64], mode[32], address[256], serverInfo[8192], systemInfo[8192];
} match;

static uint64_t MatchSummary_Milliseconds(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
}

void MatchSummary_Init(void)
{
    /* Startup +set is consumed before client initialization. Freeze the local
       privacy choice so neither a server nor an imported cfg can enable it. */
    const dvar_t *privacy = Dvar_RegisterBool_mac("cl_matchSummaries", 1, 0x1000);
    matchEnabled = privacy->current.enabled;
    Dvar_AddFlags(privacy, 0x40);
    memset(&match, 0, sizeof(match));
}

static void MatchSummary_Begin(void)
{
    memset(&match, 0, sizeof(match));
    match.active = 1;
    match.started = MatchSummary_Milliseconds();
    match.cap = com_maxfps->current.integer;
#if defined(COD2_CODX) && COD2_CODX
    match.cap = Cod2x_FrameFPS(match.cap);
#endif
    snprintf(match.serverInfo, sizeof(match.serverInfo), "%s", CL_GetConfigString(0));
    snprintf(match.systemInfo, sizeof(match.systemInfo), "%s", CL_GetConfigString(1));
    snprintf(match.map, sizeof(match.map), "%s", Info_ValueForKey(match.serverInfo, "mapname"));
    snprintf(match.mode, sizeof(match.mode), "%s", Info_ValueForKey(match.serverInfo, "g_gametype"));
    snprintf(match.address, sizeof(match.address), "%s", NET_AdrToString(clc->serverAddress));
}

void MatchSummary_Frame(int rawMsec, int frameBudgetMsec, int capFPS)
{
    if (!match.active || clc->state != CA_ACTIVE || clc->demoplaying || rawMsec < 0)
        return;
    ++match.frames;
    ++match.histogram[rawMsec < MATCH_HISTOGRAM_LIMIT ? rawMsec : MATCH_HISTOGRAM_LIMIT];
    if (rawMsec > match.frameMax)
        match.frameMax = rawMsec;
    match.cap = capFPS;
    if (capFPS > 0) {
        ++match.capFrames;
        if (rawMsec <= frameBudgetMsec)
            ++match.underCap;
    }
}

void MatchSummary_Snapshot(int ping, int pingMeasured, int intermission)
{
    if (!matchEnabled || clc->demoplaying)
        return;
    if (!match.active) {
        if (intermission)
            return;
        MatchSummary_Begin();
    }
    ++match.snapshots;
    if (pingMeasured && ping >= 0) {
        match.pingSum += (unsigned)ping;
        ++match.pingSamples;
        if (ping > match.pingMax)
            match.pingMax = ping;
    }
    if (intermission)
        MatchSummary_Finish("intermission");
}

void MatchSummary_Packet(int dropped)
{
    if (!match.active || clc->demoplaying)
        return;
    ++match.packets;
    if (dropped > 0)
        match.lost += (unsigned)dropped;
}

static int MatchSummary_Quantile(uint64_t rank)
{
    uint64_t count = 0;
    int i;
    for (i = 0; i < MATCH_HISTOGRAM_LIMIT; ++i) {
        count += match.histogram[i];
        if (count >= rank)
            return i;
    }
    /* Overflow quantiles use the observed maximum: a finite upper bound. */
    return match.frameMax;
}

static void MatchSummary_String(FILE *file, const char *value)
{
    const unsigned char *p;
    fputc('"', file);
    for (p = (const unsigned char *)value; *p; ++p) {
        if (*p == '"' || *p == '\\')
            fprintf(file, "\\%c", *p);
        else if (*p < 32 || *p >= 127)
            fprintf(file, "\\u%04x", *p);
        else
            fputc(*p, file);
    }
    fputc('"', file);
}

static void MatchSummary_Record(FILE *file, const char *reason)
{
    static const char *hints[] = { "g_competitive", "sv_maxfps", "g_maxfps", "sv_pure", "gamename", "shortversion", "protocol", "mod", "fs_game", "zpam_version" };
    struct timespec now;
    struct tm utc;
    char stamp[32];
    uint64_t elapsed = MatchSummary_Milliseconds() - match.started;
    int i, count = 0;
    clock_gettime(CLOCK_REALTIME, &now);
    gmtime_r(&now.tv_sec, &utc);
    strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    fprintf(file, "{\"schemaVersion\":1,\"endedAt\":\"%s\",\"endReason\":", stamp);
    MatchSummary_String(file, reason);
    fputs(",\"map\":", file); MatchSummary_String(file, match.map);
    fputs(",\"mode\":", file); MatchSummary_String(file, match.mode);
    fprintf(file, ",\"durationSeconds\":%.3f,\"serverAddress\":", (double)elapsed / 1000.0);
    MatchSummary_String(file, match.address);
    fputs(",\"ruleHints\":{", file);
    for (i = 0; i < (int)(sizeof(hints) / sizeof(hints[0])); ++i) {
        char hint[64];
        const char *value = Info_ValueForKey(match.serverInfo, hints[i]);
        if (!*value)
            value = Info_ValueForKey(match.systemInfo, hints[i]);
        if (!*value)
            continue;
        snprintf(hint, sizeof(hint), "%s", value);
        if (count++) fputc(',', file);
        MatchSummary_String(file, hints[i]); fputc(':', file); MatchSummary_String(file, hint);
    }
    fprintf(file, "},\"frameCount\":%llu,\"frameTimeP50MS\":", (unsigned long long)match.frames);
    if (match.frames) {
        fprintf(file, "%d,\"frameTimeP99MS\":%d,\"frameTimeMaxMS\":%d",
                MatchSummary_Quantile(match.frames / 2 + match.frames % 2),
                MatchSummary_Quantile(match.frames - match.frames / 100), match.frameMax);
    } else {
        fputs("null,\"frameTimeP99MS\":null,\"frameTimeMaxMS\":null", file);
    }
    fprintf(file, ",\"frameTimeHistogramLimitMS\":%d,\"framesUnderCap\":", MATCH_HISTOGRAM_LIMIT);
    if (match.capFrames) fprintf(file, "%llu", (unsigned long long)match.underCap);
    else fputs("null", file);
    fprintf(file, ",\"capSampleFrames\":%llu,\"capFPS\":%d,\"pingAverageMS\":", (unsigned long long)match.capFrames, match.cap);
    if (match.pingSamples) {
        fprintf(file, "%.3f,\"pingMaximumMS\":%d", (double)match.pingSum / (double)match.pingSamples, match.pingMax);
    } else {
        fputs("null,\"pingMaximumMS\":null", file);
    }
    fprintf(file, ",\"pingSamples\":%llu,\"packetsReceived\":%llu,\"packetsLost\":%llu,\"packetLossPercent\":",
            (unsigned long long)match.pingSamples, (unsigned long long)match.packets, (unsigned long long)match.lost);
    if (match.packets) fprintf(file, "%.3f", 100.0 * (double)match.lost / (double)(match.packets + match.lost));
    else fputs("null", file);
    fprintf(file, ",\"packetLossSource\":\"netchan sequence gaps\",\"snapshotsReceived\":%llu}\n", (unsigned long long)match.snapshots);
}

void MatchSummary_Finish(const char *reason)
{
    char temporary[96];
    char (*lines)[MATCH_LINE_LIMIT];
    FILE *input, *output;
    struct stat status;
    unsigned count = 0, i, keep;
    int homeFD, mainFD, statsFD, lockFD, fd, complete;
    if (!matchEnabled || !match.active)
        return;
    match.active = 0;
    homeFD = open(fs_homepath->current.string, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    if (homeFD < 0) return;
    mkdirat(homeFD, "main", 0700);
    mainFD = openat(homeFD, "main", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    close(homeFD);
    if (mainFD < 0) return;
    mkdirat(mainFD, "stats", 0700);
    statsFD = openat(mainFD, "stats", O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
    close(mainFD);
    if (statsFD < 0) return;
    /* This inode stays in place when the history is atomically replaced. Hold
       its lock from the initial read through retention and publication. */
    lockFD = openat(statsFD, ".matches.lock", O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK, 0600);
    if (lockFD < 0) {
        close(statsFD);
        return;
    }
    if (fstat(lockFD, &status) || !S_ISREG(status.st_mode) ||
        fchmod(lockFD, 0600) || flock(lockFD, LOCK_EX)) {
        close(lockFD); close(statsFD); return;
    }
    lines = malloc(MATCH_RECORD_LIMIT * MATCH_LINE_LIMIT);
    if (!lines) {
        close(lockFD);
        close(statsFD);
        return;
    }
    fd = openat(statsFD, "matches.jsonl", O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0 && errno != ENOENT) {
        free(lines); close(lockFD); close(statsFD); return;
    }
    if (fd >= 0 && (fstat(fd, &status) || !S_ISREG(status.st_mode))) {
        close(fd); free(lines); close(lockFD); close(statsFD); return;
    }
    input = fd < 0 ? NULL : fdopen(fd, "r");
    if (fd >= 0 && !input) close(fd);
    if (input) {
        while (fgets(lines[count % MATCH_RECORD_LIMIT], MATCH_LINE_LIMIT, input)) {
            size_t length = strlen(lines[count % MATCH_RECORD_LIMIT]);
            if (length && lines[count % MATCH_RECORD_LIMIT][length - 1] == '\n')
                ++count;
            else {
                int c;
                while ((c = fgetc(input)) != EOF && c != '\n') {}
            }
        }
        fclose(input);
    }
    snprintf(temporary, sizeof(temporary), ".matches-%ld-%llu.tmp", (long)getpid(), (unsigned long long)MatchSummary_Milliseconds());
    fd = openat(statsFD, temporary, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    output = fd < 0 ? NULL : fdopen(fd, "w");
    if (output) {
        keep = count < MATCH_RECORD_LIMIT - 1 ? count : MATCH_RECORD_LIMIT - 1;
        for (i = count - keep; i < count; ++i)
            fputs(lines[i % MATCH_RECORD_LIMIT], output);
        MatchSummary_Record(output, reason);
        complete = !ferror(output);
        if (fclose(output)) complete = 0;
        if (complete) renameat(statsFD, temporary, statsFD, "matches.jsonl");
    } else if (fd >= 0) {
        close(fd);
    }
    if (fd >= 0) unlinkat(statsFD, temporary, 0);
    close(lockFD);
    close(statsFD);
    free(lines);
}
#endif
