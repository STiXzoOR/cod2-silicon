/* CoD2x's vMix control for match broadcasts (CoD2x src/mss32/vmix.cpp). While
 * cl_vmix is on and the client is in a game, a change of the spectated player
 * (set by the server's scripts in cl_vmix_scr_*) switches the player's camera input
 * in vMix and points vMix's browser input at a page with the player's data, all by
 * HTTP GETs to the vMix web API at cl_vmix_cfg_url. Turning it off hides the
 * cameras and resets the browser input.
 *
 * Requests run on their own threads (Cod2x_GetStart, cod2x_report.c); their results
 * are printed on the game thread the next frame, as CoD2x's HTTP client delivers
 * them in its poll. */
#if defined(COD2_X64) && COD2_X64 && defined(COD2_CODX) && COD2_CODX && !defined(DEDICATED)
#include "common_types.h"
#include "cod2x.h"
#include "cod2x_features.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
extern void __stdcall Sleep(unsigned long milliseconds);
extern unsigned long long __stdcall GetTickCount64(void);
#else
#include <unistd.h>
#endif

extern const dvar_t *Dvar_RegisterInt(const char *, int, int, int, unsigned short);
extern const dvar_t *Dvar_RegisterBool(const char *, unsigned char, unsigned short);
extern const dvar_t *Dvar_RegisterString(const char *, const char *, unsigned short);
extern void Com_Printf(const char *, ...);

#define VMIX_FLAGS 0x1000 /* CoD2x's DVAR_CHANGEABLE_RESET */
#define VMIX_TIMEOUT_MS 5000
#define VMIX_PLAYERS 10

static dvar_t *vmixEnabled, *vmixDebug, *vmixURL, *vmixBrowserPrefix, *vmixBrowserSuffix, *vmixBrowserSource;
static dvar_t *vmixSpectatedId, *vmixSpectatedName, *vmixSpectatedTeam, *vmixSpectatedHwid, *vmixData;
static dvar_t *vmixOverlay, *vmixCameraMode, *vmixPlayerIds;
static dvar_t *vmixPlayerId[VMIX_PLAYERS], *vmixPlayerSource[VMIX_PLAYERS];
static char *vmixLastCamera;
static int vmixRunning;

typedef struct VmixRequest {
    Cod2xGet get;
    volatile int dropped;
    struct VmixRequest *next;
} VmixRequest;

static VmixRequest *vmixRequests;

static uint64_t Cod2x_VmixMilliseconds(void)
{
#if defined(_WIN32)
    return GetTickCount64();
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000 + (uint64_t)now.tv_nsec / 1000000;
#endif
}

/* CoD2x's URL grammar is shared by the request and demo upload clients. */
static int Cod2x_VmixValidURL(const char *url)
{
    const char *p = url;
    if (!p || !isalpha((unsigned char)*p))
        return 0;
    for (++p; isalnum((unsigned char)*p) || *p == '+' || *p == '-' || *p == '.'; ++p)
        ;
    if (strncmp(p, "://", 3) || !p[3])
        return 0;
    for (p += 3; *p; ++p)
        if ((unsigned char)*p <= 32 || (unsigned char)*p > 126 || strchr("\"`<>\\", *p))
            return 0;
    return 1;
}

static void Cod2x_VmixSend(const char *url)
{
    VmixRequest *request;
    if (vmixDebug->current.enabled)
        Com_Printf("VMix Sending command: %s\n", url);
    request = (VmixRequest *)calloc(1, sizeof(*request));
    if (!request)
        return;
    request->get.url = strdup(url);
    request->next = vmixRequests;
    vmixRequests = request;
    if (!request->get.url || !Cod2x_VmixValidURL(url)) {
        request->get.failed = 1;
        snprintf(request->get.error, sizeof(request->get.error), "Invalid URL");
        request->get.done = 1;
        return;
    }
    if (!Cod2x_GetStart(&request->get, VMIX_TIMEOUT_MS)) {
        request->get.failed = 1;
        snprintf(request->get.error, sizeof(request->get.error), "Failed to start the request");
        request->get.done = 1;
    }
}

/* Prints what the finished requests got and forgets them; a dropped request is
   forgotten without a word, as CoD2x's client is deleted with its callbacks. */
static void Cod2x_VmixPoll(void)
{
    VmixRequest **link = &vmixRequests;
    while (*link) {
        VmixRequest *request = *link;
        Cod2xGet *get = &request->get;
        if (!__atomic_load_n(&get->done, __ATOMIC_ACQUIRE)) {
            link = &request->next;
            continue;
        }
        if (!request->dropped) {
            if (get->failed) {
                Com_Printf("^1VMix %s error: %s\n", get->url ? get->url : "", get->error);
            } else if (get->status != 200 && get->status != 201) {
                Com_Printf("^1VMix %s failed, invalid status: %d\n%s\n", get->url, (int)get->status,
                           get->body ? get->body : "");
            } else if (vmixDebug->current.enabled) {
                Com_Printf("^2VMix %s response: %d, %s\n", get->url, (int)get->status, get->body ? get->body : "");
            }
        }
        *link = request->next;
        free(get->url);
        free(get->body);
        free(request);
    }
}

