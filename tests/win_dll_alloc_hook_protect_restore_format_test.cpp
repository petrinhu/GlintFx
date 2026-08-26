// SPDX-License-Identifier: AGPL-3.0-or-later
#include <string>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "harness/win_dll_alloc_hook.hpp"

// win_dll_alloc_hook_protect_restore_format_test.cpp - proves, on every
// platform this project builds on (not just Windows), the diagnostic-
// formatting function harness/win_dll_alloc_hook.hpp gained to fix an
// INBOX finding (GODS_LAWS.md L-40, 26/08/2026, achado 2 - "restoring
// memory protection without checking the return value"):
// glintfx_test::detail::format_protect_restore_failure() must render a
// message naming the phase and the OS error code, so a failed
// VirtualProtect-restore is visible instead of swallowed.
//
// DECLARED SCOPE (GODS_LAWS.md L-09/L-20): this function is pure
// formatting - no Windows API call, no I/O - deliberately kept outside
// harness/win_dll_alloc_hook.hpp's `#if defined(_WIN32) && ...` guard so
// this file can prove it red/green on this (Linux) machine. The two
// VirtualProtect call sites that now check their return value and call
// this function (dll_alloc_hook::patch_slot()/restore()) stay
// Windows-only and are NOT exercised by this file - that half is
// reviewed, not tested here.

GLINTFX_TEST(format_protect_restore_failure_names_the_phase_and_the_os_error) {
    const std::string message = glintfx_test::detail::format_protect_restore_failure("restore", 5);

    GLINTFX_CHECK(message.find("restore") != std::string::npos);
    GLINTFX_CHECK(message.find("5") != std::string::npos);
}

GLINTFX_TEST(format_protect_restore_failure_distinguishes_the_two_call_sites) {
    const std::string patch_phase_message =
        glintfx_test::detail::format_protect_restore_failure("patch", 0);
    const std::string restore_phase_message =
        glintfx_test::detail::format_protect_restore_failure("restore", 0);

    GLINTFX_CHECK(patch_phase_message != restore_phase_message);
    GLINTFX_CHECK(patch_phase_message.find("patch") != std::string::npos);
    GLINTFX_CHECK(restore_phase_message.find("restore") != std::string::npos);
}
