#include "common_types.h"
#include <assert.h>
#include <string.h>
#include "mantle_table.h"

int main(void)
{
    MantleAnimTransition rows[8];
    assert(sizeof(s_mantleTrans) == sizeof(rows));
    memcpy(rows, s_mantleTrans, sizeof(rows));
    for (int i = 0; i < 7; ++i) {
        assert(rows[i].upAnimIndex == i + 1);
        assert(rows[i].overAnimIndex == (i < 2 ? 8 : (i < 5 ? 9 : 10)));
        assert(rows[i].height == 57.0f - i * 6.0f);
    }
    assert(rows[7].upAnimIndex == 0 && rows[7].height == 0.0f);
}
