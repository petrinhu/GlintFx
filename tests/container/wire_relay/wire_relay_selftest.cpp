// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_selftest.cpp - autoteste de WL-ACK-SMOKE-BLUNT, sub-
// fatias A1 (transporte + decodificador do rele, modo transparente,
// sem regra) e A2 (motor de regras R1/R2 + injetor de erro) -
// docs/plano-w7c.md SS3.A, docs/plano-w7c-adendo-revalidacao.md
// SS3.A.
//
// GODS_LAWS.md L-09 (inegociavel, texto do lider verbatim: "nenhum
// teste dinamico toca minha sessao, sempre fazer em docker"; a
// EXCECAO documentada e' esta - PAR DE SOQUETES local e montante
// falso nunca tocam janela, teclado, mouse, tela, wayland-0,
// XDG_RUNTIME_DIR nem /dev/uinput; roda direto via ctest, no mesmo
// nivel dos demais *_selftest de tests/CMakeLists.txt): todo caso
// abaixo usa socketpair(AF_UNIX, SOCK_STREAM, ...) como o UNICO
// mecanismo de I/O - nenhum compositor, nenhum container.
//
// A2 acrescenta a orquestracao ("decodifica, classifica, avalia,
// repassa ou injeta erro") que os cinco atomos do rele formam juntos
// - ela mora AQUI, e nao num sexto atomo de producao, porque ligar
// isso num binario de fato em frente ao KWin e' A3 (docs/plano-w7c-
// adendo-revalidacao.md SS3.A), fora do escopo desta ordem de
// servico.
#include "wire_decoder.hpp"
#include "wire_error_injector.hpp"
#include "wire_object_table.hpp"
#include "wire_rule_engine.hpp"
#include "wire_transport.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

namespace {

using namespace glintfx::test::wire_relay;

// ---- socketpair + wire encoding helpers (test-only glue - the real
// ---- relay binary A3 builds gets its own encoder for what IT sends;
// ---- these exist only to fabricate a fake client/upstream) --------

struct socket_pair {
    int a = -1;
    int b = -1;
};

socket_pair make_socketpair() {
    int fds[2] = {-1, -1};
    ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
    return socket_pair{fds[0], fds[1]};
}

void append_u32(std::vector<std::uint8_t> &out, std::uint32_t value) {
    std::uint8_t bytes[4];
    std::memcpy(bytes, &value, sizeof(bytes));
    out.insert(out.end(), bytes, bytes + sizeof(bytes));
}

void append_wire_string(std::vector<std::uint8_t> &out, std::string_view text) {
    auto stored_len = static_cast<std::uint32_t>(text.size() + 1); // + NUL
    append_u32(out, stored_len);
    out.insert(out.end(), text.begin(), text.end());
    out.push_back(0);
    while (out.size() % 4 != 0) {
        out.push_back(0);
    }
}

std::vector<std::uint8_t> encode_message(std::uint32_t object_id, std::uint16_t opcode,
                                         const std::vector<std::uint8_t> &body) {
    std::vector<std::uint8_t> out;
    append_u32(out, object_id);
    auto opcode_and_size = static_cast<std::uint32_t>(opcode) |
                           (static_cast<std::uint32_t>(wire_header_size + body.size()) << 16);
    append_u32(out, opcode_and_size);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::vector<std::uint8_t> encode_new_id_request(std::uint32_t object_id, std::uint16_t opcode,
                                                std::uint32_t new_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, new_id);
    return encode_message(object_id, opcode, body);
}

std::vector<std::uint8_t> encode_registry_bind(std::uint32_t registry_id, std::uint32_t name,
                                               std::string_view interface, std::uint32_t version,
                                               std::uint32_t new_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, name);
    append_wire_string(body, interface);
    append_u32(body, version);
    append_u32(body, new_id);
    return encode_message(registry_id, 0, body);
}

std::vector<std::uint8_t> encode_shm_create_pool(std::uint32_t shm_id, std::uint32_t new_id,
                                                 std::int32_t size) {
    std::vector<std::uint8_t> body;
    append_u32(body, new_id);
    append_u32(body, static_cast<std::uint32_t>(size));
    return encode_message(shm_id, 0, body);
}

std::vector<std::uint8_t> encode_get_xdg_surface(std::uint32_t xdg_wm_base_id, std::uint32_t new_id,
                                                 std::uint32_t surface_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, new_id);
    append_u32(body, surface_id);
    return encode_message(xdg_wm_base_id, 2, body);
}

std::vector<std::uint8_t> encode_surface_attach(std::uint32_t surface_id, std::uint32_t buffer_id) {
    std::vector<std::uint8_t> body;
    append_u32(body, buffer_id);
    append_u32(body, 0); // x
    append_u32(body, 0); // y
    return encode_message(surface_id, 1, body);
}

