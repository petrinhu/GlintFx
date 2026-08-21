#pragma once

#include <string_view>

// check.hpp — assertivas não-fatais do harness próprio (FUND-1).
//
// GLINTFX_CHECK NÃO aborta o caso ao falhar (diferente de assert): o
// caso continua, e a falha é contada. É o harness_main que decide
// PASS/FAIL a partir da contagem ao fim do caso.

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
