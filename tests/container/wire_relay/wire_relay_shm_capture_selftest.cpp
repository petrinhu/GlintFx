// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_shm_capture_selftest.cpp - autoteste de QA-SCREEN-CAPTURE
// P1 (D-W8-32), o RELE inteiro: pump_client_direction/pump_upstream_
// direction e run_relay_cycle reais, com memfd de pixels escritos a
// mao. Provas: a copia nasce ANTES de o commit ser encaminhado, o
// relé fecha os descritores que encaminha, e a gravacao ao desconectar
// (com quadro e sem quadro). O atomo puro e o writer tem os proprios
// arquivos de autoteste.
//
// GODS_LAWS.md L-09: processo puro - socketpair, soquetes AF_UNIX de
// teste num diretorio temporario proprio e um memfd; nunca wayland-0.
#include "wire_frame_writer.hpp"
#include "wire_shm_blocked_forward_probe.hpp"
#include "wire_shm_relay_rig.hpp"
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <dirent.h>
#include <unistd.h>

using namespace glintfx::test::wire_relay;

namespace {

const words first_pixels{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u};
const words reused_pixels{0xEEEEEEE1u, 0xEEEEEEE2u, 0xEEEEEEE3u, 0xEEEEEEE4u};

// pool 6 (16 bytes) + 2x2 buffer 7 attached to surface 5, NOT committed.
void send_pool_buffer_attach(pumped_connection &rig, int pool_fd) {
    rig.send(encode_shm_create_pool(3, 6, 16), {pool_fd});
    rig.send(
        concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0), encode_surface_attach(5, 7)}));
}

bool upstream_saw_commit(const wire_transport &upstream_end, std::uint32_t surface_id) {
    wire_decoder decoder;
    for (int attempt = 0; attempt < 8; ++attempt) {
        const wire_transport::read_result chunk = upstream_end.read_once();
        for (const int fd : chunk.fds) {
            (void)::close(fd);
        }
        decoder.feed(chunk.bytes.data(), chunk.bytes.size());
        while (decoder.poll() == decode_outcome::message_ready) {
            const decoded_message message = expect_message(decoder);
            if (message.header.object_id == surface_id && message.header.opcode == 6) {
                return true;
            }
        }
    }
    return false;
}

std::string save_to_scratch(const pumped_connection &rig, const scratch_dir &dir) {
    const capture_report report =
        save_session_frames(rig.conn.session.pipe.snapshot, capture_target{dir.path(), 1});
    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{1});
    return dir.path() + "/conn1_surface5.raw";
}

// One whole client session on the relay: bootstrap, a pool with
// `pixels`, and a committed buffer on surface 5.
void send_committed_frame(relay_rig &rig, const words &pixels) {
    const fd_holder file(make_memfd(pixels));
    rig.send(concat({encode_new_id_request(1, 1, 2), encode_registry_bind(2, 0, "wl_shm", 1, 3),
                     encode_registry_bind(2, 1, "wl_compositor", 4, 4),
                     encode_new_id_request(4, 0, 5)}));
    rig.send(encode_shm_create_pool(3, 6, 16), {file.get()});
    rig.send(concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0),
                     encode_surface_attach(5, 7), encode_surface_commit(5)}));
}

int count_open_fds() {
    DIR *dir = ::opendir("/proc/self/fd");
    GLINTFX_CHECK(dir != nullptr);
    int count = 0;
    while (::readdir(dir) != nullptr) {
        ++count;
    }
    (void)::closedir(dir);
    return count;
}

} // namespace

GLINTFX_TEST(wire_shm_capture_copy_survives_the_client_reusing_the_buffer_after_commit) {
    pumped_connection rig;
    const fd_holder file(make_memfd(first_pixels));
    rig.bootstrap();
    send_pool_buffer_attach(rig, file.get());
    rig.send(encode_surface_commit(5));

    // The commit is readable by the compositor now; only AFTER that
    // does the client reuse the buffer.
    GLINTFX_CHECK(upstream_saw_commit(rig.upstream_end, 5));
    overwrite_memfd(file.get(), reused_pixels, 0);
    const std::vector<std::uint8_t> release = encode_buffer_release(7);
    rig.upstream_end.write_once(release.data(), release.size(), {});
    pump_upstream_direction(rig.conn);

    const scratch_dir dir;
    GLINTFX_CHECK(read_words(save_to_scratch(rig, dir)) == first_pixels);
}

