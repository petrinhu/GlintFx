// SPDX-License-Identifier: AGPL-3.0-or-later

// RSLT-ERR-RENAME: the hostile `error(ctx, ...)` shim has to land
// BEFORE <glintfx/core/err.hpp> is ever parsed in this TU - it is the
// same "TU that includes a hostile system header first" shape
// header_hygiene_test.cpp already exercises, here scoped to the one
// header this fatia is about. See the shim's own header comment for
// why this reproduces libattr's attr/error_context.h, and why `err()`
// (this fatia's renamed accessor) survives it while `error()` (the
// name being renamed away from) would not have.
#include "hostile_attr_error_macros_shim.hpp"

#include <string_view>
#include <utility>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// rslt_test.cpp - CE-4 of CORE-ERROR (TODO.md, GODS_LAWS.md L-20):
// proves gltfx_rslt<T> round-trips through ok()/err() for both the
// primary template and the T = void specialization, and that an
// EXAMPLE fallible function (illustrative only, not part of glintfx's
// public API - the real proof of "one convention, no second form" is
// the TYPE SHAPE itself, checked here on both forms) exercises both
// its success and error paths. The [[nodiscard]] compiler-diagnostic
// claim is proven separately, by tests/tools/check_nodiscard_rslt.sh -
// a discarded return value is a COMPILE-time fact, not something a
// runtime GLINTFX_CHECK can observe.

namespace {

// Illustrative fallible functions only - see the shell fixture in
// tests/tools/check_nodiscard_rslt.sh for the near-identical pair used
// there to prove the [[nodiscard]] diagnostic; the slight duplication
// is intentional (CONTRACT.md 6.7: two occurrences, WET is fine - one
// proves a compile-time DIAGNOSTIC, the other proves RUNTIME behavior,
// different concerns despite the similar shape).

glintfx::gltfx_rslt<int> parse_positive_int(std::string_view text) noexcept {
    if (text.empty()) {
        return glintfx::gltfx_rslt<int>::err(
            glintfx::gltfx_err(glintfx::gltfx_err_code::invalid_argument));
    }
    int value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return glintfx::gltfx_rslt<int>::err(
                glintfx::gltfx_err(glintfx::gltfx_err_code::parse_failure));
        }
        value = (value * 10) + (c - '0');
    }
    return glintfx::gltfx_rslt<int>::ok(value);
}

glintfx::gltfx_rslt<void> validate_non_empty(std::string_view text) noexcept {
    if (text.empty()) {
        return glintfx::gltfx_rslt<void>::err(
            glintfx::gltfx_err(glintfx::gltfx_err_code::invalid_argument));
    }
    return glintfx::gltfx_rslt<void>::ok();
}

} // namespace

GLINTFX_TEST(primary_template_round_trips_success) {
    const glintfx::gltfx_rslt<int> r = glintfx::gltfx_rslt<int>::ok(42);
    GLINTFX_CHECK(r.has_value());
    GLINTFX_CHECK(!r.has_error());
    GLINTFX_CHECK(r.value() == 42);
}

GLINTFX_TEST(primary_template_round_trips_error) {
    const glintfx::gltfx_rslt<int> r =
        glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::not_found));
    GLINTFX_CHECK(!r.has_value());
    GLINTFX_CHECK(r.has_error());
    GLINTFX_CHECK(r.err().code() == glintfx::gltfx_err_code::not_found);
}

GLINTFX_TEST(void_specialization_round_trips_success) {
    const glintfx::gltfx_rslt<void> r = glintfx::gltfx_rslt<void>::ok();
    GLINTFX_CHECK(r.has_value());
    GLINTFX_CHECK(!r.has_error());
}

GLINTFX_TEST(void_specialization_round_trips_error) {
    const glintfx::gltfx_rslt<void> r =
        glintfx::gltfx_rslt<void>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::unsupported));
    GLINTFX_CHECK(!r.has_value());
    GLINTFX_CHECK(r.has_error());
    GLINTFX_CHECK(r.err().code() == glintfx::gltfx_err_code::unsupported);
}

GLINTFX_TEST(example_fallible_function_success_path) {
    const auto parsed = parse_positive_int("42");
    GLINTFX_CHECK(parsed.has_value());
    GLINTFX_CHECK(parsed.value() == 42);
}

GLINTFX_TEST(example_fallible_function_error_path) {
    const auto parsed = parse_positive_int("4x");
    GLINTFX_CHECK(parsed.has_error());
    GLINTFX_CHECK(parsed.err().code() == glintfx::gltfx_err_code::parse_failure);
}

// The "sem valor de retorno" case the brief calls out explicitly:
// gltfx_rslt<void>, not a bare gltfx_err and not a bool - same
// envelope shape, same has_value()/has_error() convention.
GLINTFX_TEST(example_fallible_function_no_return_value_case) {
    const auto ok_case = validate_non_empty("not empty");
    GLINTFX_CHECK(ok_case.has_value());

    const auto err_case = validate_non_empty("");
    GLINTFX_CHECK(err_case.has_error());
    GLINTFX_CHECK(err_case.err().code() == glintfx::gltfx_err_code::invalid_argument);
}

// RSLT-ERR-RENAME: the accessor is `err()`, never `error()` - the name
// this fatia renamed it FROM collides, under a hostile function-like
// `error(ctx, ...)` macro (this TU's own hostile_attr_error_macros_
// shim.hpp, included above BEFORE <glintfx/core/err.hpp>, reproducing
// libattr's attr/error_context.h), because `r.error()` is a call
// expression - `error` immediately followed by `(` is exactly the
// shape a function-like macro pattern-matches on. `r.err()` below is
// the SAME shape of call, on a DIFFERENT token the shim never touches,
// so it survives inclusion of the hostile macro unmodified - proving
// the rename, not just documenting it.
GLINTFX_TEST(error_accessor_survives_a_function_like_error_macro) {
    const glintfx::gltfx_rslt<int> r =
        glintfx::gltfx_rslt<int>::err(glintfx::gltfx_err(glintfx::gltfx_err_code::io_failure));
    GLINTFX_CHECK(r.has_error());
    GLINTFX_CHECK(r.err().code() == glintfx::gltfx_err_code::io_failure);
}
