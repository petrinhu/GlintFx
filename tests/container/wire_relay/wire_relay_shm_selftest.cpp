// SPDX-License-Identifier: AGPL-3.0-or-later
//
// wire_relay_shm_selftest.cpp - autoteste de QA-SCREEN-CAPTURE P1
// (D-W8-20/D-W8-32/D-W8-33, /var/tmp/cto-w8/plano-w8-v3.md SS3): o rele
// guarda o ULTIMO buffer wl_shm COMPROMETIDO de cada superficie.
//
// Os pixels de cada caso sao escritos A MAO como literais num memfd
// (valores decididos aqui, nunca copiados da saida do programa): cada
// buffer tem pixels distintos, para um quadro lido do buffer errado
// nunca coincidir com o esperado. Cinco propriedades, cinco
// (grupos de) casos:
//   1. pool destruido logo depois do create_buffer continua legivel;
//   2. com tres buffers rotativos grava o ultimo COMMIT, nao o ultimo
//      attach;
//   3. a copia e feita antes de o commit ser legivel a montante, e
//      sobrevive a reescrita do buffer pelo cliente;
//   4. wl_shm_pool.resize e respeitado;
//   5. cliente que nao comprometeu nada grava "nenhum quadro", e a
//      contagem zero REPROVA (GODS_LAWS.md L-40).
//
// GODS_LAWS.md L-09: processo puro - memfd, socketpair e um diretorio
// temporario proprio; nenhuma janela, nenhum compositor, nenhum
// wayland-0.
#include "wire_frame_writer.hpp"
#include "wire_relay_connection_set.hpp"
#include "wire_shm_snapshot.hpp"
#include "wire_test_encoders.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include <dirent.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

namespace {

using namespace glintfx::test::wire_relay;
using words = std::vector<std::uint32_t>;

// Closes a descriptor exactly once, whatever path a case takes.
class fd_holder {
  public:
    explicit fd_holder(int fd) : m_fd(fd) {}
    fd_holder(const fd_holder &) = delete;
    fd_holder &operator=(const fd_holder &) = delete;
    ~fd_holder() { reset(); }
    [[nodiscard]] int get() const { return m_fd; }
    void reset() {
        if (m_fd >= 0) {
            (void)::close(m_fd);
            m_fd = -1;
        }
    }

  private:
    int m_fd;
};

// A memfd whose content is exactly `content`, written word by word at
// the offset given.
int make_memfd(const words &content) {
    const int fd = ::memfd_create("glintfx-shm-selftest", MFD_CLOEXEC);
    GLINTFX_CHECK(fd >= 0);
    GLINTFX_CHECK(::pwrite(fd, content.data(), content.size() * sizeof(std::uint32_t), 0) ==
                  static_cast<ssize_t>(content.size() * sizeof(std::uint32_t)));
    return fd;
}

void overwrite_memfd(int fd, const words &content, off_t offset) {
    GLINTFX_CHECK(::pwrite(fd, content.data(), content.size() * sizeof(std::uint32_t), offset) ==
                  static_cast<ssize_t>(content.size() * sizeof(std::uint32_t)));
}

words words_of(const std::vector<std::uint8_t> &bytes) {
    words out(bytes.size() / sizeof(std::uint32_t));
    std::memcpy(out.data(), bytes.data(), out.size() * sizeof(std::uint32_t));
    return out;
}

void feed(shm_snapshot &snapshot, known_interface source, const std::vector<std::uint8_t> &bytes,
          const std::vector<int> &fds = {}) {
    wire_decoder decoder;
    decoder.feed(bytes.data(), bytes.size());
    GLINTFX_CHECK(decoder.poll() == decode_outcome::message_ready);
    snapshot.observe(expect_message(decoder), source, fds);
}

// create_pool + create_buffer for a 2x2 ARGB8888 buffer at `offset`.
void open_buffer(shm_snapshot &snapshot, std::uint32_t pool_id, std::uint32_t buffer_id,
                 int pool_fd) {
    feed(snapshot, known_interface::wl_shm, encode_shm_create_pool(3, pool_id, 16), {pool_fd});
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(pool_id, buffer_id, 0, 2, 2, 8, 0));
}

void present(shm_snapshot &snapshot, std::uint32_t surface_id, std::uint32_t buffer_id) {
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(surface_id, buffer_id));
    feed(snapshot, known_interface::wl_surface, encode_surface_commit(surface_id));
}

