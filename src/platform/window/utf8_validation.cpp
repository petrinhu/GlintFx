// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/window/utf8_validation.hpp"

#include <cstddef>
#include <cstdint>

namespace glintfx::platform {

namespace {

[[nodiscard]] bool is_continuation_byte(unsigned char byte) noexcept {
    return (byte & 0xC0) == 0x80;
}

} // namespace

bool is_valid_utf8(std::string_view text) noexcept {
    const std::size_t size = text.size();
    std::size_t i = 0;

    while (i < size) {
        const auto lead = static_cast<unsigned char>(text[i]);

        // ASCII: one byte, always valid, no continuation to check.
        if ((lead & 0x80) == 0x00) {
            ++i;
            continue;
        }

        std::size_t continuation_count = 0;
        std::uint32_t code_point = 0;
        std::uint32_t min_code_point = 0;

        if ((lead & 0xE0) == 0xC0) {
            continuation_count = 1;
            code_point = lead & 0x1Fu;
            min_code_point = 0x80;
        } else if ((lead & 0xF0) == 0xE0) {
            continuation_count = 2;
            code_point = lead & 0x0Fu;
            min_code_point = 0x800;
        } else if ((lead & 0xF8) == 0xF0) {
            continuation_count = 3;
            code_point = lead & 0x07u;
            min_code_point = 0x10000;
        } else {
            // A stray continuation byte used as a lead, or a lead
            // value RFC 3629 never assigns (0xC0/0xC1 - always
            // overlong once decoded - and 0xF5..0xFF - always past
            // U+10FFFF once decoded - both fall into the branches
            // above and are still caught, by the range checks below,
            // for the values that DO parse as a lead byte; a byte
            // whose top bits match none of the patterns above -
            // 0xF8..0xFF's own 5/6-byte lead shapes the standard
            // retired - lands here).
            return false;
        }

        if (i + continuation_count >= size) {
            return false; // truncated: not enough bytes left in text
        }

        for (std::size_t k = 1; k <= continuation_count; ++k) {
            const auto continuation = static_cast<unsigned char>(text[i + k]);
            if (!is_continuation_byte(continuation)) {
                return false;
            }
            code_point = (code_point << 6) | (continuation & 0x3Fu);
        }

        if (code_point < min_code_point) {
            return false; // overlong encoding
        }
        if (code_point >= 0xD800 && code_point <= 0xDFFF) {
            return false; // UTF-16 surrogate half, never a scalar value
        }
        if (code_point > 0x10FFFF) {
            return false; // past the maximum Unicode scalar value
        }

        i += continuation_count + 1;
    }

    return true;
}

} // namespace glintfx::platform
