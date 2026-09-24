// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_error_injector.hpp"
#include "wire_message.hpp"

#include <cstring>

namespace glintfx::test::wire_relay {

namespace {

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &value, sizeof(bytes));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void append_wire_string(std::vector<std::uint8_t> &out, std::string_view text) {
    auto stored_len = static_cast<std::uint32_t>(text.size() + 1); // + NUL
    append_u32(out, stored_len);
    out.insert(out.end(), text.begin(), text.end());
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
    auto opcode_and_size = static_cast<std::uint32_t>(wl_display_error_event_opcode) |
                           (static_cast<std::uint32_t>(wire_header_size + body.size()) << 16);
    append_u32(out, opcode_and_size);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

} // namespace glintfx::test::wire_relay
