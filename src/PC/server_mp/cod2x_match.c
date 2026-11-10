#include "common_types.h"
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX
#include "cod2x_server.h"
#include "cod2x_stop_gametype.h"
#include "../game_mp/g_cod2x_script.h"
#include "../qcommon/cod2x_http.h"
#include "../qcommon/cod2x_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CoD2x's match service. "match create <url> [headers]" downloads a match
   from a league's web service (the teams with their players' match_login
   ids, up to five maps, anything else as plain values) and loads its first
   map. The match starts with that map; the mod reads the match with the
   match* script calls, keeps its own results with matchSetData and
   matchPlayerSetData, and posts them back to the same address with
   matchUploadData. matchFinish kicks everyone and ends the match;
   matchCancel and "match cancel" end it with a reason that is posted as an
   error. While a match runs the zPAM version stays as it is, and a match
   that asks for "hideServerIp" turns sv_hideServerIp on until it ends. */

extern void Com_Printf(const char *fmt, ...);
extern const char *va(const char *fmt, ...);
extern int Cmd_Argc(void);
extern const char *Cmd_Argv(int arg);
extern void Cmd_AddCommand(const char *cmdName, xcommand_t function);
extern void Cbuf_AddText(const char *text);
extern int I_stricmp(const char *s1, const char *s2);
extern int Sys_Milliseconds(void);
extern int Dvar_GetInt(const char *dvarName);
extern const dvar_t *Dvar_FindVar(const char *dvarName);
extern void Dvar_SetBool(const dvar_t *dvar, unsigned char value);
extern void Dvar_SetString(const dvar_t *dvar, const char *value);
extern char *Info_ValueForKey(const char *s, const char *key);
extern qboolean SV_MapExists(const char *name);
extern void SV_DropClient(client_t *drop, const char *reason);
extern const char *Com_Cod2xErrorMessage(void);
extern unsigned int Scr_GetNumParam(void);
extern const char *Scr_GetString(unsigned int index);
extern int Scr_GetFunc(unsigned int index);
extern void Scr_Error(const char *error);
extern void Scr_AddUndefined(void);
extern void Scr_AddBool(int value);
extern void Scr_AddString(const char *value);
extern void Scr_MakeArray(void);
extern void Scr_AddArray(void);
extern const dvar_t *sv_maxclients;
extern serverStatic_t svs;

#define MATCH_MAX_CLIENTS 64
#define MATCH_TEAM_PLAYERS (MATCH_MAX_CLIENTS / 2)
#define MATCH_ID_SIZE 64
#define MATCH_NAME_SIZE 32
#define MATCH_MAP_SIZE 32
#define MATCH_MAX_MAPS 5
#define MATCH_URL_SIZE 256
#define MATCH_TIMEOUT_MSEC 5000
#define MATCH_LINE "==============================================\n"
#define MATCH_WIDE_LINE "======================================================\n"

/* ---- keys and values in the order they were first set --------------------- */

typedef struct {
    char *key, *value;
} MatchPair;

typedef struct {
    MatchPair *items;
    int count, capacity;
} MatchMap;

static char *Match_Copy(const char *s)
{
    size_t length = strlen(s);
    char *copy = malloc(length + 1);
    if (copy)
        memcpy(copy, s, length + 1);
    return copy;
}

static int Map_Find(const MatchMap *map, const char *key)
{
    for (int i = 0; i < map->count; ++i)
        if (!strcmp(map->items[i].key, key))
            return i;
    return -1;
}

static const char *Map_Get(const MatchMap *map, const char *key)
{
    int i = Map_Find(map, key);
    return i < 0 ? NULL : map->items[i].value;
}

/* A known key keeps its place; a new one goes last. */
static void Map_Set(MatchMap *map, const char *key, const char *value)
{
    int i = Map_Find(map, key);
    char *copy = Match_Copy(value);
    if (!copy)
        return;
    if (i >= 0) {
        free(map->items[i].value);
        map->items[i].value = copy;
        return;
    }
    if (map->count == map->capacity) {
        int capacity = map->capacity ? map->capacity * 2 : 16;
        MatchPair *grown = realloc(map->items, sizeof(*grown) * (size_t)capacity);
        if (!grown) {
            free(copy);
            return;
        }
        map->items = grown;
        map->capacity = capacity;
    }
    map->items[map->count].key = Match_Copy(key);
    if (!map->items[map->count].key) {
        free(copy);
        return;
    }
    map->items[map->count++].value = copy;
}

static void Map_Erase(MatchMap *map, const char *key)
{
    int i = Map_Find(map, key);
    if (i < 0)
        return;
    free(map->items[i].key);
    free(map->items[i].value);
    memmove(&map->items[i], &map->items[i + 1], sizeof(map->items[0]) * (size_t)(map->count - i - 1));
    --map->count;
}

static void Map_Clear(MatchMap *map)
{
    for (int i = 0; i < map->count; ++i) {
        free(map->items[i].key);
        free(map->items[i].value);
    }
    free(map->items);
    memset(map, 0, sizeof(*map));
}

/* ---- the match ------------------------------------------------------------ */

typedef struct {
    char id[MATCH_ID_SIZE];
    char name[MATCH_NAME_SIZE];
    MatchMap other;
    int teamNumber;
    char teamName[MATCH_NAME_SIZE];
} MatchPlayer;

typedef struct {
    char id[MATCH_ID_SIZE];
    char name[MATCH_NAME_SIZE];
    MatchPlayer players[MATCH_TEAM_PLAYERS];
    int playerCount;
    MatchMap other;
} MatchTeam;

