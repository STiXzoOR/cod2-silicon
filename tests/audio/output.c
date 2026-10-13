/* Reuse the unrelated-event failure boundaries; the footstep alias boundary
   below is replaced with the real entity-sound body and a bounded picker. */
#define main UnusedFxFixtureMain
#define CG_PlayEntitySoundAlias UnusedEntitySoundStub
#include "../platform/macos_fx_events.c"
#undef CG_PlayEntitySoundAlias
#undef main
#include "imports.h"

snd_local_t g_snd;
#define SND_GLOB_PTR (&g_snd)
static struct MssLocal milesGlob;
static MssSound sound;
static int16_t pcm[480];
static snd_channelvolgroup channelVolumes;
static snd_enveffect environment;
static centity_t entities[64];
static snd_alias_t sampleAlias;
static int starts, emitted;
static snd_alias_list_t *expectedAlias;
static snd_alias_list_t *expectedEquipment;
static const snd_alias_t *pickedAlias;

extern void *AIL_allocate_3D_sample_handle(void *provider);
extern void AIL_release_sample_handle(void *sample);
extern int AIL_set_3D_sample_info(void *sample, const void *info);
extern void AIL_set_3D_sample_volume(void *sample, float volume);
extern void AIL_set_3D_sample_distances(void *sample, float min, float max);
extern int AIL_3D_sample_playback_rate(void *sample);
extern void AIL_set_3D_sample_playback_rate(void *sample, int rate);
extern void AIL_set_3D_position(void *sample, float x, float y, float z);
extern void AIL_3D_position(void *sample, float *x, float *y, float *z);
extern void AIL_set_3D_sample_loop_count(void *sample, int loops);
extern void AIL_set_3D_sample_effects_level(void *sample, float wet);
extern void AIL_set_3D_sample_offset(void *sample, int offset);
extern void AIL_resume_3D_sample(void *sample);
extern void AIL_stop_3D_sample(void *sample);
extern void AIL_end_3D_sample(void *sample);
extern unsigned long AIL_3D_sample_status(void *sample);
extern void AIL_set_3D_rolloff_factor(void *provider, float factor);
extern int AIL_WAV_info(const void *data, void *info);
extern void MacAudio_Mix(float *stereo, size_t frames);
extern int AudioOutput_Render(float *pcm, unsigned int frames, int *silent);
extern void SND_SetChannelInfo(int, int, const snd_alias_t *, const snd_alias_t *, float, const vec_t *, float,
                               float, int, int, int, int, int, int, snd_alias_system_t);
extern int SND_StartAlias3DSample(const snd_alias_t *, const snd_alias_t *, float, int, const vec_t *, float,
                                float, int, float, int, int, int *, snd_alias_system_t);
extern int CG_PlayPickedAlias(const snd_alias_t *, int, const vec_t *, int);
extern const snd_alias_t *Com_PickSoundAliasFromList(snd_alias_list_t *list);
extern void AnglesToAxis(const vec_t *, vec3_t *);
extern void AxisCopy(const vec3_t *, vec3_t *);
extern float Vec3Distance(const float *, const float *);
extern float Com_GetVolumeFalloffCurveValue(SndCurve *curve, float fraction);

int SND_FindFree3DChannel(int entity, int channel)
{
    (void)entity;
    (void)channel;
    return 0;
}

void *Com_GetSoundFileMem(const void *alias)
{
    assert(alias == &sampleAlias || alias == pickedAlias);
    return &sound;
}

float Com_GetVolumeFalloffCurveValue(SndCurve *curve, float fraction)
{
    (void)curve;
    return 1.0f - fraction;
}

const snd_alias_t *Com_PickSoundAliasFromList(snd_alias_list_t *list)
{
    assert(list == (emitted == 0 ? expectedAlias : expectedEquipment) && list->count == 1);
    ++emitted;
    pickedAlias = list->head;
    return pickedAlias;
}

/* Alias randomization/channel allocation are outside this output contract. */
int CG_PlayPickedAlias(const snd_alias_t *alias, int entity, const vec_t *origin, int timeshift)
{
    if (!alias->soundFile)
        return 0;
    ++starts;
    return SND_StartAlias3DSample(alias, alias, 0, entity, origin, 1, 1, timeshift, 0, 0, 0, NULL, SASYS_CGAME);
}

