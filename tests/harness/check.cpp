// SPDX-License-Identifier: AGPL-3.0-or-later
#include "check.hpp"

#include <print>

namespace glintfx::test {

namespace {

int g_failure_count = 0;

} // namespace

void record_check_failure(std::string_view file, int line, std::string_view expr) {
    std::println(stderr, "{}:{}: failed: {}", file, line, expr);
    ++g_failure_count;
}

int failure_count() { return g_failure_count; }

void reset_failure_count() { g_failure_count = 0; }

void ensure_check_failure_was_recorded() {
    if (failure_count() == 0) {
        record_check_failure(
            __FILE__, __LINE__,
            "case_check_failed reached a catch clause without record_check_failure() "
            "having run first (harness invariant violated)");
    }
}

} // namespace glintfx::test
