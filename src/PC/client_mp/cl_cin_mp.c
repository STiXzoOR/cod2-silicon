#include "common_types.h"
#include "imports.h"

extern e_status ROQ_StopCinematicFromHandle(int handle);
extern e_status ROQ_RunCinematicFromHandle(int handle);
extern int ROQ_PlayCinematic(const char *arg, int x, int y, int w, int h, int systemBits);
extern void ROQ_SetExtentsFromHandle(int handle, int x, int y, int w, int h);
extern void ROQ_DrawCinematicFromHandle(int handle);
extern void ROQ_PlayCinematic_f(void);
extern void ROQ_DrawCinematic(void);
extern void ROQ_RunCinematic(void);
extern void ROQ_StopCinematic(void);
extern void ROQ_UploadCinematicFromHandle(int handle);
extern void ROQ_CloseAllVideos(void);

e_status CIN_StopCinematic(int handle)
{
    return ROQ_StopCinematicFromHandle(handle);
}

e_status CIN_RunCinematic(int handle)
{
    return ROQ_RunCinematicFromHandle(handle);
}

int CIN_PlayCinematic(const char *arg, int x, int y, int w, int h, int systemBits)
{
#if COD2_APPLE_SDK
    return -1;
#else
    return ROQ_PlayCinematic(arg, x, y, w, h, systemBits);
#endif
}

void CIN_SetExtents(int handle, int x, int y, int w, int h)
{
    ROQ_SetExtentsFromHandle(handle, x, y, w, h);
}

void CIN_DrawCinematic(int handle)
{
    ROQ_DrawCinematicFromHandle(handle);
}

void CL_PlayCinematic_f(void)
{
#if COD2_APPLE_SDK
    extern const dvar_t *nextmap, *com_introPlayed;
    extern void Dvar_SetString(const dvar_t *, const char *);
    extern void Dvar_SetBool(const dvar_t *, qboolean);
    Dvar_SetString(nextmap, "");
    Dvar_SetBool(com_introPlayed, 1);
#else
    ROQ_PlayCinematic_f();
#endif
}

void SCR_DrawCinematic(void)
{
    ROQ_DrawCinematic();
}

void SCR_RunCinematic(void)
{
    ROQ_RunCinematic();
}

void SCR_StopCinematic(void)
{
    ROQ_StopCinematic();
}

void CIN_UploadCinematic(int handle)
{
    ROQ_UploadCinematicFromHandle(handle);
}

void CIN_CloseAllVideos(void)
{
    ROQ_CloseAllVideos();
}
