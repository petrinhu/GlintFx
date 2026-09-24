// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_a1_selftest.cpp - autoteste de WL-ACK-SMOKE-BLUNT, sub-
// fatia A1 (transporte + decodificador do rele, modo transparente,
// sem regra) - docs/plano-w7c.md SS3.A, docs/plano-w7c-adendo-
// revalidacao.md SS3.A. A2 tem o proprio arquivo, wire_relay_a2_
// selftest.cpp (GODS_LAWS.md L-17/L-34: um arquivo de casos por
// atomo/sub-fatia, o mesmo padrao que se aplica a codigo de producao
// tambem se aplica ao teste).
//
// GODS_LAWS.md L-09 (inegociavel, texto do lider verbatim: "nenhum
// teste dinamico toca minha sessao, sempre fazer em docker"; a
// EXCECAO documentada e' esta - PAR DE SOQUETES local e montante
// falso nunca tocam janela, teclado, mouse, tela, wayland-0,
// XDG_RUNTIME_DIR nem /dev/uinput; roda direto via ctest, no mesmo
// nivel dos demais *_selftest de tests/CMakeLists.txt): todo caso
// abaixo usa socketpair(AF_UNIX, SOCK_STREAM, ...) como o UNICO
// mecanismo de I/O - nenhum compositor, nenhum container.
#include "wire_decoder.hpp"
#include "wire_object_table.hpp"
#include "wire_test_encoders.hpp"
#include "wire_transport.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <unistd.h>

#include <cstdint>
#include <optional>
#include <vector>

using namespace glintfx::test::wire_relay;

GLINTFX_TEST(wire_relay_a1_message_split_across_reads) {
    const socket_pair sp = make_socketpair();
    GLINTFX_CHECK(sp.a >= 0 && sp.b >= 0);
    const wire_transport reader(sp.b);

    const std::vector<std::uint8_t> message = encode_registry_bind(2, 0, "xdg_wm_base", 1, 3);
    GLINTFX_CHECK(message.size() > 16);
    const std::size_t first_len = message.size() / 2;

    GLINTFX_CHECK(::send(sp.a, message.data(), first_len, MSG_NOSIGNAL) ==
                  static_cast<ssize_t>(first_len));
    wire_decoder decoder;
    const wire_transport::read_result first_chunk = reader.read_once();
    decoder.feed(first_chunk.bytes.data(), first_chunk.bytes.size());
    GLINTFX_CHECK(decoder.poll() == decode_outcome::need_more_bytes);

    const std::size_t remaining = message.size() - first_len;
    GLINTFX_CHECK(::send(sp.a, message.data() + first_len, remaining, MSG_NOSIGNAL) ==
                  static_cast<ssize_t>(remaining));
    const wire_transport::read_result second_chunk = reader.read_once();
    decoder.feed(second_chunk.bytes.data(), second_chunk.bytes.size());
    GLINTFX_CHECK(decoder.poll() == decode_outcome::message_ready);

    const decoded_message decoded = expect_message(decoder);
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

    const std::vector<std::uint8_t> get_registry = encode_new_id_request(1, 1, 2);
    const std::vector<std::uint8_t> bind_shm = encode_registry_bind(2, 0, "wl_shm", 1, 3);
    writer.write_once(get_registry.data(), get_registry.size(), {});
    writer.write_once(bind_shm.data(), bind_shm.size(), {});

    wire_decoder decoder;
    while (table.interface_of(3) != known_interface::wl_shm) {
        const wire_transport::read_result chunk = reader.read_once();
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
        while (decoder.poll() == decode_outcome::message_ready) {
            table.observe(expect_message(decoder), true);
        }
    }

    int pipe_fds[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(pipe_fds) == 0);
    const std::vector<std::uint8_t> create_pool = encode_shm_create_pool(3, 4, 100);
    writer.write_once(create_pool.data(), create_pool.size(), {pipe_fds[0]});

    wire_decoder pool_decoder;
    const wire_transport::read_result pool_chunk = reader.read_once();
    pool_decoder.feed(pool_chunk.bytes.data(), pool_chunk.bytes.size());
    GLINTFX_CHECK_EQ(pool_chunk.fds.size(), std::size_t{1});
    GLINTFX_CHECK(pool_decoder.poll() == decode_outcome::message_ready);
    const decoded_message pool_message = expect_message(pool_decoder);
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
    const std::vector<std::uint8_t> tiny_message = encode_surface_commit(9);
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

    const std::vector<std::uint8_t> get_registry = encode_new_id_request(1, 1, 2);
    const std::vector<std::uint8_t> bind_output = encode_registry_bind(2, 0, "wl_output", 2, 5);
    wire_decoder decoder;
    decoder.feed(get_registry.data(), get_registry.size());
    decoder.feed(bind_output.data(), bind_output.size());

    std::size_t decoded_count = 0;
    while (decoder.poll() == decode_outcome::message_ready) {
        table.observe(expect_message(decoder), true);
        ++decoded_count;
    }
    GLINTFX_CHECK_EQ(decoded_count, std::size_t{2});
    GLINTFX_CHECK(table.interface_of(5) == known_interface::unknown);
    GLINTFX_CHECK_EQ(table.unknown_object_count(), before + 1);

    // A message FROM that unknown object, with garbage arguments,
    // must never be analyzed - and must never crash observe().
    const std::vector<std::uint8_t> garbage = encode_message(5, 7, {0xDE, 0xAD});
    wire_decoder decoder2;
    decoder2.feed(garbage.data(), garbage.size());
    GLINTFX_CHECK(decoder2.poll() == decode_outcome::message_ready);
    const auto result = table.observe(expect_message(decoder2), true);
    GLINTFX_CHECK(!result.has_value());
}