typedef struct {
    char matchId[MATCH_ID_SIZE];
    MatchTeam teams[2];
    char maps[MATCH_MAX_MAPS][MATCH_MAP_SIZE];
    int mapCount;
    MatchMap other;
    char *json;
    char error[256];
} MatchData;

typedef struct {
    char *key;
    MatchMap fields;
} MatchPlayerProgress;

static struct {
    int downloading, loading, activated, uploading, uploadingError, canceling;
    char cancelReason[256];
    char url[MATCH_URL_SIZE];
    char *headers; /* from "match create", sent with every request */
    unsigned int generation; /* a new "match create" forgets the old requests */
    long long startTime;
    unsigned int startTick;
    MatchData *data;
    MatchMap global;
    MatchPlayerProgress *players;
    int playerCount, playerCapacity;
} match;

static int matchHidServerIp;

static void Match_DataFree(MatchData *data)
{
    if (!data)
        return;
    for (int t = 0; t < 2; ++t) {
        for (int i = 0; i < MATCH_TEAM_PLAYERS; ++i)
            Map_Clear(&data->teams[t].players[i].other);
        Map_Clear(&data->teams[t].other);
    }
    Map_Clear(&data->other);
    free(data->json);
    free(data);
}

static MatchData *Match_DataNew(void)
{
    return calloc(1, sizeof(MatchData));
}

static const MatchData *Match_Data(void)
{
    static MatchData empty;
    return match.data ? match.data : &empty;
}

static void Match_ProgressClear(void)
{
    Map_Clear(&match.global);
    for (int i = 0; i < match.playerCount; ++i) {
        free(match.players[i].key);
        Map_Clear(&match.players[i].fields);
    }
    free(match.players);
    match.players = NULL;
    match.playerCount = match.playerCapacity = 0;
}

static MatchMap *Match_PlayerProgress(const char *key)
{
    for (int i = 0; i < match.playerCount; ++i)
        if (!strcmp(match.players[i].key, key))
            return &match.players[i].fields;
    if (match.playerCount == match.playerCapacity) {
        int capacity = match.playerCapacity ? match.playerCapacity * 2 : 16;
        MatchPlayerProgress *grown = realloc(match.players, sizeof(*grown) * (size_t)capacity);
        if (!grown)
            return NULL;
        match.players = grown;
        match.playerCapacity = capacity;
    }
    memset(&match.players[match.playerCount], 0, sizeof(match.players[0]));
    match.players[match.playerCount].key = Match_Copy(key);
    if (!match.players[match.playerCount].key)
        return NULL;
    return &match.players[match.playerCount++].fields;
}

int SV_Cod2xMatchRunning(void)
{
    return match.activated || match.loading || match.downloading;
}

/* ---- reading a match ------------------------------------------------------- */

/* A plain value as CoD2x keeps it: a string (at most 511 bytes), a number with
   no decimals, true or false, and anything else as its JSON text. */
static void Match_SetValue(MatchMap *map, const char *json, const char *base, const char *key, const char *text)
{
    char path[512], value[512];
    snprintf(path, sizeof(path), "%s.%s", base, key);
    Map_Set(map, key, Cod2x_JsonString(json, path, value, sizeof(value)) ? value : text);
}

typedef struct {
    MatchData *data;
    const char *json;
    int teamNumber;
    MatchTeam *team;
    int failed;
} MatchParse;

static int Match_ParseMap(void *context, int index, const char *key, const char *value, size_t length)
{
    MatchParse *parse = context;
    MatchData *data = parse->data;
    (void)key;
    if (!length) {
        snprintf(data->error, sizeof(data->error), "Map name is empty on index %d", index);
        parse->failed = 1;
        return 0;
    }
    if (data->mapCount >= MATCH_MAX_MAPS) {
        snprintf(data->error, sizeof(data->error), "Too many maps (max %d)", MATCH_MAX_MAPS);
        parse->failed = 1;
        return 0;
    }
    /* The quotes go; nothing inside them is decoded. */
    if (length >= 2 && value[0] == '"' && value[length - 1] == '"') {
        ++value;
        length -= 2;
    }
    if (!length || length >= MATCH_MAP_SIZE) {
        snprintf(data->error, sizeof(data->error), "Invalid map name length on index %d", index);
        parse->failed = 1;
        return 0;
    }
    memcpy(data->maps[data->mapCount], value, length);
    data->maps[data->mapCount][length] = '\0';
    if (!SV_MapExists(data->maps[data->mapCount++])) {
        snprintf(data->error, sizeof(data->error), "Map '%s' does not exist on the server",
                 data->maps[data->mapCount - 1]);
        parse->failed = 1;
        return 0;
    }
    return 1;
}

typedef struct {
    MatchMap *map;
    const char *json, *base;
    const char *skip[4];
} MatchOther;

static int Match_ParseOther(void *context, int index, const char *key, const char *value, size_t length)
{
    MatchOther *other = context;
    (void)index;
    (void)length;
    for (int i = 0; i < 4; ++i)
        if (other->skip[i] && !strcmp(key, other->skip[i]))
            return 1;
    Match_SetValue(other->map, other->json, other->base, key, value);
    return 1;
}

