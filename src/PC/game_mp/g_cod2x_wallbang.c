#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "g_cod2x_wallbang.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterFloat(const char *dvarName, float value, float min, float max, unsigned short flags);
extern const dvar_t *Dvar_RegisterInt(const char *dvarName, int value, int min, int max, unsigned short flags);
extern const dvar_t *Dvar_RegisterBool(const char *dvarName, unsigned char value, unsigned short flags);
extern int Dvar_GetInt(const char *dvarName);
extern void SV_Trace(trace_t *results, const vec_t *start, const vec_t *mins, const vec_t *maxs, const vec_t *end,
                     int passEntityNum, int contentmask, qboolean locational, unsigned char *priorityMap,
                     qboolean staticmodels);
extern gentity_t *G_TempEntity(const vec_t *origin, int event);
extern const byte DirToByte(const vec_t *dir);
extern void SV_GameSendServerCommand(int clientNum, svscmd_type type, const char *text);
extern char *va(const char *format, ...);
extern const char *g_HitLocNames[];
extern serverStatic_t svs;
extern void *imp_g_entities;
#if !defined(DEDICATED)
extern void CL_AddDebugLine(const vec_t *start, const vec_t *end, const vec_t *color, qboolean depthTest, int duration,
                            qboolean fromServer);
extern void CL_AddDebugString(const vec_t *xyz, const vec_t *color, float scale, const char *text, qboolean fromServer);
#endif

#define WALLBANG_SURFACES 23
#define WALLBANG_MASK (0x2802831 & ~0x2000000) /* bullets' mask without player bodies */
#define WALLBANG_MAX_SOLIDS 8      /* touching brushes or overlapping models one wall may be made of */
#define WALLBANG_DEBUG_RANGE 256   /* how far a wall too thick to pass is measured, for the debug line only */
#define WALLBANG_DEBUG_DURATION 10000
#define WALLBANG_DEBUG_LINES 16
#define WALLBANG_DEBUG_LABELS 24
#define WALLBANG_DEBUG_VISIBLE 128

/* Surface types in bulletTrace() order, with how much of sv_wallbang_depth each
   lets a rifle bullet through. These are CoD2x's defaults. */
static const struct {
    const char *name;
    float scale;
} wallbangSurfaces[WALLBANG_SURFACES] = {
    { "none", 10.0f }, { "bark", 1.0f }, { "brick", 0.25f }, { "carpet", 4.0f }, { "cloth", 4.0f },
    { "concrete", 0.15f }, { "dirt", 0.6f }, { "flesh", 1.0f }, { "foliage", 4.0f }, { "glass", 2.0f },
    { "grass", 0.6f }, { "gravel", 0.3f }, { "ice", 0.5f }, { "metal", 0.1f }, { "mud", 0.45f },
    { "paper", 4.0f }, { "plaster", 0.8f }, { "rock", 0.1f }, { "sand", 0.4f }, { "snow", 1.5f },
    { "water", 0.5f }, { "wood", 1.0f }, { "asphalt", 0.15f },
};

static const dvar_t *sv_wallbang, *sv_wallbang_depth, *sv_wallbang_damage, *sv_wallbang_maxWalls;
static const dvar_t *sv_wallbang_debug, *sv_wallbang_surface[WALLBANG_SURFACES];

/* The bullet being fired: walls passed, power left (1 is full) and where it started. */
static struct {
    int walls;
    float power;
    vec3_t start;
} wallbangBullet;

typedef struct {
    vec3_t origin;
    const float *color;
    char text[16];
    int endTime;
} WallbangLabel;

/* sv_wallbang_debug: what the shooter is told once the bullet is done. */
static struct {
    int clientNum;
    int wallHit;
    int count;
    char lines[WALLBANG_DEBUG_LINES][160];
    int labelCount;
    WallbangLabel labels[WALLBANG_DEBUG_LABELS];
} wallbangDebug;

#if !defined(DEDICATED)
static WallbangLabel wallbangLabels[WALLBANG_DEBUG_VISIBLE];
static int wallbangLabelsNext;
#endif

