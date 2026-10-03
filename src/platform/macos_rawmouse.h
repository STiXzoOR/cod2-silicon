#ifndef COD2_MACOS_RAWMOUSE_H
#define COD2_MACOS_RAWMOUSE_H
#include <stdint.h>
void MacRawMouse_Init(void);
void MacRawMouse_Shutdown(void);
void MacRawMouse_SetActive(int active);
int MacRawMouse_Available(void);
int MacRawMouse_Read(int *dx, int *dy);
uint64_t MacRawMouse_EventCount(void);
#endif