static int Match_ParsePlayer(void *context, int index, const char *key, const char *value, size_t length)
{
    MatchParse *parse = context;
    MatchTeam *team = parse->team;
    MatchPlayer *player;
    MatchOther other = {0};
    char *json;
    (void)key;
    if (team->playerCount >= MATCH_TEAM_PLAYERS) {
        snprintf(parse->data->error, sizeof(parse->data->error), "Too many players in team %d (max %d)",
                 parse->teamNumber, MATCH_TEAM_PLAYERS);
        parse->failed = 1;
        return 0;
    }
    json = malloc(length + 1);
    if (!json) {
        parse->failed = 1;
        return 0;
    }
    memcpy(json, value, length);
    json[length] = '\0';
    player = &team->players[team->playerCount];
    player->teamNumber = parse->teamNumber;
    snprintf(player->teamName, sizeof(player->teamName), "%s", team->name);
    if (!Cod2x_JsonString(json, "$.uuid", player->id, sizeof(player->id))) {
        snprintf(parse->data->error, sizeof(parse->data->error), "Invalid player id in team %d on index %d",
                 parse->teamNumber, index);
        parse->failed = 1;
    } else if (!Cod2x_JsonString(json, "$.name", player->name, sizeof(player->name))) {
        snprintf(parse->data->error, sizeof(parse->data->error), "Invalid player name in team %d on index %d",
                 parse->teamNumber, index);
        parse->failed = 1;
    } else {
        other.map = &player->other;
        other.json = json;
        other.base = "$";
        other.skip[0] = "uuid";
        other.skip[1] = "name";
        Cod2x_JsonEach(json, "$", Match_ParseOther, &other);
        ++team->playerCount;
    }
    free(json);
    return !parse->failed;
}

static int Match_ParseTeam(const char *json, MatchData *data, int teamNumber)
{
    MatchTeam *team = &data->teams[teamNumber - 1];
    MatchParse parse = {data, json, teamNumber, team, 0};
    MatchOther other = {0};
    char path[64];

    snprintf(path, sizeof(path), "$.team%i.id", teamNumber);
    if (!Cod2x_JsonString(json, path, team->id, sizeof(team->id)) || !team->id[0]) {
        snprintf(data->error, sizeof(data->error), "Invalid team id");
        return 0;
    }
    snprintf(path, sizeof(path), "$.team%i.name", teamNumber);
    if (!Cod2x_JsonString(json, path, team->name, sizeof(team->name)) || !team->name[0]) {
        snprintf(data->error, sizeof(data->error), "Invalid team name");
        return 0;
    }
    snprintf(path, sizeof(path), "$.team%i.players", teamNumber);
    Cod2x_JsonEach(json, path, Match_ParsePlayer, &parse);
    if (parse.failed)
        return 0;
    if (team->playerCount <= 0) {
        snprintf(data->error, sizeof(data->error), "Team %d has no players", teamNumber);
        return 0;
    }
    snprintf(path, sizeof(path), "$.team%i", teamNumber);
    other.map = &team->other;
    other.json = json;
    other.base = path;
    other.skip[0] = "id";
    other.skip[1] = "name";
    other.skip[2] = "players";
    Cod2x_JsonEach(json, path, Match_ParseOther, &other);
    return 1;
}

/* The match as the league's web service describes it; error says what is wrong. */
static int Match_Parse(const char *json, MatchData *data)
{
    MatchParse parse = {data, json, 0, NULL, 0};
    MatchOther other = {0};

    if (!json || !json[0])
        return 0;
    data->json = Match_Copy(json);
    if (!Cod2x_JsonString(json, "$.matchId", data->matchId, sizeof(data->matchId)) || !data->matchId[0]) {
        snprintf(data->error, sizeof(data->error), "Missing or invalid 'matchId'");
        return 0;
    }
    Cod2x_JsonEach(json, "$.maps", Match_ParseMap, &parse);
    if (parse.failed)
        return 0;
    if (!Match_ParseTeam(json, data, 1) || !Match_ParseTeam(json, data, 2))
        return 0;
    other.map = &data->other;
    other.json = json;
    other.base = "$";
    other.skip[0] = "matchId";
    other.skip[1] = "maps";
    other.skip[2] = "team1";
    other.skip[3] = "team2";
    Cod2x_JsonEach(json, "$", Match_ParseOther, &other);
    return 1;
}

static const MatchPlayer *Match_FindPlayer(const char *uuid)
{
    const MatchData *data = Match_Data();
    if (!uuid || !uuid[0])
        return NULL;
    for (int t = 0; t < 2; ++t)
        for (int i = 0; i < data->teams[t].playerCount; ++i)
            if (!strcmp(data->teams[t].players[i].id, uuid))
                return &data->teams[t].players[i];
    return NULL;
}

/* ---- talking to the league ------------------------------------------------- */

typedef enum { MATCH_CREATE, MATCH_REDOWNLOAD, MATCH_UPLOAD, MATCH_UPLOAD_ERROR } MatchRequestKind;

typedef struct {
    MatchRequestKind kind;
    unsigned int generation;
    int onDone, onError;
    Cod2xScriptScope scope;
    char url[MATCH_URL_SIZE];
} MatchRequest;

static void Match_Done(void *context, const Cod2xHttpResponse *response);

/* The headers from "match create" go first, then the request's own. */
static void Match_Send(MatchRequest *request, const char *method, const char *body)
{
    Cod2xText headers = {0};
    Cod2xHttpRequest http;

    request->generation = match.generation;
    snprintf(request->url, sizeof(request->url), "%s", match.url);
    if (match.headers)
        Cod2x_TextAppendString(&headers, match.headers);
    if (body)
        Cod2x_TextAppendString(&headers, "Content-Type: application/json\r\n");
    memset(&http, 0, sizeof(http));
    http.method = method;
    http.url = match.url;
    http.headers = headers.data;
    http.body = body;
    http.bodyLength = body ? strlen(body) : 0;
    http.timeoutMs = MATCH_TIMEOUT_MSEC;
    http.connectTimeoutMs = MATCH_TIMEOUT_MSEC;
    /* CoD2x answers these at once, from inside the call. */
    if (!Cod2x_HttpValidUrl(match.url) || headers.failed || !Cod2x_HttpStart(&http, Match_Done, request)) {
        Cod2xHttpResponse failed;
        memset(&failed, 0, sizeof(failed));
        failed.body = "";
        failed.headers = "";
        snprintf(failed.error, sizeof(failed.error), "%s",
                 Cod2x_HttpValidUrl(match.url) ? "Failed to connect" : "Invalid URL");
        Match_Done(request, &failed);
    }
    Cod2x_TextFree(&headers);
}