static const vec4_t wallbangPath = { 1, 1, 1, 1 };
static const vec4_t wallbangEntry = { 1, 0, 0, 1 };
static const vec4_t wallbangReach = { 1, 1, 0, 1 };
static const vec4_t wallbangInside = { 1, 0.5f, 0, 1 };
static const vec4_t wallbangExit = { 0, 1, 0, 1 };
static const vec4_t wallbangStop = { 1, 0, 1, 1 };
static const vec4_t wallbangBehind = { 0, 0.6f, 1, 1 };
static const vec3_t wallbangZero = { 0, 0, 0 };

void G_Cod2xWallbangInit(void)
{
    static char names[WALLBANG_SURFACES][40];
    sv_wallbang = Dvar_RegisterBool("sv_wallbang", 0, 0x1000);
    sv_wallbang_depth = Dvar_RegisterFloat("sv_wallbang_depth", 24, 0, 256, 0x1000);
    sv_wallbang_damage = Dvar_RegisterFloat("sv_wallbang_damage", 0.6f, 0, 1, 0x1000);
    sv_wallbang_maxWalls = Dvar_RegisterInt("sv_wallbang_maxWalls", 2, 1, 8, 0x1000);
    sv_wallbang_debug = Dvar_RegisterBool("sv_wallbang_debug", 0, 0x1000);
    for (int i = 0; i < WALLBANG_SURFACES; ++i) {
        snprintf(names[i], sizeof(names[i]), "sv_wallbang_surface_%s", wallbangSurfaces[i].name);
        sv_wallbang_surface[i] = Dvar_RegisterFloat(names[i], wallbangSurfaces[i].scale, 0, 10, 0x1000);
    }
}

