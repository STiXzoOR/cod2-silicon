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
extern void Com_Error(errorParm_t, const char *, ...);
extern MaterialHandle CL_RegisterMaterial(const char *, int);
extern const char *CL_GetConfigString(int);
extern void CG_DrawRotatedPic(float, float, float, float, int, int, float, const vec_t *, MaterialHandle);
extern void UI_DrawText(const char *, int, FontHandle, float, float, int, int, float, const vec_t *, int);
extern int UI_TextWidth(const char *, int, FontHandle, float);
extern int UI_TextHeight(FontHandle, float);
extern clientActive_t clients;
extern clientStatic_t cls;
extern sharedUiInfo_t sharedUiInfo;

static const dvar_t *drawRadar, *radarScale, *mapImage, *imageX, *imageY, *rotation;
static const dvar_t *entityScale, *entityX, *entityY, *radarX, *radarY, *numberSwitch, *radarColor;
static char radarMap[64];
static int calibration, calibrationFrame = -1, lastDrawTime, commandRegistered;
static float worldPoints[2][2], imagePoints[2][2];
static const char *radarFields[6] = {"scale", "offsetX", "offsetY", "rotation", "imageOffsetX", "imageOffsetY"};
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
        /* CoD2x names the file without its mp/ folder here. */
        Com_Printf("^1Radar file not found: maps/%s.radar\n", name);
        return;
    }
    /* Each value follows its label; an empty value drops the client, as in CoD2x. */
    cursor = buffer;
    Com_Parse(&cursor);
    token = Com_Parse(&cursor);
    if (!token || !*token) {
        FS_FreeFile(buffer);
        Com_Error(ERR_DROP, "Invalid image name in radar file: maps/%s.radar\n", name);
        return;
    }
    snprintf(image, sizeof(image), "%s", token);
    for (i = 0; i < 6; ++i) {
        Com_Parse(&cursor);
        token = Com_Parse(&cursor);
        if (!token || !*token) {
            FS_FreeFile(buffer);
            Com_Error(ERR_DROP, "Invalid %s in radar file: maps/%s.radar\n", radarFields[i], name);
            return;
        }
        values[i] = i == 3 ? (float)atoi(token) : (float)atof(token);
    }
    FS_FreeFile(buffer);
    Dvar_SetString(mapImage, image);
    Dvar_SetFloat(imageX, values[4]);
    Dvar_SetFloat(imageY, values[5]);
    Dvar_SetFloat(entityScale, values[0]);
    Dvar_SetFloat(entityX, values[1]);
    Dvar_SetFloat(entityY, values[2]);
    Dvar_SetInt(rotation, (int)values[3]);
}

