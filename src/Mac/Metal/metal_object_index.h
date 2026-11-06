#ifndef COD2_METAL_OBJECT_INDEX_H
#define COD2_METAL_OBJECT_INDEX_H
/* Keep SDK dictionary declarations out of reconstructed engine headers. */
void *MacShimObjectIndex_Create(void);
void *MacShimObjectIndex_Get(void *index, const void *key);
void MacShimObjectIndex_Set(void *index, const void *key, void *value);
void MacShimObjectIndex_Remove(void *index, const void *key);
void MacShimObjectIndex_Destroy(void *index);
#endif
