#include "macos_system.h"
#include "macos_display.h"
#include <sys/sysctl.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void MacDisplay_GetVideoMemoryInfo(int *, int *);
extern int MacDisplay_GetNumModes(void);
extern uint16_t MacDisplay_GetNthMode(int, int *, int *, int *, int *);
int main(void)
{
    uint64_t memory = 0; size_t size = sizeof(memory);
    assert(sysctlbyname("hw.memsize", &memory, &size, NULL, 0) == 0);
    assert(MacSystem_MemoryBytes() == memory);
    float ghz = MacSystem_CPUFrequencyGHz();
    assert(ghz > 1 && ghz < 10);
    int video, texture;
    MacDisplay_GetVideoMemoryInfo(&video, &texture);
    assert(video >= 512 && texture == video * 1024 * 1024);
    const char **names = MacPlatform_ModeNames();
    int found = 0;
    for (int i = 0; names[i]; ++i) found |= !strcmp(names[i], "1280x720");
    assert(found);
    for (int i = 0; i < MacDisplay_GetNumModes(); ++i) {
        int w,h,d,r; char name[32];
        assert(!MacDisplay_GetNthMode(i,&w,&h,&d,&r));
        snprintf(name,sizeof(name),"%dx%d",w,h); found = 0;
        for (int j = 0; names[j]; ++j) found |= !strcmp(names[j],name);
        assert(found);
    }
    printf("native hardware: %llu MB, %.3f GHz max, %d MB texture budget; all display modes registered\n", (unsigned long long)(memory>>20), ghz, video);
}
