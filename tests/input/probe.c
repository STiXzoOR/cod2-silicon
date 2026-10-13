/* Test-only FIFO input and read-only UI observer. Never linked into the game. */
#include <SDL.h>
#include "common_types.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern int TestInput_NativeText(const char *, const char *);
extern int TestInput_NativeWheel(const char *, float, float, int);
extern void TestInput_ReleaseText(void);
extern int TestInput_WindowMetrics(SDL_Window *, float *);
static int initialized, input = -1;
static FILE *ack;
static const char *output, *nativeLibrary;
static char pending[8192];
static size_t pendingLength;
static Uint64 resumeAt;
static int traceEvents;
static int traceText;
static uiInfo_t **info;
static sharedUiInfo_t *shared;
static SDL_Window **gameWindow;
static void (*placement)(float *, float *, float *, float *, int, int);
static void (*screenX)(float *, int), (*screenY)(float *, int);
static const char *(*dvarString)(const char *);
static const char *(*translate)(const char *);
static int (*showDvar)(const itemDef_t *, int);
static void (*serverAddress)(int, int, char *, int);
static void (*serverInfo)(int, int, char *, int);
static const char *(*infoValue)(const char *, const char *);
static int (*serverCount)(int);
static int (*serverPing)(int, int), (*serverDirty)(int, int);
static void (*clientState)(uiClientState_t *);
static char *(*binding)(int);
static int *editingField, *waitingForKey;
static itemDef_t **capturedItem;
static void **captureFunction;
static void **clientImport;
static languageInfo_t *languages;

static void jsonString(FILE *stream, const char *value)
{
    fputc('"', stream);
    if (value) for (const unsigned char *p = (const unsigned char *)value; *p; ++p) {
        if (*p == '"' || *p == '\\') fprintf(stream, "\\%c", *p);
        else if (*p < 32) fprintf(stream, "\\u%04x", *p);
        else if (*p >= 128) {
            int length = (*p & 0xe0) == 0xc0 ? 2 : ((*p & 0xf0) == 0xe0 ? 3 : ((*p & 0xf8) == 0xf0 ? 4 : 0));
            unsigned code = length ? *p & ((1u << (7 - length)) - 1) : 0;
            int valid = length != 0;
            for (int n = 1; valid && n < length; ++n) {
                if (!p[n] || (p[n] & 0xc0) != 0x80) valid = 0;
                else code = (code << 6) | (p[n] & 0x3f);
            }
            if (valid && code >= (length == 2 ? 0x80 : (length == 3 ? 0x800 : 0x10000)) &&
                code <= 0x10ffff && (code < 0xd800 || code > 0xdfff)) {
                fwrite(p, 1, length, stream); p += length - 1;
            } else fprintf(stream, "\\u%04x", *p);
        } else fputc(*p, stream);
    }
    fputc('"', stream);
}

static void result(unsigned id, const char *command, int status)
{
    fprintf(ack, "{\"id\":%u,\"command\":", id); jsonString(ack, command);
    fprintf(ack, ",\"status\":%d,\"tick\":%llu}\n", status, SDL_GetTicks64());
    fflush(ack);
}