static MatchRequest *Match_NewRequest(MatchRequestKind kind)
{
    MatchRequest *request = calloc(1, sizeof(*request));
    if (request)
        request->kind = kind;
    return request;
}

static void Match_ProgressJson(Cod2xText *json)
{
    char start[32];
    Cod2x_Iso8601(match.startTime, start, sizeof(start));
    Cod2x_TextAppendString(json, "{\n  \"type\": \"data\",\n");
    Cod2x_TextPrintf(json, "  \"start_time\": \"%s\",\n", start);
    for (int i = 0; i < match.global.count; ++i) {
        Cod2x_TextAppendString(json, "  \"");
        Cod2x_TextAppendJson(json, match.global.items[i].key, strlen(match.global.items[i].key));
        Cod2x_TextAppendString(json, "\": \"");
        Cod2x_TextAppendJson(json, match.global.items[i].value, strlen(match.global.items[i].value));
        Cod2x_TextAppendString(json, "\",\n");
    }
    Cod2x_TextAppendString(json, "  \"players\": [\n");
    for (int p = 0; p < match.playerCount; ++p) {
        const MatchMap *fields = &match.players[p].fields;
        Cod2x_TextAppendString(json, p ? ",\n    {\n" : "    {\n");
        for (int i = 0; i < fields->count; ++i) {
            Cod2x_TextAppendString(json, i ? ",\n      \"" : "      \"");
            Cod2x_TextAppendJson(json, fields->items[i].key, strlen(fields->items[i].key));
            Cod2x_TextAppendString(json, "\": \"");
            Cod2x_TextAppendJson(json, fields->items[i].value, strlen(fields->items[i].value));
            Cod2x_TextAppendString(json, "\"");
        }
        Cod2x_TextAppendString(json, "\n    }");
    }
    Cod2x_TextAppendString(json, "\n  ]\n}\n");
}

/* Posts the mod's results; done and error are script callbacks (0: none). */
static int Match_Upload(int onDone, int onError)
{
    Cod2xText json = {0};
    MatchRequest *request;
    if (!match.activated) {
        Com_Printf("Match is not activated, cannot upload data.\n");
        return 0;
    }
    if (match.uploading) {
        Com_Printf("Match upload already in progress.\n");
        return 0;
    }
    Match_ProgressJson(&json);
    request = Match_NewRequest(MATCH_UPLOAD);
    if (json.failed || !json.length || !request) {
        Com_Printf("Failed to create JSON data for match upload.\n");
        Cod2x_TextFree(&json);
        free(request);
        return 0;
    }
    match.uploading = 1;
    request->onDone = onDone;
    request->onError = onError;
    GScr_Cod2xScopeNow(&request->scope);
    Match_Send(request, "POST", json.data);
    Cod2x_TextFree(&json);
    return 1;
}

/* Tells the league what went wrong. */
static int Match_UploadError(const char *error, const char *message)
{
    Cod2xText json = {0};
    MatchRequest *request;
    if (!match.activated && !match.downloading && !match.loading) {
        Com_Printf("Match is not activated, cannot upload data.\n");
        return 0;
    }
    if (match.uploadingError)
        return 0;
    Cod2x_TextAppendString(&json, "{\n  \"type\": \"error\",\n  \"error\": \"");
    Cod2x_TextAppendJson(&json, error, strlen(error));
    Cod2x_TextAppendString(&json, "\",\n  \"errorMessage\": \"");
    Cod2x_TextAppendJson(&json, message, strlen(message));
    Cod2x_TextAppendString(&json, "\"\n}\n");
    request = Match_NewRequest(MATCH_UPLOAD_ERROR);
    if (json.failed || !request) {
        Cod2x_TextFree(&json);
        free(request);
        return 0;
    }
    match.uploadingError = 1;
    Match_Send(request, "POST", json.data);
    Cod2x_TextFree(&json);
    return 1;
}

static int Match_StatusOk(const Cod2xHttpResponse *response)
{
    return response->status == 200 || response->status == 201;
}

static void Match_Created(const MatchRequest *request, const Cod2xHttpResponse *response)
{
    MatchData *data;
    if (response->error[0]) {
        match.downloading = 0;
        Com_Printf(MATCH_LINE);
        Com_Printf("Match creating error while downloading data: %s\n", response->error);
        Com_Printf("  - error: %s\n", response->error);
        Com_Printf("  - URL: %s\n", request->url);
        Com_Printf(MATCH_LINE);
        return;
    }
    /* Cancelled or timed out meanwhile. */
    if (!match.downloading || match.canceling)
        return;
    if (!Match_StatusOk(response)) {
        Com_Printf("Match creating error, invalid status of downloading data: %d\n%s\n", response->status,
                   response->body);
        match.downloading = 0;
        return;
    }
    data = Match_DataNew();
    if (!data)
        return;
    Match_DataFree(match.data);
    match.data = data;
    if (!Match_Parse(response->body, data)) {
        Com_Printf("Match creating error, failed to parse match data:\n%s\n%s\n", response->body, data->error);
        Match_UploadError("Failed to parse match data", data->error);
        match.downloading = 0;
        return;
    }
    Com_Printf(MATCH_LINE);
    Com_Printf("Match downloading from %s completed!\n", request->url);
    Com_Printf("Loading first map...\n");
    Com_Printf(MATCH_LINE);
    match.downloading = 0;
    match.loading = 1;
    if (data->mapCount > 0)
        Cbuf_AddText(va("map %s\n", data->maps[0]));
    else
        Cbuf_AddText("map_restart\n");
}

