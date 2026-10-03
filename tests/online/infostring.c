#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void Com_Error(int code, const char *format, ...)
{
    (void)code;
    (void)format;
    abort();
}
#include "infostring_source.h"
static void CheckRemovals(void (*removeKey)(char *, const char *))
{
    char info[1024] = "\\protocol\\118\\name\\macport-test\\snaps\\40\\cl_maxpackets\\125";
    removeKey(info, "protocol");
    assert(!strcmp(info, "\\name\\macport-test\\snaps\\40\\cl_maxpackets\\125"));
    removeKey(info, "snaps");
    assert(!strcmp(info, "\\name\\macport-test\\cl_maxpackets\\125"));
    removeKey(info, "absent");
    assert(!strcmp(info, "\\name\\macport-test\\cl_maxpackets\\125"));
    removeKey(info, "cl_maxpackets");
    assert(!strcmp(info, "\\name\\macport-test"));
    removeKey(info, "name");
    assert(!info[0]);
}
int main(void)
{
    CheckRemovals(Info_RemoveKey);
    CheckRemovals(Info_RemoveKey_Big);
    puts("online: overlapping userinfo removals passed");
    return 0;
}
