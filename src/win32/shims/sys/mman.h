#ifndef WIN32_SHIM_SYS_MMAN_H
#define WIN32_SHIM_SYS_MMAN_H
/* The POSIX memory calls the engine's diagnostics use. src/win32/win_system.c
 * implements them, away from the Win32 names the engine headers redeclare. */
#include <stddef.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_PRIVATE 2
#define MAP_ANON 0x20
#define MAP_FAILED ((void *)-1)

void *Win_MapPages(size_t length, int prot);
int Win_UnmapPages(void *address);
int Win_ProtectPages(void *address, size_t length, int prot);

#define mmap(address, length, prot, flags, fd, offset) Win_MapPages((length), (prot))
#define munmap(address, length) Win_UnmapPages((address))
#define mprotect(address, length, prot) Win_ProtectPages((address), (length), (prot))

#ifndef getpagesize
#define getpagesize() 4096
#endif
#endif
