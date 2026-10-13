/* Fixed libc-random inputs exercise the real challenge creation path. */
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

static int randomInputs[2], randomIndex;
static int ChallengeRand(void)
{
    assert(randomIndex < 2);
    return randomInputs[randomIndex++];
}

#define rand ChallengeRand
#define main ExistingProtocolMain
#include "test_host_protocol.c"
#undef main
#undef rand

int main(void)
{
    static const struct {
        int high, low, time;
        uint32_t expected;
    } cases[] = {
        {0, 0, 1000, UINT32_C(0x000003e8)},
        {0x7fff, 0x7fffffff, 0, UINT32_C(0x0000ffff)},
        {0x8000, 0, 1000, UINT32_C(0x800003e8)},
        {0x7fffffff, 0x7fffffff, 1000, UINT32_C(0x8000fc17)},
        {0x12345678, 0x09abcdef, 1000, UINT32_C(0x5fd3ce07)},
        {0, 0, (-2147483647 - 1), UINT32_C(0x80000000)},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        uint32_t actual;
        reset();
        revision = 0;
        svs.time = cases[i].time;
        randomInputs[0] = cases[i].high;
        randomInputs[1] = cases[i].low;
        randomIndex = 0;
        argc = 3;
        argv[0] = "getchallenge";
        argv[1] = "0";
        argv[2] = digest;
        SV_GetChallenge(remote);
        memcpy(&actual, &svs.challenges[0].challenge, sizeof(actual));
        Verify(randomIndex == 2, "challenge consumes two random values in order");
        Verify(actual == cases[i].expected, "challenge retains original 32-bit shift/xor result");
    }
    printf("challenge random vectors: %d failures\n", failures);
    return failures != 0;
}
