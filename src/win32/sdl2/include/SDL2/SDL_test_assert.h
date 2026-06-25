#ifndef SDL_test_assert_h_
#define SDL_test_assert_h_

#include "begin_code.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ASSERT_FAIL     0

#define ASSERT_PASS     1

void SDLTest_Assert(int assertCondition, SDL_PRINTF_FORMAT_STRING const char *assertDescription, ...) SDL_PRINTF_VARARG_FUNC(2);

int SDLTest_AssertCheck(int assertCondition, SDL_PRINTF_FORMAT_STRING const char *assertDescription, ...) SDL_PRINTF_VARARG_FUNC(2);

void SDLTest_AssertPass(SDL_PRINTF_FORMAT_STRING const char *assertDescription, ...) SDL_PRINTF_VARARG_FUNC(1);

void SDLTest_ResetAssertSummary(void);

void SDLTest_LogAssertSummary(void);

int SDLTest_AssertSummaryToTestResult(void);

#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif
