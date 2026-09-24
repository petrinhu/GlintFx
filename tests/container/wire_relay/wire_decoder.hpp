// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_message.hpp"

#include <cstdint>
#include <optional>
#include <vector>

// wire_decoder.hpp - the DECODER atom of the wire relay
// (docs/plano-w7c.md SS3.A: "decodificador do fio (cabecalho de 8
// bytes, mensagem partida entre leituras, teto de 4096)").
//
// Pure byte-level parser: no socket, no Wayland semantics (that is
// wire_object_table's job). feed() may be called with any chunk
// size, including a chunk that splits a single message's header or
// body across two calls - the WL-ACK-SMOKE-BLUNT A1 red control
// "mensagem partida em duas leituras" - and the decoder keeps
// whatever partial bytes it has buffered across calls.
namespace glintfx::test::wire_relay {

enum class decode_outcome {
    need_more_bytes,
    message_ready,
    // The header committed to a size beyond wire_message_size_cap -
    // GODS_LAWS.md L-40: never silently dropped, the caller must
    // treat this as fatal for the connection.
    message_too_large,
};

class wire_decoder {
  public:
    void feed(const std::uint8_t *data, std::size_t len);

    [[nodiscard]] decode_outcome poll() const;

    // Normal use: poll() == message_ready, then this consumes exactly
    // one message's bytes from the internal buffer. Defensive even
    // when that precondition is violated (called with a short or
    // partial buffer): re-checks the same bound poll() checks, INSIDE
    // this function, and returns std::nullopt instead of reading past
    // m_buffer's end - a caller that skips poll() (or a future one
    // that gets the order wrong) gets a clean "no message" instead of
    // undefined behaviour. Proven by wire_relay_a1_take_message_on_
    // short_buffer_is_clean.
    [[nodiscard]] std::optional<decoded_message> take_message();

  private:
    std::vector<std::uint8_t> m_buffer;
};

} // namespace glintfx::test::wire_relay
