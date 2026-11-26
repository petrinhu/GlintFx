// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_shm_test_support.hpp"

#include "harness/check.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <string>
#include <thread>

// wire_shm_blocked_forward_probe.hpp - test-only: the pieces of the "copy
// before forward" proof (QA-SCREEN-CAPTURE P1): a socket filled until the
// next send blocks, and a compositor thread that rewrites the client's
// buffer while the relay's sendmsg() is asleep. Not an atom of the relay.
namespace glintfx::test::wire_relay {

// Fills the socket's send queue one byte at a time until a send would
// block: the NEXT send on it sleeps in the kernel until the peer reads.
inline void fill_until_send_would_block(int fd) {
    const std::uint8_t junk = 0;
    while (::send(fd, &junk, 1, MSG_DONTWAIT | MSG_NOSIGNAL) > 0) {
    }
    GLINTFX_CHECK(errno == EAGAIN || errno == EWOULDBLOCK);
}

// "/proc/<pid>/task/<tid>/stat" of the calling thread.
inline std::string current_thread_stat_path() {
    char target[64] = {};
    const ssize_t length = ::readlink("/proc/thread-self", target, sizeof(target) - 1);
    GLINTFX_CHECK(length > 0);
    return "/proc/" + std::string(target, static_cast<std::size_t>(length)) + "/stat";
}

// True while the thread behind `stat_path` sleeps (state 'S', the
// field right after the ")" that closes the command name).
inline bool thread_is_sleeping(const std::string &stat_path) {
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
} // namespace glintfx::test::wire_relay
