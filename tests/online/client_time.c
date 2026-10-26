/* Synthetic service boundaries; execute both complete production time functions. */
#include "client_time_source.h"
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

static clientActive_t active;
static clientConnection_t connection;
static clientStatic_t client;
static clientActive_t *active_ptr = &active;
static clientConnection_t *connection_ptr = &connection;
void *imp_cl = &active_ptr;
void *imp_clc = &connection_ptr;
void *imp_cls = &client;
static dvar_t freeze, show, server_pause, client_pause, running, action;
static const dvar_t *freeze_ptr = &freeze, *show_ptr = &show;
static const dvar_t *server_pause_ptr = &server_pause, *client_pause_ptr = &client_pause;
static const dvar_t *running_ptr = &running;
const dvar_t *cl_showTimeDelta = &show;
const dvar_t *cl_activeAction = &action;
void *imp_cl_freezeDemo = &freeze_ptr;
void *imp_cl_showTimeDelta = &show_ptr;
void *imp_sv_paused = &server_pause_ptr;
void *imp_cl_paused = &client_pause_ptr;
void *imp_com_sv_running = &running_ptr;
static float test_timescale = 1.0f;
void *imp_com_timescaleValue = &test_timescale;
static int milliseconds, opens, writes, reads, open_result, read_disconnect;
static int logged_frame, logged_interval, errors, commands;
static char print_format[256], opened_name[256];
static jmp_buf error_jump;
static int expect_error;

int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int Sys_Milliseconds(void) { return milliseconds; }
int Dvar_GetInt(const char *name) { assert(!strcmp(name, "cl_timedemoMode")); return 3; }
const char *va(const char *format, ...)
{
    static char buffers[4][256];
    static unsigned index;
    char *buffer = buffers[index++ % 4];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 256, format, args);
    va_end(args);
    return buffer;
}
fileHandle_t FS_FOpenFileWrite(const char *name)
{
    assert(!active.newSnapshots); /* Real AdjustTimeDelta must precede benchmark logging. */
    ++opens;
    snprintf(opened_name, sizeof(opened_name), "%s", name);
    return open_result;
}
void FS_Printf(int handle, const char *format, ...)
{
    assert(handle == 17 && !strcmp(format, "%i,%i\n"));
    va_list args;
    va_start(args, format);
    logged_frame = va_arg(args, int);
    logged_interval = va_arg(args, int);
    va_end(args);
    ++writes;
}
void Com_Printf(const char *format, ...)
{
    /* Inspect the format directly: the baseline has a missing floating argument. */
    snprintf(print_format, sizeof(print_format), "%s", format);
}
void Com_Error(errorParm_t code, const char *format, ...)
{
    assert(expect_error && code == ERR_DROP && format);
    ++errors;
    longjmp(error_jump, 1);
}
void Cbuf_AddText(const char *text)
{
    assert(!strcmp(text, "ready") || !strcmp(text, "\n"));
    ++commands;
}
void Dvar_SetString(const dvar_t *var, const char *value)
{
    assert(var == &action && !strcmp(value, ""));
    action.current.string = "";
}
void CL_ReadDemoMessage(void)
{
    assert(++reads <= 10);
    if (read_disconnect) {
        connection.state = CA_DISCONNECTED;
        return;
    }
    active.snap.serverTime += 50;
    active.snap.valid = 1;
    active.newSnapshots = 1;
}