static void Match_Redownloaded(const Cod2xHttpResponse *response)
{
    MatchData *data;
    const MatchData *current = Match_Data();
    if (response->error[0]) {
        Com_Printf("Match redownloading error while downloading data: %s\n", response->error);
        return;
    }
    if (!Match_StatusOk(response)) {
        Com_Printf("Match redownloading error, invalid status of downloading data: %d\n%s\n", response->status,
                   response->body);
        return;
    }
    data = Match_DataNew();
    if (!data)
        return;
    if (!Match_Parse(response->body, data)) {
        Com_Printf("Match redownloading error, failed to parse match data:\n%s\n%s\n", response->body, data->error);
        Match_UploadError("Failed to parse match data", data->error);
        Match_DataFree(data);
        return;
    }
    /* Players may join a running match; the match itself must stay the same. */
    if (strcmp(current->matchId, data->matchId)) {
        Com_Printf("Match redownloading error, match id does not match: %s != %s\n", current->matchId, data->matchId);
        Match_UploadError("Match redownloading error", "Match ID does not match the original one");
        Match_DataFree(data);
        return;
    }
    if (current->mapCount != data->mapCount) {
        Com_Printf("Match redownloading error, number of maps does not match: %d != %d\n", current->mapCount,
                   data->mapCount);
        Match_UploadError("Match redownloading error", "Number of maps does not match the original one");
        Match_DataFree(data);
        return;
    }
    for (int i = 0; i < current->mapCount; ++i) {
        if (strcmp(current->maps[i], data->maps[i])) {
            Com_Printf("Match redownloading error, map %d does not match: %s != %s\n", i, current->maps[i],
                       data->maps[i]);
            Match_UploadError("Match redownloading error", "Map rotation does not match the original one");
            Match_DataFree(data);
            return;
        }
    }
    Match_DataFree(match.data);
    match.data = data;
}

static void Match_Uploaded(const MatchRequest *request, const Cod2xHttpResponse *response)
{
    char error[256];
    match.uploading = 0;
    if (response->error[0]) {
        Com_Printf("Match uploading error: %s\n", response->error);
        snprintf(error, sizeof(error), "%s", response->error);
    } else if (!Match_StatusOk(response)) {
        Cod2xText json = {0};
        Com_Printf("Match uploading error, invalid status: %d\n%s\n", response->status, response->body);
        Match_ProgressJson(&json);
        Com_Printf("Uploaded JSON data:\n%s\n", json.data ? json.data : "");
        Cod2x_TextFree(&json);
        snprintf(error, sizeof(error), "Invalid status: %d", response->status);
    } else {
        if (request->onDone && GScr_Cod2xScopeCallable(&request->scope))
            GScr_Cod2xRunCallback(request->onDone, 0);
        return;
    }
    if (request->onError && GScr_Cod2xScopeCallable(&request->scope)) {
        Scr_AddString(error);
        GScr_Cod2xRunCallback(request->onError, 1);
    }
}

static void Match_ErrorUploaded(const Cod2xHttpResponse *response)
{
    match.uploadingError = 0;
    if (response->error[0])
        Com_Printf("Match error uploading failed: %s\n", response->error);
    else if (!Match_StatusOk(response))
        Com_Printf("Match error uploading failed, invalid status: %d\n%s\n", response->status, response->body);
}

static void Match_Done(void *context, const Cod2xHttpResponse *response)
{
    MatchRequest *request = context;
    if (request->generation == match.generation) {
        switch (request->kind) {
        case MATCH_CREATE:
            Match_Created(request, response);
            break;
        case MATCH_REDOWNLOAD:
            Match_Redownloaded(response);
            break;
        case MATCH_UPLOAD:
            Match_Uploaded(request, response);
            break;
        case MATCH_UPLOAD_ERROR:
            Match_ErrorUploaded(response);
            break;
        }
    }
    free(request);
}

/* ---- the match's course ---------------------------------------------------- */

static int Match_Redownload(void)
{
    MatchRequest *request;
    if (!match.activated)
        return 0;
    request = Match_NewRequest(MATCH_REDOWNLOAD);
    if (request)
        Match_Send(request, "GET", NULL);
    return 1;
}

static void Match_Cancel(const char *reason)
{
    if (match.activated) {
        match.canceling = 1;
        if (reason && reason[0])
            snprintf(match.cancelReason, sizeof(match.cancelReason), "Match was canceled. Reason: %s", reason);
        else
            match.cancelReason[0] = '\0';
        /* Also when the mod's StopGameType blocks map changes. */
        GScr_Cod2xAllowOneMapChange();
        Com_Printf("Match is being canceled...\n");
        if (match.cancelReason[0])
            Com_Printf("%s\n", match.cancelReason);
        Cbuf_AddText("fast_restart\n");
    } else {
        Com_Printf("No match is currently active.\n");
    }
    match.downloading = 0;
    match.loading = 0;
}

static void Match_Finish(void)
{
    const MatchData *data = Match_Data();
    if (!match.activated || match.canceling)
        return;
    for (int i = 0; i < sv_maxclients->current.integer; ++i)
        if (svs.clients[i].state)
            SV_DropClient(&svs.clients[i], "\n^2Match has finished^7");
    Com_Printf(MATCH_WIDE_LINE);
    Com_Printf("Match finished successfully.\n");
    Com_Printf("- URL: %s\n", match.url);
    Com_Printf("- Match ID: %s\n", data->matchId);
    Com_Printf("- Teams: %s (%s) vs %s (%s)\n", data->teams[0].name, data->teams[0].id, data->teams[1].name,
               data->teams[1].id);
    Com_Printf(MATCH_WIDE_LINE);
    Match_Cancel(NULL);
}

