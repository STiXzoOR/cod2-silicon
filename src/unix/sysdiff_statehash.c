#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common_types.h"

extern struct serverStatic_t svs;
extern level_locals_t        level;
extern gentity_t             g_entities[];   /* real def: gentity_s g_entities[1024] in bss.c */

static uint32_t sd_tab[256];
static int      sd_ready;

static uint32_t sd_crc(uint32_t crc, const void *buf, int len)
{
    if (!sd_ready) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            sd_tab[i] = c;
        }
        sd_ready = 1;
    }
    const unsigned char *p = (const unsigned char *)buf;
    crc = ~crc;
    while (len-- > 0)
        crc = sd_tab[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

/* Quantized hash of the float-bearing fields, robust to last-ULP codegen
 * differences between the x86 and x64 builds. Origins/angles are rounded to
 * 1/64 (well below any gameplay-relevant precision) so real logic divergence
 * still shows while float rounding noise does not. */
static uint32_t sd_q(uint32_t crc, float f)
{
    int32_t q = (int32_t)(f * 64.0f + (f >= 0.0f ? 0.5f : -0.5f));
    return sd_crc(crc, &q, sizeof q);
}

/* Normalized per-entity hash for CROSS-RUN comparison. Excludes absolute-clock
 * fields (pos.trTime, apos.trTime, .time, .time2) which depend on how long the
 * map took to load and therefore differ between two independent runs even when
 * the simulation is identical. Keeps the simulation-meaningful state: type,
 * flags, trajectory shape, position/angles (quantized), weapon, animation. */
static uint32_t sd_hash_estate(uint32_t crc, const entityState_t *s)
{
    /* Conservative always-initialized set: identity, type, flags, trajectory
     * shape, position & angles. Deliberately omits type-specific fields
     * (weapon/legsAnim/torsoAnim/trDuration) that are left uninitialized for
     * entity types that don't use them and therefore carry run-varying garbage. */
    crc = sd_crc(crc, &s->number, sizeof s->number);
    crc = sd_crc(crc, &s->eType, sizeof s->eType);
    crc = sd_crc(crc, &s->eFlags, sizeof s->eFlags);
    crc = sd_crc(crc, &s->pos.trType, sizeof s->pos.trType);
    crc = sd_crc(crc, &s->apos.trType, sizeof s->apos.trType);
    for (int j = 0; j < 3; j++) crc = sd_q(crc, s->pos.trBase[j]);
    for (int j = 0; j < 3; j++) crc = sd_q(crc, s->pos.trDelta[j]);
    for (int j = 0; j < 3; j++) crc = sd_q(crc, s->apos.trBase[j]);
    for (int j = 0; j < 3; j++) crc = sd_q(crc, s->apos.trDelta[j]);
    return crc;
}

/* Seed override for deterministic A/B runs. If SYSDIFF_SEED is set, every RNG
 * seeded through this returns the fixed value instead of a wallclock seed, so
 * the two builds run an identical simulation. Otherwise returns the real seed
 * (default build behavior is byte-identical). */
int Sys_DiffSeed(int fallback)
{
    static int cached = -1;   /* -2 = env absent, else the fixed seed */
    if (cached == -1) {
        const char *s = getenv("SYSDIFF_SEED");
        cached = (s && *s) ? atoi(s) : -2;
    }
    return (cached == -2) ? fallback : cached;
}

void Sys_StateHashFrame(void)
{
    static FILE *fp;
    static int   on = -1;
    static int   maxframes;
    static int   emitted;
    static int   last_frame;
    static int   have_last;

    if (on < 0) {
        const char *path = getenv("SYSDIFF_STATEHASH");
        on = (path && *path) ? 1 : 0;
        if (on) { fp = fopen(path, "w"); if (!fp) on = 0; }
        const char *mf = getenv("SYSDIFF_MAXFRAMES");
        maxframes = (mf && *mf) ? atoi(mf) : 0;
    }
    if (!on) return;

    /* Emit exactly one record per SIM frame (level.framenum), so the two builds'
     * traces align by frame index — immune to the wallclock-dependent load time
     * that shifts the absolute svs.time base between independent runs. */
    int fn = level.framenum;
    if (have_last && fn == last_frame) return;
    if (fn <= 0) return;                 /* no level running yet */
    last_frame = fn;
    have_last = 1;

    uint32_t raw = 0;   /* strict: full struct bytes (incl. absolute times) */
    uint32_t q   = 0;   /* normalized: comparable across runs / arches       */
    int n = level.num_entities;
    int active = 0;
    if (n > 0 && n <= 4096) {
        for (int i = 0; i < n; i++) {
            if (!g_entities[i].r.inuse) continue;   /* skip free/stale slots */
            raw = sd_crc(raw, &g_entities[i].s, sizeof(entityState_t));
            q   = sd_hash_estate(q, &g_entities[i].s);
            active++;
        }
    }

    fprintf(fp, "frame %d simt %d ents %d raw %08x q %08x\n",
            fn, level.time - level.startTime, active, raw, q);
    fflush(fp);

    if (maxframes && ++emitted >= maxframes) {
        const char *dp = getenv("SYSDIFF_DUMP");
        if (dp && *dp) {
            FILE *df = fopen(dp, "w");
            if (df) {
                for (int i = 0; i < n && i <= 4096; i++) {
                    if (!g_entities[i].r.inuse) continue;
                    const entityState_t *s = &g_entities[i].s;
                    fprintf(df, "e%d eq=%08x num=%d type=%d fl=%08x org=%.9g,%.9g,%.9g "
                                "ang=%.3f,%.3f,%.3f vel=%.3f,%.3f,%.3f avel=%.3f,%.3f,%.3f "
                                "trT=%d apT=%d\n",
                            i, sd_hash_estate(0, s), s->number, s->eType, s->eFlags,
                            s->pos.trBase[0], s->pos.trBase[1], s->pos.trBase[2],
                            s->apos.trBase[0], s->apos.trBase[1], s->apos.trBase[2],
                            s->pos.trDelta[0], s->pos.trDelta[1], s->pos.trDelta[2],
                            s->apos.trDelta[0], s->apos.trDelta[1], s->apos.trDelta[2],
                            s->pos.trType, s->apos.trType);
                }
                fclose(df);
            }
        }
        fclose(fp);
        _Exit(0);   /* immediate, no atexit teardown; trace already flushed */
    }
}