static void reset(void)
{
    memset(&active, 0, sizeof(active));
    memset(&connection, 0, sizeof(connection));
    memset(&client, 0, sizeof(client));
    memset(&freeze, 0, sizeof(freeze));
    memset(&show, 0, sizeof(show));
    memset(&server_pause, 0, sizeof(server_pause));
    memset(&client_pause, 0, sizeof(client_pause));
    memset(&running, 0, sizeof(running));
    action.current.string = "";
    connection.state = CA_ACTIVE;
    connection.timeDemoBaseTime = 1000;
    active.snap.valid = 1;
    active.snap.serverTime = 2000;
    active.oldSnapServerTime = 1950;
    active.oldFrameServerTime = 1900;
    active.serverTime = active.oldServerTime = 1500;
    active.serverTimeDelta = 500;
    snprintf(active.mapname, sizeof(active.mapname), "maps/mp/test.level.d3dbsp");
    snprintf(client.servername, sizeof(client.servername), "localhost");
    client.realtime = 1000;
    test_timescale = 1.0f;
    milliseconds = 100;
    opens = writes = reads = errors = commands = 0;
    read_disconnect = expect_error = 0;
    open_result = 17;
    logged_frame = logged_interval = -1;
    print_format[0] = opened_name[0] = 0;
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    reset();
    const char *test = argv[1];
    if (!strcmp(test, "live-clock")) {
        for (int frozen = 0; frozen <= 1; ++frozen) {
            reset();
            freeze.current.enabled = frozen;
            connection.isTimeDemo = 1; /* Stale benchmark flag cannot affect live play. */
            client.realtime = 1100;
            CL_SetCGameTime();
            assert(active.serverTime == 1600 && active.oldServerTime == 1600);
            client.realtime = 900;
            CL_SetCGameTime();
            assert(active.serverTime == 1600 && active.oldServerTime == 1600);
            assert(!opens && !writes && !connection.timeDemoFrames && !reads);
        }
    } else if (!strcmp(test, "demo-clock")) {
        connection.demoplaying = 1;
        client.realtime = 1100;
        CL_SetCGameTime();
        assert(active.serverTime == 1600 && active.oldServerTime == 1600);
        assert(!opens && !connection.timeDemoFrames && !reads);
    } else if (!strcmp(test, "demo-freeze")) {
        connection.demoplaying = 1;
        freeze.current.enabled = 1;
        client.realtime = 1200;
        CL_SetCGameTime();
        assert(active.serverTime == 1500 && active.oldServerTime == 1500);
        assert(!opens && !connection.timeDemoFrames && !reads);
    } else if (!strcmp(test, "timedemo") || !strcmp(test, "timedemo-frozen")) {
        connection.demoplaying = connection.isTimeDemo = 1;
        freeze.current.enabled = !strcmp(test, "timedemo-frozen");
        active.newSnapshots = 1;
        CL_SetCGameTime();
        assert(!active.newSnapshots && connection.timeDemoFrames == 1 && active.serverTime == 1050);
        assert(connection.timeDemoStart == 100 && connection.timeDemoPrev == 100);
        assert(opens == 1 && !writes && !strcmp(opened_name, "demos/timedemo_test.level_mode_3.csv"));
        milliseconds = 117;
        CL_SetCGameTime();
        assert(connection.timeDemoFrames == 2 && active.serverTime == 1100);
        assert(opens == 1 && writes == 1 && logged_frame == 1 && logged_interval == 17);
        assert(connection.timeDemoStart == 100 && connection.timeDemoPrev == 117 && !reads);
    } else if (!strcmp(test, "timedemo-log-retry")) {
        connection.demoplaying = connection.isTimeDemo = 1;
        open_result = 0;
        milliseconds = 0;
        CL_SetCGameTime();
        assert(opens == 1 && !writes && active.serverTime == 1050 && !connection.timeDemoStart);
        open_result = 17;
        milliseconds = 10;
        CL_SetCGameTime();
        assert(opens == 2 && !writes && connection.timeDemoStart == 10 && active.serverTime == 1100);
        milliseconds = 20;
        CL_SetCGameTime();
        assert(opens == 2 && writes == 1 && logged_frame == 2 && logged_interval == 10);
        assert(active.serverTime == 1150);
    } else if (!strcmp(test, "extrapolation")) {
        for (int offset = -1; offset <= 1; ++offset) {
            reset();
            client.realtime = 1495 + offset;
            CL_SetCGameTime();
            assert(active.extrapolatedSnapshot == (offset >= 0));
        }
        reset();
        active.oldServerTime = 2100; /* Threshold uses the unclamped sum, not serverTime. */
        CL_SetCGameTime();
        assert(active.serverTime == 2100 && !active.extrapolatedSnapshot);
        active.extrapolatedSnapshot = 1;
        CL_SetCGameTime();
        assert(active.extrapolatedSnapshot == 1);
    } else if (!strcmp(test, "extrapolation-smoothing")) {
        for (int offset = -1; offset <= 0; ++offset) {
            reset();
            client.realtime = 1495 + offset;
            active.newSnapshots = 1;
            CL_SetCGameTime();
            assert(!active.newSnapshots && !active.extrapolatedSnapshot);
            assert(active.serverTimeDelta == (offset < 0 ? 499 : 498));
        }
        reset();
        client.realtime = 1495;
        active.newSnapshots = 1;
        test_timescale = 0.5f;
        CL_SetCGameTime();
        assert(active.serverTimeDelta == 500 && active.extrapolatedSnapshot == 1);
    } else if (!strcmp(test, "extrapolation-debug")) {
        show.current.enabled = 1;
        client.realtime = 1495;
        CL_SetCGameTime();
        assert(!strcmp(print_format, "Extrapolating snapshot!\n"));
    } else if (!strcmp(test, "demo-read")) {
        connection.demoplaying = 1;
        client.realtime = 1599;
        CL_SetCGameTime();
        assert(active.serverTime == 2099 && active.snap.serverTime == 2100 && reads == 2);
        reset();
        connection.demoplaying = 1;
        client.realtime = 1500;
        read_disconnect = 1;
        CL_SetCGameTime();
        assert(reads == 1 && connection.state == CA_DISCONNECTED);
    } else if (!strcmp(test, "primed")) {
        connection.state = CA_PRIMED;
        connection.demoplaying = 1;
        action.current.string = "ready";
        CL_SetCGameTime();
        assert(connection.firstDemoFrameSkipped == 1 && !reads && connection.state == CA_PRIMED);
        CL_SetCGameTime();
        assert(reads == 2 && connection.state == CA_ACTIVE && commands == 2);
        assert(active.serverTime == 2050 && connection.timeDemoBaseTime == 2050);
        assert(active.serverTimeDelta == 1050 && !strcmp(action.current.string, ""));
    } else if (!strcmp(test, "gates")) {
        for (int mask = 0; mask < 8; ++mask) {
            reset();
            server_pause.current.integer = !!(mask & 1);
            client_pause.current.integer = !!(mask & 2);
            running.current.enabled = !!(mask & 4);
            client.realtime = 1100;
            CL_SetCGameTime();
            assert(active.serverTime == (mask == 7 ? 1500 : 1600));
        }
        reset();
        connection.state = CA_CONNECTED;
        CL_SetCGameTime();
        assert(active.serverTime == 1500 && !reads);
    } else if (!strcmp(test, "errors-restart")) {
        active.snap.valid = 0;
        expect_error = 1;
        if (!setjmp(error_jump)) { CL_SetCGameTime(); assert(0); }
        assert(errors == 1);
        reset();
        active.oldFrameServerTime = 2100;
        strcpy(client.servername, "remote");
        expect_error = 1;
        if (!setjmp(error_jump)) { CL_SetCGameTime(); assert(0); }
        assert(errors == 1);
        reset();
        active.oldFrameServerTime = 2100;
        CL_SetCGameTime();
        assert(!errors && active.serverTime == 2000 && active.serverTimeDelta == 1000);
        assert(connection.timeDemoBaseTime == 2000 && active.oldFrameServerTime == 2000);
    } else {
        assert(0);
    }
    printf("client-time %s: passed\n", test);
    return 0;
}