static float WB_Dot(const vec_t *a, const vec_t *b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static float WB_Length(const vec_t *v)
{
    return sqrtf(WB_Dot(v, v));
}

static void WB_Sub(const vec_t *a, const vec_t *b, vec_t *out)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

static void WB_MA(const vec_t *v, float scale, const vec_t *dir, vec_t *out)
{
    out[0] = v[0] + dir[0] * scale;
    out[1] = v[1] + dir[1] * scale;
    out[2] = v[2] + dir[2] * scale;
}

static void WB_Lerp(const vec_t *from, const vec_t *to, float fraction, vec_t *out)
{
    out[0] = from[0] + (to[0] - from[0]) * fraction;
    out[1] = from[1] + (to[1] - from[1]) * fraction;
    out[2] = from[2] + (to[2] - from[2]) * fraction;
}

static void WB_Normalize(vec_t *v)
{
    float length = WB_Length(v);
    if (length != 0.0f) {
        float inverse = 1.0f / length;
        v[0] *= inverse;
        v[1] *= inverse;
        v[2] *= inverse;
    }
}

static int WB_SurfaceType(int surfaceFlags)
{
    return (surfaceFlags >> 20) & 0x1f;
}

/* The stock range falloff of a bullet's damage. */
static int WB_BulletDamage(const weaponParms *wp, float distance)
{
    const WeaponDef *weapDef = wp->weapDef;
    float range;
    float fraction;
    if (weapDef->fMaxDamageRange > distance)
        return weapDef->damage;
    if (weapDef->fMinDamageRange <= distance)
        return weapDef->minDamage;
    range = weapDef->fMinDamageRange - weapDef->fMaxDamageRange;
    if (range == 0.0f)
        return weapDef->damage;
    fraction = (distance - weapDef->fMaxDamageRange) / range;
    return (int)((1.0f - fraction) * (float)weapDef->damage + (float)weapDef->minDamage * fraction);
}

static void WB_Print(const gentity_t *attacker, const char *format, ...)
{
    va_list args;
    if (!sv_wallbang_debug->current.enabled || !attacker->client || wallbangDebug.count >= WALLBANG_DEBUG_LINES)
        return;
    va_start(args, format);
    vsnprintf(wallbangDebug.lines[wallbangDebug.count], sizeof(wallbangDebug.lines[0]), format, args);
    va_end(args);
    wallbangDebug.clientNum = attacker->s.number;
    ++wallbangDebug.count;
}

/* One line for a single wall, numbered and indented when the bullet passed more. */
static void WB_Flush(void)
{
    if (!wallbangDebug.wallHit)
        return;
    for (int i = 0; i < wallbangDebug.count; ++i) {
        const char *text;
        if (wallbangDebug.count == 1)
            text = va("^3WB:^7 %s", wallbangDebug.lines[i]);
        else if (i == 0)
            text = va("^3WB:^7 1. | %s", wallbangDebug.lines[i]);
        else
            text = va("       %i. | %s", i + 1, wallbangDebug.lines[i]);
        SV_GameSendServerCommand(wallbangDebug.clientNum, SV_CMD_CAN_IGNORE, va("%c \"%s\"", 101, text));
    }
    wallbangDebug.count = 0;
#if !defined(DEDICATED)
    for (int i = 0; i < wallbangDebug.labelCount; ++i) {
        wallbangLabels[wallbangLabelsNext] = wallbangDebug.labels[i];
        wallbangLabels[wallbangLabelsNext].endTime = svs.time + WALLBANG_DEBUG_DURATION;
        wallbangLabelsNext = (wallbangLabelsNext + 1) % WALLBANG_DEBUG_VISIBLE;
    }
#endif
    wallbangDebug.labelCount = 0;
}

/* The damage labels and lines are drawn only by a listen server's own client. */
static void WB_Label(const vec_t *origin, const float *color, int damage)
{
#if !defined(DEDICATED)
    WallbangLabel *label;
    if (!sv_wallbang_debug->current.enabled || wallbangDebug.labelCount >= WALLBANG_DEBUG_LABELS)
        return;
    label = &wallbangDebug.labels[wallbangDebug.labelCount++];
    label->origin[0] = origin[0];
    label->origin[1] = origin[1];
    label->origin[2] = origin[2] + 4;
    label->color = color;
    snprintf(label->text, sizeof(label->text), "%i", damage);
#else
    (void)origin, (void)color, (void)damage;
#endif
}

static int WB_LabelDamage(const weaponParms *wp, const vec_t *point, float spread, int walls, float power)
{
    vec3_t distance;
    WB_Sub(wallbangBullet.start, point, distance);
    if (walls > 0)
        return (int)((float)WB_BulletDamage(wp, WB_Length(distance)) * spread *
                     sv_wallbang_damage->current.value * power);
    return (int)((float)WB_BulletDamage(wp, WB_Length(distance)) * spread);
}

static void WB_Line(const vec_t *start, const vec_t *end, const float *color)
{
#if !defined(DEDICATED)
    if (!sv_wallbang_debug->current.enabled)
        return;
    /* The duration counts server frames. */
    CL_AddDebugLine(start, end, color, 0, WALLBANG_DEBUG_DURATION * Dvar_GetInt("sv_fps") / 1000, 1);
#else
    (void)start, (void)end, (void)color;
#endif
}

static void WB_Point(const vec_t *point, const float *color)
{
    for (int axis = 0; axis < 3; ++axis) {
        vec3_t start = { point[0], point[1], point[2] };
        vec3_t end = { point[0], point[1], point[2] };
        start[axis] -= 3;
        end[axis] += 3;
        WB_Line(start, end, color);
    }
}

/* The real thickness of a wall the bullet couldn't pass, for the debug line; -1 if unknown. */
static float WB_MeasureThickness(const vec_t *entry, float cosAngle, const vec_t *dir, int passEntityNum)
{
    trace_t solidTrace, exitTrace;
    vec3_t from, inside, next, delta;
    float thickness = 0;
    if (!sv_wallbang_debug->current.enabled)
        return -1;
    from[0] = entry[0];
    from[1] = entry[1];
    from[2] = entry[2];
    for (int solid = 0; solid < WALLBANG_MAX_SOLIDS && thickness < WALLBANG_DEBUG_RANGE; ++solid) {
        WB_MA(from, 0.25f / fmaxf(cosAngle, 0.125f), dir, inside);
        WB_MA(inside, WALLBANG_DEBUG_RANGE - thickness, dir, next);
        SV_Trace(&solidTrace, inside, wallbangZero, wallbangZero, next, passEntityNum, WALLBANG_MASK, 0, NULL, 1);
        if (solidTrace.allsolid)
            return -1;
        WB_Lerp(inside, next, solidTrace.fraction, next);
        SV_Trace(&exitTrace, next, wallbangZero, wallbangZero, from, passEntityNum, WALLBANG_MASK, 0, NULL, 1);
        if (exitTrace.fraction < 1.0f && !exitTrace.startsolid) {
            vec3_t exitPos;
            WB_Lerp(next, from, exitTrace.fraction, exitPos);
            WB_Sub(exitPos, from, delta);
            return thickness + WB_Length(delta);
        }
        if (solidTrace.fraction >= 1.0f)
            return -1;
        WB_Sub(next, from, delta);
        thickness += WB_Length(delta);
        from[0] = next[0];
        from[1] = next[1];
        from[2] = next[2];
        cosAngle = -WB_Dot(dir, solidTrace.normal);
    }
    return -1;
}

static void WB_PrintTooThick(const gentity_t *attacker, const char *surfName, float thickness, float maxLength,
                             const vec_t *entry, float cosAngle, const vec_t *dir, int passEntityNum)
{
    char real[32];
    float measured;
    if (!sv_wallbang_debug->current.enabled)
        return;
    measured = WB_MeasureThickness(entry, cosAngle, dir, passEntityNum);
    if (measured < 0)
        snprintf(real, sizeof(real), ">%i or no exit", WALLBANG_DEBUG_RANGE);
    else
        snprintf(real, sizeof(real), "%.1f", thickness + measured);
    WB_Print(attacker, "%s | %s / %.1f | ^1too thick", surfName, real, thickness + maxLength);
}

void G_Cod2xWallbangFire(G_Cod2xBulletFire fire, const gentity_t *source, gentity_s (*attacker)[16], vec_t *start,
                         vec_t *end, float spread, const weaponParms *wp, const gentity_t *weaponEnt, int gametime)
{
    vec3_t dir, labelPos;
    wallbangDebug.count = 0;
    wallbangDebug.labelCount = 0;
    wallbangDebug.wallHit = 0;
    /* The starting damage, a little in front of the shooter so it can be read. */
    WB_Sub(end, start, dir);
    WB_Normalize(dir);
    WB_MA(start, 32, dir, labelPos);
    WB_Label(labelPos, wallbangPath, (int)((float)WB_BulletDamage(wp, 0) * spread));
    fire(source, attacker, start, end, spread, 0, wp, weaponEnt, gametime);
    WB_Flush();
}

void G_Cod2xWallbangStart(const vec_t *start)
{
    wallbangBullet.walls = 0;
    wallbangBullet.power = 1.0f;
    wallbangBullet.start[0] = start[0];
    wallbangBullet.start[1] = start[1];
    wallbangBullet.start[2] = start[2];
}

int G_Cod2xWallbangWalls(void)
{
    return wallbangBullet.walls;
}

void G_Cod2xWallbangBehind(const vec_t *start, const vec_t *hitPos)
{
    if (wallbangBullet.walls > 0)
        WB_Line(start, hitPos, wallbangBehind);
}

int G_Cod2xWallbangDamage(const weaponParms *wp, const vec_t *hitPos, float spread)
{
    vec3_t distance;
    WB_Sub(wallbangBullet.start, hitPos, distance);
    return (int)((float)WB_BulletDamage(wp, WB_Length(distance)) * spread * sv_wallbang_damage->current.value *
                 wallbangBullet.power);
}

void G_Cod2xWallbangHit(gentity_t *attacker, const gentity_t *target, const trace_t *tr, const vec_t *hitPos,
                        int damage, int healthBefore)
{
    WB_Label(hitPos, wallbangPath, damage);
    if (target->client)
        WB_Print(attacker, "^2hit^7 %s | %s | %i damage", svs.clients[target->s.number].name,
                 g_HitLocNames[tr->partGroup % 19], healthBefore - target->health);
}

/* The bullet stopped on the world or on an entity that takes no damage. Find
   the far side of the wall within what the bullet's power still allows, spend
   power by the thickness against the material's depth, put the impact on the
   far side and continue the bullet from there. A trace started just inside a
   brush leaves it; the reverse trace from where it stopped finds the far side.
   If that misses, the wall goes on in a touching brush or model. */
void G_Cod2xWallbangPenetrate(G_Cod2xBulletFire fire, const gentity_t *source, gentity_t *attacker,
                              const vec_t *start, const vec_t *end, const vec_t *hitPos, const trace_t *tr,
                              float spread, int recursion, const weaponParms *wp, const gentity_t *weaponEnt,
                              int gametime)
{
    const gentity_t *entities = (const gentity_t *)imp_g_entities;
    trace_t solidTrace, exitTrace;
    const trace_t *face = tr;
    vec3_t dir, entry, inside, next, exitPos = { 0, 0, 0 };
    float baseDepth, cosAngle, power, thickness = 0;
    int surfType;
    char surfName[64];
    gentity_t *tent;

    if (!sv_wallbang->current.enabled || !wp->weapDef->bRifleBullet)
        return;
    /* Nothing hit, the sky or a body. */
    if (tr->fraction >= 1.0f || (tr->surfaceFlags & 4) || entities[tr->entityNum].s.eType == 2) {
        if (wallbangBullet.walls > 0)
            WB_Label(hitPos, wallbangBehind,
                     WB_LabelDamage(wp, hitPos, spread, wallbangBullet.walls, wallbangBullet.power));
        return;
    }

    wallbangDebug.wallHit = 1;
    WB_Label(hitPos, wallbangEntry, WB_LabelDamage(wp, hitPos, spread, wallbangBullet.walls, wallbangBullet.power));
    if (wallbangBullet.walls >= sv_wallbang_maxWalls->current.integer) {
        WB_Print(attacker, "^9max walls (%i) reached", wallbangBullet.walls);
        WB_Point(hitPos, wallbangStop);
        return;
    }

    WB_Sub(end, start, dir);
    WB_Normalize(dir);
    baseDepth = sv_wallbang_depth->current.value;
    cosAngle = -WB_Dot(dir, tr->normal);
    surfType = WB_SurfaceType(tr->surfaceFlags);
    power = wallbangBullet.power;
    entry[0] = hitPos[0];
    entry[1] = hitPos[1];
    entry[2] = hitPos[2];
    snprintf(surfName, sizeof(surfName), "%s", wallbangSurfaces[surfType].name);

    if (wallbangBullet.walls == 0)
        WB_Line(start, hitPos, wallbangPath);
    WB_Point(hitPos, wallbangEntry);

    /* A grazing hit would travel too far inside, the same limit as glass. */
    if (cosAngle < 0.125f) {
        WB_Print(attacker, "%s | ^1grazing angle", surfName);
        WB_Point(entry, wallbangStop);
        return;
    }

    for (int solid = 0;; ++solid) {
        vec3_t toEnd, delta;
        float depth, maxLength, length;
        int exitFound, nextSurfType;

        if (solid == WALLBANG_MAX_SOLIDS) {
            WB_Print(attacker, "%s | %.1f | ^1too many solids", surfName, thickness);
            return;
        }
        depth = baseDepth * sv_wallbang_surface[surfType]->current.value;
        /* Weapon clip stops bullets on purpose. */
        if (depth <= 0 || (face->contents & 0x2000)) {
            WB_Print(attacker, "%s | ^1not penetrable^7 | contents 0x%x flags 0x%x", surfName, face->contents,
                     face->surfaceFlags);
            WB_Point(entry, wallbangStop);
            return;
        }

        WB_MA(entry, 0.25f / fmaxf(cosAngle, 0.125f), dir, inside);
        WB_Sub(end, inside, toEnd);
        maxLength = fminf(power * depth, WB_Dot(toEnd, dir));
        if (maxLength <= 0) {
            WB_Print(attacker, "%s | ^1bullet range reached", surfName);
            WB_Point(entry, wallbangStop);
            return;
        }
        WB_MA(inside, maxLength, dir, next);

        SV_Trace(&solidTrace, inside, wallbangZero, wallbangZero, next, source->s.number, WALLBANG_MASK, 0, NULL, 1);
        if (solidTrace.allsolid) {
            WB_PrintTooThick(attacker, surfName, thickness, maxLength, entry, cosAngle, dir, source->s.number);
            WB_Line(inside, next, wallbangReach);
            WB_Point(next, wallbangStop);
            return;
        }
        WB_Lerp(inside, next, solidTrace.fraction, next);
        WB_Line(inside, next, wallbangReach);

        SV_Trace(&exitTrace, next, wallbangZero, wallbangZero, entry, source->s.number, WALLBANG_MASK, 0, NULL, 1);
        exitFound = exitTrace.fraction < 1.0f && !exitTrace.startsolid;
        if (exitFound) {
            WB_Lerp(next, entry, exitTrace.fraction, exitPos);
        } else if (solidTrace.fraction >= 1.0f) {
            /* Models, terrain and patches are only surfaces: the far side is past what the bullet can go. */
            WB_PrintTooThick(attacker, surfName, thickness, maxLength, entry, cosAngle, dir, source->s.number);
            WB_Point(next, wallbangStop);
            return;
        }

        WB_Sub(exitFound ? exitPos : next, entry, delta);
        length = WB_Length(delta);
        WB_Line(entry, exitFound ? exitPos : next, wallbangInside);
        power -= length / depth;
        thickness += length;
        if (power <= 0) {
            WB_Print(attacker, "%s | %.1f | ^1too thick", surfName, thickness);
            WB_Point(exitFound ? exitPos : next, wallbangStop);
            return;
        }
        if (exitFound)
            break;

        /* The wall goes on in the next solid; a hidden face without a type keeps the material. */
        entry[0] = next[0];
        entry[1] = next[1];
        entry[2] = next[2];
        face = &solidTrace;
        nextSurfType = WB_SurfaceType(solidTrace.surfaceFlags);
        if (nextSurfType != 0 && nextSurfType != surfType) {
            size_t used = strlen(surfName);
            surfType = nextSurfType;
            snprintf(surfName + used, sizeof(surfName) - used, "+%s", wallbangSurfaces[surfType].name);
        }
        cosAngle = -WB_Dot(dir, solidTrace.normal);
    }

    WB_Print(attacker, "%s | %.1f | power %.2f | damage x%.2f", surfName, thickness, power,
             sv_wallbang_damage->current.value * power);
    WB_Point(exitPos, wallbangExit);
    WB_Label(exitPos, wallbangExit, WB_LabelDamage(wp, exitPos, spread, wallbangBullet.walls + 1, power));

    /* The impact on the far side. */
    tent = G_TempEntity(exitPos, 0xb7);
    tent->s.eventParm = (unsigned char)DirToByte(exitTrace.normal);
    tent->s.eventParm2 = (unsigned char)DirToByte(dir);
    tent->s.surfType = WB_SurfaceType(exitTrace.surfaceFlags);
    tent->s.otherEntityNum = weaponEnt->s.number;

    ++wallbangBullet.walls;
    wallbangBullet.power = power;
    {
        vec3_t from = { exitPos[0], exitPos[1], exitPos[2] };
        vec3_t to = { end[0], end[1], end[2] };
        fire(source, (gentity_s (*)[16])attacker, from, to, spread, recursion + 1, wp, weaponEnt, gametime);
    }
}

#if !defined(DEDICATED)
void G_Cod2xWallbangLabelsFrame(void)
{
    for (int i = 0; i < WALLBANG_DEBUG_VISIBLE; ++i) {
        const WallbangLabel *label = &wallbangLabels[i];
        int timeLeft = label->endTime - svs.time;
        /* Expired, or a new map reset the server time. */
        if (timeLeft <= 0 || timeLeft > WALLBANG_DEBUG_DURATION)
            continue;
        CL_AddDebugString(label->origin, label->color, 0.25f, label->text, 1);
    }
}
#endif
#endif
