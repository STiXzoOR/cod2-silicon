/* Mac player_die passes offhand weapon then remaining fuse to fire_grenade. */
#include "common_types.h"
#include "imports.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

extern void player_die(gentity_t *, gentity_t *, gentity_t *, int, int, int,
                       const vec_t *, hitLocation_t, int);

gentity_t g_entities[1024];
scr_const_t scr_const;
bgs_t level_bgs;
static bgs_t *active_bgs;
void *imp_bgs = &active_bgs;
static level_locals_t fixture_level;
void *imp_level = &fixture_level;
static gclient_t client;
static int drops, deaths;

struct DObj_s *Com_GetServerDObj(int number)
{
    static struct DObj_s object;
    assert(number == 0);
    return &object;
}
void Scr_AddEntity(gentity_t *ent) { assert(ent == &g_entities[0]); }
void Scr_Notify(gentity_t *ent, unsigned short name, unsigned int count)
{
    assert(ent == &g_entities[0] && name == scr_const.death && count == 1);
}
float crandom(void) { return 0.25f; }
float randomf(void) { return 0.5f; }
gentity_t *fire_grenade(gentity_t *self, vec_t *start, vec_t *velocity,
                       int weapon, int fuse)
{
    assert(self == &g_entities[0]);
    assert(weapon == 3 && fuse == 2750);
    assert(start[0] == 10 && start[1] == 20 && start[2] == 70);
    assert(velocity[0] == 40 && velocity[1] == 40 && velocity[2] == 80);
    ++drops;
    self->client->ps.grenadeTimeLeft = 0;
    return &g_entities[1];
}
int BG_AnimScriptEvent(playerState_t *ps, scriptAnimEventTypes_t event,
                       qboolean continued, qboolean force)
{
    assert(ps == &client.ps && event == ANIM_ET_DEATH && force);
    assert(active_bgs == &level_bgs);
    return 19;
}
void Scr_PlayerKilled(gentity_t *self, gentity_t *inflictor, gentity_t *attacker,
                      int damage, int mod, int weapon, const vec_t *direction,
                      hitLocation_t location, int offset, int animation)
{
    assert(animation == 19);
    ++deaths;
}
void Cmd_Score_f(gentity_t *ent) { assert(0); }
void SV_UnlinkEntity(gentity_t *ent) {}
void SV_LinkEntity(gentity_t *ent) {}
const float vectoyaw(const vec_t *direction) { assert(0); return 0; }

int main(void)
{
    gentity_t *self = &g_entities[0];
    self->client = &client;
    self->r.currentOrigin[0] = 10;
    self->r.currentOrigin[1] = 20;
    self->r.currentOrigin[2] = 30;
    client.ps.offHandIndex = 3;
    for (int held = 0; held < 2; ++held) {
        client.ps.pm_type = 0;
        client.ps.grenadeTimeLeft = held ? 2750 : 0;
        player_die(self, self, self, 100, 4, 0, NULL, (hitLocation_t)0, 0);
        assert(drops == held && deaths == held + 1);
        assert(client.ps.grenadeTimeLeft == 0 && client.ps.pm_type == 6);
        assert(self->health == 0 && self->handler == 11);
    }
    puts("grenade death: held weapon/fuse order and unarmed death pass");
    return 0;
}
