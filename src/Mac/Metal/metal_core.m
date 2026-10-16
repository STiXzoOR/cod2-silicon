#include "metal_core.h"
#include <stdlib.h>
#include <string.h>

typedef struct Retired {
    uint64_t value;
    MacMetalRetireFn release;
    void *context;
    struct Retired *next;
} Retired;
struct MacMetalRetirement { Retired *head; };

int MacMetal_WriteRegisters(MacMetalFloat4 *bank, uint32_t capacity,
                            uint32_t first, const float *data, uint32_t count)
{
    if (!bank || first > capacity || count > capacity - first || (!data && count))
        return 0;
    if (count)
        memcpy(bank + first, data, (size_t)count * sizeof(*bank));
    return 1;
}
MacMetalRetirement *MacMetalRetirement_Create(void) { return calloc(1, sizeof(MacMetalRetirement)); }
int MacMetalRetirement_Add(MacMetalRetirement *queue, uint64_t value, MacMetalRetireFn release, void *context)
{
    if (!queue || !release)
        return 0;
    Retired *item = malloc(sizeof(*item));
    if (!item)
        return 0;
    *item = (Retired){ value, release, context, queue->head };
    queue->head = item;
    return 1;
}
void MacMetalRetirement_Collect(MacMetalRetirement *queue, uint64_t completed)
{
    if (!queue)
        return;
    Retired **link = &queue->head;
    while (*link) {
        Retired *item = *link;
        if (item->value <= completed) {
            *link = item->next;
            item->release(item->context);
            free(item);
        } else {
            link = &item->next;
        }
    }
}
void MacMetalRetirement_Destroy(MacMetalRetirement *queue)
{
    MacMetalRetirement_Collect(queue, UINT64_MAX);
    free(queue);
}