std::vector<std::uint8_t> encode_surface_commit(std::uint32_t surface_id) {
    return encode_message(surface_id, 6, {});
}

std::vector<std::uint8_t> encode_configure(std::uint32_t xdg_surface_id, std::uint32_t serial) {
    std::vector<std::uint8_t> body;
    append_u32(body, serial);
    return encode_message(xdg_surface_id, 0, body);
}

std::vector<std::uint8_t> encode_ack_configure(std::uint32_t xdg_surface_id, std::uint32_t serial) {
    std::vector<std::uint8_t> body;
    append_u32(body, serial);
    return encode_message(xdg_surface_id, 4, body);
}

// ---- the pipeline the real relay will also assemble from the same
// ---- five atoms (see this file's own header comment) --------------

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
        auto xdg_id = pipe.table.xdg_surface_for(message.header.object_id);
        if (buffer_id != 0 && xdg_id) {
            pipe.engine.note_buffer_attached(*xdg_id);
        }
        return std::nullopt;
    }
    if (from_client && source == known_interface::wl_surface && message.header.opcode == 6) {
        auto xdg_id = pipe.table.xdg_surface_for(message.header.object_id);
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
        wire_transport::read_result chunk = reader.read_once();
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
    }
    const decoded_message message = decoder.take_message();
    return observe_and_evaluate(pipe, message, from_client);
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

// ============================================================
// A1 - transporte + decodificador, modo transparente (sem regra)
// ============================================================

GLINTFX_TEST(wire_relay_a1_message_split_across_reads) {
    const socket_pair sp = make_socketpair();
    GLINTFX_CHECK(sp.a >= 0 && sp.b >= 0);
    const wire_transport reader(sp.b);

    std::vector<std::uint8_t> message = encode_registry_bind(2, 0, "xdg_wm_base", 1, 3);
    GLINTFX_CHECK(message.size() > 16);
    const std::size_t first_len = message.size() / 2;

    GLINTFX_CHECK(::send(sp.a, message.data(), first_len, MSG_NOSIGNAL) ==
                  static_cast<ssize_t>(first_len));
    wire_decoder decoder;
    wire_transport::read_result first_chunk = reader.read_once();
    decoder.feed(first_chunk.bytes.data(), first_chunk.bytes.size());
    GLINTFX_CHECK(decoder.poll() == decode_outcome::need_more_bytes);

    const std::size_t remaining = message.size() - first_len;
    GLINTFX_CHECK(::send(sp.a, message.data() + first_len, remaining, MSG_NOSIGNAL) ==
                  static_cast<ssize_t>(remaining));
    wire_transport::read_result second_chunk = reader.read_once();
    decoder.feed(second_chunk.bytes.data(), second_chunk.bytes.size());
    GLINTFX_CHECK(decoder.poll() == decode_outcome::message_ready);

    const decoded_message decoded = decoder.take_message();
    GLINTFX_CHECK_EQ(decoded.header.object_id, 2u);
    GLINTFX_CHECK_EQ(decoded.header.opcode, 0u);
    GLINTFX_CHECK_EQ(decoded.payload.size(), message.size() - wire_header_size);

    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a1_descriptor_with_correct_message) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);
    wire_object_table table;

    std::vector<std::uint8_t> get_registry = encode_new_id_request(1, 1, 2);
    std::vector<std::uint8_t> bind_shm = encode_registry_bind(2, 0, "wl_shm", 1, 3);
    writer.write_once(get_registry.data(), get_registry.size(), {});
    writer.write_once(bind_shm.data(), bind_shm.size(), {});

    wire_decoder decoder;
    while (table.interface_of(3) != known_interface::wl_shm) {
        wire_transport::read_result chunk = reader.read_once();
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
        while (decoder.poll() == decode_outcome::message_ready) {
            table.observe(decoder.take_message(), true);
        }
    }

    int pipe_fds[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(pipe_fds) == 0);
    std::vector<std::uint8_t> create_pool = encode_shm_create_pool(3, 4, 100);
    writer.write_once(create_pool.data(), create_pool.size(), {pipe_fds[0]});

    wire_decoder pool_decoder;
    wire_transport::read_result pool_chunk = reader.read_once();
    pool_decoder.feed(pool_chunk.bytes.data(), pool_chunk.bytes.size());
    GLINTFX_CHECK_EQ(pool_chunk.fds.size(), std::size_t{1});
    GLINTFX_CHECK(pool_decoder.poll() == decode_outcome::message_ready);
    const decoded_message pool_message = pool_decoder.take_message();
    GLINTFX_CHECK_EQ(fd_argument_count(known_interface::wl_shm, pool_message.header.opcode),
                     std::size_t{1});
    GLINTFX_CHECK(table.observe(pool_message, true).has_value());
    GLINTFX_CHECK(table.interface_of(4) == known_interface::wl_shm_pool);

    // Prove the descriptor is the SAME underlying pipe, not garbage:
    // write through the original write end, read back through the
    // duplicate this relay atom says arrived with THIS message.
    const char probe = 'X';
    GLINTFX_CHECK(::write(pipe_fds[1], &probe, 1) == 1);
    char observed = 0;
    GLINTFX_CHECK(::read(pool_chunk.fds[0], &observed, 1) == 1);
    GLINTFX_CHECK_EQ(observed, probe);

    ::close(pipe_fds[0]);
    ::close(pipe_fds[1]);
    ::close(pool_chunk.fds[0]);
    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a1_control_truncated_is_fatal) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);

    int pipe_fds[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(pipe_fds) == 0);
    const std::vector<int> too_many_fds(wire_transport::max_fds_per_read + 2, pipe_fds[0]);
    std::vector<std::uint8_t> tiny_message = encode_surface_commit(9);
    writer.write_once(tiny_message.data(), tiny_message.size(), too_many_fds);

    const wire_transport::read_result chunk = reader.read_once();
    GLINTFX_CHECK(chunk.control_truncated);

    for (const int fd : chunk.fds) {
        ::close(fd);
    }
    ::close(pipe_fds[0]);
    ::close(pipe_fds[1]);
    ::close(sp.a);
    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a1_unknown_interface_passthrough_counted) {
    wire_object_table table;
    const std::size_t before = table.unknown_object_count();

    std::vector<std::uint8_t> get_registry = encode_new_id_request(1, 1, 2);
    std::vector<std::uint8_t> bind_output = encode_registry_bind(2, 0, "wl_output", 2, 5);
    wire_decoder decoder;
    decoder.feed(get_registry.data(), get_registry.size());
    decoder.feed(bind_output.data(), bind_output.size());

    std::size_t decoded_count = 0;
    while (decoder.poll() == decode_outcome::message_ready) {
        table.observe(decoder.take_message(), true);
        ++decoded_count;
    }
    GLINTFX_CHECK_EQ(decoded_count, std::size_t{2});
    GLINTFX_CHECK(table.interface_of(5) == known_interface::unknown);
    GLINTFX_CHECK_EQ(table.unknown_object_count(), before + 1);

    // A message FROM that unknown object, with garbage arguments,
    // must never be analyzed - and must never crash observe().
    std::vector<std::uint8_t> garbage = encode_message(5, 7, {0xDE, 0xAD});
    wire_decoder decoder2;
    decoder2.feed(garbage.data(), garbage.size());
    GLINTFX_CHECK(decoder2.poll() == decode_outcome::message_ready);
    auto result = table.observe(decoder2.take_message(), true);
    GLINTFX_CHECK(!result.has_value());
}

