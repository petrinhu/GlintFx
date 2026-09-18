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
#include "gfui/compound_match.hpp"
#include "gfui/match_verdict.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfui_nth_child_oom_test.cpp - NOEXCEPT-ALLOC-B8 fatia F4
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F4",
// ESCOPO.md Decisao 11, 17/09/2026, GODS_LAWS.md L-20/L-22/L-40): TDD
// red/green witness for the fix, AT THE EXACT POINT ESCOPO.md's own
// Decisao 8 (16/09/2026, "a biblioteca nunca mata o processo do
// consumidor") was promised for and, before this fatia, was NOT kept
// - ":nth-child(2n+1)" reached at MATCH TIME, not only at READ time:
// match_compound() (compound_match.cpp, `noexcept`) calls
// attribute_and_structural_selectors_hold() (attribute_match.cpp,
// `noexcept`), which calls structural_functional_holds()
// (structural_match.cpp, `noexcept`), which calls parse_anb()
// (anb_parse.cpp) - and BEFORE this fatia, parse_anb() allocated
// (tokenizer.hpp's own gltfx_gfss_tokenize(), a std::vector grown one
// push_back() at a time), so an unlucky allocation failure while
// MATCHING a structural pseudo-class - never merely while first
// READING one out of a stylesheet - escaped std::bad_alloc from a
// noexcept function four call levels deep and called
// std::terminate(), ending the WHOLE consumer process with no chance
// to react. This file proves that chain no longer allocates at all.
//
// TECHNIQUE (the SAME idiom tests/gfui_complex_match_resource_
// exhausted_test.cpp already establishes for GFUI-VERDICT-RESOURCE-
// EXHAUSTED, cited here by name rather than reinvented): this
// translation unit replaces the global operator new/delete with a
// pass-through to malloc/free that can be armed to fail on demand.
// compound_match.cpp/attribute_match.cpp/structural_match.cpp/
// anb_parse.cpp/selector_parse.cpp all carry NO GLINTFX_API
// (GODS_LAWS.md L-19) - tests/CMakeLists.txt compiles all five a
// SECOND time directly into this executable's own object set, never
// sharing gfui_compound_match_test/gfui_match_struct_test's own
// binaries (each glintfx_add_test() target is its own executable;
// two different global-operator-new overrides in the same one would
// be an ODR collision).
//
// WHY THIS PROVES MORE THAN "IT DID NOT CRASH": match_verdict.hpp's
// own header comment already promises "match_compound() itself...
// NEVER [answers resource_exhausted] - it does not allocate" - before
// this fatia, that promise was FALSE for exactly the selector this
// file exercises (a `pseudo_function` simple selector reaching
// structural_functional_holds()). Case 2 below checks the allocation
// counters directly, not merely that a verdict came back: the correct
// fix removes the allocation entirely, so even with EVERY allocation
// in the whole process forced to fail from the very first call, the
// counters must stay at zero and the verdict must be the ORDINARY,
// correct one - never match_verdict::resource_exhausted, because
// there is nothing left on this path that could ever exhaust a
// resource any more.

namespace {

using glintfx::gfui::gltfx_node_view;
using glintfx::gfui::detail::match_compound;
using glintfx::gfui::detail::match_verdict;
using glintfx::style::detail::gfss_compound_selector;

bool g_force_alloc_failure = false;
std::size_t g_override_new_call_count = 0;

// Same MSVC-ASan-only declare-and-skip valve err_context_test.cpp's
// own oom_forcing_declared_not_applicable() uses (that file's own
// header comment has the full citation) - duplicated here per this
// suite's own established "one instance short of the regra de 3"
// tolerance, the SAME shape gfui_complex_match_resource_exhausted_
// test.cpp already duplicates it under.
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
                 "gfui_nth_child_oom_test: {} declared NOT APPLICABLE under MSVC "
                 "AddressSanitizer (learn.microsoft.com/cpp/sanitizers/asan-known-issues, "
                 "\"Overriding operator new and delete\": ASan's own operator new/delete wins by "
                 "default over any user override linked into the same binary - this file's "
                 "forced-failure override never gets a chance to run, so the assertion this case "
                 "exists to prove would measure nothing)",
                 case_name);
}

// Same helper gfui_compound_match_test.cpp's own parse_one_compound()
// already is, duplicated rather than shared across two test binaries
// (each glintfx_add_test() target is its own executable, harness/ has
// no shared non-header test-only library for this).
[[nodiscard]] gfss_compound_selector parse_one_compound(std::string_view text) {
    const glintfx::style::detail::selector_parse_result result =
        glintfx::style::detail::parse_selector_list(text);
    GLINTFX_CHECK(result.ok);
    GLINTFX_CHECK_EQ(result.value.selectors.size(), static_cast<std::size_t>(1));
    GLINTFX_CHECK(result.value.selectors[0].rest.empty());
    return result.value.selectors[0].head;
}

} // namespace