static void initialize(void)
{
    initialized = 1;
    const char *fifo = getenv("COD2_TEST_INPUT_FIFO");
    output = getenv("COD2_TEST_INPUT_OUTPUT");
    nativeLibrary = getenv("COD2_TEST_INPUT_SDL3");
    const char *ackPath = getenv("COD2_TEST_INPUT_ACK");
    struct stat state;
    if (!fifo || !output || !ackPath || !nativeLibrary || lstat(fifo, &state) || !S_ISFIFO(state.st_mode)) return;
    input = open(fifo, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (input < 0) return;
    int fd = open(ackPath, O_WRONLY | O_APPEND | O_CREAT | O_NOFOLLOW, 0600);
    if (fd < 0 || !(ack = fdopen(fd, "a"))) { close(input); input = -1; if (fd >= 0) close(fd); return; }
    traceEvents = getenv("COD2_TEST_INPUT_TRACE") != NULL;
    traceText = getenv("COD2_TEST_INPUT_TRACE_TEXT") != NULL;
    info = dlsym(RTLD_DEFAULT, "uiInfo");
    shared = dlsym(RTLD_DEFAULT, "sharedUiInfo");
    gameWindow = dlsym(RTLD_DEFAULT, "sdl_gl_window");
    placement = dlsym(RTLD_DEFAULT, "CalcScreenPlacement");
    screenX = dlsym(RTLD_DEFAULT, "CalcScreenX");
    screenY = dlsym(RTLD_DEFAULT, "CalcScreenY");
    dvarString = dlsym(RTLD_DEFAULT, "Dvar_GetVariantString");
    translate = dlsym(RTLD_DEFAULT, "SEH_StringEd_GetString");
    showDvar = dlsym(RTLD_DEFAULT, "Item_EnableShowViaDvar");
    serverAddress = dlsym(RTLD_DEFAULT, "LAN_GetServerAddressString");
    serverInfo = dlsym(RTLD_DEFAULT, "LAN_GetServerInfo");
    infoValue = dlsym(RTLD_DEFAULT, "Info_ValueForKey");
    serverCount = dlsym(RTLD_DEFAULT, "LAN_GetServerCount");
    serverPing = dlsym(RTLD_DEFAULT, "LAN_GetServerPing");
    serverDirty = dlsym(RTLD_DEFAULT, "LAN_ServerIsDirty");
    clientState = dlsym(RTLD_DEFAULT, "GetClientState");
    binding = dlsym(RTLD_DEFAULT, "Key_GetBinding");
    editingField = dlsym(RTLD_DEFAULT, "g_editingField");
    waitingForKey = dlsym(RTLD_DEFAULT, "g_waitingForKey");
    capturedItem = dlsym(RTLD_DEFAULT, "itemCapture");
    captureFunction = dlsym(RTLD_DEFAULT, "captureFunc");
    clientImport = dlsym(RTLD_DEFAULT, "imp_cl");
    languages = dlsym(RTLD_DEFAULT, "g_languages");
    result(0, "ready", 1);
}

static int secret(const char *name)
{
    if (name && !strncasecmp(name, "ui_browser", 10)) return 0;
    return name && (strcasestr(name, "cdkey") || strcasestr(name, "codkey") ||
                   strcasestr(name, "password") || strcasestr(name, "rcon") ||
                   strcasestr(name, "hwid") || strcasestr(name, "guid"));
}

static const char *value(const char *name)
{
    return name && dvarString && !secret(name) ? dvarString(name) : "";
}

/* The engine feeder count/text callbacks mutate caches and sometimes selection.
 * Read their backing state here so observing a menu cannot repair its behavior. */
static int listCount(float feeder)
{
    if (!shared || !info || !*info) return -1;
    if (feeder == 2) return shared->serverStatus.numDisplayServers;
    if (feeder == 4) {
        int count = 0;
        for (int n = 0; n < shared->mapCount && n < 128; ++n) count += shared->mapList[n].active != 0;
        return count;
    }
    if (feeder == 9) return shared->modCount;
    if (feeder == 13) return shared->serverStatusInfo.numLines;
    if (feeder == 7 || feeder == 20) return shared->playerCount;
    if (feeder == 24) return (*info)->playerProfileCount;
    return 0;
}

static void rowText(FILE *stream, float feeder, int row)
{
    fputc('[', stream);
    if (feeder == 4) {
        int index = 0;
        for (int n = 0; shared && n < shared->mapCount && n < 128; ++n) if (shared->mapList[n].active) {
            if (index++ == row) { jsonString(stream, shared->mapList[n].mapName); break; }
        }
    } else if (feeder == 2 && shared && serverInfo && infoValue) {
        char details[2048] = {0};
        serverInfo(atoi(value("ui_netSource")), shared->serverStatus.displayServers[row], details, sizeof(details));
        const char *keys[] = {"hostname", "mapname", "clients", "sv_maxclients", "gametype", "ping", "pswrd", "addr", NULL};
        for (int n = 0; keys[n]; ++n) { if (n) fputc(',', stream); jsonString(stream, infoValue(details, keys[n])); }
    } else if (feeder == 13 && shared && row < 128) {
        int redact = secret(shared->serverStatusInfo.lines[row][0]) || secret(shared->serverStatusInfo.lines[row][1]);
        for (int n = 0; n < 4; ++n) { if (n) fputc(',', stream); jsonString(stream, redact && n > 1 ? "<redacted>" : shared->serverStatusInfo.lines[row][n]); }
    } else if ((feeder == 7 || feeder == 20) && shared && row < 64) jsonString(stream, shared->playerNames[row]);
    else if (feeder == 24 && info && *info && row < 64) jsonString(stream, (*info)->playerProfileName[row]);
    fputc(']', stream);
}

static void rect(FILE *stream, UiRectangle r, int baseline)
{
    if (baseline && r.w) r.y -= r.h;
    placement(&r.x, &r.y, &r.w, &r.h, r.horzAlign, r.vertAlign);
    fprintf(stream, "[%g,%g,%g,%g]", r.x, r.y, r.w, r.h);
}

static void writeItem(FILE *stream, itemDef_t *it, int index)
{
    int flags = it->window.dynamicFlags[0];
    int shown = (flags & 4) && (!(it->dvarFlags & 12) || !showDvar || showDvar(it, 4));
    int enabled = !(it->dvarFlags & 3) || !showDvar || showDvar(it, 1);
    fprintf(stream, "{\"index\":%d,\"name\":", index); jsonString(stream, it->window.name);
    fputs(",\"text\":", stream); jsonString(stream, it->text);
    const char *translated = it->text;
    if (translated && translated[0] == '@' && translate) translated = translate(translated + 1);
    fputs(",\"resolvedText\":", stream); jsonString(stream, translated);
    fprintf(stream, ",\"type\":%d,\"flags\":%u,\"staticFlags\":%u,\"visible\":%s,\"enabled\":%s,\"focused\":%s,\"ownerDraw\":%d,\"rect\":",
            it->type, flags, it->window.staticFlags, shown ? "true" : "false", enabled ? "true" : "false", flags & 2 ? "true" : "false", it->window.ownerDraw);
    rect(stream, it->window.rect[0], 0);
    UiRectangle original = it->window.rect[0];
    fprintf(stream, ",\"sourceRect\":[%g,%g,%g,%g],\"alignment\":[%d,%d]", original.x, original.y, original.w, original.h, original.horzAlign, original.vertAlign);
    fputs(",\"textRect\":", stream); rect(stream, it->textRect[0], 1);
    fputs(",\"hitRect\":", stream); rect(stream, it->type == 0 ? it->textRect[0] : it->window.rect[0], it->type == 0);
    fputs(",\"action\":", stream); jsonString(stream, it->action);
    fputs(",\"onAccept\":", stream); jsonString(stream, it->onAccept);
    fputs(",\"dvar\":", stream); jsonString(stream, it->dvar);
    fputs(",\"value\":", stream); jsonString(stream, value(it->dvar));
    if (secret(it->dvar) && dvarString) fprintf(stream, ",\"redacted\":true,\"valueLength\":%zu", strlen(dvarString(it->dvar)));
    if (it->type == 6 && it->typeData.listBox) {
        listBoxDef_t *list = it->typeData.listBox;
        int count = listCount(it->special);
        UiRectangle row = it->window.rect[0];
        row.h = list->elementHeight; row.w = list->elementWidth;
        placement(&row.x, &row.y, &row.w, &row.h, row.horzAlign, row.vertAlign);
        float verticalScale = list->elementHeight ? row.h / list->elementHeight : 1;
        fprintf(stream, ",\"list\":{\"start\":%d,\"end\":%d,\"cursor\":%d,\"selected\":%d,\"count\":%d,\"feeder\":%g,\"rowHeight\":%g,\"sourceRowHeight\":%g,\"paddingHeight\":%g,\"rowInset\":%g,\"rowWidth\":%g,\"selectable\":%s,\"doubleClick\":",
                list->startPos[0], list->endPos[0], list->cursorPos[0], it->cursorPos[0], count, it->special, row.h, list->elementHeight, list->drawPadding * verticalScale, 2 * verticalScale, row.w, list->notselectable ? "false" : "true");
        jsonString(stream, list->doubleClick);
        /* Serialize displayed rows only; feeder text is read from runtime data. */
        fputs(",\"rows\":[", stream);
        int end = list->endPos[0] + 1;
        if (end > count) end = count;
        if (end > list->startPos[0] + 128) end = list->startPos[0] + 128;
        int first = 1;
        for (int n = list->startPos[0]; n >= 0 && n < end; ++n) {
            if (!first) fputc(',', stream); first = 0;
            fprintf(stream, "{\"index\":%d,\"columns\":", n);
            rowText(stream, it->special, n);
            fputc('}', stream);
        }
        fputs("]}", stream);
    }
    if (it->type == 12 && it->typeData.multi) {
        multiDef_t *multi = it->typeData.multi;
        fputs(",\"choices\":[", stream);
        for (int n = 0; n < multi->count && n < 32; ++n) {
            if (n) fputc(',', stream);
            fputs("{\"label\":", stream); jsonString(stream, multi->dvarList[n]);
            fputs(",\"value\":", stream);
            if (multi->strDef) jsonString(stream, multi->dvarStr[n]);
            else fprintf(stream, "%g", multi->dvarValue[n]);
            if (it->dvar && !strcmp(it->dvar, "ui_language") && languages && !multi->strDef) {
                int language = (int)multi->dvarValue[n];
                if (language >= 0 && language < 14) fprintf(stream, ",\"available\":%s", languages[language].bPresent ? "true" : "false");
            }
            fputc('}', stream);
        }
        fputc(']', stream);
    }
    if (it->type == 10 && it->typeData.editField) {
        UiRectangle slider = it->window.rect[0];
        slider.x = it->text ? it->textRect[0].x + it->textRect[0].w + 8 : slider.x;
        slider.w = 96;
        fprintf(stream, ",\"slider\":{\"min\":%g,\"max\":%g,\"rect\":", it->typeData.editField->minVal, it->typeData.editField->maxVal);
        rect(stream, slider, 0); fputc('}', stream);
    }
    fputc('}', stream);
}

static int snapshot(unsigned id)
{
    if (!info || !*info || !placement || !screenX || !screenY || !dvarString) return -2;
    displayContextDef_t *dc = &(*info)->uiDC;
    if (dc->menuCount < 1 || dc->menuCount > 128) return -2;
    char path[4096], temporary[4096];
    if (snprintf(path, sizeof(path), "%s/snapshot-%u.json", output, id) >= sizeof(path) ||
        snprintf(temporary, sizeof(temporary), "%s/.snapshot-%u.tmp", output, id) >= sizeof(temporary)) return -1;
    FILE *stream = fopen(temporary, "w");
    if (!stream) return -1;
    int width = 0, height = 0;
    if (gameWindow && *gameWindow) SDL_GetWindowSize(*gameWindow, &width, &height);
    float cx = dc->cursorx, cy = dc->cursory;
    screenX(&cx, 4); screenY(&cy, 4);
    fprintf(stream, "{\"id\":%u,\"realTime\":%d,\"window\":[%d,%d],\"render\":[%d,%d],\"cursor\":[%d,%d],\"cursorPixels\":[%g,%g],\"menus\":[",
            id, dc->realTime, width, height, dc->screenWidth, dc->screenHeight, dc->cursorx, dc->cursory, cx, cy);
    for (int m = 0; m < dc->menuCount; ++m) {
        menuDef_t *menu = dc->Menus[m];
        if (m) fputc(',', stream);
        if (!menu) { fputs("null", stream); continue; }
        fputs("{\"name\":", stream); jsonString(stream, menu->window.name);
        fputs(",\"onOpen\":", stream); jsonString(stream, menu->onOpen);
        fputs(",\"onClose\":", stream); jsonString(stream, menu->onClose);
        fprintf(stream, ",\"visible\":%s,\"focused\":%s,\"flags\":%u,\"items\":[", menu->window.dynamicFlags[0] & 4 ? "true" : "false", menu->window.dynamicFlags[0] & 2 ? "true" : "false", menu->window.dynamicFlags[0]);
        for (int n = 0; n < menu->itemCount && n < 4096; ++n) {
            if (n) fputc(',', stream);
            if (menu->items[n]) writeItem(stream, menu->items[n], n); else fputs("null", stream);
        }
        fputs("]}", stream);
    }
    clientActive_t *client = clientImport && *clientImport ? *(clientActive_t **)*clientImport : NULL;
    fprintf(stream, "],\"uiContext\":{\"editingField\":%d,\"waitingForKey\":%d,\"clientActive\":%d,\"keyCatchers\":%d,\"captureKnown\":%s,\"captureActive\":%s,\"captureFunctionActive\":%s,\"stack\":[", editingField ? *editingField : -1, waitingForKey ? *waitingForKey : -1, client ? client->active : -1, client ? client->keyCatchers : -1, capturedItem ? "true" : "false", capturedItem && *capturedItem ? "true" : "false", captureFunction && *captureFunction ? "true" : "false");
    for (int n = 0; n < dc->openMenuCount && n < 16; ++n) {
        if (n) fputc(',', stream);
        jsonString(stream, dc->menuStack[n] ? dc->menuStack[n]->window.name : NULL);
    }
    fputs("]},\"dvars\":{", stream);
    const char *names[] = {"ui_netSource", "g_gametype", "ui_gametype", "ui_netGameType", "ui_currentMap", "ui_currentNetMap", "ui_mapname", "mapname", "sv_running", "cl_running", "sv_maxclients", "ui_dedicated", "ui_allowConsole", "ui_connectToRunningServer", "in_mouse", "in_rawmouse", "m_rinput", "com_maxfps", NULL};
    for (int n = 0; names[n]; ++n) { if (n) fputc(',', stream); jsonString(stream, names[n]); fputc(':', stream); jsonString(stream, value(names[n])); }
    float metrics[5] = {0};
    TestInput_WindowMetrics(gameWindow ? *gameWindow : NULL, metrics);
    fprintf(stream, "},\"mouse\":{\"relative\":%s,\"cursorShown\":%s},\"contentPoints\":[%g,%g],\"desktopPoints\":[%g,%g],\"backingScale\":%g,\"bindings\":{",
            SDL_GetRelativeMouseMode() ? "true" : "false", SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE ? "true" : "false", metrics[0], metrics[1], metrics[2], metrics[3], metrics[4]);
    int first = 1;
    if (binding) for (int k = 0; k < 256; ++k) {
        const char *command = binding(k);
        if (!command || !*command) continue;
        if (!first) fputc(',', stream); first = 0;
        fprintf(stream, "\"%d\":", k);
        jsonString(stream, secret(command) || strcasestr(command, "rcon ") ? "<redacted>" : command);
    }
    fputc('}', stream);
    if (shared) {
        fputs(",\"maps\":[", stream);
        for (int n = 0; n < shared->mapCount && n < 128; ++n) {
            if (n) fputc(',', stream);
            fprintf(stream, "{\"index\":%d,\"active\":%s,\"loadName\":", n, shared->mapList[n].active ? "true" : "false");
            jsonString(stream, shared->mapList[n].mapLoadName); fputc('}', stream);
        }
        fputc(']', stream);
        int source = atoi(value("ui_netSource"));
        serverStatus_t *status = &shared->serverStatus;
        fprintf(stream, ",\"servers\":{\"source\":%d,\"count\":%d,\"displayCount\":%d,\"selected\":%d,\"refreshing\":%s,\"addresses\":[",
                source, serverCount ? serverCount(source) : -1, status->numDisplayServers, status->currentServer, status->refreshActive ? "true" : "false");
        for (int n = 0; n < status->numDisplayServers && n < 20000; ++n) {
            if (n) fputc(',', stream);
            char address[256] = {0};
            if (serverAddress) serverAddress(source, status->displayServers[n], address, sizeof(address));
            jsonString(stream, address);
        }
        fputs("],\"raw\":[", stream);
        int rawCount = serverCount ? serverCount(source) : 0;
        for (int n = 0; n < rawCount && n < 128; ++n) {
            if (n) fputc(',', stream);
            fprintf(stream, "{\"index\":%d,\"ping\":%d,\"dirty\":%d,\"address\":", n,
                    serverPing ? serverPing(source, n) : -1, serverDirty ? serverDirty(source, n) : -1);
            char address[256] = {0};
            if (serverAddress) serverAddress(source, n, address, sizeof(address));
            jsonString(stream, address);
            fputc('}', stream);
        }
        fputs("]}", stream);
        fputs(",\"favorites\":[", stream);
        int favorites = serverCount ? serverCount(2) : 0;
        for (int n = 0; n < favorites && n < 128; ++n) {
            if (n) fputc(',', stream);
            char address[256] = {0};
            if (serverAddress) serverAddress(2, n, address, sizeof(address));
            jsonString(stream, address);
        }
        fputc(']', stream);
    }
    if (clientState) {
        uiClientState_t state = {0}; clientState(&state);
        fprintf(stream, ",\"client\":{\"state\":%d,\"server\":", state.connState); jsonString(stream, state.servername); fputc('}', stream);
    }
    fputs("}\n", stream);
    int failure = ferror(stream);
    if (fclose(stream)) failure = 1;
    if (failure || rename(temporary, path)) { unlink(temporary); return -1; }
    return 1;
}

static SDL_Window *window(void) { return gameWindow ? *gameWindow : NULL; }

static void command(char *line)
{
    unsigned id = 0;
    char operation[32] = {0};
    int consumed = 0;
    if (sscanf(line, "%u %31s %n", &id, operation, &consumed) != 2) { result(id, "invalid", -1); return; }
    char *arg = line + consumed;
    int status = -1;
    SDL_Event event = {0};
    if (!strcmp(operation, "motion")) {
        if (sscanf(arg, "%d %d %d %d", &event.motion.x, &event.motion.y, &event.motion.xrel, &event.motion.yrel) == 4) {
            event.type = SDL_MOUSEMOTION;
            event.motion.which = 0x57533432; /* WS42 test-device tag */
            event.motion.windowID = window() ? SDL_GetWindowID(window()) : 0;
            status = SDL_PushEvent(&event);
        }
    } else if (!strcmp(operation, "button")) {
        unsigned button, down, clicks;
        if (sscanf(arg, "%u %u %u", &button, &down, &clicks) == 3 && button >= 1 && button <= 5 && down <= 1 && clicks <= 2) {
            event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            event.button.which = 0x57533432;
            event.button.windowID = window() ? SDL_GetWindowID(window()) : 0;
            event.button.button = button; event.button.state = down ? SDL_PRESSED : SDL_RELEASED; event.button.clicks = clicks;
            status = SDL_PushEvent(&event);
        }
    } else if (!strcmp(operation, "key")) {
        unsigned down;
        char name[64];
        if (sscanf(arg, "%63s %u", name, &down) == 2 && down <= 1) {
            SDL_Keycode key = SDL_GetKeyFromName(name);
            if (key != SDLK_UNKNOWN) {
                event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
                event.key.windowID = window() ? SDL_GetWindowID(window()) : 0;
                event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
                event.key.keysym.sym = key; event.key.keysym.scancode = SDL_GetScancodeFromKey(key);
                status = SDL_PushEvent(&event);
            }
        }
    } else if (!strcmp(operation, "wheel")) {
        float x, y; int flipped;
        if (sscanf(arg, "%f %f %d", &x, &y, &flipped) == 3 && isfinite(x) && isfinite(y) && (flipped == 0 || flipped == 1))
            status = TestInput_NativeWheel(nativeLibrary, x, y, flipped);
    } else if (!strcmp(operation, "text")) {
        size_t length = strlen(arg);
        char text[32] = {0}; int valid = length > 0 && !(length & 1) && length < sizeof(text) * 2;
        for (size_t n = 0; valid && n < length / 2; ++n) {
            unsigned byte; char pair[3] = {arg[n * 2], arg[n * 2 + 1], 0};
            if (strspn(pair, "0123456789abcdefABCDEF") != 2 || sscanf(pair, "%x", &byte) != 1 || !byte) valid = 0;
            else text[n] = byte;
        }
        if (valid) status = TestInput_NativeText(nativeLibrary, text);
    } else if (!strcmp(operation, "wait")) {
        unsigned delay;
        if (sscanf(arg, "%u", &delay) == 1 && delay <= 10000) { resumeAt = SDL_GetTicks64() + delay; status = 1; }
    } else if (!strcmp(operation, "snapshot")) status = snapshot(id);
    else if (!strcmp(operation, "barrier")) status = 1;
    result(id, operation, status);
}

static void pump(void)
{
    if (input < 0 || SDL_GetTicks64() < resumeAt) return;
    ssize_t count = read(input, pending + pendingLength, sizeof(pending) - pendingLength - 1);
    if (count > 0) pendingLength += count;
    pending[pendingLength] = 0;
    while (SDL_GetTicks64() >= resumeAt) {
        char *end = memchr(pending, '\n', pendingLength);
        if (!end) break;
        size_t length = end - pending + 1;
        *end = 0; command(pending);
        memmove(pending, pending + length, pendingLength - length);
        pendingLength -= length; pending[pendingLength] = 0;
    }
    if (pendingLength == sizeof(pending) - 1) { pendingLength = 0; result(0, "overflow", -1); }
}

static int peep(SDL_Event *events, int count, SDL_eventaction action, Uint32 min, Uint32 max)
{
    if (!initialized) initialize();
    if (input >= 0 && action == SDL_GETEVENT) pump();
    int found = SDL_PeepEvents(events, count, action, min, max);
    if (input >= 0 && found == 0 && action == SDL_GETEVENT && min == SDL_FIRSTEVENT && max == SDL_LASTEVENT)
        TestInput_ReleaseText();
    if (ack && traceEvents && action == SDL_GETEVENT && events) for (int n = 0; n < found; ++n) {
        SDL_Event *event = &events[n];
        fprintf(ack, "{\"observed\":%u,\"tick\":%llu", event->type, SDL_GetTicks64());
        if (event->type == SDL_MOUSEWHEEL) fprintf(ack, ",\"preciseY\":%g,\"y\":%d,\"direction\":%u", event->wheel.preciseY, event->wheel.y, event->wheel.direction);
        if (event->type == SDL_MOUSEMOTION) fprintf(ack, ",\"x\":%d,\"y\":%d,\"dx\":%d,\"dy\":%d,\"which\":%u", event->motion.x, event->motion.y, event->motion.xrel, event->motion.yrel, event->motion.which);
        if (event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP) fprintf(ack, ",\"button\":%u,\"down\":%u,\"which\":%u", event->button.button, event->button.state, event->button.which);
        if (event->type == SDL_WINDOWEVENT) fprintf(ack, ",\"windowEvent\":%u", event->window.event);
        if (event->type == SDL_TEXTINPUT) {
            if (traceText) { fputs(",\"text\":", ack); jsonString(ack, event->text.text); }
            else fprintf(ack, ",\"textLength\":%zu", strlen(event->text.text));
        }
        fputs("}\n", ack); fflush(ack);
    }
    return found;
}

__attribute__((used)) static const struct { const void *replacement, *original; } interpose
__attribute__((section("__DATA,__interpose"))) = { (const void *)peep, (const void *)SDL_PeepEvents };
