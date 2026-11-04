#include "common_types.h"
#include "imports.h"
#include "headers/PC/cgame_mp/cg_local.h"
#include <string.h>

extern float floorf(float x);

extern byte *_dvar_shellshock_fadein;
extern byte *_dvar_shellshock_fadeout;
extern byte *_dvar_shellshock_screenblend;
extern byte *_dvar_shellshock_screentype;
extern byte *_dvar_shellshock_screenenabled;
extern byte *_dvar_shellshock_soundfadein;
extern byte *_dvar_shellshock_soundfadeout;
extern byte *_dvar_shellshock_loopfadein;
extern byte *_dvar_shellshock_loopfadeout;
extern byte *_dvar_shellshock_looptype;
extern byte *_dvar_shellshock_sounddrylevellooptype;
extern byte *_dvar_shellshock_soundwetlevellooptype;
extern byte *_dvar_shellshock_soundloopsilent;
extern byte *_dvar_shellshock_viewkickfadein;
extern byte *_dvar_shellshock_viewkickperiod;
extern byte *_dvar_shellshock_viewkickradius;
extern byte *_dvar_shellshock_viewkickpitch;
extern byte *_dvar_shellshock_viewkickyaw;
extern byte *_dvar_shellshock_soundroomtype;
extern byte *_dvar_shellshock_sounddrylevel;
extern byte *_dvar_shellshock_soundwetlevel;
extern byte *_dvar_shellshock_soundmodenddelay;
extern byte *_dvar_shellshock_soundendduration;
extern byte *_dvar_shellshock_soundfade;
extern byte *_dvar_shellshock_mouseenable;
extern byte *_dvar_shellshock_mousefadein;
extern byte *_dvar_shellshock_mouseturnrate;
extern byte *_dvar_shellshock_mousereducemax;
extern byte *_dvar_shellshock_mousesensitivity;
extern const dvar_t *cg_shock_viewKickFadeTime;

#if defined(COD2_X64)
#define _snd_local_listener ((const vec_t *)imp_vec3_origin)
#else
extern int _snd_local_listener;
#endif

extern const vec_t Vec3Normalize(vec_t *v);
extern void Vec3Cross(const vec_t *v1, const vec_t *v2, vec_t *out);
extern void AxisCopy(vec3_t *in, vec3_t *out);
#if defined(COD2_X64)
extern void MatrixMultiply(const float (*in0)[3], const float (*in1)[3], float (*out)[3]);
#else
extern void MatrixMultiply(const vec3_t axis, const float (*in1)[3], float (*out)[3]);
#endif
extern qboolean Com_SaveDvarsToBuffer(const char **dvar_names, int count, char *buf, int bufsize);
extern qboolean Com_LoadDvarsFromBuffer(const char **dvar_names, int count, const char *buf, const char *path);
extern const char *va(const char *format, ...);
extern int FS_FOpenFileByMode(const char *name, int *fh, int mode);
extern int FS_Write(const void *buf, int len, int fh);
extern int FS_Read(void *buf, int len, int fh);
extern void FS_FCloseFile(fileHandle_t fh);
extern void *Z_MallocInternal(int size);
extern void Z_FreeInternal(void *ptr);
extern void Com_Printf(const char *fmt, ...);
extern void CL_SaveScreen(void);
extern void CL_BlendSavedScreen(int blend);
extern const char * Dvar_EnumToString(const dvar_t *dvar);
extern void SND_SetChannelVolumes(int priority, const float *channelvolume, int fademsec);
extern void SND_SetEnvironmentEffects(int type, const char *name, float drylevel, float wetlevel, int flags);
extern void SND_DeactivateChannelVolumes(int type, int flags);
extern void SND_DeactivateEnvironmentEffects(int type, int flags);
extern snd_alias_t * CL_PickSoundAlias(const char *aliasname);
#if defined(COD2_X64)
extern int SND_PlayBlendedSoundAliases(const snd_alias_t *, const snd_alias_t *, float, int, const vec_t *, int, snd_alias_system_t);
#else
extern void SND_PlayBlendedSoundAliases(void *alias0, void *alias1, float fade, int channel, int entity, int flags, int loop);
#endif
#if defined(COD2_X64)
extern int SND_PlaySoundAlias(const snd_alias_t *, int, const vec_t *, int, snd_alias_system_t);
#else
extern void SND_PlaySoundAlias(void *alias, int channel, int entity, int duration, int loop);
#endif
extern void CL_CapTurnRate(float min_rate, float max_rate);
extern void CL_SetUserCmdInShellshock(int inShellshock);