/* Before CodeCallback_StartGameType: the first map of a downloaded match starts it. */
void SV_Cod2xMatchStartGameType(void)
{
    const MatchData *data = Match_Data();
    const char *hide;
    if (!match.loading)
        return;
    match.loading = 0;
    match.activated = 1;
    /* Streams of the match must not show the server's address. */
    hide = Map_Get(&data->other, "hideServerIp");
    if (hide && (!strcmp(hide, "true") || !strcmp(hide, "1"))) {
        const dvar_t *hideServerIp = Dvar_FindVar("sv_hideServerIp");
        if (hideServerIp && !hideServerIp->current.enabled) {
            Dvar_SetBool(hideServerIp, 1);
            matchHidServerIp = 1;
        }
    }
    Com_Printf(MATCH_WIDE_LINE);
    Com_Printf("Match started successfully.\n");
    Com_Printf("- URL: %s\n", match.url);
    Com_Printf("- Match ID: %s\n", data->matchId);
    Com_Printf("- Teams: %s (%s) vs %s (%s)\n", data->teams[0].name, data->teams[0].id, data->teams[1].name,
               data->teams[1].id);
    Com_Printf("- Maps: ");
    for (int i = 0; i < data->mapCount; ++i) {
        if (i > 0)
            Com_Printf(", ");
        Com_Printf("%s", data->maps[i]);
    }
    Com_Printf("\n");
    Com_Printf(MATCH_WIDE_LINE);
}

/* After CodeCallback_StopGameType, so the mod has posted its final results. */
void SV_Cod2xMatchBeforeMapChange(int shutdown)
{
    const char *error = Com_Cod2xErrorMessage();
    if (shutdown && match.activated && error)
        Match_UploadError("Server shutdown error", error);
    if (!match.canceling && !shutdown)
        return;
    if (match.canceling && match.cancelReason[0])
        Match_UploadError("Match canceled", match.cancelReason);
    /* Frames no longer run: the posts get a second to finish. */
    for (int waited = 0; shutdown && (match.uploading || match.uploadingError) && waited < 1000; waited += 10) {
        Cod2x_HttpFrameFor(Match_Done);
        if (match.uploading || match.uploadingError)
            Cod2x_Sleep(10);
    }
    if (match.canceling)
        Com_Printf("Match ended.\n");
    match.canceling = 0;
    match.cancelReason[0] = '\0';
    match.uploading = 0;
    match.uploadingError = 0;
    match.activated = 0;
    match.loading = 0;
    match.downloading = 0;
    Match_ProgressClear();
    if (matchHidServerIp) {
        const dvar_t *hideServerIp = Dvar_FindVar("sv_hideServerIp");
        if (hideServerIp)
            Dvar_SetBool(hideServerIp, 0);
        matchHidServerIp = 0;
    }
}

void SV_Cod2xMatchFrame(void)
{
    if ((int)((unsigned int)Sys_Milliseconds() - match.startTick) > MATCH_TIMEOUT_MSEC &&
        (match.loading || match.downloading) && !match.activated) {
        match.loading = 0;
        match.downloading = 0;
        Com_Printf("Match %s timed out\n", match.loading ? "loading" : "downloading");
    }
}

/* ---- the match command ------------------------------------------------------- */

static void Match_Usage(void)
{
    Com_Printf("USAGE: match <command> <options>\n");
    Com_Printf("Client:\n");
    Com_Printf("  match login <hash>\n");
    Com_Printf("    <hash> - The hash to login with to the match server; will be removed after disconnect\n");
    Com_Printf("Server:\n");
    Com_Printf("  match create <endpoint>\n");
    Com_Printf("    <endpoint> - URL for getting match data\n");
    Com_Printf("    Example: match create http://web.com/matches/MATCHID1\n");
    Com_Printf("  match status\n");
    Com_Printf("    Displays the current match status\n");
    Com_Printf("  match upload\n");
    Com_Printf("    Forces upload of match data\n");
    Com_Printf("  match cancel\n");
    Com_Printf("    Cancels the current match\n");
}

static void Match_Create(void)
{
    const char *endpoint = Cmd_Argv(2);
    const char *headers = Cmd_Argv(3);
    MatchRequest *request;

    /* Unquoted, the "//" of a URL starts a comment. */
    if (!endpoint[0]) {
        Com_Printf("Please provide an endpoint URL.\n");
        return;
    }
    if (match.downloading) {
        Com_Printf("Match is already downloading, please wait or cancel it first.\n");
        return;
    }
    if (match.activated || match.loading) {
        Com_Printf("Match is already in progress, cancel it first.\n");
        return;
    }
    /* Answers to an earlier match are not wanted any more. */
    ++match.generation;
    free(match.headers);
    match.headers = NULL;
    Match_DataFree(match.data);
    match.data = NULL;
    if (strlen(endpoint) >= sizeof(match.url)) {
        Com_Printf("Match endpoint URL too long (max %zu characters)\n", sizeof(match.url) - 1);
        return;
    }
    snprintf(match.url, sizeof(match.url), "%s", endpoint);
    match.downloading = 1;
    match.loading = 0;
    match.activated = 0;
    match.uploading = 0;
    match.uploadingError = 0;
    match.canceling = 0;
    match.cancelReason[0] = '\0';
    match.startTime = Cod2x_UtcMsec();
    match.startTick = (unsigned int)Sys_Milliseconds();
    Match_ProgressClear();

    /* "Name: value|Name2: value2" */
    if (headers[0]) {
        Cod2xText lines = {0};
        const char *start = headers;
        for (;;) {
            const char *bar = strchr(start, '|');
            size_t length = bar ? (size_t)(bar - start) : strlen(start);
            if (length) {
                Cod2x_TextAppend(&lines, start, length);
                Cod2x_TextAppendString(&lines, "\r\n");
            }
            if (!bar)
                break;
            start = bar + 1;
        }
        if (!lines.failed && lines.length)
            match.headers = Match_Copy(lines.data);
        Cod2x_TextFree(&lines);
    }

    Com_Printf(MATCH_LINE);
    Com_Printf("Downloading match data from %s...\n", match.url);
    Com_Printf(MATCH_LINE);
    request = Match_NewRequest(MATCH_CREATE);
    if (request)
        Match_Send(request, "GET", NULL);
}