GLINTFX_TEST(wire_shm_capture_copy_is_made_before_the_commit_is_forwarded) {
    pumped_connection rig;
    const fd_holder file(make_memfd(first_pixels));
    rig.bootstrap();
    send_pool_buffer_attach(rig, file.get());
    fill_until_send_would_block(rig.conn.upstream_fd);

    // The commit's sendmsg() now BLOCKS: a compositor thread waits
    // until this thread sleeps in it, rewrites the buffer, and only
    // then drains the socket. Copy-before-forward kept the first
    // pixels; copy-after-forward would read the rewritten ones.
    blocked_forward_probe probe(rig.upstream_pair.b, file.get(), reused_pixels);
    rig.send(encode_surface_commit(5));
    probe.finish();

    GLINTFX_CHECK(probe.saw_sender_blocked());
    const scratch_dir dir;
    GLINTFX_CHECK(read_words(save_to_scratch(rig, dir)) == first_pixels);
}

GLINTFX_TEST(wire_shm_capture_forwarded_descriptors_are_closed_by_the_relay) {
    pumped_connection rig;
    const fd_holder file(make_memfd(first_pixels));
    rig.bootstrap();
    const int before = count_open_fds();
    for (std::uint32_t pool_id = 100; pool_id < 120; ++pool_id) {
        rig.send(encode_shm_create_pool(3, pool_id, 16), {file.get()});
        rig.send(encode_shm_pool_destroy(pool_id));
        const wire_transport::read_result chunk = rig.upstream_end.read_once();
        for (const int fd : chunk.fds) {
            (void)::close(fd);
        }
    }
    // 20 pools made and destroyed: nothing of them may stay open here.
    GLINTFX_CHECK_EQ(count_open_fds(), before);
}

GLINTFX_TEST(wire_shm_capture_relay_saves_the_last_commit_when_the_client_leaves) {
    relay_rig rig;
    send_committed_frame(rig, first_pixels);
    const std::string prefix = rig.capture + "/conn" + std::to_string(rig.serial());
    rig.finish();

    GLINTFX_CHECK(rig.connections.empty());
    GLINTFX_CHECK(read_words(prefix + "_surface5.raw") == first_pixels);
}

GLINTFX_TEST(wire_shm_capture_relay_writes_no_frame_marker_for_a_client_that_never_committed) {
    relay_rig rig;
    const fd_holder file(make_memfd(first_pixels));
    rig.send(concat({encode_new_id_request(1, 1, 2), encode_registry_bind(2, 0, "wl_shm", 1, 3),
                     encode_registry_bind(2, 1, "wl_compositor", 4, 4),
                     encode_new_id_request(4, 0, 5)}));
    rig.send(encode_shm_create_pool(3, 6, 16), {file.get()});
    rig.send(concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0),
                     encode_surface_attach(5, 7)})); // attached, never committed
    const std::string prefix = rig.capture + "/conn" + std::to_string(rig.serial());
    rig.finish();

    GLINTFX_CHECK(rig.connections.empty());
    GLINTFX_CHECK_EQ(read_text(prefix + "_no_frame.txt"), std::string("nenhum quadro\n"));
    GLINTFX_CHECK_EQ(read_text(prefix + "_surface5.raw"), std::string());
}

GLINTFX_TEST(wire_shm_capture_consecutive_clients_save_to_distinct_files) {
    relay_rig rig;
    send_committed_frame(rig, first_pixels);
    const std::string first_prefix = rig.capture + "/conn" + std::to_string(rig.serial());
    rig.finish();
    rig.connect_client();
    send_committed_frame(rig, reused_pixels);
    const std::string second_prefix = rig.capture + "/conn" + std::to_string(rig.serial());
    rig.finish();

    GLINTFX_CHECK(first_prefix != second_prefix);
    GLINTFX_CHECK(read_words(first_prefix + "_surface5.raw") == first_pixels);
    GLINTFX_CHECK(read_words(second_prefix + "_surface5.raw") == reused_pixels);
}

GLINTFX_TEST(wire_shm_capture_rule_violation_injects_the_error_and_stops_the_client_direction) {
    pumped_connection rig;
    rig.bootstrap();
    rig.send(
        concat({encode_registry_bind(2, 2, "xdg_wm_base", 2, 6), encode_get_xdg_surface(6, 7, 5),
                encode_surface_attach(5, 99), encode_surface_commit(5)})); // R1 violation

    GLINTFX_CHECK_EQ(rig.conn.session.stats.violations, std::size_t{1});
    GLINTFX_CHECK(!rig.conn.client_open);
    const wire_transport::read_result error = rig.client_end.read_once();
    GLINTFX_CHECK(!error.bytes.empty());
}
