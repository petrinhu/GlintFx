// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <new>
#include <print>
#include <string_view>
#include <vector>

#include <glintfx/gfss/tokenizer.hpp>

#include "gfss/anb_parse.hpp"
#include "gfss/diagnostic_vocabulary.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfss_anb_parse_no_alloc_test.cpp - NOEXCEPT-ALLOC-B8 fatia F4
// (/var/tmp/glintfx-plan/plano-conserto-noexcept.md sec. "F4",
// ESCOPO.md Decisao 11, 17/09/2026, GODS_LAWS.md L-20/L-40/L-43):
// TDD red/green witness for the fix - glintfx::style::detail::
// parse_anb() (anb_parse.cpp) used to call tokenizer.hpp's own
// gltfx_gfss_tokenize(), which grows a std::vector<gltfx_gfss_token>
// one push_back() at a time, from inside a function this file's own
// production code declares `noexcept`. This file proves, BY COUNTING
// (the SAME technique tests/err_no_alloc_test.cpp already establishes
// for CORE-ERROR's own CE-2), that parse_anb() no longer touches the
// heap at all - and separately, WITH THE REAL TOKENIZER AS ORACLE
// (test-only code, never anb_parse.cpp's own noexcept path, so it is
// free to allocate), that anb_parse.cpp's own k_max_anb_tokens teto is
// a MEASURED margin above this grammar's own worst case, not a guess.
//
// TECHNIQUE (the SAME idiom tests/err_no_alloc_test.cpp/tests/log_no_
// alloc_test.cpp/tests/gfui_complex_match_resource_exhausted_test.cpp
// already use): this translation unit replaces the GLOBAL operator
// new/delete for the whole process it links into - safe here because
// glintfx_add_test() gives every test case its own executable, so
// there is no ODR collision with another TU's own override.
// anb_parse.cpp carries no GLINTFX_API (GODS_LAWS.md L-19: an
// internal microparser, not yet public surface), so
// tests/CMakeLists.txt compiles it a SECOND time directly into this
// executable's own object set - every allocation its production code
// could still attempt therefore happens inside THIS binary image, on
// every platform, where the override below reaches it directly.

namespace {

std::size_t g_alloc_count = 0;
std::size_t g_dealloc_count = 0;

void reset_counts() {
    g_alloc_count = 0;
    g_dealloc_count = 0;
}

} // namespace

