// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <span>
#include <string>
#include <string_view>

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
    GLINTFX_CHECK(message.find('5') != std::string::npos);
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

// GODS_LAWS.md L-40, INBOX achado (revisao adversarial de 26/08/2026,
// terceiro achado deste arquivo no mesmo dia, distinto dos dois acima):
// patch_slot()'s FIRST VirtualProtect call - the one that asks for
// write access to an IAT slot BEFORE the hook's pointer is written -
// returned silently on failure. Unlike the two calls
// format_protect_restore_failure() above already cover (both a
// "restore original protection" step, run AFTER the state change that
// matters already happened), this one gates whether the candidate gets
// patched AT ALL: on failure, that candidate simply never appears in
// dll_alloc_hook::patched_count()/matched names, which is
// INDISTINGUISHABLE from "this candidate's name was never found in the
// import table" - exactly the ambiguity GODS_LAWS.md L-40 exists to
// forbid. glintfx_test::detail::format_write_access_failure() closes
// that gap: unlike format_protect_restore_failure() (phase + error
// code only), it also names WHICH matched candidate could not be
// patched, so the message is self-evidently about a name that WAS
// found, never confusable with format_patch_diagnostic()'s "matched
// NONE... searched: ..." message for a name that was never present.
//
// DECLARED SCOPE: same as above - pure formatting, no Windows API, no
// I/O. The real VirtualProtect call site
// (dll_alloc_hook::patch_slot()'s first check) stays Windows-only and
// is reviewed, not exercised, here.

GLINTFX_TEST(format_write_access_failure_names_the_candidate_and_the_os_error) {
    const std::string message =
        glintfx_test::detail::format_write_access_failure("_malloc_base", 5);

    GLINTFX_CHECK(message.find("_malloc_base") != std::string::npos);
    GLINTFX_CHECK(message.find('5') != std::string::npos);
}

GLINTFX_TEST(format_write_access_failure_is_distinguishable_from_name_not_found) {
    // format_patch_diagnostic's own "nothing matched" message (proven
    // in win_dll_alloc_hook_format_test.cpp) is the ONLY other message
    // this hook can print about a candidate that never became a
    // matched slot. The two must never read the same, or a CI log
    // reader cannot tell "the name was absent" apart from "the name
    // was present but VirtualProtect refused write access".
    const std::string write_access_message =
        glintfx_test::detail::format_write_access_failure("malloc", 5);
    const std::array<std::string_view, 0> nothing_matched{};
    const std::string not_found_message =
        glintfx_test::format_patch_diagnostic(std::span<const std::string_view>(nothing_matched));

    GLINTFX_CHECK(write_access_message != not_found_message);
    // The write-access failure message must say the candidate WAS
    // found, never that it was searched-for-and-absent.
    GLINTFX_CHECK(write_access_message.find("NONE") == std::string::npos);
}

GLINTFX_TEST(format_write_access_failure_distinguishes_two_different_candidates) {
    const std::string malloc_message =
        glintfx_test::detail::format_write_access_failure("malloc", 5);
    const std::string malloc_base_message =
        glintfx_test::detail::format_write_access_failure("_malloc_base", 5);

    GLINTFX_CHECK(malloc_message != malloc_base_message);
    GLINTFX_CHECK(malloc_message.find("malloc") != std::string::npos);
    GLINTFX_CHECK(malloc_base_message.find("_malloc_base") != std::string::npos);
}