GLINTFX_TEST(wire_relay_a1_delete_id_retires_object) {
    // wl_display.delete_id (event, opcode 1, from_client=false) is
    // the table atom's own retirement path - docs/plano-w7c.md SS3.A:
    // "delete_id aposenta o numero".
    wire_object_table table;
    std::vector<std::uint8_t> get_registry = encode_new_id_request(1, 1, 2);
    std::vector<std::uint8_t> bind_shm = encode_registry_bind(2, 0, "wl_shm", 1, 3);
    wire_decoder decoder;
    decoder.feed(get_registry.data(), get_registry.size());
    decoder.feed(bind_shm.data(), bind_shm.size());
    while (decoder.poll() == decode_outcome::message_ready) {
        table.observe(decoder.take_message(), true);
    }
    GLINTFX_CHECK(table.interface_of(3) == known_interface::wl_shm);

    std::vector<std::uint8_t> delete_id_event = encode_new_id_request(1, 1, 3);
    wire_decoder event_decoder;
    event_decoder.feed(delete_id_event.data(), delete_id_event.size());
    GLINTFX_CHECK(event_decoder.poll() == decode_outcome::message_ready);
    table.observe(event_decoder.take_message(), /*from_client=*/false);
    GLINTFX_CHECK(table.interface_of(3) == known_interface::unknown);
}

