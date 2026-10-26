/* Literal Mac 1.3 event contracts through production ClientEvents. */
#include "common_types.h"
#include "imports.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ClientEvents(gentity_t *, int);
level_locals_t level;
scr_const_t scr_const;
static dvar_t antilag;
static const dvar_t *antilagPointer = &antilag;
void *imp_g_antilag = &antilagPointer;
static gentity_t entity;
static gclient_t client;
enum { NONE, FIRE, MELEE, OFFHAND, ENTER, EXIT, BIN_FIRE, RELEASE, DROP, DIE, DAMAGE };
static int actions[8], values[8], count;

static void Record(gentity_t *ent, int action, int value)
{
    assert(ent == &entity && count < 8);
    actions[count] = action;
    values[count++] = value;
}
void FireWeaponAntiLag(gentity_t *ent, int time) { Record(ent, FIRE, time); }
void FireWeaponMelee(gentity_t *ent) { Record(ent, MELEE, 0); }
void G_UseOffHand(gentity_t *ent) { Record(ent, OFFHAND, 0); }
void Scr_Notify(gentity_t *ent, unsigned short name, unsigned int params)
{
    assert(params == 0);
    assert(name >= ENTER && name <= DROP);
    Record(ent, name, 0);
}
void player_die(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                int damage, int mod, int weapon, const vec_t *dir, int hit, int offset)
{
    assert(self == inflictor && self == attacker);
    assert(damage == 100000 && mod == 12 && weapon == 0);
    assert(dir == NULL && hit == 0 && offset == 0);
    assert(self->health == 0 && self->client->ps.stats[0] == 0);
    Record(self, DIE, 0);
}
void G_Damage(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
              const vec_t *dir, const vec_t *point, int damage, int flags,
              int mod, int hit, int offset)
{
    assert(!inflictor && !attacker && !dir && !point);
    assert(flags == 0 && mod == 11 && hit == 0 && offset == 0);
    Record(self, DAMAGE, damage);
}
static void Reset(void)
{
    memset(&entity, 0, sizeof(entity));
    memset(&client, 0, sizeof(client));
    entity.client = &client;
    entity.s.eType = 1;
    entity.health = 37;
    client.ps.stats[0] = 37;
    client.ps.stats[2] = 200;
    client.lastServerTime = 1234;
    level.time = 5678;
    antilag.current.enabled = 0;
    scr_const.binocular_enter = ENTER;
    scr_const.binocular_exit = EXIT;
    scr_const.binocular_fire = BIN_FIRE;
    scr_const.binocular_release = RELEASE;
    scr_const.binocular_drop = DROP;
    count = 0;
}
static void Queue(int sequence, int event, int parm)
{
    client.ps.events[sequence & 3] = event;
    client.ps.eventParms[sequence & 3] = parm;
    client.ps.eventSequence = sequence + 1;
}
static void One(int event, int expected, int lag)
{
    Reset();
    antilag.current.enabled = lag;
    Queue(0, event, 0);
    ClientEvents(&entity, 0);
    assert(count == (expected != NONE));
    if (expected) {
        assert(actions[0] == expected);
        assert(values[0] == (expected == FIRE ? (lag ? 1234 : 5678) : 0));
    }
    assert(client.ps.eventSequence == 1);
    if (expected != DIE)
        assert(entity.health == 37 && client.ps.stats[0] == 37);
    assert(client.ps.stats[2] == 200);
}
static void Inactive(void)
{
    const int events[] = {-1, 0, 0x73, 0x8b, 0x9d, 0xa1, 0xa2, 0xa3, 0xa5,
        0xa7, 0xad, 0xae, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
        0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2,
        0xc3, 0xc4, 0xc6, 0x7fffffff};
    for (unsigned i = 0; i < sizeof(events) / sizeof(events[0]); ++i)
        One(events[i], NONE, 0);
}
static void Suicide(void)
{
    One(0xc5, DIE, 0);
    const int flags[] = {1, 2, 3, 0x100, 0x101};
    const int deaths[] = {0, 0, 0, 1, 0};
    for (int i = 0; i < 5; ++i) {
        Reset();
        entity.flags = flags[i];
        Queue(0, 0xc5, 0);
        ClientEvents(&entity, 0);
        assert(count == deaths[i]);
        assert(entity.health == (deaths[i] ? 0 : 37));
        assert(client.ps.stats[0] == (deaths[i] ? 0 : 37));
        assert(client.ps.stats[2] == 200);
    }
}
static void Fall(int fatal)
{
    for (int event = 0x74; event <= 0x8a; ++event) {
        Reset();
        Queue(0, event, fatal ? 100 : 25);
        ClientEvents(&entity, 0);
        assert(count == 1 && actions[0] == DAMAGE);
        assert(values[0] == (fatal ? 220 : 50));
    }
}
static void FallControls(void)
{
    Reset();
    Queue(0, 0x74, 0);
    Queue(1, 0x9e, 0);
    ClientEvents(&entity, 0);
    assert(count == 1 && actions[0] == FIRE && values[0] == 5678);
    Reset();
    entity.s.eType = 2;
    Queue(0, 0x8a, 100);
    Queue(1, 0x9e, 0);
    ClientEvents(&entity, 0);
    assert(count == 0); /* Nonplayer fall exits the whole event loop. */
}
static void Ring(void)
{
    Reset();
    Queue(5, 0xa4, 0);
    Queue(6, 0xa6, 0);
    Queue(7, 0xa8, 0);
    Queue(8, 0xaf, 0);
    ClientEvents(&entity, 0); /* Clamp to the four retained entries, across wrap. */
    assert(count == 4);
    assert(actions[0] == MELEE && actions[1] == OFFHAND);
    assert(actions[2] == ENTER && actions[3] == FIRE && values[3] == 5678);
    count = 0;
    ClientEvents(&entity, 7);
    assert(count == 2 && actions[0] == ENTER && actions[1] == FIRE);
    count = 0;
    ClientEvents(&entity, 9);
    ClientEvents(&entity, 10);
    assert(count == 0 && client.ps.eventSequence == 9);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    if (!strcmp(argv[1], "fire")) {
        const int events[] = {0x9e, 0x9f, 0xa0, 0xaf};
        for (int i = 0; i < 4; ++i)
            for (int lag = 0; lag < 2; ++lag) One(events[i], FIRE, lag);
    } else if (!strcmp(argv[1], "melee")) One(0xa4, MELEE, 0);
    else if (!strcmp(argv[1], "offhand")) One(0xa6, OFFHAND, 0);
    else if (!strcmp(argv[1], "binocular")) {
        One(0xa8, ENTER, 0); One(0xa9, EXIT, 0); One(0xaa, BIN_FIRE, 0);
        One(0xab, RELEASE, 0); One(0xac, DROP, 0);
    } else if (!strcmp(argv[1], "inactive")) Inactive();
    else if (!strcmp(argv[1], "suicide")) Suicide();
    else if (!strcmp(argv[1], "fall")) Fall(0);
    else if (!strcmp(argv[1], "fall-fatal")) Fall(1);
    else if (!strcmp(argv[1], "fall-controls")) FallControls();
    else if (!strcmp(argv[1], "ring")) Ring();
    else abort();
    puts("client event contract passed");
    return 0;
}
