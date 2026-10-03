#include "common_types.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

extern void SetClientViewAngle(gentity_t *, const vec_t *);

int main(void)
{
    gentity_t entity = {0};
    gclient_s *client = calloc(1, sizeof(*client));
    vec3_t angle = {0, 210, 0};
    assert(client);
    entity.client = client;
    SetClientViewAngle(&entity, angle);
    assert(client->ps.viewangles[1] == 210 && entity.r.currentAngles[1] == 210);
    client->ps.pm_flags = 1;
    SetClientViewAngle(&entity, angle);
    assert(client->ps.viewangles[1] == 315);
    client->ps.eFlags = 0x300;
    SetClientViewAngle(&entity, angle);
    assert(client->ps.viewangles[1] == 210);
    free(client);
    puts("native standing, prone and mounted view angle rules: passed");
}
