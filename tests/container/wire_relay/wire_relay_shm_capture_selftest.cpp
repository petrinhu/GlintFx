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
#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <thread>

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

// Fills the socket's send queue one byte at a time until a send would
// block: the NEXT send on it sleeps in the kernel until the peer reads.
void fill_until_send_would_block(int fd) {
    const std::uint8_t junk = 0;
    while (::send(fd, &junk, 1, MSG_DONTWAIT | MSG_NOSIGNAL) > 0) {
    }
    GLINTFX_CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
}

// "/proc/<pid>/task/<tid>/stat" of the calling thread.
std::string current_thread_stat_path() {
    char target[64] = {};
    const ssize_t length = ::readlink("/proc/thread-self", target, sizeof(target) - 1);
    GLINTFX_CHECK(length > 0);
    return "/proc/" + std::string(target, static_cast<std::size_t>(length)) + "/stat";
}

// True while the thread behind `stat_path` sleeps (state 'S', the
// field right after the ")" that closes the command name).
bool thread_is_sleeping(const std::string &stat_path) {
    const std::string stat = read_text(stat_path);
    const std::size_t close = stat.rfind(')');
    return close != std::string::npos && close + 2 < stat.size() && stat[close + 2] == 'S';
}

// The compositor side of the "copy before forward" proof: waits until
// the test thread is asleep inside the relay's blocked sendmsg(),
// rewrites the client's buffer, and only then drains the socket so the
// sendmsg() can finish. Bounded: never waits forever.
class blocked_forward_probe {
  public:
    blocked_forward_probe(int upstream_end_fd, int memfd, const words &reused)
        : m_stat_path(current_thread_stat_path()), m_thread([this, upstream_end_fd, memfd, reused] {
              run(upstream_end_fd, memfd, reused);
          }) {}
    blocked_forward_probe(const blocked_forward_probe &) = delete;
    blocked_forward_probe &operator=(const blocked_forward_probe &) = delete;
    ~blocked_forward_probe() { finish(); }

    void finish() {
        m_done = true;
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
    [[nodiscard]] bool saw_sender_blocked() const { return m_saw_blocked; }

  private:
    void run(int upstream_end_fd, int memfd, const words &reused) {
        const auto give_up = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!thread_is_sleeping(m_stat_path) && std::chrono::steady_clock::now() < give_up) {
            std::this_thread::yield();
        }
        m_saw_blocked = thread_is_sleeping(m_stat_path);
        overwrite_memfd(memfd, reused, 0);
        std::uint8_t sink[256];
        while (!m_done) {
            if (::recv(upstream_end_fd, sink, sizeof(sink), MSG_DONTWAIT) <= 0) {
                std::this_thread::yield();
            }
        }
    }

    std::string m_stat_path;
    std::atomic<bool> m_done{false};
    std::atomic<bool> m_saw_blocked{false};
    std::thread m_thread;
};

int listen_unix(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return fd;
    }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    GLINTFX_CHECK(::bind(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0);
    GLINTFX_CHECK(::listen(fd, 4) == 0);
    return fd;
}

int connect_unix(const std::string &path) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return fd;
    }
    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    GLINTFX_CHECK(::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0);
    return fd;
}

// A client connected to a REAL relay_endpoints/run_relay_cycle with a
// fake compositor behind it, capture on, in a private directory.
struct relay_rig {
    relay_rig()
        : listen_fd(listen_unix(dir.path() + "/down")), fake_kwin(listen_unix(dir.path() + "/up")),
          endpoints{listen_fd.get(), dir.path() + "/up", capture},
          client(connect_unix(dir.path() + "/down")), compositor_side(-1) {
        GLINTFX_CHECK(::mkdir(capture.c_str(), 0755) == 0);
        run_relay_cycle(endpoints, connections); // accept
        GLINTFX_CHECK_EQ(connections.size(), std::size_t{1});
        compositor_side.reset(::accept(fake_kwin.get(), nullptr, nullptr));
    }

    void send(const std::vector<std::uint8_t> &bytes, const std::vector<int> &fds = {}) {
        wire_transport(client.get()).write_once(bytes.data(), bytes.size(), fds);
        run_relay_cycle(endpoints, connections);
    }

    // The client leaves, then the compositor side closes: only now is
    // the connection finished, and only now are the files written.
    void finish() {
        client.reset();
        run_relay_cycle(endpoints, connections);
        compositor_side.reset();
        run_relay_cycle(endpoints, connections);
    }

    [[nodiscard]] std::size_t serial() const { return connections.at(0)->serial; }

    scratch_dir dir;
    std::string capture = dir.path() + "/capture";
    fd_holder listen_fd;
    fd_holder fake_kwin;
    relay_endpoints endpoints;
    connection_list connections;
    fd_holder client;
    fd_holder compositor_side;
};

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
    const fd_holder file(make_memfd(first_pixels));
    rig.send(concat({encode_new_id_request(1, 1, 2), encode_registry_bind(2, 0, "wl_shm", 1, 3),
                     encode_registry_bind(2, 1, "wl_compositor", 4, 4),
                     encode_new_id_request(4, 0, 5)}));
    rig.send(encode_shm_create_pool(3, 6, 16), {file.get()});
    rig.send(concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0),
                     encode_surface_attach(5, 7), encode_surface_commit(5)}));
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
