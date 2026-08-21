#include "check.hpp"

#include <print>

namespace glintfx::test {

namespace {

int g_failure_count = 0;

}  // namespace

void record_check_failure(std::string_view file, int line, std::string_view expr) {
    std::println(stderr, "{}:{}: failed: {}", file, line, expr);
    ++g_failure_count;
}

int failure_count() {
    return g_failure_count;
}

void reset_failure_count() {
    g_failure_count = 0;
}

}  // namespace glintfx::test
