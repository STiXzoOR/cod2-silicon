#ifndef COD2_MACOS_SYSTEM_H
#define COD2_MACOS_SYSTEM_H

#include <stdint.h>

uint64_t MacSystem_Nanoseconds(void);
char *MacSystem_HomePath(void);
uint64_t MacSystem_MemoryBytes(void);
float MacSystem_CPUFrequencyGHz(void);
uintptr_t MacSystem_ImageOffset(const void *address);
int MacSystem_CaptureStack(void **frames, int count, int skip);

#endif