GLINTFX_TEST(wire_relay_a1_delete_id_retires_object) {
    // wl_display.delete_id (event, opcode 1, from_client=false) is
    // the table atom's own retirement path - docs/plano-w7c.md SS3.A:
    // "delete_id aposenta o numero".
    wire_object_table table;
    const std::vector<std::uint8_t> get_registry = encode_new_id_request(1, 1, 2);
    const std::vector<std::uint8_t> bind_shm = encode_registry_bind(2, 0, "wl_shm", 1, 3);
    wire_decoder decoder;
    decoder.feed(get_registry.data(), get_registry.size());
    decoder.feed(bind_shm.data(), bind_shm.size());
    while (decoder.poll() == decode_outcome::message_ready) {
        table.observe(expect_message(decoder), true);
    }
    GLINTFX_CHECK(table.interface_of(3) == known_interface::wl_shm);

    const std::vector<std::uint8_t> delete_id_event = encode_new_id_request(1, 1, 3);
    wire_decoder event_decoder;
    event_decoder.feed(delete_id_event.data(), delete_id_event.size());
    GLINTFX_CHECK(event_decoder.poll() == decode_outcome::message_ready);
    table.observe(expect_message(event_decoder), /*from_client=*/false);
    GLINTFX_CHECK(table.interface_of(3) == known_interface::unknown);
}

GLINTFX_TEST(wire_relay_a1_take_message_on_short_buffer_is_clean) {
    // wire_decoder.hpp's own defense: take_message() re-checks the
    // same bound poll() checks, INSTEAD of trusting the caller to
    // have called poll() first. This feeds it 4 bytes - less than
    // wire_header_size - directly, WITHOUT the poll()-gated loop
    // every other test uses, and expects a clean std::nullopt, never
    // an out-of-bounds read (the mutant this control exists to kill:
    // docs/plano-w7c-adendo-revalidacao.md SS3.A, mutation report).
    wire_decoder decoder;
    const std::uint8_t four_bytes[4] = {1, 0, 0, 0};
    decoder.feed(four_bytes, sizeof(four_bytes));
    GLINTFX_CHECK(decoder.poll() == decode_outcome::need_more_bytes);

    const std::optional<decoded_message> result = decoder.take_message();
    GLINTFX_CHECK(!result.has_value());
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

    const std::uint8_t partial_header[4] = {1, 0, 0, 0};
    writer.write_once(partial_header, sizeof(partial_header), {});
    ::close(sp.a); // vanish before completing even the header

    wire_decoder decoder;
    std::size_t decoded_count = 0;
    while (true) {
        const wire_transport::read_result chunk = reader.read_once();
        if (chunk.end_of_file) {
            break;
        }
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
        while (decoder.poll() == decode_outcome::message_ready) {
            (void)expect_message(decoder);
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

    const std::vector<std::uint8_t> message = encode_surface_commit(9);
    upstream_write.write_once(message.data(), message.size(), {});
    ::close(upstream.a); // fake upstream is done - EOF on the next read

    // D-A4 lives in wire_transport::relay_until_eof() now (production
    // code, not test glue) - A3 reuses the exact same function for the
    // real relay's own upstream->client loop.
    const std::vector<std::uint8_t> forwarded =
        relay_until_eof(relay_upstream_read, relay_downstream_write);
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