GLINTFX_TEST(wire_relay_a1_zero_messages_decoded_is_flagged) {
    // A peer that writes less than one header's worth of bytes and
    // vanishes never yields a message - GODS_LAWS.md L-40: the
    // counting mechanism a real gate relies on (docs/plano-w7c.md
    // SS3.A A5: "N mensagens ...  N = 0 reprova") has to be able to
    // SEE that zero, not paper over it.
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);

    std::uint8_t partial_header[4] = {1, 0, 0, 0};
    writer.write_once(partial_header, sizeof(partial_header), {});
    ::close(sp.a); // vanish before completing even the header

    wire_decoder decoder;
    std::size_t decoded_count = 0;
    while (true) {
        wire_transport::read_result chunk = reader.read_once();
        if (chunk.end_of_file) {
            break;
        }
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
        while (decoder.poll() == decode_outcome::message_ready) {
            (void)decoder.take_message();
            ++decoded_count;
        }
    }
    GLINTFX_CHECK_EQ(decoded_count, std::size_t{0});

    ::close(sp.b);
}

GLINTFX_TEST(wire_relay_a1_eof_drains_before_shutdown) {
    const socket_pair upstream = make_socketpair();   // fake upstream <-> relay
    const socket_pair downstream = make_socketpair(); // relay <-> client

    const wire_transport upstream_write(upstream.a);
    const wire_transport relay_upstream_read(upstream.b);
    const wire_transport relay_downstream_write(downstream.a);
    const wire_transport client_read(downstream.b);

    std::vector<std::uint8_t> message = encode_surface_commit(9);
    upstream_write.write_once(message.data(), message.size(), {});
    ::close(upstream.a); // fake upstream is done - EOF on the next read

    // The relay's transparent loop (A1: no rules) - read from
    // upstream, forward whatever arrived to the client, THEN react
    // to EOF (D-A4: propagate every byte already read before
    // shutdown(SHUT_WR), never close before draining).
    std::vector<std::uint8_t> forwarded;
    while (true) {
        wire_transport::read_result chunk = relay_upstream_read.read_once();
        if (chunk.end_of_file) {
            relay_downstream_write.shutdown_write();
            break;
        }
        relay_downstream_write.write_once(chunk.bytes.data(), chunk.bytes.size(), chunk.fds);
        forwarded.insert(forwarded.end(), chunk.bytes.begin(), chunk.bytes.end());
    }
    GLINTFX_CHECK(forwarded == message);

    const wire_transport::read_result client_chunk = client_read.read_once();
    GLINTFX_CHECK(client_chunk.bytes == message);
    GLINTFX_CHECK(!client_chunk.end_of_file);

    const wire_transport::read_result client_eof = client_read.read_once();
    GLINTFX_CHECK(client_eof.end_of_file);

    ::close(upstream.b);
    ::close(downstream.a);
    ::close(downstream.b);
}

// ============================================================
// A2 - motor de regras R1/R2 e injetor de erro
// ============================================================

GLINTFX_TEST(wire_relay_a2_ack_after_configure_passes) {
    const socket_pair sp = make_socketpair();
    const wire_transport writer(sp.a);
    const wire_transport reader(sp.b);
    pipeline pipe;
    bootstrap_xdg_surface(writer, reader, pipe, 10, 11);
    GLINTFX_CHECK(pipe.table.interface_of(11) == known_interface::xdg_surface);

    auto configure_result = feed_one(writer, reader, pipe, encode_configure(11, 42), false);
    GLINTFX_CHECK(!configure_result.has_value());

    auto ack_result = feed_one(writer, reader, pipe, encode_ack_configure(11, 42), true);
    GLINTFX_CHECK(!ack_result.has_value());

    feed_one(writer, reader, pipe, encode_surface_attach(10, 99), true);
    auto commit_result = feed_one(writer, reader, pipe, encode_surface_commit(10), true);
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
    auto violation = feed_one(writer, reader, pipe, encode_surface_commit(10), true);
    GLINTFX_CHECK(violation.has_value());
    GLINTFX_CHECK_EQ(violation->offending_object_id, 11u);
    GLINTFX_CHECK(violation->code == xdg_surface_error::unconfigured_buffer);

    std::vector<std::uint8_t> encoded =
        encode_display_error(*violation, "attach without configure");
    wire_decoder verify;
    verify.feed(encoded.data(), encoded.size());
    GLINTFX_CHECK(verify.poll() == decode_outcome::message_ready);
    decoded_message error_message = verify.take_message();
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
    auto violation = feed_one(writer, reader, pipe, encode_ack_configure(11, 999), true);
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
    auto ack_result = feed_one(writer, reader, pipe, encode_ack_configure(11, 2), true);
    GLINTFX_CHECK(!ack_result.has_value());

    feed_one(writer, reader, pipe, encode_surface_attach(10, 99), true);
    auto commit_result = feed_one(writer, reader, pipe, encode_surface_commit(10), true);
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