#include "output_source.h"

static void Setup(void)
{
    memset(&g_snd, 0, sizeof(g_snd));
    memset(&sound, 0, sizeof(sound));
    memset(entities, 0, sizeof(entities));
    cg_entities = entities;
    environment.drylevel = 1;
    environment.wetlevel = 0;
    g_snd.effect = &environment;
    g_snd.channelvol = &channelVolumes;
    for (int i = 0; i < 11; ++i)
        channelVolumes.channelvol[i].volume = 1;
    g_snd.Initialized2d = 1;
    g_snd.volume = g_snd.timescale = 1;
    for (int i = 0; i < 480; ++i)
        pcm[i] = 8192;
    sound.info.format = 1;
    sound.info.data_ptr = pcm;
    sound.info.data_len = sizeof(pcm);
    sound.info.rate = 48000;
    sound.info.bits = 16;
    sound.info.channels = 1;
    sound.info.samples = 480;
    memset(&sampleAlias, 0, sizeof(sampleAlias));
    sampleAlias.fDistMax = 1000;
    sampleAlias.flags = 0;
    milesGlob.handle_3D[0] = AIL_allocate_3D_sample_handle((void *)(uintptr_t)2);
    assert(milesGlob.handle_3D[0]);
    /* Driver initialization disables provider rolloff; game curves attenuate. */
    AIL_set_3D_rolloff_factor((void *)(uintptr_t)2, 0);
    vec3_t origin = {17, 29, 41}, angles = {0, 0, 0}, axis[3];
    AnglesToAxis(angles, axis);
    SND_SetListener(0, origin, axis);
}

static void CheckPosition(const float *expected)
{
    float observed[3];
    AIL_3D_position(milesGlob.handle_3D[0], &observed[0], &observed[1], &observed[2]);
    for (int i = 0; i < 3; ++i) {
        if (fabsf(observed[i] - expected[i]) >= 0.0001f) {
            fprintf(stderr, "position component %d: observed=%g expected=%g\n", i, observed[i], expected[i]);
            abort();
        }
    }
}

static void CheckOutput(float expectedLeft, float expectedRight, int render)
{
    float mixed[16];
    int silent = -1;
    if (render) {
        assert(AudioOutput_Render(mixed, 8, &silent) == 0);
        assert(silent == (expectedLeft == 0 && expectedRight == 0));
    } else {
        MacAudio_Mix(mixed, 8);
    }
    for (int i = 0; i < 8; ++i) {
        if (fabsf(mixed[i * 2] - expectedLeft) >= 0.00001f || fabsf(mixed[i * 2 + 1] - expectedRight) >= 0.00001f) {
            fprintf(stderr, "PCM frame %d: observed=(%g,%g) expected=(%g,%g)\n",
                    i, mixed[i * 2], mixed[i * 2 + 1], expectedLeft, expectedRight);
            abort();
        }
    }
}

static void PositionCases(void)
{
    const float local[][3] = {{0, 0, 100}, {100, 0, 0}, {-100, 0, 0}, {0, 0, -100}, {0, 100, 0}, {0, -100, 0}};
    const float pan[] = {0, 1, -1, 0, 0, 0};
    for (int rotation = 0; rotation < 3; ++rotation) {
        vec3_t angles = {rotation == 2 ? 30 : 0, rotation * 90, rotation == 2 ? 15 : 0}, axis[3];
        AnglesToAxis(angles, axis);
        SND_SetListener(0, g_snd.listeners[0].orient.origin, axis);
        for (int i = 0; i < 6; ++i) {
            vec3_t world;
            for (int j = 0; j < 3; ++j)
                world[j] = g_snd.listeners[0].orient.origin[j] - local[i][0] * axis[1][j]
                           + local[i][1] * axis[2][j] + local[i][2] * axis[0][j];
            assert(AIL_set_3D_sample_info(milesGlob.handle_3D[0], &sound.info));
            AIL_set_3D_sample_volume(milesGlob.handle_3D[0], 0.5f);
            SND_Set3DPosition(0, world);
            CheckPosition(local[i]);
            AIL_resume_3D_sample(milesGlob.handle_3D[0]);
            float left = 0.125f * sqrtf((1 - pan[i]) * 0.5f);
            float right = 0.125f * sqrtf((1 + pan[i]) * 0.5f);
            CheckOutput(left, right, 0);
            CheckOutput(left, right, 1);
        }
    }
    puts("PASS 18 listener-space directions: translated, yawed, pitched/rolled listener; MacAudio_Mix and Render PCM");
}