static inline float dvar_get_float(byte *dvar_pp)
{
    dvar_t *dvar = *(dvar_t **)dvar_pp;
    return dvar->current.value;
}

static inline int dvar_get_int(byte *dvar_pp)
{
    dvar_t *dvar = *(dvar_t **)dvar_pp;
    return dvar->current.integer;
}

static inline int dvar_get_bool(byte *dvar_pp)
{
    dvar_t *dvar = *(dvar_t **)dvar_pp;
    return (int)dvar->current.enabled;
}

static inline dvar_t *dvar_get_ptr(byte *dvar_pp)
{
    return *(dvar_t **)dvar_pp;
}

static inline int float_seconds_to_ms(float val)
{
    return (int)floorf(val * 1000.0f + 0.5f);
}

/* migrated from the ILP32 data blob to typed C (x64 port Stage 2). */
#if defined(COD2_X64)
const char *cg_shock_dvar_names[29] = {
#else
const char *cg_shock_dvar_names[30] = {
#endif
    "cg_shock_screenBlendTime",
    "cg_shock_screenBlendFadeTime",
    "cg_shock_viewKickPeriod",
    "cg_shock_viewKickRadius",
#if !defined(COD2_X64)
    "cg_shock_viewKickFadeTime",
#endif
    "cg_shock_sound",
    "cg_shock_soundFadeInTime",
    "cg_shock_soundFadeOutTime",
    "cg_shock_soundLoopFadeTime",
    "cg_shock_soundLoopEndDelay",
    "cg_shock_soundRoomType",
    "cg_shock_soundDryLevel",
    "cg_shock_soundWetLevel",
    "cg_shock_soundModEndDelay",
    "cg_shock_volume_auto",
    "cg_shock_volume_auto2d",
    "cg_shock_volume_menu",
    "cg_shock_volume_weapon",
    "cg_shock_volume_voice",
    "cg_shock_volume_item",
    "cg_shock_volume_body",
    "cg_shock_volume_local",
    "cg_shock_volume_music",
    "cg_shock_volume_announcer",
    "cg_shock_volume_shellshock",
    "cg_shock_mouse",
    "cg_shock_mouse_maxpitchspeed",
    "cg_shock_mouse_maxyawspeed",
    "cg_shock_mouse_sensitivityscale",
    "cg_shock_mouse_fadeTime",
};

#if defined(COD2_X64)
/* The last three pairs repeat the start for the four-point interpolation. */
static vec2_t cg_perturbations[131] = {
    { -0.563551f, -0.004430f },
    { -0.282062f, -0.757933f },
    { 0.413047f, 0.244246f },
    { 0.527895f, -0.723897f },
    { -0.329777f, 0.669800f },
    { -0.394248f, -0.763090f },
    { 0.126207f, 0.497769f },
    { 0.004986f, -0.014130f },
    { 0.559913f, 0.112825f },
    { -0.333089f, -0.573283f },
    { 0.335404f, -0.107176f },
    { -0.569060f, -0.213141f },
    { -0.166676f, 0.785084f },
    { 0.299592f, 0.037593f },
    { -0.516867f, 0.510759f },
    { 0.138009f, 0.034823f },
    { -0.156167f, 0.829048f },
    { -0.999458f, 0.020317f },
    { 0.300029f, 0.252944f },
    { 0.030215f, -0.295732f },
    { -0.917362f, -0.050711f },
    { 0.044177f, -0.269289f },
    { 0.588424f, 0.362577f },
    { -0.379913f, 0.619214f },
    { 0.204432f, -0.019423f },
    { 0.018499f, 0.468079f },
    { 0.916187f, -0.247878f },
    { 0.003799f, 0.108210f },
    { 0.057363f, 0.606240f },
    { 0.324595f, 0.158733f },
    { -0.130529f, -0.183388f },
    { 0.715672f, -0.363858f },
    { 0.984258f, 0.106096f },
    { -0.003313f, 0.345535f },
    { -0.320351f, -0.573936f },
    { 0.063455f, -0.003239f },
    { -0.570173f, -0.759313f },
    { 0.106456f, 0.283726f },
    { -0.668163f, 0.142388f },
    { -0.501119f, -0.720006f },
    { -0.253281f, 0.524032f },
    { -0.064084f, -0.165943f },
    { -0.194672f, 0.433550f },
    { -0.281800f, -0.417744f },
    { 0.045786f, 0.402986f },
    { 0.105064f, -0.558937f },
    { 0.312244f, 0.688318f },
    { -0.263294f, -0.256811f },
    { 0.659186f, 0.070672f },
    { 0.093625f, -0.046812f },
    { -0.875020f, 0.288509f },
    { 0.329359f, 0.105941f },
    { -0.181309f, 0.259865f },
    { 0.261597f, -0.074070f },
    { -0.296082f, 0.031858f },
    { 0.038584f, 0.565947f },
    { -0.253445f, -0.717865f },
    { -0.211836f, 0.336521f },
    { 0.890123f, 0.004950f },
    { -0.979825f, -0.170790f },
    { 0.045346f, 0.022240f },
    { -0.345796f, 0.522712f },
    { 0.108525f, 0.165424f },
    { -0.572796f, -0.473399f },
    { 0.368605f, -0.865844f },
    { 0.075571f, -0.327703f },
    { -0.466353f, -0.565594f },
    { -0.358837f, 0.610302f },
    { 0.603884f, 0.440023f },
    { 0.002465f, -0.144449f },
    { -0.294915f, 0.799970f },
    { -0.028347f, -0.112071f },
    { -0.009472f, 0.686061f },
    { 0.071150f, 0.019910f },
    { 0.962690f, 0.024926f },
    { 0.309208f, 0.871549f },
    { -0.123782f, -0.312301f },
    { -0.433055f, -0.895981f },
    { 0.962495f, -0.263777f },
    { -0.511460f, -0.359478f },
    { -0.044013f, 0.020210f },
    { -0.109340f, -0.761230f },
    { 0.171003f, -0.107461f },
    { 0.418912f, 0.435294f },
    { 0.444940f, -0.139643f },
    { 0.518574f, 0.365965f },
    { -0.506997f, 0.655597f },
    { 0.510525f, 0.508961f },
    { -0.296173f, -0.675837f },
    { 0.851332f, 0.307192f },
    { -0.008474f, -0.188744f },
    { 0.552703f, 0.427086f },
    { 0.080334f, -0.002805f },
    { 0.035656f, 0.610991f },
    { 0.770593f, 0.398874f },
    { -0.522137f, 0.324362f },
    { 0.006045f, 0.042788f },
    { 0.482456f, 0.848994f },
    { 0.226058f, -0.522367f },
    { -0.674606f, -0.547814f },
    { -0.441998f, 0.598840f },
    { -0.183957f, -0.270234f },
    { 0.518850f, 0.634946f },
    { 0.430386f, 0.125257f },
    { -0.185496f, -0.264459f },
    { 0.023690f, 0.312978f },
    { -0.444287f, 0.849928f },
    { 0.291978f, -0.897679f },
    { -0.045826f, -0.047128f },
    { -0.114246f, 0.511975f },
    { 0.738133f, 0.607667f },
    { -0.786889f, -0.384057f },
    { 0.182993f, 0.265086f },
    { -0.399450f, -0.309031f },
    { -0.482895f, 0.265662f },
    { 0.059671f, 0.097760f },
    { 0.793174f, -0.015972f },
    { 0.201658f, 0.492445f },
    { -0.707371f, -0.026190f },
    { -0.320882f, 0.372280f },
    { 0.572813f, -0.537255f },
    { 0.337619f, 0.116293f },
    { -0.606537f, 0.173373f },
    { -0.166593f, -0.335112f },
    { -0.583993f, 0.182916f },
    { -0.573519f, -0.623348f },
    { -0.392707f, 0.449474f },
    { 0.151474f, 0.840401f },
    { -0.563551f, -0.004430f },
    { -0.282062f, -0.757933f },
    { 0.413047f, 0.244246f },
};
#else
static vec2_t cg_perturbations[131];
#endif

void CG_PerturbCamera(void);
qboolean CG_SaveShellShockDvars(const char *name);
qboolean CG_LoadShellShockDvars(const char *name);
qboolean CG_DrawShellShockSavedScreenBlend(const shellshock_parms_t *parms, int start, int duration);
void CG_SetShellShockParmsFromDvars(shellshock_parms_t *parms);
void CG_UpdateShellShock(const shellshock_parms_t *parms, int start, int duration);

void CG_PerturbCamera(void)
{
    cg_t *cgp = cg;
#if defined(COD2_X64)
    vec3_t perturbAxis[3];
    vec_t *rot = perturbAxis[0];
    vec_t *cross = perturbAxis[1];
    vec_t *up = perturbAxis[2];
#else
    vec3_t rot;
    vec3_t up;
    vec3_t cross;
#endif
    float (*refdefAxis)[3];
    float axis[3][3];

    if (cgp->shellshock.viewDelta[0] == 0.0f && cgp->shellshock.viewDelta[1] == 0.0f) {
        return;
    }

    rot[0] = 1.0f;
    rot[1] = cgp->shellshock.viewDelta[0];
    rot[2] = cgp->shellshock.viewDelta[1];

    up[0] = 0.0f;
    up[1] = 0.0f;
    up[2] = 1.0f;

    Vec3Normalize(rot);
    Vec3Cross(up, rot, cross);
    Vec3Normalize(cross);
    Vec3Cross(rot, cross, up);

    refdefAxis = (float (*)[3])cgp->refdef.viewaxis;
    AxisCopy(refdefAxis, axis);
#if defined(COD2_X64)
    MatrixMultiply(perturbAxis, axis, refdefAxis);
#else
    MatrixMultiply(rot, axis, refdefAxis);
#endif
}

qboolean CG_SaveShellShockDvars(const char *name)
{
    char filebuf[0x10000];
    int fh;
    const char *path;

    if (!Com_SaveDvarsToBuffer(cg_shock_dvar_names, 29, filebuf, 0x10000)) {
        return 0;
    }

    path = va("shock/%s.shock", name);

    if (FS_FOpenFileByMode(path, &fh, 1) < 0) {
        return 0;
    }

    FS_Write(filebuf, (int)strlen(filebuf), fh);

    FS_FCloseFile(fh);
    return 1;
}

qboolean CG_LoadShellShockDvars(const char *name)
{
    const char *fullpath;
#if defined(COD2_X64)
    char fullpathBuffer[1024];
#endif
    int fh;
    int filesize;
    char *buf;
    qboolean result;

    fullpath = va("shock/%s.shock", name);
#if defined(COD2_X64)
    /* File loading and dvar parsing can reuse both thread-local va buffers. */
    strncpy(fullpathBuffer, fullpath, sizeof(fullpathBuffer));
    fullpathBuffer[sizeof(fullpathBuffer) - 1] = '\0';
    fullpath = fullpathBuffer;
#endif

    filesize = FS_FOpenFileByMode(fullpath, &fh, 0);
    if (filesize < 0) {

        Com_Printf("^1couldn't open '%s'.\n", fullpath);

        filesize = FS_FOpenFileByMode("shock/default.shock", &fh, 0);
        if (filesize < 0) {

#if defined(COD2_X64)
            Com_Printf("^1couldn't open 'shock/default.shock'. This is a default shock file that you should have.\n");
#else
            Com_Printf("^1couldn't open 'shock/default.shock'.\n");
#endif
            filesize = 0;
            return filesize;
        }
    }

    buf = (char *)Z_MallocInternal(filesize + 1);

    FS_Read(buf, filesize, fh);

    buf[filesize] = '\0';

    FS_FCloseFile(fh);

    result = Com_LoadDvarsFromBuffer(cg_shock_dvar_names, 29, buf, fullpath);

    Z_FreeInternal(buf);

    return result;
}

qboolean CG_DrawShellShockSavedScreenBlend(const shellshock_parms_t *parms, int start, int duration)
{
    cg_t *cgp;
    int timeLeft;
    int blend;
    int fadeDuration;
    int fadeIn;

    if (start == 0 || duration <= 0) {

        cgp = cg;
        cgp->shellshock.hasSavedScreen = 0;
        return 0;
    }

    cgp = cg;
    timeLeft = start + duration - cgp->time;

    if (timeLeft <= 0) {

        cgp->shellshock.hasSavedScreen = 0;
        return 0;
    }

    fadeIn = parms->screenBlend.effectTime;
    fadeDuration = parms->screenBlend.fadeTime;

    blend = fadeIn;
    if (timeLeft < fadeDuration) {

#if defined(COD2_X64)
        blend = (int)floorf((float)fadeIn * ((float)timeLeft / (float)fadeDuration) + 0.5f);
#else
        blend = (int)floorf((float)fadeIn * (float)timeLeft / (float)fadeDuration + 0.5f);
#endif
    }

    if (cgp->shellshock.hasSavedScreen != 0) {

        CL_BlendSavedScreen(blend);
    }

    CL_SaveScreen();

    cgp->shellshock.hasSavedScreen = 1;
    return 1;
}

void CG_SetShellShockParmsFromDvars(shellshock_parms_t *parms)
{
    float val;
    float epsilon;

    parms->screenBlend.fadeTime = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_fadein));

    parms->screenBlend.effectTime = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_fadeout));

    parms->view.fadeTime = 3000;

    val = dvar_get_float(_dvar_shellshock_screenblend);
    epsilon = 0.001f;