static void Cod2x_VmixSendFormat(const char *format, ...)
{
    va_list args, measure;
    int length;
    char *url;
    va_start(args, format);
    va_copy(measure, args);
    length = vsnprintf(NULL, 0, format, measure);
    va_end(measure);
    url = length >= 0 ? malloc((size_t)length + 1) : NULL;
    if (url)
        vsnprintf(url, (size_t)length + 1, format, args);
    va_end(args);
    if (!url)
        return;
    Cod2x_VmixSend(url);
    free(url);
}

static void Cod2x_VmixOverlay(const char *input, int overlay, const char *state)
{
    Cod2x_VmixSendFormat("%s/api/?Function=OverlayInput%d%s&Input=%s", vmixURL->current.string, overlay, state,
                         input);
}

static void Cod2x_VmixBrowser(const char *input, const char *address)
{
    Cod2x_VmixSendFormat("%s/api/?Function=BrowserNavigate&Input=%s&Value=%s", vmixURL->current.string, input,
                         address);
}

/* mongoose's mg_url_encode: letters, digits and ".-_~" stay; nothing when it
   doesn't fit. */
static size_t Cod2x_VmixEncode(const char *text, char *out, size_t size)
{
    static const char hex[] = "0123456789abcdef";
    size_t used = 0;
    for (; *text; ++text) {
        unsigned char c = (unsigned char)*text;
        if (used + 4 >= size) {
            out[0] = '\0';
            return 0;
        }
        if (isalnum(c) || c == '.' || c == '_' || c == '-' || c == '~') {
            out[used++] = (char)c;
        } else {
            out[used++] = '%';
            out[used++] = hex[c >> 4];
            out[used++] = hex[c & 15];
        }
    }
    out[used] = '\0';
    return used;
}

void Cod2x_VmixFrame(int clientState)
{
    if (vmixEnabled && vmixEnabled->current.enabled && clientState == CA_ACTIVE) {
        const char *spectatedHwid, *found = NULL;
        /* The name must fit whenever the complete encoded page fits. */
        char page[4096], name[sizeof(page)], encoded[1024], *p;
        int activated = 0, i;
        vmixRunning = 1;
        Cod2x_VmixPoll();
        if (!vmixSpectatedId->modified && !vmixSpectatedHwid->modified)
            return;
        vmixSpectatedId->modified = 0;
        vmixSpectatedHwid->modified = 0;
        spectatedHwid = vmixSpectatedHwid->current.string;
        /* Is the spectated player one with a camera? */
        for (i = 0; i < VMIX_PLAYERS; ++i) {
            const char *player = vmixPlayerId[i]->current.string;
            if (vmixDebug->current.enabled)
                Com_Printf("Comparing spectatedHWID: '%s' with playerId[%d]: '%s'\n", spectatedHwid, i, player);
            if (spectatedHwid[0] && player[0] && !strcmp(spectatedHwid, player)) {
                if (vmixDebug->current.enabled)
                    Com_Printf("  Matched\n");
                if (vmixCameraMode->current.integer == 1) {
                    Cod2x_VmixOverlay(vmixPlayerSource[i]->current.string, vmixOverlay->current.integer, "In");
                    free(vmixLastCamera);
                    vmixLastCamera = strdup(vmixPlayerSource[i]->current.string);
                    activated = 1;
                }
                found = vmixPlayerSource[i]->current.string;
                break;
            }
        }
        if (!activated && vmixLastCamera) {
            Cod2x_VmixOverlay(vmixLastCamera, vmixOverlay->current.integer, "Off");
            free(vmixLastCamera);
            vmixLastCamera = NULL;
        }
        /* "<prefix><id>|<hwid>|<camera>|<team>|<name>|<data>", the whole of it URL-encoded. */
        snprintf(name, sizeof(name), "%s", vmixSpectatedName->current.string);
        for (p = name; *p; ++p)
            if (*p == '|')
                *p = ' ';
        snprintf(page, sizeof(page), "%s%s|%s|%s|%s|%s|%s", vmixBrowserPrefix->current.string,
                 vmixSpectatedId->current.string, spectatedHwid,
                 vmixCameraMode->current.integer == 2 && found ? found : "", vmixSpectatedTeam->current.string, name,
                 vmixData->current.string);
        Cod2x_VmixEncode(page, encoded, sizeof(encoded));
        Cod2x_VmixBrowser(vmixBrowserSource->current.string, encoded);
        return;
    }
    if (vmixRunning) {
        VmixRequest *request;
        int i;
        uint64_t deadline;
        for (i = 0; i < VMIX_PLAYERS; ++i) {
            if (vmixPlayerSource[i]->current.string[0])
                Cod2x_VmixOverlay(vmixPlayerSource[i]->current.string, vmixOverlay->current.integer, "Off");
        }
        if (vmixBrowserSource->current.string[0])
            Cod2x_VmixBrowser(vmixBrowserSource->current.string, vmixBrowserPrefix->current.string);
        /* Up to 200 ms for what is still under way, then the rest is dropped. */
        deadline = Cod2x_VmixMilliseconds() + 200;
        while (Cod2x_VmixMilliseconds() < deadline) {
            int pending = 0;
            Cod2x_VmixPoll();
            for (request = vmixRequests; request; request = request->next)
                pending |= !__atomic_load_n(&request->get.done, __ATOMIC_ACQUIRE);
            if (!pending)
                break;
#if defined(_WIN32)
            Sleep(10);
#else
            usleep(10000);
#endif
        }
        for (request = vmixRequests; request; request = request->next) {
            request->dropped = 1;
            __atomic_store_n(&request->get.cancel, 1, __ATOMIC_RELEASE);
        }
        vmixRunning = 0;
    }
    /* Dropped requests are freed once their threads finish. */
    Cod2x_VmixPoll();
}