static void UpdateCase(void)
{
    entities[1].lerpOrigin[0] = 117;
    entities[1].lerpOrigin[1] = 29;
    entities[1].lerpOrigin[2] = 41;
    vec3_t world = {117, 4, 53};
    SND_SetChannelInfo(0, 1, &sampleAlias, &sampleAlias, 0, world, 0.5f, 1, 1, 48000, 10, 0, 0, 0, SASYS_CGAME);
    assert(g_snd.chaninfo[0].offset[0] == 0 && g_snd.chaninfo[0].offset[1] == -25 && g_snd.chaninfo[0].offset[2] == 12);
    /* Move and turn the emitter after its entity-local offset is recorded. */
    memcpy(entities[1].lerpOrigin, (vec3_t){67,39,21}, sizeof(vec3_t));
    entities[1].lerpAngles[1] = 90;
    assert(AIL_set_3D_sample_info(milesGlob.handle_3D[0], &sound.info));
    AIL_resume_3D_sample(milesGlob.handle_3D[0]);
    SND_Update3DChannel(0, 16);
    /* Yaw 90 maps the offset to world (25,0,12), yielding (92,39,33). */
    const float expected[] = {-10, -8, 75};
    CheckPosition(expected);
    float distance = sqrtf(5625 + 100 + 64), pan = -10 / distance;
    float amplitude = 0.125f * (1 - distance / 1000);
    CheckOutput(amplitude * sqrtf((1 - pan) * 0.5f), amplitude * sqrtf((1 + pan) * 0.5f), 1);
    puts("PASS moving 3D channel preserves entity-space offset through Render PCM");
}

static void TimelineCase(void)
{
    vec3_t world = {117, 29, 41};
    entities[1].lerpOrigin[0] = 117;
    entities[1].lerpOrigin[1] = 29;
    entities[1].lerpOrigin[2] = 41;
    assert(SND_StartAlias3DSample(&sampleAlias, &sampleAlias, 0, 1, world, 0.5f, 2, 0, 0, 32, 0, NULL, SASYS_CGAME) == 42);
    assert(AIL_3D_sample_playback_rate(milesGlob.handle_3D[0]) == 96000);
    CheckOutput(0, 0, 1);
    SND_Update3DChannel(0, 16);
    CheckOutput(0, 0, 0);
    SND_Update3DChannel(0, 16);
    assert(g_snd.chaninfo[0].startDelay == 0);
    float amplitude = 0.1125f / sqrtf(2);
    CheckOutput(amplitude, amplitude, 1);
    AIL_stop_3D_sample(milesGlob.handle_3D[0]);
    CheckOutput(0, 0, 1);
    AIL_resume_3D_sample(milesGlob.handle_3D[0]);
    float mixed[464];
    MacAudio_Mix(mixed, 232);
    for (int i = 0; i < 232 * 2; ++i)
        assert(fabsf(mixed[i] - amplitude) < 0.00001f);
    CheckOutput(0, 0, 1);
    assert(AIL_3D_sample_status(milesGlob.handle_3D[0]) == 2);
    puts("PASS sample start: 48k PCM, 96k playback, delay, stop/resume cursor and exact 240 output-frame duration");
}

static float ExpectedPcm(size_t frame)
{
    const unsigned char *data = sound.info.data_ptr;
    size_t samples = sound.info.data_len / 2;
    double cursor = (double)frame * sound.info.rate / 48000;
    size_t current = (size_t)cursor;
    if (current >= samples)
        return 0;
    size_t next = current + 1 < samples ? current + 1 : current;
    int first = (int16_t)((unsigned int)data[current * 2] | (unsigned int)data[current * 2 + 1] << 8);
    int second = (int16_t)((unsigned int)data[next * 2] | (unsigned int)data[next * 2 + 1] << 8);
    return (first + (second - first) * (float)(cursor - current)) / 32768.0f;
}

