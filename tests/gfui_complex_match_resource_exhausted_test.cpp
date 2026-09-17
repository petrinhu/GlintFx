// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <string_view>

#include <glintfx/gfui/node_view.hpp>

#include "fake/fake_arena_tree.hpp"
#include "gfss/selector_ast.hpp"
#include "gfss/selector_parse.hpp"
#include "gfui/complex_match.hpp"
#include "gfui/match_verdict.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_complex_match_resource_exhausted_test.cpp -
// GFUI-VERDICT-RESOURCE-EXHAUSTED (TODO.md, GODS_LAWS.md
// L-20/L-22/L-40; ESCOPO.md "Ordem de produto de 15/09/2026",
// Decisao 8; docs/plano-w6-folha-de-estilo.md SS11): TDD red/green
// witness for the fix - match_complex() (complex_match.cpp) is
// `noexcept` AND allocates its own explicit std::vector<frame> stack;
// BEFORE this fatia, letting that allocation fail meant an escaped
// std::bad_alloc, which calls std::terminate() on a noexcept function
// - the CONSUMER's whole process dies, with no chance to react. This
// file proves the allocation failure is caught INSIDE match_complex()
// (and propagated honestly through deferred_simple_match.cpp's own
// judge_not(), for the ":not()" recursive case) and turned into
// match_verdict::resource_exhausted instead - never a guessed
// matched/rejected/deferred (GODS_LAWS.md L-40's own "adiamento vence
// acerto silencioso", extended here to "exaustao vence tudo").
//
// FORCED, NOT INSPECTED (same technique tests/err_context_test.cpp
// already uses for CORE-ERROR's own OOM-degradation cases, cited by
// name in this fatia's own briefing so as not to invent a second
// mechanism): this translation unit replaces the global throwing
// operator new/delete with a pass-through to malloc/free that can be
// armed to fail after a chosen number of calls. NO DLL-CROSSING GAP
// TO CLOSE HERE, unlike err_context_test.cpp's own elaborate
// win_dll_alloc_hook.hpp machinery: complex_match.cpp and
// deferred_simple_match.cpp define glintfx::gfui::detail::
// match_complex()/judge_deferred_simple_selectors() with NO
// GLINTFX_API (GODS_LAWS.md L-19, deliberately - see complex_match.
// hpp's own header comment) - tests/CMakeLists.txt compiles BOTH a
// SECOND time directly into THIS test's own executable (the same
// second-compile gfui_complex_match_test.cpp's own CMakeLists.txt
// comment already documents), never linking the hidden copy inside
// glintfx.dll/.so. Every allocation this file's production code needs
// therefore happens inside THIS SAME binary image, on every platform
// - the override below reaches all of it directly, by ordinary
// strong-symbol linker precedence, with nothing left for a DLL-side
// hook to patch around.
//
// MSVC ASan IS STILL A REAL GAP, THE SAME ONE, FOR A DIFFERENT REASON:
// err_context_test.cpp's own "ASAN-OOM-FORCE-GAP" comment cites
// Microsoft's own current documentation
// (learn.microsoft.com/cpp/sanitizers/asan-known-issues, "Overriding
// operator new and delete") that ASan's OWN operator new/delete wins
// by default over ANY user override linked into the same MSVC binary,
// static or shared alike - a property of linking the ASan runtime at
// all, NOT of the DLL-crossing problem this file has no other trace
// of. The three cases below therefore still need the same declared
// downgrade on that one leg (`windows-sanitizer` CI job), or they
// would measure nothing there while claiming green.