#if defined(COD2_X64)
    if (!(epsilon - val < 0.0f)) {
#else
    if (epsilon - val >= 0.0f) {
#endif
        parms->view.kickRate = 1.0f;
    } else {
        parms->view.kickRate = epsilon / val;
    }

    parms->view.kickRadius = dvar_get_float(_dvar_shellshock_screentype);

    parms->sound.use = dvar_get_bool(_dvar_shellshock_screenenabled);

    parms->sound.fadeInTime = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_soundfadein));

    parms->sound.fadeOutTime = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_soundfadeout));

    parms->sound.loopFadeTime = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_loopfadein));

    parms->sound.loopEndDelay = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_loopfadeout));

    strncpy(parms->sound.roomtype, Dvar_EnumToString(dvar_get_ptr(_dvar_shellshock_looptype)), 0xf);
    parms->sound.roomtype[15] = 0;

    *(int *)&parms->sound.drylevel = dvar_get_int(_dvar_shellshock_sounddrylevellooptype);

    *(int *)&parms->sound.wetlevel = dvar_get_int(_dvar_shellshock_soundwetlevellooptype);

    parms->sound.modEndDelay = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_soundloopsilent));

    val = dvar_get_float(_dvar_shellshock_viewkickfadein);
    parms->sound.channelvolume[0] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_viewkickperiod);
    parms->sound.channelvolume[1] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_viewkickradius);
    parms->sound.channelvolume[2] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_viewkickpitch);
    parms->sound.channelvolume[5] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_viewkickyaw);
    parms->sound.channelvolume[6] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_soundroomtype);
    parms->sound.channelvolume[4] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_sounddrylevel);
    parms->sound.channelvolume[3] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_soundwetlevel);
    parms->sound.channelvolume[7] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_soundmodenddelay);
    parms->sound.channelvolume[8] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_soundendduration);
    parms->sound.channelvolume[9] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    val = dvar_get_float(_dvar_shellshock_soundfade);
    parms->sound.channelvolume[10] = (val > 0.0f) ? ((val < 1.0f) ? val : 1.0f) : 0.0f;

    parms->mouse.use = dvar_get_bool(_dvar_shellshock_mouseenable);

    parms->mouse.fadeTime = float_seconds_to_ms(dvar_get_float(_dvar_shellshock_mousefadein));

    *(int *)&parms->mouse.maxPitchSpeed = dvar_get_int(_dvar_shellshock_mouseturnrate);

    *(int *)&parms->mouse.maxYawSpeed = dvar_get_int(_dvar_shellshock_mousereducemax);

    *(int *)&parms->mouse.sensitivity = dvar_get_int(_dvar_shellshock_mousesensitivity);
}

