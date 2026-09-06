// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "platform/gl/gfx_option_validation.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// gfx_option_validation_test.cpp - C-OPT (docs/plano-w6b-placa-e-
// laco.md fatia 2a, D-W6b-16, GODS_LAWS.md L-20/L-22): the TDD red/
// green witness for glintfx::platform::validate_gfx_option_entry()
// (src/platform/gl/gfx_option_validation.hpp), the atom every opening
// list and every set_option() call (fatia 2b) funnels through before
// any adapter ever sees the entry.
//
// RED, SEEN: before gfx_option_validation.{hpp,cpp} existed, this
// file's own #include line failed to compile - validate_gfx_option_
// entry() was undeclared. Green is this file compiling and all six
// cases below passing (docs/plano-w6b-placa-e-laco.md's own row for
// this fatia names exactly six: id fora da tabela, valor abaixo,
// valor acima, open_only depois de aberto, read_only em qualquer call
// shape, valor valido aceita).

using glintfx::gltfx_gfx_option;
using glintfx::gltfx_gfx_option_entry;
using glintfx::platform::validate_gfx_option_entry;

GLINTFX_TEST(validate_gfx_option_entry_refuses_an_id_outside_the_table) {
    // The out-of-range cast below is the point of this case, not a
    // mistake (the same pattern gfss_property_registry_test.cpp's own
    // out-of-range case already establishes).
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: see comment above
    const auto unknown_id = static_cast<gltfx_gfx_option>(static_cast<std::uint16_t>(1000));
    const glintfx::gltfx_rslt<void> result =
        validate_gfx_option_entry(gltfx_gfx_option_entry{.id = unknown_id, .value = 0}, false);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "1000");
}

GLINTFX_TEST(validate_gfx_option_entry_refuses_a_value_below_the_range_naming_the_option) {
    // frame_rate_cap (id 1) has min_value 0.
    const glintfx::gltfx_rslt<void> result = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::frame_rate_cap, .value = -1}, false);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "frame_rate_cap");
}

GLINTFX_TEST(validate_gfx_option_entry_refuses_a_value_above_the_range_naming_the_option) {
    // frame_rate_cap (id 1) has max_value 1000.
    const glintfx::gltfx_rslt<void> result = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::frame_rate_cap, .value = 1001}, false);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "frame_rate_cap");
}

GLINTFX_TEST(validate_gfx_option_entry_refuses_an_open_only_option_after_the_context_is_open) {
    // msaa_samples (id 3) is open_only, and 2 is inside its own range -
    // the ONLY thing wrong here is `already_open == true`.
    const glintfx::gltfx_rslt<void> result = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::msaa_samples, .value = 2}, true);
    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == "msaa_samples");

    // The SAME entry, at open() time (already_open == false), is
    // accepted - proving the refusal above is really about the call
    // shape, not the value.
    const glintfx::gltfx_rslt<void> at_open = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::msaa_samples, .value = 2}, false);
    GLINTFX_CHECK(at_open.has_value());
}

GLINTFX_TEST(validate_gfx_option_entry_refuses_a_read_only_option_in_either_call_shape) {
    // power_source (id 7) is read_only - never accepted from a
    // consumer, regardless of already_open.
    const glintfx::gltfx_rslt<void> at_open = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::power_source, .value = 0}, false);
    GLINTFX_CHECK(at_open.has_error());
    GLINTFX_CHECK(at_open.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(at_open.error().rejected_value() == "power_source");

    const glintfx::gltfx_rslt<void> after_open = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::power_source, .value = 0}, true);
    GLINTFX_CHECK(after_open.has_error());
    GLINTFX_CHECK(after_open.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(after_open.error().rejected_value() == "power_source");
}

GLINTFX_TEST(validate_gfx_option_entry_accepts_a_valid_value) {
    // vsync (id 0) is `live`: valid in both call shapes.
    const glintfx::gltfx_rslt<void> at_open = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::vsync, .value = 1}, false);
    GLINTFX_CHECK(at_open.has_value());

    const glintfx::gltfx_rslt<void> after_open = validate_gfx_option_entry(
        gltfx_gfx_option_entry{.id = gltfx_gfx_option::vsync, .value = 0}, true);
    GLINTFX_CHECK(after_open.has_value());

    std::println("validate_gfx_option_entry_accepts_a_valid_value: 6/6 cases of this file's own "
                 "closed enumeration passed");
}
