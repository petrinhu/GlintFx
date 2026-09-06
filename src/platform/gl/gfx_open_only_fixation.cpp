// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gfx_open_only_fixation.hpp"

#include "platform/gl/gfx_option_registry.hpp"

namespace glintfx::platform {

namespace {

// The value this entry set would carry for `id`: whatever `requested`
// asks for, or the row's own default when `id` is omitted from
// `requested` entirely (D-W6b-25's own "a pedida, ou o default quando
// nao pedida").
std::int64_t resolve_requested_or_default(const gfx_option_row &row,
                                          std::span<const gltfx_gfx_option_entry> requested) {
    for (const gltfx_gfx_option_entry &entry : requested) {
        if (entry.id == row.id) {
            return entry.value;
        }
    }
    return row.default_value;
}

std::optional<std::int64_t> find_fixed_value(gltfx_gfx_option id,
                                             std::span<const gltfx_gfx_option_entry> fixed) {
    for (const gltfx_gfx_option_entry &entry : fixed) {
        if (entry.id == id) {
            return entry.value;
        }
    }
    return std::nullopt;
}

} // namespace

gfx_open_only_fixation_result
resolve_gfx_open_only_fixation(std::optional<std::span<const gltfx_gfx_option_entry>> already_fixed,
                               std::span<const gltfx_gfx_option_entry> requested) noexcept {
    gfx_open_only_fixation_result result{};

    for (const gfx_option_row &row : k_gfx_option_table) {
        // rule 1 (D-W6b-25/14.2): live/read_only options never enter
        // fixation.
        if (row.when != gltfx_gfx_option_when::open_only) {
            continue;
        }

        const std::int64_t resolved = resolve_requested_or_default(row, requested);

        if (!already_fixed.has_value()) {
            result.fixed.push_back(gltfx_gfx_option_entry{.id = row.id, .value = resolved});
            continue;
        }

        const std::optional<std::int64_t> fixed_value = find_fixed_value(row.id, *already_fixed);
        if (!fixed_value.has_value() || *fixed_value != resolved) {
            return gfx_open_only_fixation_result{
                .outcome = gfx_open_only_fixation_outcome::refuse,
                .fixed = {},
                .refused_id = row.id,
            };
        }
    }

    result.outcome = already_fixed.has_value() ? gfx_open_only_fixation_outcome::accept
                                               : gfx_open_only_fixation_outcome::fix_now;
    return result;
}

} // namespace glintfx::platform