static void CG_Cod2xRadarCalibrate_f(void)
{
    float diffRadar[2], diffWorld[2], lengthRadar, lengthWorld, scale, offsetX, offsetY;
    if (!CG_Cod2xRadarCheats()) {
        Com_Printf("Radar calibration is only available when cheats are enabled or demo is playing.\n");
        return;
    }
    if (!drawRadar->current.enabled) {
        Com_Printf("Radar is not enabled.\n");
        return;
    }
    /* 1: follow the camera for the first world point, 2: move the first map point with
       the mouse, 3 and 4: the same for the second points, then compute. */
    if (calibration == 0) {
        calibration = 1;
        imagePoints[0][0] = imagePoints[1][0] = 400;
        imagePoints[0][1] = imagePoints[1][1] = 0;
        return;
    }
    if (++calibration <= 4)
        return;
    calibration = 0;
    diffRadar[0] = (imagePoints[1][0] - imagePoints[0][0]) / 4;
    diffRadar[1] = (imagePoints[1][1] - imagePoints[0][1]) / 4;
    diffWorld[0] = worldPoints[1][0] - worldPoints[0][0];
    diffWorld[1] = worldPoints[1][1] - worldPoints[0][1];
    lengthRadar = sqrtf(diffRadar[0] * diffRadar[0] + diffRadar[1] * diffRadar[1]);
    lengthWorld = sqrtf(diffWorld[0] * diffWorld[0] + diffWorld[1] * diffWorld[1]);
    scale = lengthRadar / lengthWorld;
    offsetX = imagePoints[0][0] / 4 - worldPoints[0][0] * scale;
    offsetY = imagePoints[0][1] / 4 + worldPoints[0][1] * scale - 100;
    /* Two identical world points give no scale; keep the old values rather than store NaN. */
    if (isfinite(scale) && isfinite(offsetX) && isfinite(offsetY)) {
        Dvar_SetFloat(entityScale, scale);
        Dvar_SetFloat(entityX, offsetX);
        Dvar_SetFloat(entityY, offsetY);
    }
    Com_Printf("Calibration done\n");
    Com_Printf("Point 1:        %f, %f\n", imagePoints[0][0], imagePoints[0][1]);
    Com_Printf("Point 2:        %f, %f\n", imagePoints[1][0], imagePoints[1][1]);
    Com_Printf("World Point 1:  %f, %f\n", worldPoints[0][0], worldPoints[0][1]);
    Com_Printf("World Point 2:  %f, %f\n", worldPoints[1][0], worldPoints[1][1]);
    Com_Printf("Diff Radar:     %f, %f\n", diffRadar[0], diffRadar[1]);
    Com_Printf("Diff World:     %f, %f\n", diffWorld[0], diffWorld[1]);
    Com_Printf("Length Radar:   %f\n", lengthRadar);
    Com_Printf("Length World:   %f\n", lengthWorld);
    Com_Printf("---\n");
    Com_Printf("Scale:          %f\n", scale);
    Com_Printf("OffsetX:        %f\n", offsetX);
    Com_Printf("OffsetY:        %f\n", offsetY);
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
    /* CoD2x places players at their snapshot position and heading. */
    for (i = 0; i < 3; ++i)
        fireEvents[fireIndex].origin[i] = cg_entities[entityNum].currentState.pos.trBase[i];
    fireEvents[fireIndex].yaw = cg_entities[entityNum].nextState.apos.trBase[1];
}

static void CG_Cod2xRadarCoordinates(const vec_t *origin, float *x, float *y)
{
    Cod2x_RadarPoint(origin[0], origin[1], entityScale->current.value,
                    entityX->current.value, entityY->current.value, radarScale->current.value,
                    radarX->current.value, radarY->current.value, imageX->current.value,
                    imageY->current.value, rotation->current.integer, x, y);
}

static void CG_Cod2xRadarDrawCalibration(float x, float y)
{
    static const vec4_t red = {1, 0, 0, 1}, picked = {0, 1, 0, .5f};
    static const char *steps[4] = {
        "1. Move the camera to the first position on the map",
        "2. Select current location on the map",
        "3. Move the camera to the second position on the map",
        "4. Select current location on the map"
    };
    FontHandle font = sharedUiInfo.assets.textFont;
    vec4_t textColor = {1, 1, 1, 1};
    int i;
    /* Blink between white and cyan every second. */
    textColor[0] = cg->snap->ps.commandTime % 2000 > 1000 ? 1 : 0;
    UI_DrawText("Calibrating radar", INT_MAX, font, 0, 100, 2, 1, .5f, textColor, 0);
    if (calibration == 1 || calibration == 3) {
        worldPoints[calibration > 2][0] = cg->refdef.vieworg[0];
        worldPoints[calibration > 2][1] = cg->refdef.vieworg[1];
    } else if (calibrationFrame != cls.realtime) {
        int index = clients.mouseIndex ^ 1;
        imagePoints[calibration > 2][0] += clients.mouseDx[index] / 10.0f;
        imagePoints[calibration > 2][1] += clients.mouseDy[index] / 10.0f;
    }
    calibrationFrame = cls.realtime;
    for (i = 0; i < calibration; ++i)
        UI_DrawText(steps[i], INT_MAX, font, 0, 120 + 10 * i, 2, 1, .2f, textColor, 3);
    for (i = 0; i < 2 && calibration >= 2 * (i + 1); ++i) {
        CG_DrawRotatedPic(x + imagePoints[i][0] - 5, y + imagePoints[i][1] - 5, 10, 10, 1, 1, 0,
                          picked, cgs->media.whiteMaterial);
        CG_DrawRotatedPic(x + imagePoints[i][0] - 1, y + imagePoints[i][1] - 1, 2, 2, 1, 1, 0,
                          red, cgs->media.whiteMaterial);
    }
}