static void FootstepsCase(void)
{
    cgs_t *cgs = (cgs_t *)cgsArray;
    snapshot_t snapshot = {0};
    cgArray[0].nextSnap = &snapshot;
    snapshot.ps.clientNum = 0;
    dvar_t enabled = {0};
    enabled.current.enabled = 1;
    cg_footsteps = &enabled;
    static snd_alias_t footstep, equipment;
    static snd_alias_list_t stepLists[2][3][23], gearLists[2];
    SoundFile file = {0};
    footstep = sampleAlias;
    footstep.soundFile = &file;
    gearLists[0].head = gearLists[1].head = &equipment;
    gearLists[0].count = gearLists[1].count = 1;
    cgs->media.runningEquipmentSound = cgs->media.walkingEquipmentSound = &gearLists[0];
    cgs->media.runningEquipmentSoundPlayer = cgs->media.walkingEquipmentSoundPlayer = &gearLists[1];
    for (int view = 0; view < 2; ++view) for (int kind = 0; kind < 3; ++kind) for (int surface = 0; surface < 23; ++surface) {
        stepLists[view][kind][surface].head = &footstep;
        stepLists[view][kind][surface].count = 1;
    }
    for (int i = 0; i < 23; ++i) {
        cgs->media.stepRunSound[i] = &stepLists[0][0][i]; cgs->media.stepRunSoundPlayer[i] = &stepLists[1][0][i];
        cgs->media.stepWalkSound[i] = &stepLists[0][1][i]; cgs->media.stepWalkSoundPlayer[i] = &stepLists[1][1][i];
        cgs->media.stepProneSound[i] = &stepLists[0][2][i]; cgs->media.stepProneSoundPlayer[i] = &stepLists[1][2][i];
    }
    centity_t *cent = &entities[1];
    cent->nextState.number = 1;
    memcpy(cent->lerpOrigin, (vec3_t){117,29,41}, sizeof(vec3_t));
    for (int view = 0; view < 2; ++view) for (int kind = 0; kind < 3; ++kind) for (int surface = 0; surface < 23; ++surface) {
        /* The original flag/client test controls the first-person slot. */
        snapshot.ps.pm_flags = view ? 0xc00000 : 0;
        snapshot.ps.clientNum = view ? 1 : 0;
        expectedAlias = &stepLists[view][kind][surface];
        expectedEquipment = &gearLists[view];
        emitted = starts = 0;
        /* Equipment shares the picker boundary; accept it after the step. */
        CG_EntityEvent(cent, 1 + kind * 23 + surface);
        assert(emitted == 2 && starts == 1);
        float mixed[16];
        int silent;
        assert(AudioOutput_Render(mixed, 8, &silent) == 0 && !silent);
        float energy = 0;
        for (int i = 0; i < 8; ++i) {
            assert(isfinite(mixed[i * 2]) && fabsf(mixed[i * 2] - mixed[i * 2 + 1]) < 0.00001f);
            float expected = ExpectedPcm((size_t)i) * 0.9f / sqrtf(2);
            assert(fabsf(mixed[i * 2] - expected) < 0.00001f);
            energy += fabsf(mixed[i * 2]);
        }
        assert(energy > 0);
        AIL_end_3D_sample(milesGlob.handle_3D[0]);
    }
    puts("PASS 138 run/walk/prone first/third-person surface events: emitted alias slots reach native Render PCM");
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
    Setup();
    void *wave = NULL;
    if (argc == 3) {
        FILE *file = fopen(argv[2], "rb");
        assert(file && fseek(file, 0, SEEK_END) == 0);
        long length = ftell(file);
        assert(length > 44 && length < 16 * 1024 * 1024 && fseek(file, 0, SEEK_SET) == 0);
        wave = malloc((size_t)length);
        assert(wave && fread(wave, 1, (size_t)length, file) == (size_t)length);
        fclose(file);
        assert(AIL_WAV_info(wave, &sound.info));
        assert(sound.info.format == 1 && sound.info.channels == 1 && sound.info.bits == 16);
        printf("Private WAV input: %lu Hz, %lu mono PCM16 samples\n", sound.info.rate, sound.info.samples);
    }
    if (!strcmp(argv[1], "position")) PositionCases();
    else if (!strcmp(argv[1], "update")) UpdateCase();
    else if (!strcmp(argv[1], "timeline")) TimelineCase();
    else if (!strcmp(argv[1], "footsteps")) FootstepsCase();
    else abort();
    AIL_release_sample_handle(milesGlob.handle_3D[0]);
    free(wave);
    return 0;
}