namespace {

using glintfx::gfui::gltfx_node_view;
using glintfx::gfui::detail::match_complex;
using glintfx::gfui::detail::match_verdict;
using glintfx::style::detail::gfss_complex_selector;

bool g_force_alloc_failure = false;

// How many heap allocations this override still lets through, once
// armed, before the NEXT one is forced to fail - lets a case choose
// WHICH allocation fails (e.g. the outer match_complex() call's own
// std::vector<frame>::reserve() succeeding while a nested one, reached
// through ":not()"'s own recursive match_complex() call, is the one
// that fails), never just "the very next allocation anywhere".
std::size_t g_calls_to_allow_before_failure = 0;

// Counts every call to this TU's own operator new/new(nothrow)
// overrides below, armed or not - lets a case prove the override was
// actually REACHED before trusting what the library did in response
// (same "DECLARED COVERAGE" reasoning err_context_test.cpp's own
// header comment gives for its own identical counter).
std::size_t g_override_new_call_count = 0;

[[nodiscard]] bool should_fail_this_allocation() noexcept {
    if (!g_force_alloc_failure) {
        return false;
    }
    if (g_calls_to_allow_before_failure > 0) {
        --g_calls_to_allow_before_failure;
        return false;
    }
    return true;
}

// Same MSVC-ASan-only declare-and-skip valve err_context_test.cpp's
// own oom_forcing_declared_not_applicable() uses, duplicated here
// (GODS_LAWS.md L-33's own "regra de 3" - this is the SECOND
// occurrence of this exact shape in this suite, one short of the bar
// CONTRACT.md sets for pulling it into a shared header) rather than
// invented fresh: the reasoning is identical (Microsoft's own current
// documentation, cited in full in err_context_test.cpp), and this
// file's own header comment above explains why it still applies
// despite having no DLL-crossing gap of its own.
[[nodiscard]] bool oom_forcing_declared_not_applicable() {
#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)
    return true;
#elif defined(_WIN32)
    return false;
#else
    return std::getenv("GLINTFX_RESOURCE_EXHAUSTED_TEST_FORCE_OOM_NOT_APPLICABLE") != nullptr;
#endif
}

void declare_oom_forcing_not_applicable(std::string_view case_name) {
    std::println(stderr,
                 "gfui_complex_match_resource_exhausted_test: {} declared NOT APPLICABLE under "
                 "MSVC AddressSanitizer (learn.microsoft.com/cpp/sanitizers/asan-known-issues, "
                 "\"Overriding operator new and delete\": ASan's own operator new/delete wins by "
                 "default over any user override linked into the same binary - this file's "
                 "forced-failure override never gets a chance to run, so the assertion this case "
                 "exists to prove would measure nothing)",
                 case_name);
}

// Parses `text` as exactly one selector (no comma) - same helper
// gfui_complex_match_test.cpp's own parse_one_complex() already is,
// duplicated rather than shared across two test binaries (each
// glintfx_add_test() target is its own executable, harness/ has no
// shared non-header test-only library for this).
[[nodiscard]] gfss_complex_selector parse_one_complex(std::string_view text) {
    const glintfx::style::detail::selector_parse_result result =
        glintfx::style::detail::parse_selector_list(text);
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
    return result.value.selectors[0];
}

constexpr gltfx_node_view k_no_scope{};

} // namespace

void *operator new(std::size_t size) {
    ++g_override_new_call_count;
    if (should_fail_this_allocation()) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
    ++g_override_new_call_count;
    if (should_fail_this_allocation()) {
        return nullptr;
    }
    return std::malloc(size);
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

// --- case 1: the simplest possible allocation - one compound, no
// combinators, no `:not()` - fails and the process does NOT terminate.
GLINTFX_TEST(match_complex_returns_resource_exhausted_instead_of_terminating) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "match_complex_returns_resource_exhausted_instead_of_terminating");
        return;
    }

    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    arena tree;
    const std::size_t idx = tree.add(entry{.tag = "div",
                                           .id = "",
                                           .classes = {},
                                           .attributes = {},
                                           .state = glintfx::gfui::gltfx_node_state::none,
                                           .parent = k_no_index,
                                           .previous_sibling = k_no_index,
                                           .next_sibling = k_no_index,
                                           .child_count = 0,
                                           .first_child = k_no_index});
    const gltfx_node_view node = view(tree, idx);
    const gfss_complex_selector selector = parse_one_complex("div");

    // Everything above (tree, entry, selector parsing) runs with a
    // HEALTHY allocator - only the call under test is armed, the same
    // discipline err_context_test.cpp's own cases use.
    const std::size_t calls_before = g_override_new_call_count;
    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0; // the very first allocation fails
    const match_verdict verdict = match_complex(selector, node, k_no_scope);
    g_force_alloc_failure = false;

    // Proves the override was actually REACHED before trusting the
    // result below - a failure HERE means this run could not force
    // the allocation to fail at all, not that match_complex()
    // mishandled a real one.
    GLINTFX_CHECK(g_override_new_call_count > calls_before);

    // THE CHECK THAT CATCHES THE REAL DEFECT: reaching this line at
    // all already proves the process did not std::terminate() (an
    // escaped std::bad_alloc from this noexcept function would have
    // ended the whole test binary before this assertion could ever
    // run - the exact crash GFUI-VERDICT-RESOURCE-EXHAUSTED exists to
    // remove). The verdict itself must be the honest "could not
    // finish", never a guessed matched/rejected/deferred.
    GLINTFX_CHECK(verdict == match_verdict::resource_exhausted);
}

