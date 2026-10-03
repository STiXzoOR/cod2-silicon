#include "common_types.h"
#include "PC/qcommon/cdkey_hash.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static char sampleKey[33];
void *imp_cl_cdkey = sampleKey;
char *strupr(char *text)
{
    for (char *p = text; *p; ++p)
        *p = (char)toupper((unsigned char)*p);
    return text;
}
#include "cdkey_hash_source.h"
int main(void)
{
    char hash[33];
    strcpy(sampleKey, "abcd-1234");
    CL_BuildMd5StrFromCDKey(hash);
    assert(!strcmp(hash, "77b51bb0130bd4bbbfc94d0c64bd6fdc"));
    strcpy(sampleKey, "0123456789ABCDEF0123456789ABCDEF");
    CL_BuildMd5StrFromCDKey(hash);
    assert(!strcmp(hash, "97306e1d8843c95de2a232634b138d14"));
    puts("online: stock seeded CD-key authorization digest passed");
    return 0;
}
