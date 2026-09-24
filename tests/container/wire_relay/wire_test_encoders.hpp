// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_decoder.hpp"
#include "wire_transport.hpp"

#include "harness/check.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

// wire_test_encoders.hpp - shared test-only glue for the wire_relay
// selftests (WL-ACK-SMOKE-BLUNT A1/A2, docs/plano-w7c.md SS3.A). NOT
// an atom of the relay itself (GODS_LAWS.md L-17's "um arquivo por
// atomo" does not apply here the way it applies to wire_decoder.hpp/
// wire_transport.hpp/etc - this is infrastructure the TEST files
// share, the same role tests/harness/ plays for the whole suite):
// socketpair setup, and encoders that build the raw Wayland wire
// bytes a fake client/upstream sends in these labs. `inline` because
// this header is included from more than one translation unit
// (wire_relay_a1_selftest.cpp and wire_relay_a2_selftest.cpp).
namespace glintfx::test::wire_relay {

struct socket_pair {
    int a = -1;
    int b = -1;
};

inline socket_pair make_socketpair() {
    int fds[2] = {-1, -1};
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    return socket_pair{fds[0], fds[1]};
}

inline void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &value, sizeof(bytes));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

inline void append_wire_string(std::vector<std::uint8_t> &out, std::string_view text) {
    const auto stored_len = static_cast<std::uint32_t>(text.size() + 1); // + NUL
    append_u32(out, stored_len);
    out.insert(out.end(), text.begin(), text.end());
    out.push_back(0); // the NUL terminator stored_len counts
    while (out.size() % 4 != 0) {
        out.push_back(0); // pad the argument to 4-byte alignment
    }
}

inline std::vector<std::uint8_t> encode_message(std::uint32_t object_id, std::uint16_t opcode,
                                                const std::vector<std::uint8_t> &body) {
    std::vector<std::uint8_t> out;
    append_u32(out, object_id);
    const auto opcode_and_size = static_cast<std::uint32_t>(opcode) |
                                 (static_cast<std::uint32_t>(wire_header_size + body.size()) << 16);
    append_u32(out, opcode_and_size);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

inline std::vector<std::uint8_t> encode_new_id_request(std::uint32_t object_id,
                                                       std::uint16_t opcode, std::uint32_t new_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, new_id);
    return encode_message(object_id, opcode, body);
}

inline std::vector<std::uint8_t> encode_registry_bind(std::uint32_t registry_id, std::uint32_t name,
                                                      std::string_view interface,
                                                      std::uint32_t version, std::uint32_t new_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, name);
    append_wire_string(body, interface);
    append_u32(body, version);
    append_u32(body, new_id);
    return encode_message(registry_id, 0, body);
}

inline std::vector<std::uint8_t> encode_get_xdg_surface(std::uint32_t xdg_wm_base_id,
                                                        std::uint32_t new_id,
                                                        std::uint32_t surface_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, new_id);
    append_u32(body, surface_id);
    return encode_message(xdg_wm_base_id, 2, body);
}

inline std::vector<std::uint8_t> encode_shm_create_pool(std::uint32_t shm_id, std::uint32_t new_id,
                                                        std::int32_t size) {
    std::vector<std::uint8_t> body;
    append_u32(body, new_id);
    append_u32(body, static_cast<std::uint32_t>(size));
    return encode_message(shm_id, 0, body);
}

inline std::vector<std::uint8_t> encode_surface_attach(std::uint32_t surface_id,
                                                       std::uint32_t buffer_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, buffer_id);
    append_u32(body, 0); // x
    append_u32(body, 0); // y
    return encode_message(surface_id, 1, body);
}

inline std::vector<std::uint8_t> encode_surface_commit(std::uint32_t surface_id) {
    return encode_message(surface_id, 6, {});
}

inline std::vector<std::uint8_t> encode_configure(std::uint32_t xdg_surface_id,
                                                  std::uint32_t serial) {
    std::vector<std::uint8_t> body;
    append_u32(body, serial);
    return encode_message(xdg_surface_id, 0, body);
}

inline std::vector<std::uint8_t> encode_ack_configure(std::uint32_t xdg_surface_id,
                                                      std::uint32_t serial) {
    std::vector<std::uint8_t> body;
    append_u32(body, serial);
    return encode_message(xdg_surface_id, 4, body);
}

// take_message() is defensive (returns std::optional, see wire_
// decoder.hpp's own comment); every NORMAL test call site already
// confirmed poll() == message_ready first, so a std::nullopt here
// would mean the harness's own precondition tracking is broken, not
// the scenario under test - GLINTFX_CHECK stops the case cleanly
// instead of dereferencing an empty optional.
inline decoded_message expect_message(wire_decoder &decoder) {
    std::optional<decoded_message> message = decoder.take_message();
    GLINTFX_CHECK(message.has_value());
    return *message;
}

} // namespace glintfx::test::wire_relay
