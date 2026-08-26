// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <span>
#include <string>
#include <string_view>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "harness/win_dll_alloc_hook.hpp"

// win_dll_alloc_hook_format_test.cpp - proves, on every platform this
// project builds on (not just Windows), the diagnostic-formatting
// function harness/win_dll_alloc_hook.hpp gained to fix an INBOX
// finding (GODS_LAWS.md L-40, 26/08/2026, achado 1 - "the gate passes
// but does not say what it verified"): glintfx_test::format_patch_
// diagnostic() must NAME which candidate matched on success, and must
// turn "nothing matched" into a declared failure that lists every
// candidate that WAS searched for - never a silent zero
// indistinguishable from a genuine pass.
//
// DECLARED SCOPE (GODS_LAWS.md L-09/L-20): this function is pure
// formatting - no Windows API call, no I/O - deliberately kept outside
// harness/win_dll_alloc_hook.hpp's `#if defined(_WIN32) && ...` guard so
// this file can prove it red/green on this (Linux) machine. The PE
// import-table walk that calls it (dll_alloc_hook::patch/patch_slot)
// stays Windows-only and is NOT exercised by this file - that half is
// reviewed, not tested here.

GLINTFX_TEST(format_patch_diagnostic_names_every_matched_candidate) {
    const std::array<std::string_view, 2> matched = {"_malloc_base", "malloc"};
    const std::string message =
        glintfx_test::format_patch_diagnostic(std::span<const std::string_view>(matched));

    GLINTFX_CHECK(message.find("_malloc_base") != std::string::npos);
    GLINTFX_CHECK(message.find("malloc") != std::string::npos);
    // Not a declared-failure message when something DID match.
    GLINTFX_CHECK(message.find("NONE") == std::string::npos);
}

GLINTFX_TEST(format_patch_diagnostic_names_the_single_matched_candidate) {
    const std::array<std::string_view, 1> matched = {"_malloc_base"};
    const std::string message =
        glintfx_test::format_patch_diagnostic(std::span<const std::string_view>(matched));

    GLINTFX_CHECK(message.find("_malloc_base") != std::string::npos);
    GLINTFX_CHECK(message.find("NONE") == std::string::npos);
}

GLINTFX_TEST(format_patch_diagnostic_declares_failure_and_lists_the_full_candidate_set) {
    const std::string message =
        glintfx_test::format_patch_diagnostic(std::span<const std::string_view>{});

    // GODS_LAWS.md L-40: absence of a match is a DECLARED failure, not
    // a quiet zero - the message says so explicitly...
    GLINTFX_CHECK(message.find("NONE") != std::string::npos);
    // ...and enumerates the whole closed candidate list that was
    // searched, so a stale list is legible from this line alone.
    for (const std::string_view candidate : glintfx_test::detail::k_candidate_names) {
        GLINTFX_CHECK(message.find(candidate) != std::string_view::npos);
    }
}
