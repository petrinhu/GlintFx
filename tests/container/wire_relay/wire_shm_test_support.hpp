// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_relay_connection_set.hpp"
#include "wire_shm_snapshot.hpp"
#include "wire_test_encoders.hpp"

#include "harness/check.hpp"

#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

// wire_shm_test_support.hpp - test-only glue shared by the three
// QA-SCREEN-CAPTURE P1 selftests (snapshot, writer, capture), the
// same role wire_test_encoders.hpp plays for A1/A2: memfd pixels
// written by hand, a private scratch directory, and a rig that
// drives one relay connection through socketpairs. NOT an atom of the
// relay. `inline` because it is included from more than one TU.
//
// GODS_LAWS.md L-09: pure process - memfd, socketpair and a directory
// this header's own mkdtemp() creates; no window, no compositor.
namespace glintfx::test::wire_relay {

using words = std::vector<std::uint32_t>;

// Closes a descriptor exactly once, whatever path a case takes.
class fd_holder {
  public:
    explicit fd_holder(int fd) : m_fd(fd) {}
    fd_holder(const fd_holder &) = delete;
    fd_holder &operator=(const fd_holder &) = delete;
    ~fd_holder() { reset(); }
    [[nodiscard]] int get() const { return m_fd; }
    // Closes the held descriptor (if any) and takes `next` instead.
    void reset(int next = -1) {
        if (m_fd >= 0) {
            (void)::close(m_fd);
        }
        m_fd = next;
    }

  private:
    int m_fd;
};

inline int make_memfd(const words &content) {
    const int fd = ::memfd_create("glintfx-shm-selftest", MFD_CLOEXEC);
    GLINTFX_CHECK(fd >= 0);
    GLINTFX_CHECK(::pwrite(fd, content.data(), content.size() * sizeof(std::uint32_t), 0) ==
                  static_cast<ssize_t>(content.size() * sizeof(std::uint32_t)));
    return fd;
}

inline void overwrite_memfd(int fd, const words &content, off_t offset) {
    GLINTFX_CHECK(::pwrite(fd, content.data(), content.size() * sizeof(std::uint32_t), offset) ==
                  static_cast<ssize_t>(content.size() * sizeof(std::uint32_t)));
}

inline words words_of(const std::vector<std::uint8_t> &bytes) {
    words out(bytes.size() / sizeof(std::uint32_t));
    std::memcpy(out.data(), bytes.data(), out.size() * sizeof(std::uint32_t));
    return out;
}

inline std::string read_text(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline words read_words(const std::string &path) {
    const std::string raw = read_text(path);
    words out(raw.size() / sizeof(std::uint32_t));
    std::memcpy(out.data(), raw.data(), out.size() * sizeof(std::uint32_t));
    return out;
}

inline std::vector<std::uint8_t> concat(std::initializer_list<std::vector<std::uint8_t>> parts) {
    std::vector<std::uint8_t> out;
    for (const std::vector<std::uint8_t> &part : parts) {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

// Removes `path` and everything under it. Only ever called on a
// directory scratch_dir's own mkdtemp() created.
inline void remove_tree(const std::string &path) {
    DIR *dir = ::opendir(path.c_str());
    if (dir != nullptr) {
        while (const struct dirent *entry = ::readdir(dir)) {
            const std::string name = entry->d_name;
            if (name == "." || name == "..") {
                continue;
            }
            const std::string child = path + "/" + name;
            if (entry->d_type == DT_DIR) {
                remove_tree(child);
            } else {
                (void)::unlink(child.c_str());
            }
        }
        (void)::closedir(dir);
    }
    (void)::rmdir(path.c_str());
}

// A private directory under TMPDIR, removed with everything in it
// (sub-directories included).
class scratch_dir {
  public:
    scratch_dir() {
        const char *tmpdir = std::getenv("TMPDIR");
        std::string pattern = (tmpdir != nullptr && tmpdir[0] != '\0') ? tmpdir : "/tmp";
        pattern += "/glintfx-shm-selftest-XXXXXX";
        std::vector<char> buffer(pattern.begin(), pattern.end());
        buffer.push_back('\0');
        GLINTFX_CHECK(::mkdtemp(buffer.data()) != nullptr);
        m_path = buffer.data();
    }
    scratch_dir(const scratch_dir &) = delete;
    scratch_dir &operator=(const scratch_dir &) = delete;
    ~scratch_dir() { remove_tree(m_path); }
    [[nodiscard]] const std::string &path() const { return m_path; }

  private:
    std::string m_path;
};

// One relay connection driven by hand: the test writes as the client
// (client_end) and reads/writes as the compositor (upstream_end);
// pump_*_direction() is the real relay code in between.
struct pumped_connection {
    pumped_connection() {
        conn.client_fd = client_pair.b;
        conn.upstream_fd = upstream_pair.a;
    }
    pumped_connection(const pumped_connection &) = delete;
    pumped_connection &operator=(const pumped_connection &) = delete;
    ~pumped_connection() {
        for (const int fd : {client_pair.a, client_pair.b, upstream_pair.a, upstream_pair.b}) {
            (void)::close(fd);
        }
    }

    void send(const std::vector<std::uint8_t> &bytes, const std::vector<int> &fds = {}) {
        client_end.write_once(bytes.data(), bytes.size(), fds);
        pump_client_direction(conn);
    }

    // get_registry, bind wl_shm (3) and wl_compositor (4), surface 5.
    void bootstrap() {
        send(concat({encode_new_id_request(1, 1, 2), encode_registry_bind(2, 0, "wl_shm", 1, 3),
                     encode_registry_bind(2, 1, "wl_compositor", 4, 4),
                     encode_new_id_request(4, 0, 5)}));
    }

    socket_pair client_pair = make_socketpair();
    socket_pair upstream_pair = make_socketpair();
    wire_transport client_end{client_pair.a};
    wire_transport upstream_end{upstream_pair.b};
    active_connection conn;
};

// Feeds one client request straight to a snapshot (no relay in
// between); `source` is the interface the message is addressed to.
inline void feed(shm_snapshot &snapshot, known_interface source,
                 const std::vector<std::uint8_t> &bytes, const std::vector<int> &fds = {}) {
    wire_decoder decoder;
    decoder.feed(bytes.data(), bytes.size());
    GLINTFX_CHECK(decoder.poll() == decode_outcome::message_ready);
    snapshot.observe(expect_message(decoder), source, fds);
}

// create_pool (16 bytes) + create_buffer for a 2x2 ARGB8888 buffer at
// offset 0 of the pool.
inline void open_buffer(shm_snapshot &snapshot, std::uint32_t pool_id, std::uint32_t buffer_id,
                        int pool_fd) {
    feed(snapshot, known_interface::wl_shm, encode_shm_create_pool(3, pool_id, 16), {pool_fd});
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(pool_id, buffer_id, 0, 2, 2, 8, 0));
}

inline void present(shm_snapshot &snapshot, std::uint32_t surface_id, std::uint32_t buffer_id) {
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(surface_id, buffer_id));
    feed(snapshot, known_interface::wl_surface, encode_surface_commit(surface_id));
}

} // namespace glintfx::test::wire_relay
