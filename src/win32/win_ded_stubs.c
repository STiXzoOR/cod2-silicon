/* The dedicated server does not build the client, renderer or sound units. Its
 * effect code still names them from functions a server never calls. ELF and Mach-O
 * links strip those callers; a PE link keeps them, so it needs these symbols. */
#include <stdlib.h>

#define COD2_CLIENT_ONLY(name) void name(void) { abort(); }

COD2_CLIENT_ONLY(CG_DObjCalcBoneGeneric)
COD2_CLIENT_ONLY(CG_GetDObjOrientation)
COD2_CLIENT_ONLY(CG_ImpactMark)
COD2_CLIENT_ONLY(CL_AddDebugLine)
COD2_CLIENT_ONLY(Cloud_Cloud)
COD2_CLIENT_ONLY(Cylinder_Cylinder)
COD2_CLIENT_ONLY(Effect_SetBoltFrame)
COD2_CLIENT_ONLY(Effect_SetTimeStartEnd)
COD2_CLIENT_ONLY(Emitter_Emitter)
COD2_CLIENT_ONLY(Flash_Init)
COD2_CLIENT_ONLY(FxArchive_ArchiveEffect)
COD2_CLIENT_ONLY(FxArchive_ArchiveFxBoltInfo)
COD2_CLIENT_ONLY(FxArchive_BeginReading)
COD2_CLIENT_ONLY(FxArchive_FxArchive)
COD2_CLIENT_ONLY(FxArchive_ReadData)
COD2_CLIENT_ONLY(FxArchive_WriteData)
COD2_CLIENT_ONLY(FxBoltFrame_Acquire)
COD2_CLIENT_ONLY(FxBoltFrame_GetOrientation)
COD2_CLIENT_ONLY(FxBoltFrame_Release)
COD2_CLIENT_ONLY(FxChannelInstance_Create)
COD2_CLIENT_ONLY(Light_Light)
COD2_CLIENT_ONLY(Line_Line)
COD2_CLIENT_ONLY(OrientedParticle_OrientedParticle)
COD2_CLIENT_ONLY(Particle_GetTotalVelocityAtTime0)
COD2_CLIENT_ONLY(Particle_IntegrateTotalVelocity)
COD2_CLIENT_ONLY(Particle_Particle)
COD2_CLIENT_ONLY(Particle_SetAxis)
COD2_CLIENT_ONLY(Particle_SetRandomVelocity2Weights)
COD2_CLIENT_ONLY(Particle_SetRandomVelocityWeights)
COD2_CLIENT_ONLY(R_AddRefEntityToScene)
COD2_CLIENT_ONLY(R_BeginFrame)
COD2_CLIENT_ONLY(R_EndFrame)
COD2_CLIENT_ONLY(R_RenderScene)
COD2_CLIENT_ONLY(SND_GetSoundFileSize)
COD2_CLIENT_ONLY(Tail_InitEndPoint)
COD2_CLIENT_ONLY(Tail_Tail)
COD2_CLIENT_ONLY(glOrtho)

/* Named by the crash report, which prints them; only the renderer ever sets them. */
volatile int g_rigid_step;
volatile int g_lastdraw[6];
struct { int type, surface, entityIndex; void *material; int iter, total, sort; } g_rb_draw_dbg;
unsigned char __ZTV5Flash[256];
#if !defined(__x86_64__)
void *__ZTIl = 0;
#endif