static inline __attribute__((always_inline)) void CG_DeactivateShellShockSound(cg_t *cgp)
{
    SND_DeactivateChannelVolumes(3, 0);
    SND_DeactivateEnvironmentEffects(2, 0);

    if (cgp->shellshock.loopEndTime != 0) {
        cgp->shellshock.loopEndTime = 0;

        {
#if defined(COD2_X64)
            void *alias = CL_PickSoundAlias("shellshock_end_abort");
#else
            void *alias = CL_PickSoundAlias("shellshock_loop_end");
#endif
            SND_PlaySoundAlias(alias, 0x3ff, _snd_local_listener, 0, 1);
        }
    }
}

static inline __attribute__((always_inline)) void CG_ResetShellShockMotion(cg_t *cgp)
{

    cgp->shellshock.sensitivity = 1.0f;

    CL_CapTurnRate(0.0f, 0.0f);
}

void CG_UpdateShellShock(const shellshock_parms_t *parms, int start, int duration)
{
    cg_t *cgp;
    int time;
    int timeSinceStart;
    float fade;
    float channelvolume[11];
    int i;
    int loopTimeLeft;
    int loopFadeOut;

    cgp = cg;
    time = cgp->time - start;

    if (start == 0 || time < 0) {

        CG_DeactivateShellShockSound(cgp);

        CG_ResetShellShockMotion(cgp);

        cgp->shellshock.viewDelta[0] = 0.0f;
        cgp->shellshock.viewDelta[1] = 0.0f;

        CL_SetUserCmdInShellshock(0);
        return;
    }

    if (parms->sound.use == 0) {

        SND_DeactivateChannelVolumes(3, 0);
        SND_DeactivateEnvironmentEffects(2, 0);

        if (cgp->shellshock.loopEndTime != 0) {
            cgp->shellshock.loopEndTime = 0;
            {
#if defined(COD2_X64)
                void *alias = CL_PickSoundAlias("shellshock_end_abort");
#else
                void *alias = CL_PickSoundAlias("shellshock_loop_end");
#endif
                SND_PlaySoundAlias(alias, 0x3ff, _snd_local_listener, 0, 1);
            }
        }
        goto check_mouse;
    }

    {

        int soundFadeOut = parms->sound.fadeOutTime;
        int soundFadeIn = parms->sound.fadeInTime;
        int totalWithFade = duration + parms->sound.modEndDelay + soundFadeOut;
        int soundTimeLeft = totalWithFade - time;

#if defined(COD2_X64)
        if (soundFadeOut <= soundTimeLeft) {
#else
        if (soundFadeOut > soundTimeLeft) {
#endif

            if (time < soundFadeIn) {

                fade = (float)time / (float)soundFadeIn;
            } else {
                fade = 1.0f;
            }
        } else {

            fade = (float)soundTimeLeft / (float)soundFadeOut;
        }

        if (fade < 0.0f) {
            fade = 0.0f;
        }

        if (fade == 0.0f) {

            SND_DeactivateChannelVolumes(3, 0);
            SND_DeactivateEnvironmentEffects(2, 0);
        } else {

            for (i = 0; i < 11; i++) {
                channelvolume[i] = (parms->sound.channelvolume[i] - 1.0f) * fade + 1.0f;
            }

            SND_SetChannelVolumes(3, channelvolume, 0);

            SND_SetEnvironmentEffects(2, parms->sound.roomtype,
                                      fade * parms->sound.drylevel,
                                      fade * parms->sound.wetlevel, 0);
        }
    }

    {

        int loopFadeOutVal = parms->sound.loopEndDelay;
        loopTimeLeft = duration + loopFadeOutVal + parms->sound.loopFadeTime - time;

        if (loopTimeLeft > 0) {

            void *pAlias0 = CL_PickSoundAlias("shellshock_loop");
#if defined(COD2_X64)
            void *pAlias1 = CL_PickSoundAlias("shellshock_loop_silent");
#else
            void *pAlias1 = CL_PickSoundAlias("shellshock_loop2");
#endif
            int loopFadeIn = parms->sound.loopFadeTime;

            if (loopFadeIn == 0) {

            } else {

                fade = 1.0f - (float)loopTimeLeft / (float)loopFadeIn;

                if (fade < 0.0f)
                    fade = 0.0f;
            }

            SND_PlayBlendedSoundAliases(pAlias0, pAlias1, fade, 0x3ff, _snd_local_listener, 0, 1);

            loopFadeOutVal = parms->sound.loopEndDelay;
        }

        cgp = cg;
        {
            int cgTime = cgp->time;
            int loopEndTime = cgTime - time + duration + loopFadeOutVal;

            if (cgTime < loopEndTime) {

                if (cgp->shellshock.loopEndTime != 0) {

                    cgp->shellshock.loopEndTime = 0;

                    {
#if defined(COD2_X64)
                        void *alias = CL_PickSoundAlias("shellshock_end_abort");
#else
                        void *alias = CL_PickSoundAlias("shellshock_loop_end");
#endif
                        SND_PlaySoundAlias(alias, 0x3ff, _snd_local_listener, 0, 1);
                    }
                }
            } else {

                if (cgp->shellshock.loopEndTime != loopEndTime) {

                    cgp->shellshock.loopEndTime = loopEndTime;

                    {
                        int delayMs = cgTime - loopEndTime;
                        void *alias = CL_PickSoundAlias("shellshock_end");
                        SND_PlaySoundAlias(alias, 0x3ff, _snd_local_listener, delayMs, 1);
                    }
                }
            }
        }
    }

check_mouse: {

    int mouseEnabled = parms->mouse.use;
    int timeSinceStart2 = duration - time;

    if (mouseEnabled == 0) {

        cgp = cg;
        CG_ResetShellShockMotion(cgp);

        timeSinceStart2 = duration - time;
        goto check_viewkick;
    }

    {

        int mouseFadeIn = parms->mouse.fadeTime;
        if (timeSinceStart2 >= mouseFadeIn) {

            cgp = cg;
            cgp->shellshock.sensitivity = parms->mouse.sensitivity;

            CL_CapTurnRate(parms->mouse.maxPitchSpeed, parms->mouse.maxYawSpeed);
        } else if (timeSinceStart2 <= 0) {

            cgp = cg;
            CG_ResetShellShockMotion(cgp);
        } else {

            float t = (float)timeSinceStart2 / (float)mouseFadeIn;

            if (t == 1.0f) {

                cgp = cg;
                cgp->shellshock.sensitivity = parms->mouse.sensitivity;
                CL_CapTurnRate(parms->mouse.maxPitchSpeed, parms->mouse.maxYawSpeed);
            } else {

                cgp = cg;
                float sensitivity = parms->mouse.sensitivity;
                cgp->shellshock.sensitivity = (sensitivity - 1.0f) * t + 1.0f;

                float minRate = parms->mouse.maxPitchSpeed / t;
                float maxRate = parms->mouse.maxYawSpeed / t;
                CL_CapTurnRate(minRate, maxRate);
            }
        }
    }
}

check_viewkick:

    timeSinceStart = duration - time;
    if (timeSinceStart <= 0) {

        cgp = cg;
        cgp->shellshock.viewDelta[0] = 0.0f;
        cgp->shellshock.viewDelta[1] = 0.0f;
    } else {

        int viewKickTime = parms->view.fadeTime;
        float t;
        float amplitude;

        if (timeSinceStart >= viewKickTime) {
            t = 1.0f;
            amplitude = (-2.0f + 3.0f) * t * t;
        } else {

            t = (float)timeSinceStart / (float)viewKickTime;
            amplitude = (-2.0f * t + 3.0f) * t * t;
        }

        amplitude = amplitude * parms->view.kickRadius;

        {
            float phase = (float)time * parms->view.kickRate;
            int phaseFloor = (int)floorf(phase);
            float frac = phase - (float)phaseFloor;

            int idx = (phaseFloor + duration * 61) & 0x7f;
            int byteOff = idx * 8;
            byte *base = (byte *)cg_perturbations + byteOff;

            float x0 = *(float *)(base + 0);
            float x1 = *(float *)(base + 8);
            float x2 = *(float *)(base + 16);
            float x3 = *(float *)(base + 24);

            float coeff_a = x3 - x2 + x1 - x0;
            float c_x = x2 - x0;
            float b_x = x0 - x1 - coeff_a;
            b_x = (coeff_a * frac + b_x) * frac;
            c_x = (c_x + b_x) * frac;
            float resultX = x1 + c_x;

            cgp = cg;
            cgp->shellshock.viewDelta[0] = amplitude * resultX;

            float y0 = *(float *)(base + 4);
            float y1 = *(float *)(base + 12);
            float y2 = *(float *)(base + 20);
            float y3 = *(float *)(base + 28);

            float coeff_a_y = y3 - y2 + y1 - y0;
            float yxmm2 = y2 - y0;
            float yxmm1 = y0 - y1 - coeff_a_y;
            float ytmp = coeff_a_y * frac;
            yxmm1 += ytmp;
            yxmm1 *= frac;
            yxmm2 += yxmm1;
            yxmm2 *= frac;
            float resultY = y1 + yxmm2;

            cgp->shellshock.viewDelta[1] = amplitude * resultY;
        }
    }

    CL_SetUserCmdInShellshock(time < duration ? 1 : 0);
}
