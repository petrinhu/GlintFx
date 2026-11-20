// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gfx_option_validation.hpp"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>

#include <glintfx/core/err_code.hpp>

#include "platform/gl/gfx_option_registry.hpp"

namespace glintfx::platform {

gltfx_rslt<void> validate_gfx_option_entry(const gltfx_gfx_option_entry &entry,
                                           bool already_open) noexcept {
    const gfx_option_row *row = find_gfx_option_row(entry.id);
    if (row == nullptr) {
        // No name to report for an id nobody registered - the decimal
        // numeric value is the only honest token available (docs/
        // api-conventions.md R7: a token, never a sentence).
        //
        // GODS_LAWS.md L-22: printed by std::to_chars into a stack
        // buffer, so this branch owns no allocation at all and nothing
        // in it can throw across this function's own noexcept. The
        // buffer holds the widest std::uint16_t (digits10 + 1 = five
        // digits), so to_chars always succeeds; the fixed placeholder
        // token below (never a guess at the real numeric id, the same
        // class of fallback option_name_or_placeholder() in
        // gl_context_facade.cpp uses when a row cannot be found at
        // all) is only what a too-small buffer would degrade to. The
        // shape before this one built a std::string and caught
        // std::bad_alloc, and the fallback assignment inside its own
        // catch handler was still an allocating call on paper -
        // clang-tidy 23's bugprone-exception-escape flagged exactly
        // that over the Microsoft STL.
        std::array<char, std::numeric_limits<std::uint16_t>::digits10 + 1> digits{};
        const std::to_chars_result printed = std::to_chars(
            digits.data(), digits.data() + digits.size(), static_cast<std::uint16_t>(entry.id));
        const std::string_view unknown_id =
            printed.ec == std::errc{}
                ? std::string_view(digits.data(),
                                   static_cast<std::size_t>(printed.ptr - digits.data()))
                : std::string_view("gfx_option");
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(unknown_id));
    }

    if (row->when == gltfx_gfx_option_when::read_only) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(row->name));
    }

    if (row->when == gltfx_gfx_option_when::open_only && already_open) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(row->name));
    }

    if (entry.value < row->min_value || entry.value > row->max_value) {
        return gltfx_rslt<void>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value(row->name));
    }

    return gltfx_rslt<void>::ok();
}

} // namespace glintfx::platform
