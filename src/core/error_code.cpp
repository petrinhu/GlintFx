// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/error_code.hpp>

#include <array>

// error_code.cpp - CE-1 of CORE-ERROR (TODO.md, GODS_LAWS.md L-17):
// name and value live in ONE table, not a switch that grows a case per
// feature. tests/error_code_test.cpp is the TDD red/green witness for
// this file (GODS_LAWS.md L-20).

namespace glintfx {

namespace {

struct error_code_entry {
    // Default member initializers, not a user-declared constructor
    // (which would forfeit aggregate-init at every row of k_table
    // below): cppcheck's uninitMemberVarNoCtor cannot see that
    // aggregate init already sets both fields at every use site, so
    // this is the fix, not a suppression.
    error_code code = error_code::unknown;
    std::string_view name;
};

// THE table. Adding a v2 value means adding its enumerator in
// error_code.hpp plus exactly one row here; nothing else in this file
// changes shape (append-only, see the header's own contract comment).
constexpr std::array<error_code_entry, 8> k_table{{
    {error_code::unknown, "unknown"},
    {error_code::out_of_memory, "out_of_memory"},
    {error_code::io_failure, "io_failure"},
    {error_code::not_found, "not_found"},
    {error_code::invalid_argument, "invalid_argument"},
    {error_code::parse_failure, "parse_failure"},
    {error_code::unsupported, "unsupported"},
    {error_code::platform_failure, "platform_failure"},
}};

} // namespace

std::string_view error_code_name(error_code code) noexcept {
    for (const error_code_entry &entry : k_table) {
        if (entry.code == code) {
            return entry.name;
        }
    }
    // A raw value this build's table does not recognize: a consumer
    // linked against a NEWER glintfx than the one that produced this
    // error. Graceful degradation, never undefined behavior.
    return "unknown";
}

} // namespace glintfx
