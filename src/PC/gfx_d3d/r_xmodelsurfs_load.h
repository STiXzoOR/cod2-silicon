#ifndef R_XMODELSURFS_LOAD_H
#define R_XMODELSURFS_LOAD_H

void R_XModelSurfs_SetRefImport(void *rimp_v60);

void *R_LoadXModelSurfsSurface(void *surfsCtx, void *partBitsArg,
                               int **streamCursor, void *(*alloc)(int size));

void *R_RegisterXModelSurfs(void *xmodel);

#endif
