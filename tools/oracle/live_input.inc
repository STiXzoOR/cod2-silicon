/* Reference recording workload only. Replays inject the saved commands.
 * Repeat every 30 seconds: move/jump, crouch, prone, fire/reload, hold a
 * grenade then release it, and pulse use/attack for the DM respawn handler.
 * Grenade use gives combat coverage without teleporting or damaging
 * players through script, and leaves movement/collision entirely in-engine.
 */
int phase = frame % 600;
cmd.angles[1] = (frame * 97 + i * 8192) & 65535;
cmd.offHandIndex = (byte)g_entities[i].client->ps.offHandIndex;
if (phase < 200) {
    cmd.forwardmove = (frame / 60) % 2 ? -127 : 127;
    cmd.rightmove = (frame / 40 + i) % 2 ? -64 : 64;
    if (phase < 120 && phase % 40 == 0)
        cmd.buttons |= 0x400; /* jump */
    if (phase >= 120 && phase < 160)
        cmd.buttons |= 0x200; /* crouch */
    if (phase >= 160)
        cmd.buttons |= 0x100; /* prone */
} else if (phase < 260) {
    cmd.buttons = 1; /* attack */
} else if (phase < 280) {
    cmd.buttons = 0x10; /* reload */
} else if (phase < 430) {
    cmd.buttons = 0x10000; /* hold frag until phase 430 */
} else {
    cmd.buttons = phase % 20 < 10 ? 9 : 0; /* attack + use (DM respawn) */
}
