// SPDX-License-Identifier: AGPL-3.0-or-later
#include <print>

#include <glintfx/core/version.hpp>

// main.cpp - proves the REAL glintfx::glintfx alias still links and
// runs correctly even in a tree where the consumer's own `glintfx`
// target (declared in CMakeLists.txt, dummy.cpp) coexists with it
// (FIX-CONSUMO-2, achado QA-3). The configure-time proof is the
// add_library(glintfx ...) call in CMakeLists.txt not erroring; this
// executable is the runtime proof that the rename did not silently
// link the wrong library.

int main() {
    std::println("glintfx {} (name collision consumer)", glintfx::version_string());
    return 0;
}