static void Match_Status(void)
{
    if (match.activated) {
        Cod2xText progress = {0};
        Match_ProgressJson(&progress);
        Com_Printf("Match is currently active.\n");
        Com_Printf("URL: %s\n", match.url);
        Com_Printf("Match data:\n%s\n", Match_Data()->json ? Match_Data()->json : "");
        Com_Printf("Progress data:\n%s\n", progress.data ? progress.data : "");
        Cod2x_TextFree(&progress);
    } else if (match.downloading) {
        Com_Printf("Match is currently downloading...\n");
    } else if (match.loading) {
        Com_Printf("Match is currently loading...\n");
    } else {
        Com_Printf("No match is currently active.\n");
    }
}

static void SV_Cod2xMatch_f(void)
{
    const char *command;
    if (Cmd_Argc() < 2) {
        Match_Usage();
        return;
    }
    command = Cmd_Argv(1);
    if (!I_stricmp(command, "login")) {
        const dvar_t *login;
        if (Dvar_GetInt("dedicated") > 0) {
            Com_Printf("Match login command is only available for clients.\n");
            return;
        }
        if (!Cmd_Argv(2)[0]) {
            Com_Printf("Please provide a hash to login.\n");
            return;
        }
        login = Dvar_FindVar("match_login");
        if (login)
            Dvar_SetString(login, Cmd_Argv(2));
    } else if (!I_stricmp(command, "create")) {
        Match_Create();
    } else if (!I_stricmp(command, "status")) {
        Match_Status();
    } else if (!I_stricmp(command, "redownload")) {
        Match_Redownload();
    } else if (!I_stricmp(command, "upload")) {
        Match_Upload(0, 0);
    } else if (!I_stricmp(command, "cancel")) {
        Match_Cancel("console command");
    } else {
        Com_Printf("Unknown match command: %s\n", command);
        Match_Usage();
    }
}

void SV_Cod2xMatchInit(void)
{
    Cmd_AddCommand("match", SV_Cod2xMatch_f);
}

/* ---- script calls ---------------------------------------------------------- */

/* The mod's view of the match: its fields, the teams' (as team1_..., team2_...)
   and anything it set itself. */
static void Match_RefreshGlobal(void)
{
    const MatchData *data = Match_Data();
    Map_Set(&match.global, "match_id", data->matchId);
    Map_Set(&match.global, "team1_id", data->teams[0].id);
    Map_Set(&match.global, "team2_id", data->teams[1].id);
    Map_Set(&match.global, "team1_name", data->teams[0].name);
    Map_Set(&match.global, "team2_name", data->teams[1].name);
    for (int i = 0; i < data->other.count; ++i)
        Map_Set(&match.global, data->other.items[i].key, data->other.items[i].value);
    for (int t = 0; t < 2; ++t) {
        for (int i = 0; i < data->teams[t].other.count; ++i) {
            char key[512];
            snprintf(key, sizeof(key), "team%d_%s", t + 1, data->teams[t].other.items[i].key);
            Map_Set(&match.global, key, data->teams[t].other.items[i].value);
        }
    }
}

void GScr_Cod2xMatchUploadData(void)
{
    unsigned int count;
    int onDone, onError;
    if (!match.activated) {
        Scr_AddBool(0);
        return;
    }
    count = Scr_GetNumParam();
    onDone = count >= 1 ? Scr_GetFunc(0) : 0;
    onError = count >= 2 ? Scr_GetFunc(1) : 0;
    Match_Upload(onDone, onError);
    Scr_AddBool(1);
}

static void Match_GetSetGlobal(int set)
{
    const char *key, *value;
    if (!match.activated) {
        Scr_AddBool(0);
        return;
    }
    Match_RefreshGlobal();
    if (set) {
        unsigned int count = Scr_GetNumParam();
        for (unsigned int i = 0; i + 1 < count; i += 2)
            Map_Set(&match.global, Scr_GetString(i), Scr_GetString(i + 1));
        Scr_AddBool(1);
        return;
    }
    key = Scr_GetString(0);
    value = Map_Get(&match.global, key);
    if (value) {
        Scr_AddString(value);
        return;
    }
    {
        const MatchData *data = Match_Data();
        int team = !strncmp(key, "team1_", 6) ? 0 : !strncmp(key, "team2_", 6) ? 1 : -1;
        if (team >= 0 && (!strcmp(key + 6, "player_uuids") || !strcmp(key + 6, "player_names"))) {
            int names = !strcmp(key + 6, "player_names");
            Scr_MakeArray();
            for (int i = 0; i < data->teams[team].playerCount; ++i) {
                Scr_AddString(names ? data->teams[team].players[i].name : data->teams[team].players[i].id);
                Scr_AddArray();
            }
        } else if (!strcmp(key, "maps")) {
            Scr_MakeArray();
            for (int i = 0; i < data->mapCount; ++i) {
                Scr_AddString(data->maps[i]);
                Scr_AddArray();
            }
        } else {
            Scr_AddString("");
        }
    }
}