// --- case 2: one forced failure does not corrupt anything - a SECOND
// call, with the allocator healthy again, answers the real question
// correctly.
GLINTFX_TEST(match_complex_recovers_and_matches_normally_after_a_forced_failure) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "match_complex_recovers_and_matches_normally_after_a_forced_failure");
        return;
    }

    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    arena tree;
    const std::size_t idx = tree.add(entry{.tag = "div",
                                           .id = "",
                                           .classes = {},
                                           .attributes = {},
                                           .state = glintfx::gfui::gltfx_node_state::none,
                                           .parent = k_no_index,
                                           .previous_sibling = k_no_index,
                                           .next_sibling = k_no_index,
                                           .child_count = 0,
                                           .first_child = k_no_index});
    const gltfx_node_view node = view(tree, idx);
    const gfss_complex_selector selector = parse_one_complex("div");

    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 0;
    const match_verdict degraded = match_complex(selector, node, k_no_scope);
    g_force_alloc_failure = false;
    GLINTFX_CHECK(degraded == match_verdict::resource_exhausted);

    // No override armed this time - a plain, healthy call.
    const match_verdict recovered = match_complex(selector, node, k_no_scope);
    GLINTFX_CHECK(recovered == match_verdict::matched);
}

// --- case 3: the allocation that fails is NOT the outer call's own
// stack, but a NESTED one - reached through ":not()"'s own recursive
// match_complex() call (deferred_simple_match.cpp's own judge_not()) -
// proving the honest answer propagates through judge_not(), then
// judge_deferred_simple_selectors(), then complex_match.cpp's own
// combine()/first-visit branch, all the way out.
GLINTFX_TEST(match_complex_propagates_resource_exhausted_from_a_nested_not_argument) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "match_complex_propagates_resource_exhausted_from_a_nested_not_argument");
        return;
    }

    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    // "div:not(.hidden)" against a plain <div>, no "hidden" class: one
    // compound (no combinator, `rest` empty), so the OUTER
    // match_complex() call's own std::vector<frame> needs exactly ONE
    // allocation (reserve(1)). match_compound() marks the compound
    // `deferred` (":not()" is owned by this fatia, never by
    // compound_match.cpp itself), so judge_deferred_simple_selectors()
    // -> judge_not() recurses BACK into match_complex() for ".hidden"
    // alone - a SEPARATE std::vector<frame>, a SECOND, independent
    // allocation. Allowing exactly one call through before failing
    // lets the outer allocation succeed and forces the INNER one.
    arena tree;
    const std::size_t idx = tree.add(entry{.tag = "div",
                                           .id = "",
                                           .classes = {},
                                           .attributes = {},
                                           .state = glintfx::gfui::gltfx_node_state::none,
                                           .parent = k_no_index,
                                           .previous_sibling = k_no_index,
                                           .next_sibling = k_no_index,
                                           .child_count = 0,
                                           .first_child = k_no_index});
    const gltfx_node_view node = view(tree, idx);
    const gfss_complex_selector selector = parse_one_complex("div:not(.hidden)");

    const std::size_t calls_before = g_override_new_call_count;
    g_force_alloc_failure = true;
    g_calls_to_allow_before_failure = 1; // the outer reserve() succeeds; the nested one fails
    const match_verdict verdict = match_complex(selector, node, k_no_scope);
    g_force_alloc_failure = false;

    // Proves the override intercepted AT LEAST the two allocations
    // this case's own header comment predicts (outer + nested) - a
    // count of exactly one here would mean the nested call never even
    // tried to allocate, i.e. this case measured nothing.
    GLINTFX_CHECK(g_override_new_call_count - calls_before >= static_cast<std::size_t>(2));
    GLINTFX_CHECK(verdict == match_verdict::resource_exhausted);
}
