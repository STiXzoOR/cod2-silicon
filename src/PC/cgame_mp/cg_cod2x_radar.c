#include "cod2_feature_config.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "common_types.h"
#include "headers/PC/cgame_mp/cg_local.h"
#include "PC/qcommon/cod2x_features.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterFloat(const char *, float, float, float, unsigned short);
extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern const dvar_t *Dvar_RegisterVec3(const char *, float, float, float, float, float, unsigned short);
extern const dvar_t *Dvar_FindVar(const char *);
extern void Dvar_SetFloat(const dvar_t *, float);
extern void Dvar_SetInt(const dvar_t *, int);
extern void Dvar_SetString(const dvar_t *, const char *);
extern void Dvar_GetUnpackedColor(const dvar_t *, vec_t *);
extern void Cmd_AddCommand(const char *, void (*)(void));
extern int Cmd_Argc(void);
extern char *Cmd_Argv(int);
extern void Com_Printf(const char *, ...);
extern char *Com_Parse(const char **);
extern int FS_ReadFile(const char *, void **);
extern void FS_FreeFile(void *);
extern qboolean FS_WriteFile(const char *, const void *, int);
extern MaterialHandle CL_RegisterMaterial(const char *, int);
extern const char *CL_GetConfigString(int);
extern void CG_DrawRotatedPic(float, float, float, float, int, int, float, const vec_t *, MaterialHandle);
extern FontHandle UI_GetFontHandle(int, float);
extern void UI_DrawText(const char *, int, FontHandle, float, float, int, int, float, const vec_t *, int);
extern int UI_TextWidth(const char *, int, FontHandle, float);
extern clientActive_t clients;
extern clientStatic_t cls;

static const dvar_t *drawRadar, *radarScale, *mapImage, *imageX, *imageY, *rotation;
static const dvar_t *entityScale, *entityX, *entityY, *radarX, *radarY, *numberSwitch, *radarColor;
static char radarMap[64];
static int calibration, calibrationFrame = -1, lastDrawTime, commandRegistered;
static float worldPoints[2][2], imagePoints[2][2];
static struct {
    int time;
    vec3_t origin;
    float yaw;
} fireEvents[128];
static unsigned int fireIndex;

static int CG_Cod2xRadarCheats(void)
{
    const dvar_t *cheats = Dvar_FindVar("sv_cheats");
    return Cod2x_FeaturesDemo() || (cheats && cheats->current.enabled);
}

static void CG_Cod2xRadarMapName(char name[64])
{
    const char *path = cgs->mapname, *slash = strrchr(path, '/');
    size_t length;
    if (slash)
        path = slash + 1;
    length = strcspn(path, ".");
    if (length >= 64)
        length = 63;
    memcpy(name, path, length);
    name[length] = '\0';
}

static void CG_Cod2xRadarLoad(void)
{
    char name[64], filename[128], image[128];
    char *buffer = NULL;
    const char *cursor, *token;
    float values[6];
    int i, length;
    CG_Cod2xRadarMapName(name);
    if (!strcmp(name, radarMap))
        return;
    snprintf(radarMap, sizeof(radarMap), "%s", name);
    calibration = 0;
    memset(fireEvents, 0, sizeof(fireEvents));
    Dvar_SetString(mapImage, "");
    if (!*name)
        return;
    snprintf(filename, sizeof(filename), "maps/mp/%s.radar", name);
    length = FS_ReadFile(filename, (void **)&buffer);
    if (length <= 0 || !buffer) {
        if (buffer)
            FS_FreeFile(buffer);
        return;
    }
    cursor = buffer;
    Com_Parse(&cursor);
    token = Com_Parse(&cursor);
    snprintf(image, sizeof(image), "%s", token);
    for (i = 0; i < 6; ++i) {
        char *end;
        Com_Parse(&cursor);
        token = Com_Parse(&cursor);
        values[i] = strtof(token, &end);
        if (!*token || *end || !isfinite(values[i]))
            break;
    }
    FS_FreeFile(buffer);
    if (!*image || i != 6 || values[0] < 0 || values[0] > 1 ||
        values[3] < -180 || values[3] > 180) {
        Com_Printf("Invalid radar definition: %s\n", filename);
        return;
    }
    Dvar_SetString(mapImage, image);
    Dvar_SetFloat(entityScale, values[0]);
    Dvar_SetFloat(entityX, values[1]);
    Dvar_SetFloat(entityY, values[2]);
    Dvar_SetInt(rotation, (int)values[3]);
    Dvar_SetFloat(imageX, values[4]);
    Dvar_SetFloat(imageY, values[5]);
}

