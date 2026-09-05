// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gfss/property_status.hpp"

#include "gfss/property_table.hpp"

// property_status.cpp - GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_
// LAWS.md L-17/L-19/L-20/L-40, docs/gfss-property-registry-v1.md SS1
// E1): implements property_status.hpp's own two INTERNAL accessors -
// per-property lifecycle status, and the applied/reserved/total debt
// count - both reading property_table.hpp's own shared k_property_
// table (see that header's own top comment for the table's full
// provenance).
//
// A SEPARATE .cpp FROM property.cpp, ON PURPOSE - THE REASON IS A REAL
// LINK-TIME CONSTRAINT, NOT A STYLE PREFERENCE (see property.cpp's own
// header comment for the mirror of this note): neither function
// declared here carries GLINTFX_API (property_status.hpp's own header
// comment: "GFSS-API ... is what would [promote this], not an
// implementer's own guess") - the shared library's default hidden
// visibility (GlintfxCompileOptions.cmake) means gfss_property_
// registry_test.cpp cannot reach either symbol through glintfx::
// glintfx alone, the SAME situation gfss_value_test.cpp already
// resolves for glintfx::style::detail::parse_value() (value_parse.cpp)
// by recompiling that ONE internal .cpp directly into the test
// executable (tests/CMakeLists.txt's own target_sources() pattern).
// This file follows the identical pattern - but it had to be split out
// of property.cpp first, because property.cpp ALSO defines three
// PUBLIC, GLINTFX_API functions (gltfx_gfss_property_name/is_
// inherited/initial) that the test ALREADY links from glintfx_
// library's own .so; recompiling property.cpp itself into the test
// executable would define those same public symbols a SECOND time in
// the same binary. property_status.cpp defines no public symbol at
// all, so recompiling it carries none of that risk.

namespace glintfx::style::detail {

property_status gfss_property_status(gltfx_gfss_property property) noexcept {
    if (const property_entry *entry = find_property_entry(property)) {
        return entry->status;
    }
    // docs/api-conventions.md R4's own "never undefined behavior"
    // convention, applied here as property_status.hpp's own header
    // comment directs: an unrecognized property degrades to the
    // SAFEST answer (`reserved`), never `applied`.
    return property_status::reserved;
}

property_status_counts gfss_property_status_counts() noexcept {
    property_status_counts counts{};
    for (const property_entry &entry : k_property_table) {
        if (entry.status == property_status::applied) {
            ++counts.applied;
        } else {
            ++counts.reserved;
        }
    }
    counts.total = k_property_table.size();
    return counts;
}

} // namespace glintfx::style::detail
