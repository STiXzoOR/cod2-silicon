#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include "mantle_source.h"
int main(void)
{
    const MantleAnimTransition *table = (const MantleAnimTransition *)s_mantleTrans;
    for (int i = 0; i < 7; ++i) {
        assert(table[i].upAnimIndex == i + 1);
        assert(table[i].height == 57.0f - 6.0f * i);
        assert(table[i].overAnimIndex == (i < 2 ? 8 : i < 5 ? 9 : 10));
    }
    puts("online: native mantle table keeps its 12-byte records");
    return 0;
}