void *operator new(std::size_t size) {
    ++g_alloc_count;
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void operator delete(void *p) noexcept {
    ++g_dealloc_count;
    std::free(p);
}

void operator delete(void *p, std::size_t /*size*/) noexcept { ::operator delete(p); }

namespace {

using glintfx::style::detail::anb_parse_result;
using glintfx::style::detail::parse_anb;

} // namespace

// --- case 1: the three inputs the plan's own F4 section names by
// name ("2n+1", "odd", "-3n-2"), plus a spread across the 16-
// production grammar (this file's own top comment) and the single
// richest form anb_parse.cpp's own k_max_anb_tokens comment measures
// by hand ("+n - 1", nine tokens including whitespace and <EOF-
// token> - see that file's own top-of-namespace comment) - ALL parsed
// with the allocator counted, never merely "the case that happens to
// be handy".
GLINTFX_TEST(gltfx_gfss_parse_anb_never_allocates) {
    reset_counts();

    static constexpr std::string_view k_samples[] = {
        "2n+1",   "odd",  "even", "-3n-2", "5",    "3n",    "n",    "-n",    "3n-1",
        "n-1",    "-n-1", "3n+1", "n+1",   "-n+1", "3n- 1", "n- 1", "-n- 1", "3n + 1",
        "+n - 1", // the richest form this grammar accepts, per that
                  // file's own comment - leading/trailing whitespace,
                  // the optional leading '+', and the SEPARATED sign
                  // offset all at once.
    };

    std::size_t swept = 0;
    for (const std::string_view sample : k_samples) {
        const anb_parse_result result = parse_anb(sample);
        // Every one of these is a GENUINELY VALID An+B value (this
        // file's own top comment on where each spelling comes from) -
        // a case that failed to PARSE would prove nothing about
        // ALLOCATION, only that this file's own sample table is wrong.
        GLINTFX_CHECK(result.ok);
        ++swept;
    }

    // GODS_LAWS.md L-40: zero swept is a floor violation, never a pass.
    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, std::size(k_samples));

    // SNAPSHOT IMMEDIATELY (the same discipline err_no_alloc_test.cpp's
    // own header comment documents, and the exact CI finding that
    // motivated it - std::println's OWN <print>/<format> backend can
    // allocate on some libstdc++ builds): freeze the counts BEFORE
    // anything else, including std::println itself, runs.
    const std::size_t final_alloc_count = g_alloc_count;
    const std::size_t final_dealloc_count = g_dealloc_count;

    std::println("gltfx_gfss_parse_anb_never_allocates: {} sample(s) parsed, {} allocation(s), "
                 "{} deallocation(s)",
                 swept, final_alloc_count, final_dealloc_count);

    GLINTFX_CHECK_EQ(final_alloc_count, static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(final_dealloc_count, static_cast<std::size_t>(0));
}

// --- case 2: the SAME proof for the HOSTILE inputs (malformed An+B
// text that fails to parse, and the two overflow shapes case 3 below
// exercises by diagnostic) - a defect that only stopped allocating on
// the HAPPY path and still allocated while building the diagnostic
// for a rejected argument would still be reachable from match-time
// under low memory (ESCOPO.md Decisao 8), just through a narrower
// door.
GLINTFX_TEST(gltfx_gfss_parse_anb_never_allocates_on_hostile_input) {
    reset_counts();

    static constexpr std::string_view k_samples[] = {
        "",
        "   ",
        "foo",
        "3.5n",
        "3.5",
        "-",
        "n-",
        "3n-",
        "n+1.5",
        "n+ 1.5",
        "n++1",
        "n+-1",
        "n+",
        "3n extra",
        "odd 1",
        "5 5",
        "n 1",
        "1 1 1 1 1 1+1", // one token past the teto - see case 3
    };

    std::size_t swept = 0;
    for (const std::string_view sample : k_samples) {
        const anb_parse_result result = parse_anb(sample);
        GLINTFX_CHECK(!result.ok);
        ++swept;
    }

    GLINTFX_CHECK(swept > 0);
    GLINTFX_CHECK_EQ(swept, std::size(k_samples));

    const std::size_t final_alloc_count = g_alloc_count;
    const std::size_t final_dealloc_count = g_dealloc_count;

    std::println("gltfx_gfss_parse_anb_never_allocates_on_hostile_input: {} sample(s) rejected, "
                 "{} allocation(s), {} deallocation(s)",
                 swept, final_alloc_count, final_dealloc_count);

    GLINTFX_CHECK_EQ(final_alloc_count, static_cast<std::size_t>(0));
    GLINTFX_CHECK_EQ(final_dealloc_count, static_cast<std::size_t>(0));
}

// --- case 3: the teto's own boundary, ONE TOKEN AT A TIME, proved
// against the REAL tokenizer (gltfx_gfss_tokenize(), test-only code -
// it allocates on purpose, as the oracle this case measures against,
// never as the thing under test) - "1 1 1 1 1 1" is six number
// tokens, five whitespace tokens between them and the terminal
// <EOF-token>: exactly twelve, anb_parse.cpp's own k_max_anb_tokens.
// "1 1 1 1 1 1+1" adds ONE more number token, adjacent to the last
// one with no separating whitespace (the SAME adjacency this file's
// own "3n+1" sample already proves the tokenizer honors - a
// dimension token immediately followed by a signed number token, no
// gap needed): thirteen tokens, one past the teto.
GLINTFX_TEST(gltfx_gfss_anb_token_teto_boundary_is_exact) {
    static constexpr std::string_view k_at_teto = "1 1 1 1 1 1";
    static constexpr std::string_view k_one_past_teto = "1 1 1 1 1 1+1";

    const std::vector<glintfx::style::gltfx_gfss_token> at_teto_tokens =
        glintfx::style::gltfx_gfss_tokenize(k_at_teto);
    const std::vector<glintfx::style::gltfx_gfss_token> one_past_tokens =
        glintfx::style::gltfx_gfss_tokenize(k_one_past_teto);

    std::println("gltfx_gfss_anb_token_teto_boundary_is_exact: \"{}\" -> {} token(s), \"{}\" -> "
                 "{} token(s) (real tokenizer, oracle only)",
                 k_at_teto, at_teto_tokens.size(), k_one_past_teto, one_past_tokens.size());

    GLINTFX_CHECK_EQ(at_teto_tokens.size(), static_cast<std::size_t>(12));
    GLINTFX_CHECK_EQ(one_past_tokens.size(), static_cast<std::size_t>(13));

    // AT the teto: the buffer holds every token, so parse_anb() fails
    // for an ORDINARY grammar reason (a bare integer followed by five
    // more integers is trailing garbage after a complete value, the
    // SAME "end_of_anb_expression" shape "5 5"/"3n extra" already get
    // in gfss_selector_parse_test.cpp's own hostile-input table) -
    // NEVER the overflow diagnostic, because nothing overflowed.
    const anb_parse_result at_teto_result = parse_anb(k_at_teto);
    GLINTFX_CHECK(!at_teto_result.ok);
    GLINTFX_CHECK(at_teto_result.diagnostic.expected ==
                  glintfx::style::detail::k_expected_end_of_anb_expression);

    // ONE PAST the teto: the buffer has nowhere left to put the
    // thirteenth token - refused with the DEDICATED overflow
    // diagnostic, never the ordinary "trailing garbage" one (the
    // ordinary parser never even gets a chance to look at this text).
    const anb_parse_result one_past_result = parse_anb(k_one_past_teto);
    GLINTFX_CHECK(!one_past_result.ok);
    GLINTFX_CHECK(one_past_result.diagnostic.expected ==
                  glintfx::style::detail::k_expected_anb_expression_too_long);
}
