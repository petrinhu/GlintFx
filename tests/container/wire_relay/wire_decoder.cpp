// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_decoder.hpp"

#include <cstring>

namespace glintfx::test::wire_relay {

namespace {

// Precondition the CALLER must already have checked (bytes points at
// at least wire_header_size valid bytes) - this function itself never
// re-checks it, on purpose: poll() and take_message() below each do
// their OWN independent length check before calling this, in code
// that does not call through the other. Team-lead's review, 24/09/
// 2026: a SHARED checked wrapper here would mean one defect (a
// mutation, or a future edit) disabling the check in ONE place
// silently disables it for BOTH callers - measured live, the mutant
// that only broke poll()'s guard still crashed take_message() too,
// because both went through the same guarded function. Duplicating
// the bound (two call sites, not the "rule of three" DRY threshold)
// is the point, not an oversight - see each call site's own comment.
message_header parse_header_unchecked(const std::uint8_t *bytes) {
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
    // resize()+memcpy(), never insert(pos, first, last) - see wire_
    // error_injector.cpp's own comment on this same GCC 12-14 false
    // positive (PR libstdc++/117983), hit for real by the Ubuntu CI
    // job on this project's append_u32 on 24/09/2026.
    const std::size_t offset = m_buffer.size();
    m_buffer.resize(offset + len);
    if (len > 0) {
        std::memcpy(m_buffer.data() + offset, data, len);
    }
}

decode_outcome wire_decoder::poll() const {
    if (m_buffer.size() < wire_header_size) {
        return decode_outcome::need_more_bytes;
    }
    const message_header header = parse_header_unchecked(m_buffer.data());
    if (header.size < wire_header_size || header.size > wire_message_size_cap) {
        return decode_outcome::message_too_large;
    }
    if (m_buffer.size() < header.size) {
        return decode_outcome::need_more_bytes;
    }
    return decode_outcome::message_ready;
}

std::optional<decoded_message> wire_decoder::take_message() {
    // Independent defense, deliberately NOT sharing poll()'s check
    // (see parse_header_unchecked's own comment): a caller that skips
    // poll() - or calls this directly on a buffer that never held a
    // full message, GODS_LAWS.md L-09's own control - gets a clean
    // std::nullopt here, never an out-of-bounds read, regardless of
    // whether poll() was ever called at all.
    if (m_buffer.size() < wire_header_size) {
        return std::nullopt;
    }
    const message_header header = parse_header_unchecked(m_buffer.data());
    if (header.size < wire_header_size || header.size > wire_message_size_cap ||
        m_buffer.size() < header.size) {
        return std::nullopt;
    }
    decoded_message result;
    result.header = header;
    result.payload.assign(m_buffer.begin() + static_cast<std::ptrdiff_t>(wire_header_size),
                          m_buffer.begin() + static_cast<std::ptrdiff_t>(header.size));
    m_buffer.erase(m_buffer.begin(), m_buffer.begin() + static_cast<std::ptrdiff_t>(header.size));
    return result;
}

} // namespace glintfx::test::wire_relay
