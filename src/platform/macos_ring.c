#if defined(COD2_X64)
#include "common_types.h"
#include <stdlib.h>
#include <string.h>

void CCircularBuffer_CCircularBuffer(CCircularBuffer *self) { memset(self, 0, sizeof(*self)); }
void CCircularBuffer_Reset(CCircularBuffer *self) { self->readOffset = self->lastReadSize = self->writeOffset = 0; }
UInt32 CCircularBuffer_ReadPtrSize(const CCircularBuffer *self)
{
    if (!self->capacity)
        return 0;
    UInt32 position = self->readOffset + self->lastReadSize;
    if (position >= self->capacity)
        position = 0;
    return (position <= self->writeOffset ? self->writeOffset : self->capacity) - position;
}
void *CCircularBuffer_ReadPtr(CCircularBuffer *self, UInt32 *size)
{
    UInt32 available = CCircularBuffer_ReadPtrSize(self);
    if (!self->capacity) { *size = 0; return NULL; }
    self->readOffset += self->lastReadSize;
    if (self->readOffset >= self->capacity)
        self->readOffset = 0;
    self->lastReadSize = *size < available ? *size : available;
    *size = self->lastReadSize;
    return *size ? self->buffer + self->readOffset : NULL;
}
void ZN15CCircularBufferD1Ev(CCircularBuffer *self) { free(self->buffer); memset(self, 0, sizeof(*self)); }
void CCircularBuffer_Alloc(CCircularBuffer *self, UInt32 capacity)
{
    ZN15CCircularBufferD1Ev(self);
    self->buffer = malloc(capacity);
    self->capacity = self->buffer ? capacity : 0;
}
void CCircularBuffer_Write(const CCircularBuffer *object, const void *buffer, UInt32 *size)
{
    CCircularBuffer *self = (CCircularBuffer *)object;
    if (!self->capacity) { *size = 0; return; }
    if (self->writeOffset < self->readOffset) {
        UInt32 available = self->readOffset - self->writeOffset;
        if (*size > available)
            *size = available;
        if (*size) {
            memcpy(self->buffer + self->writeOffset, buffer, *size);
            self->writeOffset += *size;
            if (self->writeOffset >= self->capacity)
                self->writeOffset = 0;
        }
        return;
    }

    UInt32 first = self->capacity - self->writeOffset;
    if (first > *size)
        first = *size;
    if (first) {
        memcpy(self->buffer + self->writeOffset, buffer, first);
        self->writeOffset += first;
        if (self->writeOffset >= self->capacity)
            self->writeOffset = 0;
    }
    if (first < *size) {
        UInt32 second = *size - first;
        UInt32 available = self->readOffset - self->writeOffset;
        if (second > available)
            second = available;
        if (second) {
            memcpy(self->buffer + self->writeOffset, (const char *)buffer + first, second);
            self->writeOffset += second;
            if (self->writeOffset >= self->capacity)
                self->writeOffset = 0;
        }
        *size = first + second;
    }
}
#else
#include "common_types.h"
#include <stdlib.h>
#include <string.h>

void CCircularBuffer_CCircularBuffer(CCircularBuffer *self) { memset(self, 0, sizeof(*self)); }
void CCircularBuffer_Reset(CCircularBuffer *self) { self->readOffset = self->lastReadSize = self->writeOffset = 0; }
UInt32 CCircularBuffer_ReadPtrSize(const CCircularBuffer *self)
{
    if (!self->capacity)
        return 0;
    UInt32 position = (self->readOffset + self->lastReadSize) % self->capacity;
    return (position <= self->writeOffset ? self->writeOffset : self->capacity) - position;
}
void *CCircularBuffer_ReadPtr(CCircularBuffer *self, UInt32 *size)
{
    UInt32 available = CCircularBuffer_ReadPtrSize(self);
    if (!self->capacity) { *size = 0; return NULL; }
    self->readOffset = (self->readOffset + self->lastReadSize) % self->capacity;
    self->lastReadSize = *size < available ? *size : available;
    *size = self->lastReadSize;
    return *size ? self->buffer + self->readOffset : NULL;
}
void ZN15CCircularBufferD1Ev(CCircularBuffer *self) { free(self->buffer); memset(self, 0, sizeof(*self)); }
void CCircularBuffer_Alloc(CCircularBuffer *self, UInt32 capacity)
{
    ZN15CCircularBufferD1Ev(self);
    self->buffer = malloc(capacity);
    self->capacity = self->buffer ? capacity : 0;
}
void CCircularBuffer_Write(const CCircularBuffer *object, const void *buffer, UInt32 *size)
{
    CCircularBuffer *self = (CCircularBuffer *)object;
    if (!self->capacity) { *size = 0; return; }
    UInt32 used = (self->writeOffset + self->capacity - self->readOffset) % self->capacity;
    UInt32 available = self->capacity - used - 1;
    if (*size > available)
        *size = available;
    UInt32 first = self->capacity - self->writeOffset;
    if (first > *size)
        first = *size;
    memcpy(self->buffer + self->writeOffset, buffer, first);
    memcpy(self->buffer, (const char *)buffer + first, *size - first);
    self->writeOffset = (self->writeOffset + *size) % self->capacity;
}
#endif
