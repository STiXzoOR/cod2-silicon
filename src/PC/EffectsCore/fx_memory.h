#ifndef COD2_FX_MEMORY_H
#define COD2_FX_MEMORY_H

/* Windows 1.3 has one 64-block arena shared by these typed managers. */
typedef enum FxMemoryType {
    FX_MEMORY_FxBoltFrame, FX_MEMORY_Effect, FX_MEMORY_Light,
    FX_MEMORY_Particle, FX_MEMORY_Line, FX_MEMORY_OrientedParticle,
    FX_MEMORY_Tail, FX_MEMORY_Cylinder, FX_MEMORY_Emitter, FX_MEMORY_Cloud,
    FX_MEMORY_PrimitiveTemplate, FX_MEMORY_ScheduledEffect, FX_MEMORY_TYPE_COUNT
} FxMemoryType;

#if COD2_APPLE_SDK
void *FxMem_Alloc(FxMemoryType type);
void FxMem_Free(void *pointer);
#define FX_MEMORY_ALLOC(type, size) FxMem_Alloc(FX_MEMORY_##type)
#define FX_MEMORY_FREE(pointer) FxMem_Free(pointer)
#else
#define FX_MEMORY_ALLOC(type, size) __Znam(size)
#define FX_MEMORY_FREE(pointer) __ZdaPv(pointer)
#endif
#endif
