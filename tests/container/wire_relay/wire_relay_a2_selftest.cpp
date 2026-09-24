// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_a2_selftest.cpp - autoteste de WL-ACK-SMOKE-BLUNT, sub-
// fatia A2 (motor de regras R1/R2 + injetor de erro) - docs/plano-
// w7c.md SS3.A. A1 tem o proprio arquivo, wire_relay_a1_selftest.cpp
// (GODS_LAWS.md L-17/L-34: um arquivo de casos por sub-fatia).
//
// A orquestracao abaixo ("decodifica, classifica, avalia, repassa ou
// injeta erro") compoe os cinco atomos do rele exatamente como o
// rele real vai compo-los - ela mora AQUI, e nao num sexto atomo de
// producao, porque ligar isso num binario de fato em frente ao KWin
// e' A3 (docs/plano-w7c-adendo-revalidacao.md SS3.A), fora do escopo
// desta ordem de servico.
#include "wire_decoder.hpp"
#include "wire_error_injector.hpp"
#include "wire_object_table.hpp"
#include "wire_rule_engine.hpp"
#include "wire_test_encoders.hpp"
#include "wire_transport.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace {

using namespace glintfx::test::wire_relay;

struct pipeline {
    wire_object_table table;
    wire_rule_engine engine;
};

std::optional<rule_violation> observe_and_evaluate(pipeline &pipe, const decoded_message &message,
                                                   bool from_client) {
    pipe.table.observe(message, from_client);
    const known_interface source = pipe.table.interface_of(message.header.object_id);

    if (from_client && source == known_interface::wl_surface && message.header.opcode == 1) {
        std::uint32_t buffer_id = 0;
        if (message.payload.size() >= sizeof(buffer_id)) {
            std::memcpy(&buffer_id, message.payload.data(), sizeof(buffer_id));
        }
        const auto xdg_id = pipe.table.xdg_surface_for(message.header.object_id);
        if (buffer_id != 0 && xdg_id) {
            pipe.engine.note_buffer_attached(*xdg_id);
        }
        return std::nullopt;
    }
    if (from_client && source == known_interface::wl_surface && message.header.opcode == 6) {
        const auto xdg_id = pipe.table.xdg_surface_for(message.header.object_id);
        return xdg_id ? pipe.engine.note_commit(*xdg_id) : std::nullopt;
    }
    if (from_client && source == known_interface::xdg_surface && message.header.opcode == 4) {
        std::uint32_t serial = 0;
        if (message.payload.size() >= sizeof(serial)) {
            std::memcpy(&serial, message.payload.data(), sizeof(serial));
        }
        return pipe.engine.note_ack_configure(message.header.object_id, serial);
    }
    if (!from_client && source == known_interface::xdg_surface && message.header.opcode == 0) {
        std::uint32_t serial = 0;
        if (message.payload.size() >= sizeof(serial)) {
            std::memcpy(&serial, message.payload.data(), sizeof(serial));
        }
        pipe.engine.note_configure_sent(message.header.object_id, serial);
    }
    return std::nullopt;
}

// Sends one message down `writer`, reads it back up through `reader`
// (looping read_once() until a complete message is framed - proves
// nothing here depends on a message arriving in a single recv()),
// and feeds it through the shared pipeline.
std::optional<rule_violation> feed_one(const wire_transport &writer, const wire_transport &reader,
                                       pipeline &pipe, const std::vector<std::uint8_t> &bytes,
                                       bool from_client) {
    writer.write_once(bytes.data(), bytes.size(), {});
    wire_decoder decoder;
    while (decoder.poll() != decode_outcome::message_ready) {
        const wire_transport::read_result chunk = reader.read_once();
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
    }
    return observe_and_evaluate(pipe, expect_message(decoder), from_client);
}

// Client bootstrap common to every A2 case: get_registry -> bind
// wl_compositor -> bind xdg_wm_base -> create_surface -> get_xdg_
// surface. Mirrors the exact request order a real client makes.
void bootstrap_xdg_surface(const wire_transport &writer, const wire_transport &reader,
                           pipeline &pipe, std::uint32_t surface_id, std::uint32_t xdg_surface_id) {
    feed_one(writer, reader, pipe, encode_new_id_request(1, 1, 2), true); // get_registry -> 2
    feed_one(writer, reader, pipe, encode_registry_bind(2, 0, "wl_compositor", 4, 3), true);
    feed_one(writer, reader, pipe, encode_registry_bind(2, 1, "xdg_wm_base", 2, 4), true);
    feed_one(writer, reader, pipe, encode_new_id_request(3, 0, surface_id), true);
    feed_one(writer, reader, pipe, encode_get_xdg_surface(4, xdg_surface_id, surface_id), true);
}

} // namespace