void GScr_Cod2xMatchSetData(void)
{
    unsigned int count = Scr_GetNumParam();
    if (count < 2) {
        Scr_Error(va("matchSetData: not enough parameters, expected 2, got %u", count));
        return;
    }
    if (count % 2) {
        Scr_Error(va("matchSetData: expected even number of parameters, got %u", count));
        return;
    }
    Match_GetSetGlobal(1);
}

void GScr_Cod2xMatchGetData(void)
{
    unsigned int count = Scr_GetNumParam();
    if (count < 1) {
        Scr_Error(va("matchGetData: not enough parameters, expected 1, got %u", count));
        return;
    }
    Match_GetSetGlobal(0);
}

void GScr_Cod2xMatchRedownloadData(void)
{
    Scr_AddBool(Match_Redownload());
}

void GScr_Cod2xMatchClearData(void)
{
    Match_ProgressClear();
    Scr_AddBool(1);
}

void GScr_Cod2xMatchIsActivated(void)
{
    Scr_AddBool(match.activated);
}

void GScr_Cod2xMatchCancel(void)
{
    if (!match.activated) {
        Scr_AddBool(0);
        return;
    }
    Match_Cancel(Scr_GetNumParam() >= 1 ? Scr_GetString(0) : NULL);
    Scr_AddBool(1);
}

void GScr_Cod2xMatchFinish(void)
{
    if (!match.activated) {
        Scr_AddBool(0);
        return;
    }
    Match_Finish();
    Scr_AddBool(1);
}

/* ---- player methods ---------------------------------------------------------- */

static client_t *Match_ScriptClient(scr_entref_t entref)
{
    if (entref.entnum >= MATCH_MAX_CLIENTS) {
        Scr_Error(va("entity %d is not a player", entref.entnum));
        Scr_AddUndefined();
        return NULL;
    }
    return &svs.clients[entref.entnum];
}

/* The player's results are kept under their match_login id when it belongs to
   a team, else under their GUID, else under their name. */
static void Match_PlayerGetSet(scr_entref_t entref, int set)
{
    client_t *client = Match_ScriptClient(entref);
    const MatchPlayer *player;
    MatchMap *fields;
    char uuid[MATCH_ID_SIZE * 4], key[MATCH_ID_SIZE * 4 + 8];

    if (!client)
        return;
    if (client->state < 2 /* connected */ || !match.activated) {
        Scr_AddBool(0);
        return;
    }
    snprintf(uuid, sizeof(uuid), "%s", Info_ValueForKey(client->userinfo, "match_login"));
    player = Match_FindPlayer(uuid);
    if (player)
        snprintf(key, sizeof(key), "UUID_%s", uuid);
    else if (client->guid)
        snprintf(key, sizeof(key), "GUID_%i", client->guid);
    else
        snprintf(key, sizeof(key), "NAME_%s", client->name);

    fields = Match_PlayerProgress(key);
    if (!fields) {
        Scr_AddBool(0);
        return;
    }
    /* CoD2x means to stamp the first sighting here but its check always
       finds the entry it has just made, so first_time is never written. */
    Map_Set(fields, "key", key);
    Map_Set(fields, "uuid", uuid);
    Map_Set(fields, "name", player ? player->name : client->name);
    Map_Set(fields, "team", player ? va("team%i", player->teamNumber) : "");
    Map_Set(fields, "team_name", player ? player->teamName : "");
    if (!player)
        Map_Set(fields, "debug",
                uuid[0] ? "Player's UUID is not part of any team" : "Player did not login with /match login <uuid>");
    else
        Map_Erase(fields, "debug");
    if (player)
        for (int i = 0; i < player->other.count; ++i)
            Map_Set(fields, player->other.items[i].key, player->other.items[i].value);

    if (set) {
        unsigned int count = Scr_GetNumParam();
        for (unsigned int i = 0; i + 1 < count; i += 2)
            Map_Set(fields, Scr_GetString(i), Scr_GetString(i + 1));
        Scr_AddBool(1);
    } else {
        const char *value = Map_Get(fields, Scr_GetString(0));
        Scr_AddString(value ? value : "");
    }
}

void GScr_Cod2xMatchPlayerSetData(scr_entref_t entref)
{
    unsigned int count = Scr_GetNumParam();
    if (count < 2) {
        Scr_Error(va("matchSetData: not enough parameters, expected 2, got %u", count));
        Scr_AddUndefined();
        return;
    }
    if (count % 2) {
        Scr_Error(va("matchSetData: expected even number of parameters, got %u", count));
        Scr_AddUndefined();
        return;
    }
    Match_PlayerGetSet(entref, 1);
}

void GScr_Cod2xMatchPlayerGetData(scr_entref_t entref)
{
    unsigned int count = Scr_GetNumParam();
    if (count != 1) {
        Scr_Error(va("matchGetData: expected 1 parameter, got %u", count));
        Scr_AddUndefined();
        return;
    }
    Match_PlayerGetSet(entref, 0);
}

void GScr_Cod2xMatchPlayerIsAllowed(scr_entref_t entref)
{
    client_t *client = Match_ScriptClient(entref);
    const char *login;
    if (!client)
        return;
    if (!match.activated) {
        Scr_AddBool(0);
        return;
    }
    login = Info_ValueForKey(client->userinfo, "match_login");
    Scr_AddBool(login && login[0] && Match_FindPlayer(login) != NULL);
}
#endif
