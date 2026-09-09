// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/global_catalog.hpp"
#include "platform/wayland/shell_requirements.hpp"

// shell_requirements_test.cpp - WL-WINDOW fatia W-B (docs/plano-w6a-
// janela.md fatia 5): check_shell_requirements() against a global_
// catalog built by hand, no compositor required - shell_requirements.hpp's
// own header comment names why this is honest as a unit test (no
// Wayland type reachable from the code under test). The real bind()
// against a live compositor is fatia 5's own shell_smoke, inside the
// container (GODS_LAWS.md L-09).
//
// global_catalog.cpp is compiled a SECOND time directly into this
// executable's own object set (tests/CMakeLists.txt) - the same
// technique global_catalog_test.cpp already uses for a symbol with no
// GLINTFX_API.

GLINTFX_TEST(shell_requirements_ok_when_compositor_and_shell_present) {
    glintfx::platform::global_catalog catalog;
    GLINTFX_CHECK(catalog.insert(1, "wl_compositor", 6));
    GLINTFX_CHECK(catalog.insert(2, "xdg_wm_base", 6));

    const glintfx::gltfx_rslt<void> result = glintfx::platform::check_shell_requirements(catalog);
    GLINTFX_CHECK(result.has_value());
}

GLINTFX_TEST(shell_requirements_names_missing_compositor) {
    glintfx::platform::global_catalog catalog;
    GLINTFX_CHECK(catalog.insert(2, "xdg_wm_base", 6));

    const glintfx::gltfx_rslt<void> result = glintfx::platform::check_shell_requirements(catalog);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::not_found);
    GLINTFX_CHECK(result.err().rejected_value() == "wl_compositor");
}

GLINTFX_TEST(shell_requirements_names_missing_shell) {
    glintfx::platform::global_catalog catalog;
    GLINTFX_CHECK(catalog.insert(1, "wl_compositor", 6));

    const glintfx::gltfx_rslt<void> result = glintfx::platform::check_shell_requirements(catalog);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.err().code() == glintfx::gltfx_err_code::not_found);
    GLINTFX_CHECK(result.err().rejected_value() == "xdg_wm_base");
}
