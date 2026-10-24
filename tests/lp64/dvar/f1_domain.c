/* Production dvar API checks against Mac 1.3 domain and promotion semantics. */
#define main f1_existing_lifetime_checks
#include "../../platform/dvar_lifetime.c"
#undef main
#include <math.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "FAIL: %s\n", #condition); ++failures; } } while (0)

int main(void)
{
    f1_existing_lifetime_checks();
    const dvar_t *scalar = Dvar_RegisterFloat("f1_scalar", 2, -4, 4, 0);
    Dvar_SetFromString(scalar, "nan");
    CHECK(isnan(scalar->current.value));
    CHECK(isnan(scalar->latched.value));
    Dvar_SetFromString(scalar, "inf");
    CHECK(isnan(scalar->current.value)); /* Out of domain: leave current alone. */
    Dvar_Reset(scalar, DVAR_SOURCE_INTERNAL);
    CHECK(scalar->current.value == 2);

    const dvar_t *vector = Dvar_RegisterVec3("f1_vector", 1, 2, 3, -4, 4, 0);
    Dvar_SetFromString(vector, "nan 3 -2");
    CHECK(isnan(vector->current.vector[0]));
    CHECK(vector->current.vector[1] == 3 && vector->current.vector[2] == -2);

    Dvar_SetCommand("f1_promoted_scalar", "inf");
    scalar = Dvar_RegisterFloat("f1_promoted_scalar", 2, -4, 4, 0);
    CHECK(scalar->current.value == 4);
    Dvar_SetCommand("f1_promoted_vector", "nan inf -inf");
    vector = Dvar_RegisterVec3("f1_promoted_vector", 1, 2, 3, -4, 4, 0);
    CHECK(isnan(vector->current.vector[0]));
    CHECK(vector->current.vector[1] == 4 && vector->current.vector[2] == -4);
    const char *cities[] = {"bristol", "brighton", "berlin", NULL};
    const dvar_t *choice = Dvar_RegisterEnum("f1_enum", cities, 2, 0);
    Dvar_SetFromString(choice, "BRIGHTON");
    CHECK(choice->current.integer == 1);
    Dvar_SetFromString(choice, "bri");
    CHECK(choice->current.integer == 2); /* A nonnumeric prefix is invalid. */
    Dvar_SetFromString(choice, "0");
    CHECK(choice->current.integer == 0);
    Dvar_SetFromString(choice, "");
    CHECK(choice->current.integer == 0);
    const char *numbered[] = {"12long", "other", NULL};
    choice = Dvar_RegisterEnum("f1_numbered_enum", numbered, 1, 0);
    Dvar_SetFromString(choice, "12");
    CHECK(choice->current.integer == 0); /* Out-of-range digits still try prefix. */
    const dvar_t *latched = Dvar_RegisterInt("f1_latch", 1, 0, 9, DVAR_FLAG_LATCH);
    Dvar_SetIntFromSource(latched, 7, DVAR_SOURCE_EXTERNAL);
    CHECK(latched->current.integer == 1 && latched->latched.integer == 7);
    CHECK(!latched->modified);
    CHECK(Dvar_RegisterInt("f1_latch", 3, 0, 9, DVAR_FLAG_LATCH) == latched);
    CHECK(latched->current.integer == 7 && latched->latched.integer == 7);
    CHECK(latched->reset.integer == 1 && latched->modified);
    Dvar_Reset(latched, DVAR_SOURCE_EXTERNAL);
    CHECK(latched->current.integer == 7 && latched->latched.integer == 1);
    Dvar_SetInt(latched, 7);
    CHECK(latched->latched.integer == 7); /* Same current cancels pending reset. */
    Dvar_SetCommand("f1_readonly", "8");
    const dvar_t *readonly = Dvar_RegisterInt("f1_readonly", 2, 0, 9, DVAR_FLAG_READONLY);
    CHECK(readonly->current.integer == 2);
    Dvar_SetIntFromSource(readonly, 6, DVAR_SOURCE_EXTERNAL);
    CHECK(readonly->current.integer == 2);
    Dvar_SetInt(readonly, 6);
    CHECK(readonly->current.integer == 6);
    Dvar_Shutdown();
    CHECK(allocations == 0);
    if (!failures)
        puts("F1 dvar domain/promotion: Mac domain, enum, latch and promotion semantics pass");
    return failures != 0;
}