void *operator new(std::size_t size) {
    ++g_override_new_call_count;
    if (g_force_alloc_failure) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

// --- case 1: reaching this assertion AT ALL already proves the
// process did not std::terminate() - the exact crash this fatia
// exists to remove. Three siblings, node under test at position 1
// (":nth-child(2n+1)" -> a=2,b=1 -> positions 1,3,5... match).
GLINTFX_TEST(match_compound_nth_child_survives_forced_allocation_failure_and_matches) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "match_compound_nth_child_survives_forced_allocation_failure_and_matches");
        return;
    }

    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    // Parsed and VALIDATED (selector_parse.cpp's own attach_anb_
    // validation()) with a HEALTHY allocator, before anything below
    // is armed - the same discipline gfui_complex_match_resource_
    // exhausted_test.cpp's own cases already use.
    const gfss_compound_selector selector = parse_one_compound("div:nth-child(2n+1)");

    arena tree;
    const std::size_t idx0 = tree.add(entry{.tag = "div",
                                            .id = "",
                                            .classes = {},
                                            .attributes = {},
                                            .state = glintfx::gfui::gltfx_node_state::none,
                                            .parent = k_no_index,
                                            .previous_sibling = k_no_index,
                                            .next_sibling = k_no_index,
                                            .child_count = 0,
                                            .first_child = k_no_index});
    const std::size_t idx1 = tree.add(entry{.tag = "div",
                                            .id = "",
                                            .classes = {},
                                            .attributes = {},
                                            .state = glintfx::gfui::gltfx_node_state::none,
                                            .parent = k_no_index,
                                            .previous_sibling = idx0,
                                            .next_sibling = k_no_index,
                                            .child_count = 0,
                                            .first_child = k_no_index});
    const std::size_t idx2 = tree.add(entry{.tag = "div",
                                            .id = "",
                                            .classes = {},
                                            .attributes = {},
                                            .state = glintfx::gfui::gltfx_node_state::none,
                                            .parent = k_no_index,
                                            .previous_sibling = idx1,
                                            .next_sibling = k_no_index,
                                            .child_count = 0,
                                            .first_child = k_no_index});
    tree.entries[idx0].next_sibling = idx1;
    tree.entries[idx1].next_sibling = idx2;

    const gltfx_node_view node_at_position_1 = view(tree, idx0);

    const std::size_t calls_before = g_override_new_call_count;
    g_force_alloc_failure = true; // EVERY allocation fails, from the very first call
    const match_verdict verdict = match_compound(selector, node_at_position_1);
    g_force_alloc_failure = false;

    // THE CHECK THAT CATCHES THE REAL DEFECT: this fatia's own fix
    // removes the allocation entirely, rather than catching a failure
    // that no longer happens - so the override must NOT have been
    // reached at all on this path.
    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);

    // Reaching this line at all already proves the process did not
    // std::terminate(). The verdict itself must be the ORDINARY,
    // correct answer for position 1 against "2n+1" - never a guessed
    // rejected, and never resource_exhausted (match_verdict.hpp's own
    // header comment: match_compound() "does not allocate", now
    // actually true of this path).
    GLINTFX_CHECK(verdict == match_verdict::matched);
}

// --- case 2: the SAME allocator-forcing, for the node ":nth-child
// (2n+1)" does NOT match (position 2) - proves the fix did not turn
// every answer into a guessed "matched", only removed the allocation.
GLINTFX_TEST(match_compound_nth_child_survives_forced_allocation_failure_and_rejects) {
    if (oom_forcing_declared_not_applicable()) {
        declare_oom_forcing_not_applicable(
            "match_compound_nth_child_survives_forced_allocation_failure_and_rejects");
        return;
    }

    using glintfx::test::fake_arena::arena;
    using glintfx::test::fake_arena::entry;
    using glintfx::test::fake_arena::k_no_index;
    using glintfx::test::fake_arena::view;

    const gfss_compound_selector selector = parse_one_compound("div:nth-child(2n+1)");

    arena tree;
    const std::size_t idx0 = tree.add(entry{.tag = "div",
                                            .id = "",
                                            .classes = {},
                                            .attributes = {},
                                            .state = glintfx::gfui::gltfx_node_state::none,
                                            .parent = k_no_index,
                                            .previous_sibling = k_no_index,
                                            .next_sibling = k_no_index,
                                            .child_count = 0,
                                            .first_child = k_no_index});
    const std::size_t idx1 = tree.add(entry{.tag = "div",
                                            .id = "",
                                            .classes = {},
                                            .attributes = {},
                                            .state = glintfx::gfui::gltfx_node_state::none,
                                            .parent = k_no_index,
                                            .previous_sibling = idx0,
                                            .next_sibling = k_no_index,
                                            .child_count = 0,
                                            .first_child = k_no_index});
    tree.entries[idx0].next_sibling = idx1;

    const gltfx_node_view node_at_position_2 = view(tree, idx1);

    const std::size_t calls_before = g_override_new_call_count;
    g_force_alloc_failure = true;
    const match_verdict verdict = match_compound(selector, node_at_position_2);
    g_force_alloc_failure = false;

    GLINTFX_CHECK_EQ(g_override_new_call_count, calls_before);
    GLINTFX_CHECK(verdict == match_verdict::rejected);

    std::println("gfui_nth_child_oom_test: position-1 matched, position-2 rejected, {} "
                 "allocation attempt(s) reached the override across both cases",
                 g_override_new_call_count);
}
