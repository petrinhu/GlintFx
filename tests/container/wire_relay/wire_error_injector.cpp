// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_error_injector.hpp"
#include "wire_message.hpp"

#include <cstring>

namespace glintfx::test::wire_relay {

namespace {

// resize()+memcpy(), never insert(pos, first, last): GCC 12-14 has a
// confirmed false-positive -Wstringop-overflow in libstdc++'s
// vector::insert range overload (PR libstdc++/117983, "[12/13/14
// Regression] -Wstringop-overflow false positive for
// __builtin_memmove from vector::insert" - fixed upstream by adding
// an unreachable begin()<=end() hint to _M_range_insert, backported
// to the gcc-14 branch 28/03/2025, but the CI's Ubuntu GCC 14 image
// predates that backport). Hit for real on 24/09/2026: the Ubuntu CI
// job (GCC 14, -Werror) reproved this project's own append_u32/
// encode_message on exactly this pattern - team-lead's finding,
// verified against the upstream bug report before this fix.
void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    const std::size_t offset = out.size();
    out.resize(offset + sizeof(value));
    std::memcpy(out.data() + offset, &value, sizeof(value));
}

void append_wire_string(std::vector<std::uint8_t> &out, std::string_view text) {
    const auto stored_len = static_cast<std::uint32_t>(text.size() + 1); // + NUL
    append_u32(out, stored_len);
    const std::size_t offset = out.size();
    out.resize(offset + text.size());
    if (!text.empty()) {
        std::memcpy(out.data() + offset, text.data(), text.size());
    }
    out.push_back(0); // the NUL terminator stored_len counts
    while (out.size() % 4 != 0) {
        out.push_back(0); // pad the argument to 4-byte alignment
    }
}

} // namespace

std::vector<std::uint8_t> encode_display_error(const rule_violation &violation,
                                               std::string_view message) {
    std::vector<std::uint8_t> body;
    append_u32(body, violation.offending_object_id);
    append_u32(body, static_cast<std::uint32_t>(violation.code));
    append_wire_string(body, message);

    std::vector<std::uint8_t> out;
    append_u32(out, wl_display_object_id);
    const auto opcode_and_size = static_cast<std::uint32_t>(wl_display_error_event_opcode) |
                                 (static_cast<std::uint32_t>(wire_header_size + body.size()) << 16);
    append_u32(out, opcode_and_size);
    const std::size_t offset = out.size();
    out.resize(offset + body.size());
    if (!body.empty()) {
        std::memcpy(out.data() + offset, body.data(), body.size());
    }
    return out;
}

} // namespace glintfx::test::wire_relay
