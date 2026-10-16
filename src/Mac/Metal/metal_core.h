#ifndef COD2_METAL_CORE_H
#define COD2_METAL_CORE_H
#include "metal_abi.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct MacMetalGPU MacMetalGPU;
typedef struct MacMetalSubmission MacMetalSubmission;
typedef void (*MacMetalCompletion)(void *context, uint64_t value, int success);
/* All Objective-C pointers are borrowed. Create retains the given device,
 * owns exactly one MTL4 queue/event, and fails on unsupported OS/device. */
MacMetalGPU *MacMetalGPU_Create(void *device);
/* Lifecycle ownership: resize can keep the exact transport across layers. */
MacMetalGPU *MacMetalGPU_Retain(MacMetalGPU *gpu);
void MacMetalGPU_Destroy(MacMetalGPU *gpu);
void *MacMetalGPU_Device(MacMetalGPU *gpu);
void *MacMetalGPU_Queue(MacMetalGPU *gpu);
void *MacMetalGPU_Event(MacMetalGPU *gpu);
void *MacMetalGPU_Compiler(MacMetalGPU *gpu);
uint64_t MacMetalGPU_Submitted(MacMetalGPU *gpu);
uint64_t MacMetalGPU_Completed(MacMetalGPU *gpu);
/* role 0 renderer / role 1 presenter, two retirement slots each. NULL means
 * drop/coalesce this frame, never wait in a client frame. */
MacMetalSubmission *MacMetalGPU_Begin(MacMetalGPU *gpu, unsigned int role);
void *MacMetalGPU_Command(MacMetalSubmission *submission);
/* Retains objects to completion; allocations also join the residency set. */
void MacMetalGPU_Keep(MacMetalSubmission *submission, void *object, int allocation);
uint64_t MacMetalGPU_Commit(MacMetalSubmission *submission, void *drawable,
                            MacMetalCompletion completion, void *context);
void MacMetalGPU_Cancel(MacMetalSubmission *submission);
/* Drain only for screenshots, resize, restart or shutdown. */
int MacMetalGPU_Drain(MacMetalGPU *gpu);

typedef void (*MacMetalRetireFn)(void *);
typedef struct MacMetalRetirement MacMetalRetirement;
MacMetalRetirement *MacMetalRetirement_Create(void);
int MacMetalRetirement_Add(MacMetalRetirement *queue, uint64_t value,
                           MacMetalRetireFn release, void *context);
void MacMetalRetirement_Collect(MacMetalRetirement *queue, uint64_t completed);
void MacMetalRetirement_Destroy(MacMetalRetirement *queue);
#ifdef __cplusplus
}
#endif
#endif
