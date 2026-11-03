/* Mac 1.3 userinfo and password contracts through the production client TU. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern void ClientUserinfoChanged(int clientNum);
extern char *ClientConnect(int clientNum, int scriptPersId);

level_locals_t level;
bgs_t level_bgs;
gentity_t g_entities[1];
static gclient_t client;
static dvar_t password;
const dvar_t *g_password = &password;
static const char *predict = "0", *handicap = "0", *name = "NewName", *suppliedPassword = "wrong";
static int freed, connected, ranked;

void SV_GetUserinfo(int number, char *buffer, int size)
{
    assert(number == 0 && size == 1024);
    strcpy(buffer, "fixture userinfo");
}
int Info_Validate(const char *info) { assert(!strcmp(info, "fixture userinfo")); return 1; }
int SV_IsLocalClient(int number) { assert(number == 0); return 0; }
char *Info_ValueForKey(const char *info, const char *key)
{
    assert(!strcmp(info, "fixture userinfo"));
    if (!strcmp(key, "cg_predictItems")) return (char *)predict;
    if (!strcmp(key, "handicap")) return (char *)handicap;
    if (!strcmp(key, "name")) return (char *)name;
    if (!strcmp(key, "password")) return (char *)suppliedPassword;
    abort();
}
void I_strncpyz(char *dest, const char *src, int size)
{
    assert(size > 0);
    size_t count = strlen(src);
    if (count >= (size_t)size) count = (size_t)size - 1;
    memcpy(dest, src, count);
    dest[count] = 0;
}
int I_stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
int ColorIndex(int c)
{
    unsigned char index = (unsigned char)(c - '0');
    return index > 9 ? 7 : index;
}
void G_InitGentity(gentity_t *ent) { assert(ent == g_entities); }
void G_FreeEntity(gentity_t *ent)
{
    assert(ent == g_entities && connected == 0 && ranked == 0);
    ++freed;
}
void Scr_PlayerConnect(gentity_t *ent)
{
    assert(ent == g_entities && freed == 0 && ranked == 0);
    ++connected;
}
void CalculateRanks(void) { assert(connected == 1 && freed == 0); ++ranked; }

static void CheckPassword(const char *server, const char *supplied, int accept)
{
    password.current.string = server;
    suppliedPassword = supplied;
    const char *denial = ClientConnect(0, 17);
    if (accept) {
        assert(denial == NULL && connected == 1 && ranked == 1 && freed == 0);
    } else {
        assert(denial && !strcmp(denial, "GAME_INVALIDPASSWORD"));
        assert(freed == 1 && connected == 0 && ranked == 0);
    }
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    level.clients = &client;
    g_entities[0].client = &client;
    strcpy(client.sess.cs.name, "ExistingName");
    client.sess.cs.team = TEAM_AXIS;
    if (!strcmp(argv[1], "password-none")) CheckPassword("none", "wrong", 1);
    else if (!strcmp(argv[1], "password-none-mixed")) CheckPassword("NoNe", "wrong", 1);
    else if (!strcmp(argv[1], "password-correct")) CheckPassword("Secret", "Secret", 1);
    else if (!strcmp(argv[1], "password-wrong")) CheckPassword("Secret", "secret", 0);
    else if (!strcmp(argv[1], "predict-enabled")) {
        predict = "1";
        handicap = "0";
        ClientUserinfoChanged(0);
        assert(client.sess.predictItemPickup == 1);
    } else if (!strcmp(argv[1], "predict-disabled")) {
        predict = "0";
        handicap = "100";
        ClientUserinfoChanged(0);
        assert(client.sess.predictItemPickup == 0);
    } else if (!strcmp(argv[1], "manual-name")) {
        client.sess.connected = CON_CONNECTED;
        level.manualNameChange = 1;
        ClientUserinfoChanged(0);
        assert(!strcmp(client.sess.cs.name, "ExistingName"));
        assert(!strcmp(client.sess.newnetname, "NewName"));
        assert(!strcmp(level_bgs.clientinfo[0].name, "ExistingName"));
        assert(level_bgs.clientinfo[0].team == TEAM_AXIS);
    } else abort();
    puts("client lifecycle contract passed");
    return 0;
}
