void MatchSummary_Frame(int rawMsec, int frameBudgetMsec, int capFPS);
#define main original_main
#include "timing.c"
#undef main

static int matchFrames;
void MatchSummary_Frame(int rawMsec, int frameBudgetMsec, int capFPS)
{
    static const struct { int rawMsec, budgetMsec, capFPS; } samples[] = {
        {8, 8, 125}, {4, 4, 250}, {3, 3, 333},
#if defined(COD2_CODX) && COD2_CODX
        {4, 4, 250},
#else
        {3, 3, 333},
#endif
        {3, 3, 333}, {14, 3, 333}
    };
    /* Four 1,000-frame runs, followed by the two renderer-hitch frames. */
    assert(matchFrames < 4002);
    int sample = matchFrames < 4000 ? matchFrames / 1000 : 4 + matchFrames - 4000;
    assert(rawMsec == com_frameTime - com_lastFrameTime);
    assert(com_frameTime == clockMsec);
    assert(rawMsec == samples[sample].rawMsec);
    assert(frameBudgetMsec == samples[sample].budgetMsec);
    assert(capFPS == samples[sample].capFPS);
    ++matchFrames;
}

int main(void)
{
    int result = original_main();
    assert(matchFrames == 4002);
    return result;
}