// A private directory under TMPDIR; its (flat) content is removed with
// it. Only ever deletes inside the directory mkdtemp() itself created.
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
    ~scratch_dir() {
        DIR *dir = ::opendir(m_path.c_str());
        if (dir != nullptr) {
            while (const struct dirent *entry = ::readdir(dir)) {
                const std::string name = entry->d_name;
                if (name != "." && name != "..") {
                    (void)::unlink((m_path + "/" + name).c_str());
                }
            }
            (void)::closedir(dir);
        }
        (void)::rmdir(m_path.c_str());
    }
    [[nodiscard]] const std::string &path() const { return m_path; }

  private:
    std::string m_path;
};

std::string read_text(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> concat(std::initializer_list<std::vector<std::uint8_t>> parts) {
    std::vector<std::uint8_t> out;
    for (const std::vector<std::uint8_t> &part : parts) {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

} // namespace

// ---- 1. pool destruido continua legivel -------------------------------

GLINTFX_TEST(wire_shm_snapshot_pool_destroyed_after_create_buffer_is_still_readable) {
    fd_holder pool_file(make_memfd({0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, pool_file.get());
    // The client closes ITS descriptor and destroys the pool right
    // away (M1): only the snapshot's own duplicate keeps the pixels.
    pool_file.reset();
    feed(snapshot, known_interface::wl_shm_pool, encode_shm_pool_destroy(6));
    present(snapshot, 5, 7);

    GLINTFX_CHECK_EQ(snapshot.frames().size(), std::size_t{1});
    const captured_frame &frame = snapshot.frames().at(5);
    GLINTFX_CHECK_EQ(frame.layout.width, 2u);
    GLINTFX_CHECK_EQ(frame.layout.height, 2u);
    GLINTFX_CHECK_EQ(frame.layout.stride, 8u);
    GLINTFX_CHECK_EQ(frame.layout.format, 0u);
    GLINTFX_CHECK(words_of(frame.bytes) ==
                  (words{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{0});
}

// ---- 2. tres buffers rotativos: o ultimo COMMIT, nao o ultimo attach ----

GLINTFX_TEST(wire_shm_snapshot_three_rotating_buffers_keep_the_last_commit_not_the_last_attach) {
    const fd_holder file_a(make_memfd({0xA0000001u, 0xA0000002u, 0xA0000003u, 0xA0000004u}));
    const fd_holder file_b(make_memfd({0xB0000001u, 0xB0000002u, 0xB0000003u, 0xB0000004u}));
    const fd_holder file_c(make_memfd({0xC0000001u, 0xC0000002u, 0xC0000003u, 0xC0000004u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 10, 11, file_a.get());
    open_buffer(snapshot, 12, 13, file_b.get());
    open_buffer(snapshot, 14, 15, file_c.get());

    present(snapshot, 5, 11); // A attached and committed
    GLINTFX_CHECK(words_of(snapshot.frames().at(5).bytes) ==
                  (words{0xA0000001u, 0xA0000002u, 0xA0000003u, 0xA0000004u}));
    present(snapshot, 5, 13); // B attached and committed
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 15)); // C: attach only

    GLINTFX_CHECK_EQ(snapshot.frames().size(), std::size_t{1});
    GLINTFX_CHECK(words_of(snapshot.frames().at(5).bytes) ==
                  (words{0xB0000001u, 0xB0000002u, 0xB0000003u, 0xB0000004u}));
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{0});
}

GLINTFX_TEST(wire_shm_snapshot_null_attach_clears_the_pending_buffer) {
    const fd_holder file(make_memfd({1u, 2u, 3u, 4u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 7));
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 0)); // null attach
    feed(snapshot, known_interface::wl_surface, encode_surface_commit(5));

    GLINTFX_CHECK(snapshot.frames().empty());
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{0});
}

// ---- 4. wl_shm_pool.resize respeitado ---------------------------------

GLINTFX_TEST(wire_shm_snapshot_pool_resize_is_respected) {
    const fd_holder file(make_memfd({0x01010101u, 0x02020202u, 0x03030303u, 0x04040404u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get()); // pool declared with 16 bytes

    // A buffer past the declared size is refused: the commit finds no
    // frame to copy and the loss is COUNTED, not swallowed.
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(6, 9, 16, 2, 2, 8, 0));
    present(snapshot, 5, 9);
    GLINTFX_CHECK(snapshot.frames().empty());
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{1});

    // The client grows the file, writes the new region, then resizes.
    GLINTFX_CHECK(::ftruncate(file.get(), 32) == 0);
    overwrite_memfd(file.get(), {0x0A0B0C0Du, 0x1A1B1C1Du, 0x2A2B2C2Du, 0x3A3B3C3Du}, 16);
    feed(snapshot, known_interface::wl_shm_pool, encode_shm_pool_resize(6, 32));
    feed(snapshot, known_interface::wl_shm_pool,
         encode_shm_pool_create_buffer(6, 9, 16, 2, 2, 8, 0));
    present(snapshot, 5, 9);

    GLINTFX_CHECK(words_of(snapshot.frames().at(5).bytes) ==
                  (words{0x0A0B0C0Du, 0x1A1B1C1Du, 0x2A2B2C2Du, 0x3A3B3C3Du}));
    GLINTFX_CHECK_EQ(snapshot.copy_failures(), std::size_t{1});
}

// ---- 5. cliente sem commit: "nenhum quadro", e zero reprova -------------

GLINTFX_TEST(wire_shm_snapshot_client_that_never_committed_saves_no_frame_and_fails_the_count) {
    const fd_holder file(make_memfd({1u, 2u, 3u, 4u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    feed(snapshot, known_interface::wl_surface, encode_surface_attach(5, 7)); // never committed

    const scratch_dir dir;
    const capture_report report = save_session_frames(snapshot, capture_target{dir.path(), 3});

    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{0});
    GLINTFX_CHECK(!capture_passes(report));
    GLINTFX_CHECK_EQ(describe_capture(report, 3),
                     std::string("wire_relay: capture conn3 - nenhum quadro"));
    GLINTFX_CHECK_EQ(read_text(dir.path() + "/conn3_no_frame.txt"), std::string("nenhum quadro\n"));
}

GLINTFX_TEST(wire_shm_snapshot_saved_frame_has_raw_bytes_and_a_layout_sidecar) {
    const fd_holder file(make_memfd({0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    shm_snapshot snapshot;
    open_buffer(snapshot, 6, 7, file.get());
    present(snapshot, 5, 7);

    const scratch_dir dir;
    const capture_report report = save_session_frames(snapshot, capture_target{dir.path(), 2});

    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{1});
    GLINTFX_CHECK_EQ(report.files_failed, std::size_t{0});
    GLINTFX_CHECK(capture_passes(report));
    GLINTFX_CHECK_EQ(describe_capture(report, 2),
                     std::string("wire_relay: capture conn2 - 1 frame(s) saved, 0 file(s) failed"));
    const std::string raw = read_text(dir.path() + "/conn2_surface5.raw");
    words saved(raw.size() / sizeof(std::uint32_t));
    std::memcpy(saved.data(), raw.data(), saved.size() * sizeof(std::uint32_t));
    GLINTFX_CHECK(saved == (words{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    GLINTFX_CHECK_EQ(read_text(dir.path() + "/conn2_surface5.meta"),
                     std::string("width=2\nheight=2\nstride=8\nformat=0\n"));
    GLINTFX_CHECK_EQ(read_text(dir.path() + "/conn2_no_frame.txt"), std::string());
}

// ---- 3. a copia nasce antes de o commit chegar a montante ---------------

namespace {

void send_and_pump(const wire_transport &client_end, active_connection &conn,
                   const std::vector<std::uint8_t> &bytes, const std::vector<int> &fds = {}) {
    client_end.write_once(bytes.data(), bytes.size(), fds);
    pump_client_direction(conn);
}

// Reads from the (test-owned) compositor end until the commit of
// `surface_id` shows up; closes every descriptor that came along.
// Bounded: a relay that never forwards the commit fails the case
// instead of hanging it.
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

} // namespace

GLINTFX_TEST(wire_shm_snapshot_copy_survives_the_client_reusing_the_buffer_after_commit) {
    const socket_pair client_pair = make_socketpair();
    const socket_pair upstream_pair = make_socketpair();
    const wire_transport client_end(client_pair.a);
    const wire_transport upstream_end(upstream_pair.b);
    active_connection conn;
    conn.client_fd = client_pair.b;
    conn.upstream_fd = upstream_pair.a;

    const fd_holder file(make_memfd({0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    send_and_pump(
        client_end, conn,
        concat({encode_new_id_request(1, 1, 2), encode_registry_bind(2, 0, "wl_shm", 1, 3),
                encode_registry_bind(2, 1, "wl_compositor", 4, 4),
                encode_new_id_request(4, 0, 5)}));
    send_and_pump(client_end, conn, encode_shm_create_pool(3, 6, 16), {file.get()});
    send_and_pump(client_end, conn,
                  concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0),
                          encode_surface_attach(5, 7), encode_surface_commit(5)}));

    // The commit is readable by the compositor now; only AFTER that
    // does the client reuse the buffer (what a real client does once
    // it believes the compositor is done with it).
    GLINTFX_CHECK(upstream_saw_commit(upstream_end, 5));
    overwrite_memfd(file.get(), {0xEEEEEEE1u, 0xEEEEEEE2u, 0xEEEEEEE3u, 0xEEEEEEE4u}, 0);
    const std::vector<std::uint8_t> release = encode_buffer_release(7);
    upstream_end.write_once(release.data(), release.size(), {});
    pump_upstream_direction(conn);

    const scratch_dir dir;
    const capture_report report =
        save_session_frames(conn.session.pipe.snapshot, capture_target{dir.path(), 1});
    GLINTFX_CHECK_EQ(report.frames_saved, std::size_t{1});
    const std::string raw = read_text(dir.path() + "/conn1_surface5.raw");
    words saved(raw.size() / sizeof(std::uint32_t));
    std::memcpy(saved.data(), raw.data(), saved.size() * sizeof(std::uint32_t));
    GLINTFX_CHECK(saved == (words{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));

    for (const int fd : {client_pair.a, client_pair.b, upstream_pair.a, upstream_pair.b}) {
        (void)::close(fd);
    }
}

// ---- a fiacao: run_relay_cycle grava ao desconectar -------------------------

namespace {

int listen_unix(const std::string &path) {
    (void)::unlink(path.c_str());
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

} // namespace

GLINTFX_TEST(wire_shm_snapshot_relay_saves_the_last_commit_when_the_client_leaves) {
    const scratch_dir dir;
    const std::string downstream = dir.path() + "/down";
    const std::string upstream = dir.path() + "/up";
    const std::string capture = dir.path() + "/capture";
    GLINTFX_CHECK(::mkdir(capture.c_str(), 0755) == 0);
    const fd_holder listen_fd(listen_unix(downstream));
    const fd_holder fake_kwin(listen_unix(upstream));
    const relay_endpoints endpoints{listen_fd.get(), upstream, capture};
    connection_list connections;

    fd_holder client(connect_unix(downstream));
    run_relay_cycle(endpoints, connections); // accept
    GLINTFX_CHECK_EQ(connections.size(), std::size_t{1});
    const std::size_t serial = connections[0]->serial;
    fd_holder compositor_side(::accept(fake_kwin.get(), nullptr, nullptr));

    const fd_holder file(make_memfd({0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
    const wire_transport client_end(client.get());
    const auto send = [&](const std::vector<std::uint8_t> &bytes, const std::vector<int> &fds) {
        client_end.write_once(bytes.data(), bytes.size(), fds);
        run_relay_cycle(endpoints, connections);
    };
    send(
        concat({encode_new_id_request(1, 1, 2), encode_registry_bind(2, 0, "wl_shm", 1, 3),
                encode_registry_bind(2, 1, "wl_compositor", 4, 4), encode_new_id_request(4, 0, 5)}),
        {});
    send(encode_shm_create_pool(3, 6, 16), {file.get()});
    send(concat({encode_shm_pool_create_buffer(6, 7, 0, 2, 2, 8, 0), encode_surface_attach(5, 7),
                 encode_surface_commit(5)}),
         {});

    // The client leaves, then the compositor side closes: only now is
    // the connection finished, and only now are the files written.
    GLINTFX_CHECK(!connections.empty());
    client.reset();
    run_relay_cycle(endpoints, connections);
    compositor_side.reset();
    run_relay_cycle(endpoints, connections);
    GLINTFX_CHECK(connections.empty());

    const std::string raw = read_text(capture + "/conn" + std::to_string(serial) + "_surface5.raw");
    words saved(raw.size() / sizeof(std::uint32_t));
    std::memcpy(saved.data(), raw.data(), saved.size() * sizeof(std::uint32_t));
    GLINTFX_CHECK(saved == (words{0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u}));
}
