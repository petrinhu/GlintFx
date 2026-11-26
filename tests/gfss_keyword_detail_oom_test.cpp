// SPDX-License-Identifier: AGPL-3.0-or-later
//
// gfss_keyword_detail_oom_test.cpp - TIDY23-ESCAPE (clang-tidy 23's
// bugprone-exception-escape over the Microsoft STL, CI run
// 44764144174). The FIRST keyword refusal on a contract that also
// accepts a numeric nature builds declaration_value_check.cpp's own
// program-lifetime table of accepted-keyword details (one
// std::string per property, built by concatenation). If that first
// build cannot allocate, std::bad_alloc has to reach the caller like
// every other allocation failure of this parse chain (sheet_parse.hpp's
// own header comment: the chain is not noexcept, on purpose). It must
// never end the process. Before this fix check_one_component() was
// noexcept, and the same failure ended in std::terminate().
//
// Its own binary ON PURPOSE: the table is a function-local static,
// initialized once per process, so the forced failure has to be the
// very first use of it, before any other case could build it.
//
// NOT APPLICABLE, declared and printed, never skipped in silence:
//   - MSVC AddressSanitizer: ASan's own operator new wins over this
//     TU's override by default (err_context_test.cpp's own header
//     comment has the citation in full), so the forcing would measure
//     nothing;
//   - MSVC with iterator debugging on (_ITERATOR_DEBUG_LEVEL != 0, the
//     /MDd default): the parse chain's own std::vector default
//     constructor allocates a debug proxy inside a constructor the
//     language requires to be noexcept (family A, ESCOPO.md Decisao 13),
//     so the forced failure would end the process there, before this
//     file's own subject is ever reached.
// In both cases the unforced half still runs.

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <print>
#include <vector>

#include <glintfx/gfss/property.hpp>
#include <glintfx/gfss/token.hpp>
#include <glintfx/gfss/tokenizer.hpp>

#include "gfss/declaration_value_check.hpp"
#include "gfss/diagnostic_vocabulary.hpp"
#include "gfss/property_value_contract.hpp"
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

namespace {

std::atomic<bool> g_force_alloc_failure{false};
std::atomic<std::size_t> g_override_new_call_count{0};

[[nodiscard]] bool oom_forcing_declared_not_applicable() noexcept {
#if defined(_WIN32) && defined(__SANITIZE_ADDRESS__)
    return true;
#elif defined(_MSC_VER) && defined(_ITERATOR_DEBUG_LEVEL) && (_ITERATOR_DEBUG_LEVEL != 0)
    return true;
#else
    return false;
#endif
}

} // namespace

void *operator new(std::size_t size) {
    g_override_new_call_count.fetch_add(1, std::memory_order_relaxed);
    if (g_force_alloc_failure.load(std::memory_order_relaxed)) {
        throw std::bad_alloc();
    }
    if (void *p = std::malloc(size); p != nullptr) {
        return p;
    }
    throw std::bad_alloc();
}

void *operator new(std::size_t size, const std::nothrow_t & /*tag*/) noexcept {
    g_override_new_call_count.fetch_add(1, std::memory_order_relaxed);
    if (g_force_alloc_failure.load(std::memory_order_relaxed)) {
        return nullptr;
    }
    return std::malloc(size);
}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmismatched-new-delete"
#endif

void operator delete(void *p) noexcept { std::free(p); }

void operator delete(void *p, std::size_t /*size*/) noexcept { std::free(p); }

void operator delete(void *p, const std::nothrow_t & /*tag*/) noexcept { std::free(p); }

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

GLINTFX_TEST(first_keyword_detail_build_out_of_memory_reaches_the_caller_as_bad_alloc) {
    using namespace glintfx::style;
    using namespace glintfx::style::detail;

    // `inline` is a keyword `width` does not accept, and `width` also
    // accepts lengths and percentages, so the check goes through
    // check_one_component(), never check_keyword_only(). Tokenized
    // BEFORE the forcing is armed: only the check itself is under test.
    const std::vector<gltfx_gfss_token> tokens = gltfx_gfss_tokenize("inline");
    const property_value_contract &contract =
        property_value_contract_for(gltfx_gfss_property::width);
    const std::size_t value_end = tokens.size() - 1; // the trailing <EOF-token> is not the value

    if (oom_forcing_declared_not_applicable()) {
        std::println(stderr,
                     "gfss_keyword_detail_oom_test: the forced-failure half is declared NOT "
                     "APPLICABLE on this build (MSVC AddressSanitizer, or MSVC iterator "
                     "debugging - see this file's own header comment); the unforced half runs");
        const declaration_value_check_result unforced =
            check_declaration_value(tokens, 0, value_end, contract);
        GLINTFX_CHECK(!unforced.ok);
        GLINTFX_CHECK(unforced.diagnostic.detail == "auto");
        return;
    }

    bool reached_the_caller = false;
    const std::size_t before_forced = g_override_new_call_count.load();
    g_force_alloc_failure = true;
    try {
        static_cast<void>(check_declaration_value(tokens, 0, value_end, contract));
    } catch (const std::bad_alloc &) {
        reached_the_caller = true;
    }
    g_force_alloc_failure = false;
    const std::size_t forced_calls = g_override_new_call_count.load() - before_forced;
    GLINTFX_CHECK(reached_the_caller);
    GLINTFX_CHECK(forced_calls >= 1);

    // A static whose initialization exited by an exception is not
    // initialized, and the next pass tries again ([stmt.dcl]/4): the
    // table builds now, and the refusal names the one keyword `width`
    // accepts.
    const std::size_t before_second = g_override_new_call_count.load();
    const declaration_value_check_result second =
        check_declaration_value(tokens, 0, value_end, contract);
    const std::size_t second_calls = g_override_new_call_count.load() - before_second;
    GLINTFX_CHECK(!second.ok);
    GLINTFX_CHECK(second.diagnostic.expected == k_expected_keyword_for_property);
    GLINTFX_CHECK(second.diagnostic.detail == "auto");
    GLINTFX_CHECK(second_calls >= 1);

    // Built once: a third refusal allocates nothing, which is what ties
    // the allocations of the second call, and the failure forced in the
    // first, to the table build and to nothing else on the path.
    const std::size_t before_third = g_override_new_call_count.load();
    const declaration_value_check_result third =
        check_declaration_value(tokens, 0, value_end, contract);
    const std::size_t third_calls = g_override_new_call_count.load() - before_third;
    GLINTFX_CHECK(third_calls == 0);
    GLINTFX_CHECK(third.diagnostic.detail == "auto");

    std::println("first_keyword_detail_build_out_of_memory_reaches_the_caller_as_bad_alloc: "
                 "forced_calls={} second_calls={} third_calls={}",
                 forced_calls, second_calls, third_calls);
}
