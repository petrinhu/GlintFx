// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_fd_passthrough_selftest.cpp - WIRE-RELAY-FD-PASSTHROUGH (D-W8-F07): a message that
// carries a file descriptor reaches the other side WITH it, in both directions, even on an
// interface the relay does not model. libwayland reads descriptors as a queue (the next one for
// every 'h' argument), so a descriptor the relay drops leaves the receiver with "file descriptor
// expected": measured on 12/10/2026 with the format_table event of zwp_linux_dmabuf_feedback_v1
// and a real GPU (DECISOES_AUTONOMAS.md, D-W8-F07).
//
// GODS_LAWS.md L-09: pure process - test AF_UNIX sockets in a private directory and a pipe; never
// wayland-0.
#include "wire_object_table.hpp"
#include "wire_shm_relay_rig.hpp"
#include "wire_test_encoders.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace glintfx::test::wire_relay;

namespace {

// Object ids the relay never saw created: it cannot know their interface.
constexpr std::uint32_t k_params_id = 9;    // a zwp_linux_buffer_params_v1
constexpr std::uint32_t k_feedback_id = 13; // a zwp_linux_dmabuf_feedback_v1

// zwp_linux_buffer_params_v1.add (request 1): fd out of band, then plane_idx, offset, stride,
// modifier_hi, modifier_lo (linux-dmabuf-v1.xml).
std::vector<std::uint8_t> encode_params_add() {
    std::vector<std::uint8_t> body;
    for (const std::uint32_t value : {0U, 0U, 2560U, 0U, 0U}) {
        append_u32(body, value);
    }
    return encode_message(k_params_id, 1, body);
}

// zwp_linux_dmabuf_feedback_v1.format_table (event 0): fd out of band, then the table size.
std::vector<std::uint8_t> encode_format_table() {
    std::vector<std::uint8_t> body;
    append_u32(body, 2496U);
    return encode_message(k_feedback_id, 0, body);
}

// True when exactly one descriptor came with `chunk` and it is the read end of the same pipe whose
// write end is `pipe_write`: one byte written there comes back through it.
bool carries_the_pipe(const wire_transport::read_result &chunk, int pipe_write) {
    if (chunk.fds.size() != 1) {
        for (const int fd : chunk.fds) {
            (void)::close(fd);
        }
        return false;
    }
    const char sent = 'k';
    char seen = 0;
    const bool wrote = ::write(pipe_write, &sent, 1) == 1;
    const bool read_back = ::read(chunk.fds[0], &seen, 1) == 1;
    const bool same = wrote && read_back && seen == sent;
    (void)::close(chunk.fds[0]);
    return same;
}

} // namespace

GLINTFX_TEST(wire_relay_fd_request_with_a_descriptor_reaches_the_compositor_unmodelled) {
    relay_rig rig;
    int ends[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(ends) == 0);
    const fd_holder read_end(ends[0]);
    const fd_holder write_end(ends[1]);
    rig.send(encode_params_add(), {read_end.get()});
    const wire_transport::read_result chunk = wire_transport(rig.compositor_side.get()).read_once();
    GLINTFX_CHECK_EQ(chunk.bytes.size(), encode_params_add().size());
    GLINTFX_CHECK(carries_the_pipe(chunk, write_end.get()));
}

GLINTFX_TEST(wire_relay_fd_event_with_a_descriptor_reaches_the_client_unmodelled) {
    relay_rig rig;
    int ends[2] = {-1, -1};
    GLINTFX_CHECK(::pipe(ends) == 0);
    const fd_holder read_end(ends[0]);
    const fd_holder write_end(ends[1]);
    rig.send_from_compositor(encode_format_table(), {read_end.get()});
    const wire_transport::read_result chunk = wire_transport(rig.client.get()).read_once();
    GLINTFX_CHECK_EQ(chunk.bytes.size(), encode_format_table().size());
    GLINTFX_CHECK(carries_the_pipe(chunk, write_end.get()));
}

// The count itself: the same opcode differs by direction, and an interface the relay does not
// model is admitted as unknown instead of being read as "no descriptor".
GLINTFX_TEST(wire_relay_fd_count_knows_the_direction_and_admits_what_it_does_not_model) {
    GLINTFX_CHECK(fd_argument_count(known_interface::wl_shm, 0, wire_direction::from_client) ==
                  std::optional<std::size_t>{1});
    GLINTFX_CHECK(fd_argument_count(known_interface::wl_shm, 0, wire_direction::from_compositor) ==
                  std::optional<std::size_t>{0});
    GLINTFX_CHECK(fd_argument_count(known_interface::wl_surface, 1, wire_direction::from_client) ==
                  std::optional<std::size_t>{0});
    GLINTFX_CHECK(!fd_argument_count(known_interface::unknown, 0, wire_direction::from_compositor)
                       .has_value());
}
