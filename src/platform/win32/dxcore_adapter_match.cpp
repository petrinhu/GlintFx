// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/win32/dxcore_adapter_match.hpp"

#include <cstddef>

namespace glintfx::platform {

namespace {

enum class luid_lookup_status : std::uint8_t { not_attempted, unique_match, ambiguous, no_match };

struct luid_lookup_result {
    luid_lookup_status status = luid_lookup_status::not_attempted;
    std::size_t index = 0;
};

[[nodiscard]] luid_lookup_result look_up_by_luid(std::span<const dxcore_adapter_facts> facts,
                                                 std::uint64_t gl_luid) noexcept {
    if (gl_luid == 0) {
        return {luid_lookup_status::not_attempted, 0};
    }

    std::size_t match_count = 0;
    std::size_t first_match = 0;
    for (std::size_t i = 0; i < facts.size(); ++i) {
        if (facts[i].luid == gl_luid) {
            if (match_count == 0) {
                first_match = i;
            }
            ++match_count;
        }
    }

    if (match_count == 0) {
        return {luid_lookup_status::no_match, 0};
    }
    if (match_count == 1) {
        return {luid_lookup_status::unique_match, first_match};
    }
    return {luid_lookup_status::ambiguous, 0};
}

[[nodiscard]] std::optional<std::size_t>
match_by_substring(std::span<const dxcore_adapter_facts> facts,
                   std::string_view gl_renderer) noexcept {
    if (gl_renderer.empty()) {
        return std::nullopt;
    }

    std::optional<std::size_t> found;
    for (std::size_t i = 0; i < facts.size(); ++i) {
        const std::string &description = facts[i].description;
        if (description.empty()) {
            continue; // an empty description never matches (cell 5)
        }
        if (gl_renderer.find(description) != std::string_view::npos) {
            if (found.has_value()) {
                return std::nullopt; // ambiguous: two descriptions both substrings
            }
            found = i;
        }
    }
    return found;
}

} // namespace

std::optional<std::size_t> match_dxcore_adapter(std::span<const dxcore_adapter_facts> facts,
                                                std::uint64_t gl_luid,
                                                std::string_view gl_renderer) noexcept {
    const luid_lookup_result luid_result = look_up_by_luid(facts, gl_luid);

    switch (luid_result.status) {
    case luid_lookup_status::unique_match:
        return luid_result.index;
    case luid_lookup_status::ambiguous:
        return std::nullopt; // an ambiguous exact match is never rescued by a fuzzier one
    case luid_lookup_status::not_attempted:
    case luid_lookup_status::no_match:
        break; // fall through to the substring reserve
    }

    return match_by_substring(facts, gl_renderer);
}

} // namespace glintfx::platform