GLINTFX_TEST(wire_relay_a2_ack_after_configure_passes) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);
    pipeline pipe;
    bootstrap_xdg_surface(writer, reader, pipe, 10, 11);
    GLINTFX_CHECK(pipe.table.interface_of(11) == known_interface::xdg_surface);

    const auto configure_result = feed_one(writer, reader, pipe, encode_configure(11, 42), false);
    GLINTFX_CHECK(!configure_result.has_value());

    const auto ack_result = feed_one(writer, reader, pipe, encode_ack_configure(11, 42), true);
    GLINTFX_CHECK(!ack_result.has_value());

    feed_one(writer, reader, pipe, encode_surface_attach(10, 99), true);
    const auto commit_result = feed_one(writer, reader, pipe, encode_surface_commit(10), true);
    GLINTFX_CHECK(!commit_result.has_value());
    GLINTFX_CHECK(pipe.engine.rules_evaluated() > 0);

    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a2_commit_before_ack_raises_error3) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);
    pipeline pipe;
    bootstrap_xdg_surface(writer, reader, pipe, 10, 11);

    feed_one(writer, reader, pipe, encode_surface_attach(10, 99), true);
    const auto violation = feed_one(writer, reader, pipe, encode_surface_commit(10), true);
    GLINTFX_CHECK(violation.has_value());
    GLINTFX_CHECK_EQ(violation->offending_object_id, 11u);
    GLINTFX_CHECK(violation->code == xdg_surface_error::unconfigured_buffer);

    const std::vector<std::uint8_t> encoded =
        encode_display_error(*violation, "attach without configure");
    wire_decoder verify;
    verify.feed(encoded.data(), encoded.size());
    GLINTFX_CHECK(verify.poll() == decode_outcome::message_ready);
    const decoded_message error_message = expect_message(verify);
    GLINTFX_CHECK_EQ(error_message.header.object_id, wl_display_object_id);
    GLINTFX_CHECK_EQ(error_message.header.opcode, wl_display_error_event_opcode);
    std::uint32_t decoded_object_id = 0;
    std::memcpy(&decoded_object_id, error_message.payload.data(), sizeof(decoded_object_id));
    GLINTFX_CHECK_EQ(decoded_object_id, 11u);

    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a2_unknown_serial_raises_error4) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);
    pipeline pipe;
    bootstrap_xdg_surface(writer, reader, pipe, 10, 11);

    feed_one(writer, reader, pipe, encode_configure(11, 5), false);
    const auto violation = feed_one(writer, reader, pipe, encode_ack_configure(11, 999), true);
    GLINTFX_CHECK(violation.has_value());
    GLINTFX_CHECK_EQ(violation->offending_object_id, 11u);
    GLINTFX_CHECK(violation->code == xdg_surface_error::invalid_serial);

    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a2_ack_only_last_of_two_configures_passes) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);
    pipeline pipe;
    bootstrap_xdg_surface(writer, reader, pipe, 10, 11);

    feed_one(writer, reader, pipe, encode_configure(11, 1), false);
    feed_one(writer, reader, pipe, encode_configure(11, 2), false);
    const auto ack_result = feed_one(writer, reader, pipe, encode_ack_configure(11, 2), true);
    GLINTFX_CHECK(!ack_result.has_value());

    feed_one(writer, reader, pipe, encode_surface_attach(10, 99), true);
    const auto commit_result = feed_one(writer, reader, pipe, encode_surface_commit(10), true);
    GLINTFX_CHECK(!commit_result.has_value());

    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a2_empty_scan_is_flagged) {
    // GODS_LAWS.md L-40: a rule engine nobody ever fed has to report
    // that honestly (zero, not a made-up "nothing failed so it
    // passed") - the same zero-floor A1's message counter proves.
    const wire_rule_engine engine;
    GLINTFX_CHECK_EQ(engine.rules_evaluated(), std::size_t{0});
}