void CG_Cod2xRadarDraw(void)
{
    static const vec4_t white = {1, 1, 1, 1}, yellow = {1, 1, 0, 1}, background = {0, 0, 0, .9f};
    vec4_t color, teamColors[3] = {{1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}};
    MaterialHandle material, arrow, circle;
    FontHandle numberFont, measureFont;
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
    /* The map points are picked on a darkened screen. */
    if (calibration == 2 || calibration == 4)
        CG_DrawRotatedPic(0, 0, 640, 480, 4, 4, 0, background, cgs->media.whiteMaterial);
    for (i = 0; i < 3; ++i)
        color[i] = radarColor->current.vector[i];
    color[3] = 1;
    material = CL_RegisterMaterial(mapImage->current.string, 7);
    CG_DrawRotatedPic(x, y, size, size, 1, 1, calibration ? 0 : (float)rotation->current.integer, color, material);
    if (calibration) {
        CG_Cod2xRadarDrawCalibration(x, y);
        return;
    }
    size = 10 * factor;
    material = CL_RegisterMaterial("radar_player_fire", 7);
    for (i = 0; i < 128; ++i) {
        int age = cg->time - fireEvents[i].time;
        if (!fireEvents[i].time || age <= 0 || age >= 1000)
            continue;
        CG_Cod2xRadarCoordinates(fireEvents[i].origin, &x, &y);
        color[0] = 1; color[1] = color[2] = .5f; color[3] = 1 - age / 1000.0f;
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
    /* Team colours come from g_TeamColor_*; without them the circles stay white. */
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
    numberFont = sharedUiInfo.assets.bigFont;
    measureFont = sharedUiInfo.assets.textFont;
    for (i = 0; i < 64; ++i) {
        const centity_t *cent = &cg_entities[i];
        const clientInfo_t *ci;
        char number[16];
        int team;
        float arrowScale = 1, circleScale = 1, heading, textScale;
        if (cent->currentState.eType != 1 || !cent->nextValid ||
            (cent->currentState.eFlags & 0x20001) || cent->currentState.clientNum < 0 ||
            cent->currentState.clientNum >= 64)
            continue;
        ci = &cg->bgs.clientinfo[cent->currentState.clientNum];
        team = ci->team;
        if (!ci->infoValid || team >= TEAM_SPECTATOR || team < TEAM_FREE)
            continue;
        CG_Cod2xRadarCoordinates(cent->currentState.pos.trBase, &x, &y);
        heading = 90 + rotation->current.integer - cent->nextState.apos.trBase[1];
        /* The followed player has a larger yellow arrow and a slightly larger circle. */
        if (cg->snap->ps.clientNum == i) {
            arrowScale = 1.5f;
            circleScale = 1.2f;
        }
        size = 10 * factor;
        CG_DrawRotatedPic(x - size * arrowScale * .5f, y - size * arrowScale * .5f, size * arrowScale,
                          size * arrowScale, 1, 1, heading, arrowScale > 1 ? yellow : white, arrow);
        CG_DrawRotatedPic(x - size * circleScale * .5f, y - size * circleScale * .5f, size * circleScale,
                          size * circleScale, 1, 1, heading, teamColors[team], circle);
        snprintf(number, sizeof(number), "%d", teamNumbers[i]);
        /* Measured in the normal font, drawn in the big one, as CoD2x does. */
        textScale = .1f * circleScale * factor;
        UI_DrawText(number, INT_MAX, numberFont,
                    x - UI_TextWidth(number, 0, measureFont, textScale) / 2.0f,
                    y + UI_TextHeight(measureFont, textScale) / 2.0f, 1, 1, textScale, white, 3);
    }
}
#endif