static void CG_Cod2xRadarSave(void)
{
    char filename[128], text[640], name[64];
    int size;
    CG_Cod2xRadarMapName(name);
    if (!*name)
        return;
    snprintf(filename, sizeof(filename), "maps/mp/%s.radar", name);
    size = snprintf(text, sizeof(text),
                    "image %s\nentityScale %.9g\nentityOffsetX %.9g\nentityOffsetY %.9g\n"
                    "rotation %d\nimageOffsetX %.9g\nimageOffsetY %.9g\n",
                    mapImage->current.string, entityScale->current.value, entityX->current.value,
                    entityY->current.value, rotation->current.integer, imageX->current.value, imageY->current.value);
    if (size > 0 && size < (int)sizeof(text) && FS_WriteFile(filename, text, size))
        Com_Printf("Radar calibration saved to your homepath: %s\n", filename);
}

static void CG_Cod2xRadarCalibrate_f(void)
{
    float scale, x, y;
    int point;
    if (!cg->snap || !CG_Cod2xRadarCheats()) {
        Com_Printf("Radar calibration requires cheats or demo playback\n");
        return;
    }
    if (!drawRadar->current.enabled || !*mapImage->current.string) {
        Com_Printf("Enable cg_drawRadar and select cg_hudRadarMapImage first\n");
        return;
    }
    if (calibration == 0) {
        calibration = 1;
        imagePoints[0][0] = imagePoints[1][0] = 400;
        imagePoints[0][1] = imagePoints[1][1] = 0;
        return;
    }
    point = calibration > 2;
    if (calibration & 1) {
        worldPoints[point][0] = cg->refdef.vieworg[0];
        worldPoints[point][1] = cg->refdef.vieworg[1];
    } else if (Cmd_Argc() == 3) {
        char *endX, *endY;
        x = strtof(Cmd_Argv(1), &endX);
        y = strtof(Cmd_Argv(2), &endY);
        if (*endX || *endY || !isfinite(x) || !isfinite(y))
            return;
        imagePoints[point][0] = x;
        imagePoints[point][1] = y;
    }
    if (++calibration <= 4)
        return;
    calibration = 0;
    if (!Cod2x_RadarCalibrate(worldPoints[0], worldPoints[1], imagePoints[0], imagePoints[1], &scale, &x, &y) || scale > 1) {
        Com_Printf("Radar calibration needs two distinct world and image points\n");
        return;
    }
    Dvar_SetFloat(entityScale, scale);
    Dvar_SetFloat(entityX, x);
    Dvar_SetFloat(entityY, y);
    Com_Printf("Radar calibration: scale=%g offset=(%g, %g)\n", scale, x, y);
    CG_Cod2xRadarSave();
}

void CG_Cod2xRadarInit(void)
{
    /* CoD2x radar.cpp:646 defines the user controls and .radar file order. */
    drawRadar = Dvar_RegisterBool("cg_drawRadar", 0, 0x1000);
    radarScale = Dvar_RegisterFloat("cg_hudRadarScale", 1, -FLT_MAX, FLT_MAX, 0x1000);
    mapImage = Dvar_RegisterString("cg_hudRadarMapImage", "", 0x1000);
    imageX = Dvar_RegisterFloat("cg_hudRadarMapImageOffsetX", 0, -FLT_MAX, FLT_MAX, 0x1000);
    imageY = Dvar_RegisterFloat("cg_hudRadarMapImageOffsetY", 0, -FLT_MAX, FLT_MAX, 0x1000);
    rotation = Dvar_RegisterInt("cg_hudRadarMapImageRotation", 0, -180, 180, 0x1000);
    entityScale = Dvar_RegisterFloat("cg_hudRadarEntityScale", 1, 0, 1, 0x1000);
    entityX = Dvar_RegisterFloat("cg_hudRadarEntityOffsetX", 0, -FLT_MAX, FLT_MAX, 0x1000);
    entityY = Dvar_RegisterFloat("cg_hudRadarEntityOffsetY", 0, -FLT_MAX, FLT_MAX, 0x1000);
    radarX = Dvar_RegisterFloat("cg_hudRadarOffsetX", 5, -FLT_MAX, FLT_MAX, 0x1000);
    radarY = Dvar_RegisterFloat("cg_hudRadarOffsetY", 20, -FLT_MAX, FLT_MAX, 0x1000);
    numberSwitch = Dvar_RegisterBool("cg_hudRadarPlayersNumberSwitch", 0, 0x1000);
    radarColor = Dvar_RegisterVec3("cg_hudRadarColor", 1, 1, 1, 0, 1, 0x1000);
    if (!commandRegistered) {
        Cmd_AddCommand("cg_hudRadarCalibrate", CG_Cod2xRadarCalibrate_f);
        commandRegistered = 1;
    }
    radarMap[0] = '\0';
    calibration = 0;
    memset(fireEvents, 0, sizeof(fireEvents));
}

