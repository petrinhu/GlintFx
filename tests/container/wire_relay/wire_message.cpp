// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_message.hpp"

#include <cstring>

namespace glintfx::test::wire_relay {

namespace {

// resize()+memcpy(), never insert(pos, first, last) - GCC 12-14 false
// positive -Wstringop-overflow, see wire_error_injector.cpp's own
// comment (PR libstdc++/117983).
void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    const std::size_t offset = out.size();
    out.resize(offset + sizeof(value));
    std::memcpy(out.data() + offset, &value, sizeof(value));
}

} // namespace

std::vector<std::uint8_t> encode_raw(const decoded_message &message) {
    // The exact inverse of wire_decoder::take_message(): header.size
    // already IS wire_header_size + payload.size() (the decoder never
    // stores a header whose size field disagrees with what it framed),
    // so re-emitting object_id + opcode_and_size + payload reproduces
    // the ORIGINAL bytes byte-for-byte - decode never lossily
    // transforms anything, it only copies. This is what lets the
    // relay forward one validated message at a time (wire_relay_main.
    // cpp) instead of an entire raw read chunk, without ever altering
    // a single byte of what a real client or compositor sent.
    std::vector<std::uint8_t> out;
    append_u32(out, message.header.object_id);
    const auto opcode_and_size = static_cast<std::uint32_t>(message.header.opcode) |
                                 (static_cast<std::uint32_t>(message.header.size) << 16);
    append_u32(out, opcode_and_size);
    const std::size_t offset = out.size();
    out.resize(offset + message.payload.size());
    if (!message.payload.empty()) {
        std::memcpy(out.data() + offset, message.payload.data(), message.payload.size());
    }
    return out;
}

} // namespace glintfx::test::wire_relay
