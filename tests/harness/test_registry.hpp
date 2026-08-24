// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>
#include <vector>

// test_registry.hpp - test case registry of glintfx's own harness
// (FUND-1, GODS_LAWS.md L-07: no Catch2/GoogleTest).
//
// Each test TU declares a case with GLINTFX_TEST(name); the macro
// creates a static object whose constructor registers it in the
// global list before main() runs (the classic self-registration idiom
// via static init).

namespace glintfx::test {

struct Case {
    std::string_view name;
    void (*fn)() = nullptr;
};

void register_case(Case c);
[[nodiscard]] const std::vector<Case> &all_cases();

struct CaseRegistrar {
    CaseRegistrar(std::string_view name, void (*fn)());
};

} // namespace glintfx::test

#define GLINTFX_TEST(name)                                                                         \
    static void glintfx_test_fn_##name();                                                          \
    namespace {                                                                                    \
    const ::glintfx::test::CaseRegistrar glintfx_test_reg_##name(#name, &glintfx_test_fn_##name);  \
    }                                                                                              \
    static void glintfx_test_fn_##name()
