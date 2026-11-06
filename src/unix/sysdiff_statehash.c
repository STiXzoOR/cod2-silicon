#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "common_types.h"

extern struct serverStatic_t svs;
extern level_locals_t        level;
extern gentity_t             g_entities[];   /* real def: gentity_s g_entities[1024] in bss.c */
#if defined(COD2_X64)
#include <limits.h>
#include <string.h>
static void sd_players_frame(int frame);
static void sd_finish_commands(int frame);
#endif

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
#if defined(COD2_X64)
    sd_players_frame(fn);
#endif

    if (maxframes && ++emitted >= maxframes) {
#if defined(COD2_X64)
        sd_finish_commands(fn);
#endif
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
#if defined(COD2_X64)
extern void SV_ClientThink(client_t *cl, usercmd_t *cmd);

static FILE *sd_commands, *sd_replay;
static int sd_mode = -1, sd_injecting, sd_next_frame, sd_next_client, sd_end_frame;
static usercmd_t sd_next_cmd;

static void sd_fail(const char *message)
{
    fprintf(stderr, "SYSDIFF harness: %s\n", message);
    fflush(stderr);
    _Exit(2);
}

static FILE *sd_open(const char *path, const char *mode)
{
    FILE *fp = fopen(path, mode);
    if (!fp)
        sd_fail("cannot open trace/command file");
    return fp;
}

/* Read ahead one event. The terminal frame makes a truncated input fail even
 * when its last complete command happens to precede the requested stop. */
static void sd_read_command(void)
{
    char line[256], extra;
    int frame, client, time, weapon, offhand, a0, a1, a2, forward, right;
    unsigned int buttons;
    int previous = sd_next_frame;
    if (!fgets(line, sizeof line, sd_replay))
        sd_fail("command stream is missing its end record");
    if (sscanf(line, "end %d %c", &frame, &extra) == 1) {
        if (frame < previous || frame < 1 || fgetc(sd_replay) != EOF)
            sd_fail("invalid command stream end record");
        sd_end_frame = frame;
        sd_next_frame = 0;
        return;
    }
    if (sscanf(line, "cmd %d %d %d %u %d %d %d %d %d %d %d %c",
               &frame, &client, &time, &buttons, &weapon, &offhand,
               &a0, &a1, &a2, &forward, &right, &extra) != 11 ||
        frame < 1 || frame < previous || client < 0 || client >= 64 ||
        weapon < 0 || weapon > 255 || offhand < 0 || offhand > 255 ||
        forward < -128 || forward > 127 || right < -128 || right > 127)
        sd_fail("invalid command record or frame order");
    memset(&sd_next_cmd, 0, sizeof sd_next_cmd);
    sd_next_frame = frame;
    sd_next_client = client;
    sd_next_cmd.serverTime = time;
    sd_next_cmd.buttons = (int)buttons;
    sd_next_cmd.weapon = (byte)weapon;
    sd_next_cmd.offHandIndex = (byte)offhand;
    sd_next_cmd.angles[0] = a0;
    sd_next_cmd.angles[1] = a1;
    sd_next_cmd.angles[2] = a2;
    sd_next_cmd.forwardmove = (signed char)forward;
    sd_next_cmd.rightmove = (signed char)right;
}

static int sd_command_mode(void)
{
    if (sd_mode < 0) {
        const char *record = getenv("SYSDIFF_USERCMDS");
        const char *replay = getenv("SYSDIFF_REPLAY");
        sd_mode = 0;
        if (record && *record && replay && *replay)
            sd_fail("record and replay are mutually exclusive");
        if (record && *record) {
            sd_mode = 1;
            sd_commands = sd_open(record, "w");
            fputs("cod2-replay 1\n", sd_commands);
        }
        if (replay && *replay) {
            char header[32];
            sd_mode = 2;
            sd_replay = sd_open(replay, "r");
            if (!fgets(header, sizeof header, sd_replay) || strcmp(header, "cod2-replay 1\n"))
                sd_fail("unsupported command stream header");
            sd_read_command();
        }
    }
    return sd_mode;
}

/* Replay owns input; live network commands are discarded. Recording is at
 * SV_ClientThink, before lastUsercmd is copied and ClientThink consumes it. */
int Sys_DiffClientThink(int clientNum, const usercmd_t *cmd)
{
    int mode = sd_command_mode();
    if (mode == 2 && !sd_injecting)
        return 0;
    if (mode == 1) {
        fprintf(sd_commands, "cmd %d %d %d %u %u %u %d %d %d %d %d\n",
                level.framenum + 1, clientNum, cmd->serverTime - level.startTime,
                (unsigned int)cmd->buttons, (unsigned int)cmd->weapon, (unsigned int)cmd->offHandIndex,
                cmd->angles[0], cmd->angles[1], cmd->angles[2],
                (int)(signed char)cmd->forwardmove, (int)(signed char)cmd->rightmove);
        if (fflush(sd_commands))
            sd_fail("command stream write failed");
    }
    return 1;
}

int Sys_DiffBotMove(void)
{
    /* Both modes avoid the stock bot's random input and its RNG consumption. */
    return sd_command_mode() != 0;
}

void Sys_DiffInputFrame(void)
{
    int mode = sd_command_mode();
    int frame = level.framenum + 1;
    if (mode == 2) {
        if ((sd_next_frame && sd_next_frame < frame) || (sd_end_frame && frame > sd_end_frame))
            sd_fail("replay missed a frame or exceeded its end");
        while (sd_next_frame == frame) {
            usercmd_t cmd = sd_next_cmd;
            int64_t time = (int64_t)cmd.serverTime + level.startTime;
            int client = sd_next_client;
            if (client >= level.maxclients || !svs.clients || svs.clients[client].state != 4)
                sd_fail("replay command targets an inactive client");
            if (time < INT_MIN || time > INT_MAX)
                sd_fail("replay command time overflow");
            cmd.serverTime = (int)time;
            sd_injecting = 1;
            svs.clients[client].deltaMessage = svs.clients[client].netchan.outgoingSequence - 1;
            SV_ClientThink(&svs.clients[client], &cmd);
            sd_injecting = 0;
            sd_read_command();
        }
    } else if (mode == 1 && svs.clients) {
        for (int i = 0; i < level.maxclients && i < 64; ++i) {
            usercmd_t cmd;
            client_t *cl = &svs.clients[i];
            if (cl->state != 4 || !cl->bIsTestClient || !g_entities[i].client)
                continue;
            memset(&cmd, 0, sizeof cmd);
            cmd.serverTime = svs.time;
            cmd.weapon = (byte)g_entities[i].client->ps.weapon;
            cmd.angles[1] = (frame * 97 + i * 8192) & 65535;
            cmd.forwardmove = (frame / 60) % 2 ? -127 : 127;
            cmd.rightmove = (frame / 40 + i) % 2 ? -64 : 64;
            cmd.buttons = frame % 40 < 5 ? 1 : 0;
            cl->deltaMessage = cl->netchan.outgoingSequence - 1;
            SV_ClientThink(cl, &cmd);
        }
    }
}

static void sd_finish_commands(int frame)
{
    if (sd_commands) {
        fprintf(sd_commands, "end %d\n", frame);
        if (fclose(sd_commands))
            sd_fail("command stream close failed");
        sd_commands = NULL;
    }
    if (sd_replay && sd_end_frame && sd_end_frame != frame)
        sd_fail("replay end does not match final frame");
}

static unsigned int sd_float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static void sd_players_frame(int frame)
{
    static FILE *fp;
    static int initialized;
    int active = 0;
    if (!initialized) {
        const char *path = getenv("SYSDIFF_PLAYERS");
        initialized = 1;
        if (path && *path)
            fp = sd_open(path, "w");
    }
    if (!fp)
        return;
    for (int i = 0; svs.clients && i < level.maxclients && i < 64; ++i)
        if (svs.clients[i].state == 4 && g_entities[i].r.inuse && g_entities[i].client)
            ++active;
    fprintf(fp, "frame %d clients %d\n", frame, active);
    for (int i = 0; svs.clients && i < level.maxclients && i < 64; ++i) {
        const playerState_t *ps;
        if (svs.clients[i].state != 4 || !g_entities[i].r.inuse || !g_entities[i].client)
            continue;
        ps = &g_entities[i].client->ps;
        fprintf(fp, "frame %d client %d origin %08x,%08x,%08x velocity %08x,%08x,%08x "
                    "viewangles %08x,%08x,%08x pm_type %d pm_flags %08x pm_time %d "
                    "weapon %d weaponstate %d health %d\n",
                frame, i, sd_float_bits(ps->origin[0]), sd_float_bits(ps->origin[1]), sd_float_bits(ps->origin[2]),
                sd_float_bits(ps->velocity[0]), sd_float_bits(ps->velocity[1]), sd_float_bits(ps->velocity[2]),
                sd_float_bits(ps->viewangles[0]), sd_float_bits(ps->viewangles[1]), sd_float_bits(ps->viewangles[2]),
                ps->pm_type, (unsigned int)ps->pm_flags, ps->pm_time, ps->weapon, ps->weaponstate, ps->stats[0]);
    }
    if (fflush(fp))
        sd_fail("player trace write failed");
}
#endif
