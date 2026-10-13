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
#include "wire_fd_plan.hpp"
#include "wire_frame_writer.hpp"
#include "wire_object_table.hpp"
#include "wire_shm_relay_rig.hpp"
#include "wire_test_encoders.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <poll.h>
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

// ---- WIRE-RELAY-FD-PASSTHROUGH-fix1 (D-W8-F13): the batch plan, then the relay with it ----

namespace {

constexpr std::uint32_t k_unmodelled_id = 40; // an object the relay never saw created
const words k_pool_pixels{0x11223344U, 0x55667788U, 0x99AABBCCU, 0xDDEEFF00U};

[[nodiscard]] std::vector<std::optional<std::size_t>>
demands(std::initializer_list<std::optional<std::size_t>> list) {
    return std::vector<std::optional<std::size_t>>(list);
}

[[nodiscard]] bool same_share(const fd_share &share, std::size_t count, bool certain) {
    return share.count == count && share.certain == certain;
}

// How many descriptors reached the compositor side: every read while data is waiting, never a
// blocking read (each descriptor closed here).
[[nodiscard]] std::size_t descriptors_upstream(pumped_connection &rig) {
    std::size_t total = 0;
    pollfd waiting{.fd = rig.upstream_pair.b, .events = POLLIN, .revents = 0};
    while (::poll(&waiting, 1, 0) > 0 && (waiting.revents & POLLIN) != 0) {
        const wire_transport::read_result chunk = rig.upstream_end.read_once();
        total += chunk.fds.size();
        for (const int fd : chunk.fds) {
            (void)::close(fd);
        }
    }
    return total;
}

[[nodiscard]] std::size_t frames_saved(const pumped_connection &rig, const scratch_dir &dir) {
    return save_session_frames(rig.conn.session.pipe.snapshot, capture_target{dir.path(), 1})
        .frames_saved;
}

} // namespace

// The review's A1: an unmodelled message WITHOUT descriptor first, then create_pool with one. All
// pending descriptors are claimed by modelled demand, so the unmodelled message takes none.
GLINTFX_TEST(wire_fd_plan_unmodelled_message_without_descriptor_takes_none) {
    const fd_plan plan = plan_fd_shares(demands({std::nullopt, 1U}), 1U);
    GLINTFX_CHECK_EQ(plan.shares.size(), std::size_t{2});
    GLINTFX_CHECK(same_share(plan.shares.at(0), 0U, true));
    GLINTFX_CHECK(same_share(plan.shares.at(1), 1U, true));
    GLINTFX_CHECK_EQ(plan.carried, std::size_t{0});
}

GLINTFX_TEST(wire_fd_plan_surplus_goes_with_the_first_unmodelled_message) {
    const fd_plan alone = plan_fd_shares(demands({std::nullopt}), 1U);
    GLINTFX_CHECK(same_share(alone.shares.at(0), 1U, true));
    const fd_plan two = plan_fd_shares(demands({std::nullopt, std::nullopt}), 2U);
    GLINTFX_CHECK(same_share(two.shares.at(0), 2U, true));
    GLINTFX_CHECK(same_share(two.shares.at(1), 0U, true));
    // A second unmodelled message never takes what a modelled one after it claims.
    const fd_plan later = plan_fd_shares(demands({std::nullopt, std::nullopt, 1U}), 2U);
    GLINTFX_CHECK(same_share(later.shares.at(0), 1U, true));
    GLINTFX_CHECK(same_share(later.shares.at(1), 0U, true));
    GLINTFX_CHECK(same_share(later.shares.at(2), 1U, false));
}

// Surplus and an unmodelled message before create_pool: the wire stays in order, but create_pool's
// descriptor may be the unmodelled message's, so it is not certain. After it, it is.
GLINTFX_TEST(wire_fd_plan_modelled_after_unmodelled_with_surplus_is_not_certain) {
    const fd_plan before = plan_fd_shares(demands({std::nullopt, 1U}), 2U);
    GLINTFX_CHECK(same_share(before.shares.at(0), 1U, true));
    GLINTFX_CHECK(same_share(before.shares.at(1), 1U, false));
    const fd_plan after = plan_fd_shares(demands({1U, std::nullopt}), 2U);
    GLINTFX_CHECK(same_share(after.shares.at(0), 1U, true));
    GLINTFX_CHECK(same_share(after.shares.at(1), 1U, true));
}

// No unmodelled message: the surplus waits (a message split across reads); none is lost.
GLINTFX_TEST(wire_fd_plan_surplus_without_unmodelled_message_waits) {
    const fd_plan plan = plan_fd_shares(demands({1U, 0U}), 2U);
    GLINTFX_CHECK(same_share(plan.shares.at(0), 1U, true));
    GLINTFX_CHECK(same_share(plan.shares.at(1), 0U, true));
    GLINTFX_CHECK_EQ(plan.carried, std::size_t{1});
    const fd_plan empty = plan_fd_shares(demands({}), 1U);
    GLINTFX_CHECK(empty.shares.empty());
    GLINTFX_CHECK_EQ(empty.carried, std::size_t{1});
}

GLINTFX_TEST(wire_fd_plan_missing_descriptor_is_counted_and_not_certain) {
    const fd_plan plan = plan_fd_shares(demands({1U, 1U}), 1U);
    GLINTFX_CHECK(same_share(plan.shares.at(0), 1U, true));
    GLINTFX_CHECK(same_share(plan.shares.at(1), 0U, false));
    GLINTFX_CHECK_EQ(plan.missing, std::size_t{1});
}

// A1 through the real relay: the capture keeps the pool, and the compositor still gets the one
// descriptor.
GLINTFX_TEST(wire_relay_fd_unmodelled_message_before_create_pool_leaves_the_pool_to_the_capture) {
    pumped_connection rig;
    const fd_holder file(make_memfd(k_pool_pixels));
    rig.bootstrap();
    rig.send(concat({encode_message(k_unmodelled_id, 0, {}), encode_shm_create_pool(3, 6, 16)}),
             {file.get()});
    rig.send(concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0),
                     encode_surface_attach(5, 7), encode_surface_commit(5)}));
    GLINTFX_CHECK_EQ(descriptors_upstream(rig), std::size_t{1});
    const scratch_dir dir;
    GLINTFX_CHECK_EQ(frames_saved(rig, dir), std::size_t{1});
    GLINTFX_CHECK(read_words(dir.path() + "/conn1_surface5.raw") == k_pool_pixels);
}

// An unmodelled message WITH a descriptor before create_pool: both descriptors reach the
// compositor, in order, and the capture does not guess which one is the pool (it keeps no frame).
GLINTFX_TEST(wire_relay_fd_ambiguous_batch_keeps_the_wire_and_never_a_wrong_pool) {
    pumped_connection rig;
    const fd_holder file(make_memfd(k_pool_pixels));
    const fd_holder other(make_memfd(words{0U, 0U, 0U, 0U}));
    rig.bootstrap();
    rig.send(concat({encode_message(k_unmodelled_id, 0, {}), encode_shm_create_pool(3, 6, 16)}),
             {other.get(), file.get()});
    rig.send(concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0),
                     encode_surface_attach(5, 7), encode_surface_commit(5)}));
    GLINTFX_CHECK_EQ(descriptors_upstream(rig), std::size_t{2});
    const scratch_dir dir;
    GLINTFX_CHECK_EQ(frames_saved(rig, dir), std::size_t{0});
}
