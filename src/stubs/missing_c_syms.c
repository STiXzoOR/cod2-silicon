void ZN14CAudioRecorderD1Ev(void) {}

#include "common_types.h"
#include <string.h>

extern int FS_FOpenFileRead(const char *, int *, int);
extern int FS_FOpenFileWrite(const char *);

extern void R_BeginFrame(void);
extern void R_EndFrame(void);
extern void R_RenderScene(void *refdef);
void RE_BeginFrame(void)
{
    R_BeginFrame();
}
void RE_EndFrame(void)
{
    R_EndFrame();
}
void RE_RenderScene(void *refdef)
{
    R_RenderScene(refdef);
}

int COpenGL_sOpenGLE = 0;
int CVAOPacket_sAllPackets[16] = { 0 };
int CVAOPacket_sCurrentPacket[16] = { 0 };
int CVAOPacket_sGenericPacket[16] = { 0 };
int CVAOPacket_sVAOStatus[16] = { 0 };