void CG_Cod2xRadarFire(int entityNum)
{
    int i;
    if (!drawRadar || entityNum < 0 || entityNum >= 64 || !cg_entities)
        return;
    fireIndex = (fireIndex + 1) % 128;
    fireEvents[fireIndex].time = cg->time;
    for (i = 0; i < 3; ++i)
        fireEvents[fireIndex].origin[i] = cg_entities[entityNum].lerpOrigin[i];
    fireEvents[fireIndex].yaw = cg_entities[entityNum].lerpAngles[1];
}

static void CG_Cod2xRadarCoordinates(const vec_t *origin, float *x, float *y)
{
    Cod2x_RadarPoint(origin[0], origin[1], entityScale->current.value,
                    entityX->current.value, entityY->current.value, radarScale->current.value,
                    radarX->current.value, radarY->current.value, imageX->current.value,
                    imageY->current.value, rotation->current.integer, x, y);
}

void CG_Cod2xRadarDraw(void)
{
    static const vec4_t white = {1, 1, 1, 1}, yellow = {1, 1, 0, 1};
    vec4_t color, teamColors[3] = {{1, .2f, .2f, 1}, {1, .2f, .2f, 1}, {.2f, .7f, 1, 1}};
    MaterialHandle material, arrow, circle;
    FontHandle font;
    float x, y, size, factor;
    int i, teamCounts[3] = {0, 0, 0}, teamNumbers[64], clientNum;
    if (!drawRadar || !cg->snap || !cg_entities)
        return;
    CG_Cod2xRadarLoad();
    clientNum = cg->clientNum;
    if (!drawRadar->current.enabled || clientNum < 0 || clientNum >= 64 ||
        (cg->bgs.clientinfo[clientNum].team != TEAM_SPECTATOR && !CG_Cod2xRadarCheats()) ||
        !*mapImage->current.string)
        return;
    if (cg->time < lastDrawTime)
        memset(fireEvents, 0, sizeof(fireEvents));
    lastDrawTime = cg->time;
    factor = calibration ? 4 : 2 * radarScale->current.value;
    size = factor * 100;
    x = radarX->current.value + imageX->current.value;
    y = radarY->current.value + imageY->current.value;
    for (i = 0; i < 3; ++i)
        color[i] = radarColor->current.vector[i];
    color[3] = 1;
    material = CL_RegisterMaterial(mapImage->current.string, 7);
    CG_DrawRotatedPic(x, y, size, size, 1, 1, calibration ? 0 : (float)rotation->current.integer, color, material);
    font = UI_GetFontHandle(0, .2f);
    if (calibration) {
        static const char *steps[4] = {
            "Move camera to first world position; run cg_hudRadarCalibrate",
            "Move mouse to first image position; run cg_hudRadarCalibrate",
            "Move camera to second world position; run cg_hudRadarCalibrate",
            "Move mouse to second image position; run cg_hudRadarCalibrate"
        };
        int point = calibration > 2;
        if (!(calibration & 1) && calibrationFrame != cls.realtime) {
            int index = clients.mouseIndex ^ 1;
            imagePoints[point][0] += clients.mouseDx[index] * .1f;
            imagePoints[point][1] += clients.mouseDy[index] * .1f;
        }
        calibrationFrame = cls.realtime;
        UI_DrawText(steps[calibration - 1], INT_MAX, font, 10, 450, 1, 1, .2f, white, 3);
        for (i = 0; i < 2; ++i)
            CG_DrawRotatedPic(x + imagePoints[i][0] - 3, y + imagePoints[i][1] - 3,
                              6, 6, 1, 1, 0, yellow, cgs->media.whiteMaterial);
        return;
    }
    size = 10 * factor;
    material = CL_RegisterMaterial("radar_player_fire", 7);
    for (i = 0; i < 128; ++i) {
        int age = cg->time - fireEvents[i].time;
        if (!fireEvents[i].time || age < 0 || age >= 1000)
            continue;
        CG_Cod2xRadarCoordinates(fireEvents[i].origin, &x, &y);
        color[0] = 1; color[1] = color[2] = .5f; color[3] = 1 - age * .001f;
        CG_DrawRotatedPic(x - size * 2, y - size * 2, size * 4, size * 4, 1, 1,
                          90 + rotation->current.integer - fireEvents[i].yaw, color, material);
    }
    for (i = 0; i < 16; ++i) {
        const objective_t *obj = &cg->snap->ps.objective[i];
        const vec_t *origin = obj->origin;
        MaterialHandle icon = cgs->media.objectiveMaterials[0];
        if (obj->state != OBJST_CURRENT)
            continue;
        if (obj->entNum != 1023) {
            if (obj->entNum < 0 || obj->entNum >= 1023)
                continue;
            origin = cg_entities[obj->entNum].lerpOrigin;
        }
        CG_Cod2xRadarCoordinates(origin, &x, &y);
        /* Stock shader configstrings occupy 1566..1693. */
        if (obj->icon > 0 && obj->icon < 128) {
            const char *name = CL_GetConfigString(1566 + obj->icon);
            if (name && *name)
                icon = CL_RegisterMaterial(name, 7);
        }
        CG_DrawRotatedPic(x - factor * 4, y - factor * 4, factor * 8, factor * 8, 1, 1, 0,
                          white, icon);
    }
    arrow = CL_RegisterMaterial("radar_player_arrow", 7);
    circle = CL_RegisterMaterial("radar_player_circle", 7);
    for (i = TEAM_AXIS; i <= TEAM_ALLIES; ++i) {
        const dvar_t *teamColor = Dvar_FindVar(i == TEAM_ALLIES ? "g_TeamColor_Allies" : "g_TeamColor_Axis");
        if (teamColor && (teamColor->type == 7 || teamColor->type == 8)) {
            Dvar_GetUnpackedColor(teamColor, teamColors[i]);
            teamColors[i][3] = 1;
        }
    }
    for (i = 0; i < 64; ++i) {
        const centity_t *cent = &cg_entities[i];
        int team = TEAM_FREE;
        teamNumbers[i] = i;
        if (cent->currentState.eType == 1 && cent->currentState.clientNum >= 0 && cent->currentState.clientNum < 64) {
            const clientInfo_t *ci = &cg->bgs.clientinfo[cent->currentState.clientNum];
            if (ci->infoValid)
                team = ci->team;
        }
        if ((team == TEAM_ALLIES || team == TEAM_AXIS) && teamCounts[team] < 5) {
            teamNumbers[i] = ++teamCounts[team];
            if ((team == TEAM_AXIS) != numberSwitch->current.enabled)
                teamNumbers[i] += 5;
            if (teamNumbers[i] == 10)
                teamNumbers[i] = 0;
        }
    }
    for (i = 0; i < 64; ++i) {
        const centity_t *cent = &cg_entities[i];
        const clientInfo_t *ci;
        char number[8];
        int team, n;
        float highlight;
        if (cent->currentState.eType != 1 || !cent->nextValid ||
            (cent->currentState.eFlags & 0x20001) || cent->currentState.clientNum < 0 ||
            cent->currentState.clientNum >= 64)
            continue;
        ci = &cg->bgs.clientinfo[cent->currentState.clientNum];
        team = ci->team;
        if (!ci->infoValid || team >= TEAM_SPECTATOR || team < TEAM_FREE)
            continue;
        CG_Cod2xRadarCoordinates(cent->lerpOrigin, &x, &y);
        highlight = cg->snap->ps.clientNum == cent->currentState.clientNum ? 1.5f : 1;
        size = 10 * factor;
        CG_DrawRotatedPic(x - size * highlight * .5f, y - size * highlight * .5f, size * highlight,
                          size * highlight, 1, 1, 90 + rotation->current.integer - cent->lerpAngles[1],
                          highlight > 1 ? yellow : white, arrow);
        CG_DrawRotatedPic(x - size * .5f, y - size * .5f, size, size, 1, 1, 0, teamColors[team], circle);
        n = teamNumbers[i];
        snprintf(number, sizeof(number), "%d", n);
        UI_DrawText(number, INT_MAX, font, x - UI_TextWidth(number, 0, font, .2f * factor) * .5f,
                    y + 3 * factor, 1, 1, .2f * factor, white, 3);
    }
}
#endif
