#pragma once

#include <string_view>

// check.hpp - non-fatal assertions of the in-house harness (FUND-1).
//
// GLINTFX_CHECK does NOT abort the case on failure (unlike assert): the
// case continues, and the failure is counted. It is harness_main that
// decides PASS/FAIL from the count at the end of the case.

namespace glintfx::test {

void record_check_failure(std::string_view file, int line, std::string_view expr);
[[nodiscard]] int failure_count();
void reset_failure_count();

}  // namespace glintfx::test

#define GLINTFX_CHECK(cond)                                                 \
    do {                                                                    \
        if (!(cond)) {                                                      \
            ::glintfx::test::record_check_failure(__FILE__, __LINE__, #cond); \
        }                                                                   \
    } while (false)

#define GLINTFX_CHECK_EQ(a, b) GLINTFX_CHECK((a) == (b))
