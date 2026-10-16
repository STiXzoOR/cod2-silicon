#define MAC_SHIM_IMPLEMENTATION 1
#include "shim_dispatch.h"
#include <stdlib.h>
#include <string.h>
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
#include <CoreFoundation/CoreFoundation.h>
#include "metal_object_index.h"
static CFMutableDictionaryRef recordedIndex;
void *MacShimObjectIndex_Create(void)
{ return CFDictionaryCreateMutable(NULL, 0, NULL, NULL); }
void *MacShimObjectIndex_Get(void *index, const void *key)
{ return (void *)CFDictionaryGetValue(index, key); }
void MacShimObjectIndex_Set(void *index, const void *key, void *value)
{ CFDictionarySetValue(index, key, value); }
void MacShimObjectIndex_Remove(void *index, const void *key)
{ CFDictionaryRemoveValue(index, key); }
void MacShimObjectIndex_Destroy(void *index)
{ CFRelease(index); }
#endif

/* Recorded resource backends survive selection changes. Objects must be
 * unregistered before their addresses can be reused by another backend. */
typedef struct RecordedBackend {
    const void *object;
    int backend;
    struct RecordedBackend *next;
} RecordedBackend;
static RecordedBackend *recorded;
static int selected;
static MacShimBackendOps tables[2];
int MacShim_InstallBackend(unsigned int backend, const MacShimBackendOps *ops)
{
    if (backend > 1 || !ops || !ops->invoke) return 0;
    tables[backend] = *ops;
    return 1;
}
void MacShim_RecordObject(const void *object, int backend)
{
    if (!object || backend < 0 || backend > 1) return;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (recordedIndex) {
        if (CFDictionaryContainsKey(recordedIndex, object)) return;
    } else
#endif
    for (RecordedBackend *item = recorded; item; item = item->next)
        if (item->object == object) return;
    RecordedBackend *item = malloc(sizeof(*item));
    if (!item) abort();
    *item = (RecordedBackend){ object, backend, recorded };
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (!recorded) recordedIndex = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    if (recordedIndex) CFDictionarySetValue(recordedIndex, object, item);
#endif
    recorded = item;
}
void MacShim_ForgetObject(const void *object)
{
    RecordedBackend **link = &recorded;
    while (*link) {
        RecordedBackend *item = *link;
        if (item->object == object) {
            *link = item->next;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
            if (recordedIndex) CFDictionaryRemoveValue(recordedIndex, object);
            if (!recorded && recordedIndex) { CFRelease(recordedIndex); recordedIndex = NULL; }
#endif
            free(item); return;
        }
        link = &item->next;
    }
}
void MacShim_SelectRenderer(int metal) { selected = !!metal; }
int MacShim_IsMetal(void) { return selected; }
int MacShim_Invoke(const char *method, const void *object, const void *const *args, void *result, size_t size)
{
    int backend = selected;
#if defined(COD2_X64) && defined(__APPLE__) && defined(__aarch64__)
    if (recordedIndex) {
        const RecordedBackend *item = object ? CFDictionaryGetValue(recordedIndex, object) : NULL;
        if (item) backend = item->backend;
    } else
#endif
    for (RecordedBackend *item = recorded; item; item = item->next)
        if (item->object == object) { backend = item->backend; break; }
    if (!backend) {
        /* The original typed GL body implements the GL table's continuation.
         * Constructors pin the owning backend; destructors discard the tag. */
        const char *constructor = strchr(method, '_');
        if (object && constructor && !strncmp(method, constructor + 1, (size_t)(constructor - method)))
            MacShim_RecordObject(object, 0);
        if (method[0] == 'Z' && strstr(method, "D0Ev")) MacShim_ForgetObject(object);
    }
    if (!tables[backend].invoke) return 0;
    return tables[backend].invoke(method, object, args, result, size);
}

MacShimBoundaryHooks MacShim_Hooks = { MacShim_Invoke, MacShim_IsMetal };
