// SPDX-License-Identifier: AGPL-3.0-or-later
#include "platform/gl/gfx_open_only_fixation.hpp"

#include <new>

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
    // docs/api-conventions.md R3 ("a lib NUNCA aborta o processo do
    // consumidor"): `result.fixed.push_back()` below grows a
    // std::vector - on the machine's own allocator running out of
    // memory it throws std::bad_alloc, and letting that escape this
    // noexcept function would call std::terminate(), killing the
    // consumer's process on the very path that opens a window. Caught
    // here and degraded to `alloc_failed`, the same shape err.cpp's own
    // with_path()/with_rejected_value() already use for a best-effort
    // std::string::assign() that can fail the identical way.
    //
    // CONSERTO (07/09/2026, WIN-DEBUG-CTORALLOC): `gfx_open_only_
    // fixation_result result{}` used to sit BEFORE this try{} - a
    // std::vector default-construction that never allocates on Linux/
    // libstdc++ (FACT: no allocation for a default-constructed empty
    // vector, guaranteed by the standard's own complexity clause), but
    // an integrator's real Windows Debug run (job "Windows - Debug")
    // measured this exact test dying silently, uncaught, on the
    // out-of-memory case specifically - never on Linux, and never on
    // the six other cells of the same test file, which do not force an
    // allocation failure. INFERENCE (not independently proven on this
    // machine - no Windows toolchain here, GODS_LAWS.md L-27): MSVC's
    // debug-iterator-support machinery, active by default in Debug
    // builds (`_ITERATOR_DEBUG_LEVEL == 2`, learn.microsoft.com/cpp/
    // standard-library/iterator-debug-level, fetched 07/09/2026 -
    // "Enables iterator debugging" is the documented Debug default),
    // attaches bookkeeping to every container INCLUDING an empty,
    // freshly-constructed one, through the SAME allocator a later
    // push_back() would use - so with the test's own forced-failure
    // flag already armed before this function is even entered, that
    // bookkeeping allocation could throw OUTSIDE the try{} the line
    // below used to start one line too late. Declaring `result` INSIDE
    // the try{} - the only change from the previous fatia - closes
    // that gap without changing what either branch returns.
    try {
        gfx_open_only_fixation_result result{};

        for (const gfx_option_row &row : k_gfx_option_table) {
            // rule 1 (D-W6b-25/14.2): live/read_only options never
            // enter fixation.
            if (row.when != gltfx_gfx_option_when::open_only) {
                continue;
            }

            const std::int64_t resolved = resolve_requested_or_default(row, requested);

            if (!already_fixed.has_value()) {
                result.fixed.push_back(gltfx_gfx_option_entry{.id = row.id, .value = resolved});
                continue;
            }

            const std::optional<std::int64_t> fixed_value =
                find_fixed_value(row.id, *already_fixed);
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
    } catch (const std::bad_alloc &) { // NOLINT(bugprone-empty-catch) reason: intentionally
                                       // empty, see the comment above try{} - `result` (if it
                                       // was constructed at all) keeps whatever partial content
                                       // it grew before the allocation that failed, but
                                       // `alloc_failed` below tells the caller never to read it.
        return gfx_open_only_fixation_result{
            .outcome = gfx_open_only_fixation_outcome::alloc_failed,
            .fixed = {},
            .refused_id = gltfx_gfx_option::vsync,
        };
    }
}

} // namespace glintfx::platform
