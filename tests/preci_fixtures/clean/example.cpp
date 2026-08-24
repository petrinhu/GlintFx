// SPDX-License-Identifier: AGPL-3.0-or-later
//
// example.cpp - clean control fixture for tools/preci.sh --selftest
// (FUND-4). Deliberately self-contained (no glintfx headers): the
// selftest proves the format/tidy/cppcheck STAGES, not the project
// build, so this file only needs the standard library. Never break
// this file on purpose; it exists to stay green.

#include <cstdio>

namespace {

int add(int a, int b) { return a + b; }

} // namespace

int main() {
    std::printf("%d\n", add(2, 3));
    return 0;
}
