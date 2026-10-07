#include "mantle_source.h"

void FixtureMantleEnabled(int enabled)
{
    static dvar_t value;
    value.current.enabled = enabled;
    mantle_enable = &value;
}
