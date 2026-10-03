#ifndef COD2X_POSE_H
#define COD2X_POSE_H

#include "common_types.h"

void Cod2x_PoseInit(void);
int Cod2x_PoseNeutral(void);
void Cod2x_PoseReset(entityState_t *entity);
void Cod2x_PoseOffsets(vec3_t goals[8]);
void Cod2x_PoseDiagnostics(const entityState_t *entity, const clientInfo_t *info, int fromServer, int frametime);

#endif
