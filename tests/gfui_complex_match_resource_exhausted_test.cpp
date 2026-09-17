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

// --- case 4: the allocation fails at a MIDDLE compound (index strictly
// between 0 and selector.rest.size()), found by mutation testing
// during this fatia's own review to be a gap the three cases above
// never close.
//
// Every case above uses a selector with `selector.rest.size() == 0`
// ("div", "div:not(.hidden)"): ONE compound, so `resource_exhausted`
// can only ever be produced for the frame at index 0. complex_match.cpp's
// own while-loop first-visit branch has TWO ways to turn a frame's
// `local == resource_exhausted` into the final answer: the explicit
// check right before "rejeicao vence adiamento"
// (`if (top.local == match_verdict::resource_exhausted) { pending = ...;
// }`, complex_match.cpp around line 232) and, separately, the
// `top.index == 0` chain-exhausted fallback a few lines below it
// (`pending = top.local;`) - and for index 0 BOTH branches agree, so
// removing the FIRST one is invisible: the second one still forwards
// the same value. A mutation-tested copy of this file's own tree
// (GODS_LAWS.md L-27) proved this directly: deleting the explicit
// check left this file's own case 1-3 at 3/3 PASS, RC=0 - the mutation
// SURVIVED. The other propagation point, `combine()` (complex_match.cpp
// lines 107-121, called only from the "have_pending" branch around
// line 268), is never even REACHED by a one-compound selector: combine()
// only runs once a candidate frame has already popped with a real
// verdict for `top` to fold in, which needs a SECOND frame on the
// stack.
//
// This case selects the shape the reviewer's own dossier note names,
// ".a :not(.x) .c" (the SAME three-compound shape gfui_complex_match_
// test.cpp's own "body :not(table) a" dossier case already proves is
// valid grammar - a compound made of nothing but a pseudo-function,
// with no type/class of its own, sitting in the MIDDLE position): head
// ".a" (index 0), middle ":not(.x)" (index 1), subject ".c" (index 2,
// `selector.rest.size() == 2`). The tree below makes the MIDDLE node
// the ROOT (`parent = k_no_index`) on purpose: this is what forces
// BOTH propagation points to matter, not just one -
//
//   - the explicit `resource_exhausted` check (line ~232): with it
//     REMOVED, index 1 is neither `rejected` nor `index == 0`, so
//     first-visit execution falls through to "try a candidate"
//     (complex_match.cpp's own `next_candidate()`) - and because the
//     middle node has NO parent, that search finds none, and the
//     "no candidate left" branch a few lines further down sets
//     `pending = match_verdict::rejected`, silently DISCARDING the
//     resource_exhausted local verdict. Removing the check therefore
//     changes this case's own answer from `resource_exhausted` to
//     `rejected` - the mutation is caught.
//   - `combine()` (lines 107-121): WITH the explicit check present,
//     the middle frame still pops with `pending = resource_exhausted`
//     immediately (no candidate search even attempted), and the
//     subject frame (index 2, a plain, real, non-allocating ".c" class
//     match, `local == matched`) is what is left on top of the stack -
//     its own "have_pending" branch calls `combine(top.local ==
//     matched, pending == resource_exhausted)` to fold the middle
//     frame's answer in. A `combine()` that skipped its own
//     `resource_exhausted` check would fall to the plain matched/
//     deferred logic and answer `matched` instead - the mutation is
//     caught too, by the SAME test.
//
// One allocator-forcing shape therefore kills BOTH survivors the
// review found, without needing two separate selector shapes (this
// fatia's own briefing's acceptance criterion (c) asks for exactly
// this when one shape can do it).
GLINTFX_TEST(match_complex_propagates_resource_exhausted_from_an_unreachable_middle_compound) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "match_complex_propagates_resource_exhausted_from_an_unreachable_middle_compound");
        return;
    }

    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    // The MIDDLE node (tested against ":not(.x)") is the tree ROOT -
    // `parent = k_no_index` - so no ancestor exists for a search
    // towards the head compound (".a") to ever find, on purpose (see
    // this case's own header comment above for why that is what makes
    // the explicit resource_exhausted check at complex_match.cpp's own
    // line ~232 observably matter, not just the index==0 fallback).
    arena tree;
    const std::size_t middle_idx = tree.add(entry{.tag = "div",
                                                  .id = "",
                                                  .classes = {},
                                                  .attributes = {},
                                                  .state = glintfx::gfui::gltfx_node_state::none,
                                                  .parent = k_no_index,
                                                  .previous_sibling = k_no_index,
                                                  .next_sibling = k_no_index,
                                                  .child_count = 1,
                                                  .first_child = k_no_index});
    // The SUBJECT node (tested against ".c") is the middle node's own
    // child - a plain, real, non-allocating class match
    // (`local == matched`), never itself at risk of resource_exhausted,
    // exactly what makes it the frame still on top of the stack when
    // combine() is called (this case's own header comment, second
    // bullet).
    const std::size_t subject_idx = tree.add(entry{.tag = "span",
                                                   .id = "",
                                                   .classes = {"c"},
                                                   .attributes = {},
                                                   .state = glintfx::gfui::gltfx_node_state::none,
                                                   .parent = middle_idx,
                                                   .previous_sibling = k_no_index,
                                                   .next_sibling = k_no_index,
                                                   .child_count = 0,
                                                   .first_child = k_no_index});
    tree.entries[middle_idx].first_child = subject_idx;

    const gltfx_node_view subject_node = view(tree, subject_idx);
    const gfss_complex_selector selector = parse_one_complex(".a :not(.x) .c");
    GLINTFX_CHECK_EQ(selector.rest.size(), static_cast<std::size_t>(2));

    const std::size_t calls_before = g_override_new_call_count;
    g_force_alloc_failure = true;
    // The outer match_complex() call's own reserve(3) (selector.rest.
    // size() + 1) succeeds; the NESTED match_complex() call
    // judge_not() drives for ".x" (against the middle/root node) is
    // the second allocation attempt, and that one fails.
    g_calls_to_allow_before_failure = 1;
    const match_verdict verdict = match_complex(selector, subject_node, k_no_scope);
    g_force_alloc_failure = false;

    // Proves the override was actually reached at least twice (outer +
    // nested) before trusting the verdict below.
    GLINTFX_CHECK(g_override_new_call_count - calls_before >= static_cast<std::size_t>(2));
    GLINTFX_CHECK(verdict == match_verdict::resource_exhausted);
}
