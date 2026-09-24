// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// wire_message.hpp - shared vocabulary type of the wire relay
// (docs/plano-w7c.md SS3.A, WL-ACK-SMOKE-BLUNT sub-fatias A1/A2).
//
// Every atom of the relay (wire_decoder, wire_object_table, wire_
// rule_engine, wire_error_injector) speaks the SAME 8-byte-header
// Wayland wire format - the same wl_closure wire layout xdg-shell.xml
// and wayland.xml describe and every Wayland client library
// implements (read to LEARN the shape, GODS_LAWS.md L-29 - nothing
// here links or copies libwayland): a uint32 object id, then a
// uint32 word packing a uint16 opcode in its low bits and a uint16
// total-message-size (header included) in its high bits, then the
// argument bytes.
//
// This header carries only the DATA this format implies - no parsing,
// no sockets, no Wayland semantics (interface names, argument
// layouts). Those live one layer up, in wire_decoder.hpp (framing)
// and wire_object_table.hpp (semantics).
namespace glintfx::test::wire_relay {

struct message_header {
    std::uint32_t object_id = 0;
    std::uint16_t opcode = 0;
    std::uint16_t size = 0; // includes this 8-byte header
};

// A fully framed message: header plus its raw argument bytes
// (payload.size() == header.size - wire_header_size), never
// interpreted by the decoder that produces it.
struct decoded_message {
    message_header header;
    std::vector<std::uint8_t> payload;
};

inline constexpr std::size_t wire_header_size = 8;

// docs/plano-w7c.md SS3.A, "arquitetura interna": "mensagens ficam
// abaixo de 4096 bytes" (mstoeckl's relay-technique note, cited in
// the plan's own research section) - the cap this decoder enforces
// so a message this relay cannot bound is never silently grown
// without limit (GODS_LAWS.md L-40).
inline constexpr std::size_t wire_message_size_cap = 4096;

} // namespace glintfx::test::wire_relay