void Cod2x_VmixInit(void)
{
    static const char *const ids[VMIX_PLAYERS] = {
        "cl_vmix_cfg_camera_player1_id", "cl_vmix_cfg_camera_player2_id", "cl_vmix_cfg_camera_player3_id",
        "cl_vmix_cfg_camera_player4_id", "cl_vmix_cfg_camera_player5_id", "cl_vmix_cfg_camera_player6_id",
        "cl_vmix_cfg_camera_player7_id", "cl_vmix_cfg_camera_player8_id", "cl_vmix_cfg_camera_player9_id",
        "cl_vmix_cfg_camera_player10_id"};
    static const char *const sources[VMIX_PLAYERS] = {
        "cl_vmix_cfg_camera_player1_sourceName", "cl_vmix_cfg_camera_player2_sourceName",
        "cl_vmix_cfg_camera_player3_sourceName", "cl_vmix_cfg_camera_player4_sourceName",
        "cl_vmix_cfg_camera_player5_sourceName", "cl_vmix_cfg_camera_player6_sourceName",
        "cl_vmix_cfg_camera_player7_sourceName", "cl_vmix_cfg_camera_player8_sourceName",
        "cl_vmix_cfg_camera_player9_sourceName", "cl_vmix_cfg_camera_player10_sourceName"};
    int i;
    vmixEnabled = (dvar_t *)Dvar_RegisterBool("cl_vmix", 0, VMIX_FLAGS);
    vmixDebug = (dvar_t *)Dvar_RegisterBool("cl_vmix_debug", 0, VMIX_FLAGS);
    /* Set by the server's scripts. */
    vmixSpectatedId = (dvar_t *)Dvar_RegisterString("cl_vmix_scr_spectatedUserId", "", VMIX_FLAGS);
    vmixSpectatedName = (dvar_t *)Dvar_RegisterString("cl_vmix_scr_spectatedUserName", "", VMIX_FLAGS);
    vmixSpectatedTeam = (dvar_t *)Dvar_RegisterString("cl_vmix_scr_spectatedUserTeam", "", VMIX_FLAGS);
    vmixSpectatedHwid = (dvar_t *)Dvar_RegisterString("cl_vmix_scr_spectatedHWID", "", VMIX_FLAGS);
    vmixData = (dvar_t *)Dvar_RegisterString("cl_vmix_scr_data", "", VMIX_FLAGS);
    vmixPlayerIds = (dvar_t *)Dvar_RegisterString("cl_vmix_scr_playerIds", "", VMIX_FLAGS);
    vmixURL = (dvar_t *)Dvar_RegisterString("cl_vmix_cfg_url", "", VMIX_FLAGS);
    vmixBrowserPrefix = (dvar_t *)Dvar_RegisterString("cl_vmix_cfg_webbrowser_url_prefix", "", VMIX_FLAGS);
    vmixBrowserSuffix = (dvar_t *)Dvar_RegisterString("cl_vmix_cfg_webbrowser_url_suffix", "", VMIX_FLAGS);
    vmixBrowserSource = (dvar_t *)Dvar_RegisterString("cl_vmix_cfg_webbrowser_sourceName", "", VMIX_FLAGS);
    /* Which of vMix's overlays 1-4 shows the cameras; 1: switch the camera in vMix,
       2: give its name to the browser page. */
    vmixOverlay = (dvar_t *)Dvar_RegisterInt("cl_vmix_cfg_camera_overlayNum", 1, 1, 4, VMIX_FLAGS);
    vmixCameraMode = (dvar_t *)Dvar_RegisterInt("cl_vmix_cfg_camera_mode", 1, 1, 2, VMIX_FLAGS);
    for (i = 0; i < VMIX_PLAYERS; ++i) {
        vmixPlayerId[i] = (dvar_t *)Dvar_RegisterString(ids[i], "", VMIX_FLAGS);
        vmixPlayerSource[i] = (dvar_t *)Dvar_RegisterString(sources[i], "", VMIX_FLAGS);
    }
    (void)vmixPlayerIds;
    (void)vmixBrowserSuffix;
}
#endif
