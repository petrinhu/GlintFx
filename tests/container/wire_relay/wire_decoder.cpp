// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_decoder.hpp"

#include <cstring>

namespace glintfx::test::wire_relay {

namespace {

message_header parse_header(const std::uint8_t *bytes) {
    message_header header;
    std::memcpy(&header.object_id, bytes, sizeof(header.object_id));
    // Second wire word: opcode in the low 16 bits, message size
    // (header included) in the high 16 bits - the wl_closure layout
    // learned from the protocol's own wire format (GODS_LAWS.md L-29:
    // learned, not copied from any library's source).
    std::uint32_t opcode_and_size = 0;
    std::memcpy(&opcode_and_size, bytes + 4, sizeof(opcode_and_size));
    header.opcode = static_cast<std::uint16_t>(opcode_and_size & 0xFFFFu);
    header.size = static_cast<std::uint16_t>(opcode_and_size >> 16);
    return header;
}

} // namespace

void wire_decoder::feed(const std::uint8_t *data, std::size_t len) {
    m_buffer.insert(m_buffer.end(), data, data + len);
}

decode_outcome wire_decoder::poll() const {
    if (m_buffer.size() < wire_header_size) {
        return decode_outcome::need_more_bytes;
    }
    message_header header = parse_header(m_buffer.data());
    if (header.size < wire_header_size || header.size > wire_message_size_cap) {
        return decode_outcome::message_too_large;
    }
    if (m_buffer.size() < header.size) {
        return decode_outcome::need_more_bytes;
    }
    return decode_outcome::message_ready;
}

decoded_message wire_decoder::take_message() {
    message_header header = parse_header(m_buffer.data());
    decoded_message result;
    result.header = header;
    result.payload.assign(m_buffer.begin() + static_cast<std::ptrdiff_t>(wire_header_size),
                          m_buffer.begin() + static_cast<std::ptrdiff_t>(header.size));
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(header.size));
    return result;
}

} // namespace glintfx::test::wire_relay
