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
    return ROQ_PlayCinematic(arg, x, y, w, h, systemBits);
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
    ROQ_PlayCinematic_f();
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
