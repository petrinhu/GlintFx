// SPDX-License-Identifier: AGPL-3.0-or-later
#include "test_registry.hpp"

namespace glintfx::test {

namespace {

std::vector<Case> &mutable_cases() {
    static std::vector<Case> cases;
    return cases;
}

} // namespace

void register_case(Case c) { mutable_cases().push_back(c); }

const std::vector<Case> &all_cases() { return mutable_cases(); }

CaseRegistrar::CaseRegistrar(std::string_view name, void (*fn)()) { register_case(Case{name, fn}); }

} // namespace glintfx::test
