#include "common_types.h"
#include "imports.h"
#include "PC/EffectsCore/fx_memory.h"

#if COD2_APPLE_SDK
#include <assert.h>
#include <stdint.h>
#include <string.h>
extern void Com_DPrintf(const char *format, ...);

enum { FX_MEMORY_BLOCK_COUNT = 64, FX_MEMORY_WINDOWS_BLOCK_SIZE = 32768 };

/* Original descriptors 0x5dd668..0x5dd6c0. Each 32KiB block reserves its
 * final 16 bytes for free count, free list and the two block links. Native
 * objects and pointers need more physical storage, but retain these exact
 * slot counts and share the same 64 logical blocks. Templates are loaded
 * from the hunk on Windows; the template descriptor is not used by loading. */
static const unsigned short fxMemorySlots[FX_MEMORY_TYPE_COUNT] = {
    481, 167, 129, 55, 54, 54, 51, 51, 49, 51, 48, 409
};
static const size_t fxMemorySizes[FX_MEMORY_TYPE_COUNT] = {
    sizeof(FxBoltFrame), sizeof(Effect), sizeof(Light), sizeof(Particle),
    sizeof(Line), sizeof(OrientedParticle), sizeof(Tail), sizeof(Cylinder),
    sizeof(Emitter), sizeof(Cloud), sizeof(PrimitiveTemplate), sizeof(ScheduledEffect)
};

typedef union FxMemoryStorage {
    FxBoltFrame bolt[481];
    Effect effect[167];
    Light light[129];
    Particle particle[55];
    Line line[54];
    OrientedParticle oriented[54];
    Tail tail[51];
    Cylinder cylinder[51];
    Emitter emitter[49];
    Cloud cloud[51];
    PrimitiveTemplate primitive[48];
    ScheduledEffect scheduled[409];
} FxMemoryStorage;

typedef struct FxMemoryBlock {
    int freeCount;
    FxMemoryType type;
    void *freeList;
    struct FxMemoryBlock *next, *previous;
} FxMemoryBlock;

static FxMemoryStorage fxMemoryStorage[FX_MEMORY_BLOCK_COUNT];
static FxMemoryBlock fxMemoryBlocks[FX_MEMORY_BLOCK_COUNT];
static FxMemoryBlock *fxMemoryHeads[FX_MEMORY_TYPE_COUNT];
static unsigned int fxMemoryHighWater;

static void FxMem_Unlink(FxMemoryBlock *block)
{
    if (block->previous) block->previous->next = block->next;
    if (block->next) block->next->previous = block->previous;
    block->next = NULL;
    block->previous = NULL;
}

static void FxMem_MoveToFront(FxMemoryBlock *block, FxMemoryType type)
{
    FxMem_Unlink(block);
    block->next = fxMemoryHeads[type];
    fxMemoryHeads[type]->previous = block;
    fxMemoryHeads[type] = block;
}

void *FxMem_Alloc(FxMemoryType type)
{
    FxMemoryBlock *head = fxMemoryHeads[type], *block = head;
    size_t size = fxMemorySizes[type];
    while (block && !block->freeList) block = block->next;
    if (block) {
        if (block != head) FxMem_MoveToFront(block, type);
    } else {
        unsigned int index;
        for (index = 0; index != FX_MEMORY_BLOCK_COUNT; ++index)
            if (fxMemoryBlocks[index].freeCount < 0) break;
        if (index == FX_MEMORY_BLOCK_COUNT) {
            Com_DPrintf("^1Out of effects memory!\n");
            return NULL;
        }
        block = &fxMemoryBlocks[index];
        block->freeCount = fxMemorySlots[type];
        block->type = type;
        unsigned char *data = (unsigned char *)&fxMemoryStorage[index];
        for (unsigned int i = 0; i != fxMemorySlots[type]; ++i) {
            void *next = i + 1 != fxMemorySlots[type] ? data + (i + 1) * size : NULL;
            memcpy(data + i * size, &next, sizeof(next));
        }
        block->freeList = data;
        block->next = head;
        block->previous = NULL;
        if (head) head->previous = block;
        fxMemoryHeads[type] = block;
        unsigned int highWater = (index + 1) * FX_MEMORY_WINDOWS_BLOCK_SIZE;
        if (fxMemoryHighWater < highWater) fxMemoryHighWater = highWater;
    }
    void *item = block->freeList;
    memcpy(&block->freeList, item, sizeof(block->freeList));
    --block->freeCount;
    memset(item, 0, size);
    return item;
}

void FxMem_Free(void *pointer)
{
    if (!pointer) return;
    uintptr_t offset = (uintptr_t)pointer - (uintptr_t)fxMemoryStorage;
    size_t index = offset / sizeof(FxMemoryStorage);
    assert(index < FX_MEMORY_BLOCK_COUNT);
    FxMemoryBlock *block = &fxMemoryBlocks[index];
    FxMemoryType type = block->type;
    assert(block->freeCount >= 0);
    assert(offset % sizeof(FxMemoryStorage) % fxMemorySizes[type] == 0);
    assert(offset % sizeof(FxMemoryStorage) / fxMemorySizes[type] < fxMemorySlots[type]);
    memcpy(pointer, &block->freeList, sizeof(block->freeList));
    block->freeList = pointer;
    ++block->freeCount;
    FxMemoryBlock *head = fxMemoryHeads[type];
    if (!head->freeList) {
        FxMem_MoveToFront(block, type);
    } else if (block->freeCount >= fxMemorySlots[type]) {
        if (block == head) {
            if (!block->next || !block->next->freeCount) return;
            fxMemoryHeads[type] = block->next;
        }
        FxMem_Unlink(block);
        block->freeCount = -1;
    }
}
#endif

void FxMem_Init(void)
{
#if COD2_APPLE_SDK
    memset(fxMemoryHeads, 0, sizeof(fxMemoryHeads));
    memset(fxMemoryBlocks, 0, sizeof(fxMemoryBlocks));
    for (unsigned int i = 0; i != FX_MEMORY_BLOCK_COUNT; ++i)
        fxMemoryBlocks[i].freeCount = -1;
    fxMemoryHighWater = 0;
#endif
}
