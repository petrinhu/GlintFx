// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>
#include <type_traits>

#include <glintfx/core/log/value.hpp>

// core/log/field.hpp - CL-2 of CORE-LOG (TODO.md W5, GODS_LAWS.md
// L-19/L-22/L-26): gltfx_log_field, a name paired with one
// gltfx_log_value (value.hpp). `name` is a view, same lifetime rule
// as gltfx_log_value::text() - valid only during the sink call
// (core/log/event.hpp documents the whole event's lifetime once
// CL-3 lands).

namespace glintfx {

struct gltfx_log_field {
    std::string_view name;
    // `= {}` even though gltfx_log_value already default-constructs
    // itself fully (value.hpp's own header comment) - cppcheck's
    // uninitMemberVarNoCtor does not look inside a member's own
    // default constructor, only at the declaration here.
    gltfx_log_value value{};
};

static_assert(std::is_standard_layout_v<gltfx_log_field>,
              "gltfx_log_field layout is the contract, CORE-LOG CL-2");
static_assert(std::is_trivially_copyable_v<gltfx_log_field>,
              "gltfx_log_field must stay a plain, non-allocating value type, CORE-LOG CL-2");

} // namespace glintfx
