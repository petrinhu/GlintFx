// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/gfss/property.hpp>

#include "gfss/property_table.hpp"

// property.cpp - GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_LAWS.md
// L-17/L-19): the three PUBLIC accessors property.hpp declares
// (name/is_inherited/initial), reading property_table.hpp's own
// shared k_property_table - see that header's own top comment for the
// table's full provenance and design rationale (not repeated here,
// GODS_LAWS.md L-17: one subject per file, and the table's own reason
// to change is different from this file's - this file only changes
// when the PUBLIC accessor SHAPE changes, the table changes whenever
// SS6 grows).
//
// WHY THIS FILE DOES NOT ALSO DEFINE property_status.hpp's OWN
// detail:: ACCESSORS (a real link-time constraint, not a style
// preference - see property_status.cpp's own header comment for the
// full reasoning): gfss_property_registry_test.cpp needs to exercise
// those two INTERNAL, non-GLINTFX_API functions the same way gfss_
// value_test.cpp already exercises glintfx::style::detail::
// parse_value() - by recompiling their OWN .cpp directly into the
// test executable (they carry no GLINTFX_API, so the shared library's
// own symbol table hides them). Recompiling THIS file the same way
// would ALSO recompile gltfx_gfss_property_name/is_inherited/initial
// - which the test executable already links from glintfx_library's
// own .so - producing two competing definitions of the same PUBLIC
// symbol in one executable. Splitting the internal accessor into its
// own .cpp (property_status.cpp), reading the SAME header-only table,
// sidesteps that entirely: the test recompiles ONLY property_status.
// cpp, which defines no public symbol at all.

namespace glintfx::style {

std::string_view gltfx_gfss_property_name(gltfx_gfss_property property) noexcept {
    if (const detail::property_entry *entry = detail::find_property_entry(property)) {
        return entry->sheet_name;
    }
    // docs/api-conventions.md R4: never undefined behavior.
    return "unknown";
}

bool gltfx_gfss_property_is_inherited(gltfx_gfss_property property) noexcept {
    if (const detail::property_entry *entry = detail::find_property_entry(property)) {
        return entry->inherited;
    }
    return false;
}

gltfx_gfss_value gltfx_gfss_property_initial(gltfx_gfss_property property) noexcept {
    if (const detail::property_entry *entry = detail::find_property_entry(property)) {
        return entry->initial;
    }
    return gltfx_gfss_value{};
}

} // namespace glintfx::style
