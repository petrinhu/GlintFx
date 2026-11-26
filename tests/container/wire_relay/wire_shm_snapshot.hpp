// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "wire_message.hpp"
#include "wire_object_table.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <vector>

// wire_shm_snapshot.hpp - the SNAPSHOT atom of the wire relay
// (QA-SCREEN-CAPTURE P1, D-W8-20/D-W8-32): keeps, for every surface a
// client talks about, a private COPY of the last wl_shm buffer that
// client COMMITTED.
//
// Why a copy and why at commit: a wl_buffer's pixels are only stable
// between wl_surface.commit and the compositor's wl_buffer.release;
// after that the client reuses the memory (the gl_context_parity_test
// rotates 3 buffers, measured in M1). Why a duplicated descriptor: the
// client destroys each wl_shm_pool right after create_buffer (also M1),
// and the pixels must stay readable after that. Reading is pread() on
// the duplicate, so a wl_shm_pool.resize needs no remap - only the
// pool's declared size (the bound every buffer is checked against)
// changes.
//
// Not a rule engine, not a file writer: it only watches client
// requests (already framed by wire_decoder, classified by
// wire_object_table) and answers "what was the last committed frame
// of surface S". Persisting it is wire_frame_writer.hpp's job.
namespace glintfx::test::wire_relay {

// Largest frame this atom copies: 64 MiB is a 4096x4096 ARGB8888
// surface. A pool declaring more than the file really holds would
// otherwise make a hostile (or buggy) client allocate without bound
// (GODS_LAWS.md L-40: refused and counted, never silently grown).
inline constexpr std::size_t shm_frame_size_cap = 64u * 1024u * 1024u;

// What a wl_shm buffer says about itself; stride is bytes per row and
// format is the raw wl_shm.format value (0 = ARGB8888, 1 = XRGB8888).
struct frame_layout {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    std::uint32_t format = 0;
};

struct captured_frame {
    frame_layout layout;
    std::vector<std::uint8_t> bytes; // stride * height bytes, as committed
};

// Opaque: owns the duplicated descriptor and the pool's declared size.
struct shm_pool_state;

class shm_snapshot {
  public:
    // Feeds one CLIENT request. `source` is the interface of the
    // object the message is addressed to (wire_object_table::
    // interface_of); `fds` are the descriptors that arrived with this
    // message (wl_shm.create_pool takes the first). Everything this
    // atom does not care about is ignored, never analyzed.
    void observe(const decoded_message &message, known_interface source,
                 const std::vector<int> &fds);

    // Last committed frame per wl_surface object id.
    [[nodiscard]] const std::map<std::uint32_t, captured_frame> &frames() const { return m_frames; }

    // Commits whose buffer could not be copied (unknown buffer, out of
    // the pool's bounds, short read, over the size cap). Counted so a
    // capture that silently lost frames is visible (L-40).
    [[nodiscard]] std::size_t copy_failures() const { return m_copy_failures; }

  private:
    struct buffer_record {
        std::shared_ptr<shm_pool_state> pool;
        std::uint32_t offset = 0;
        frame_layout layout;
    };

    void observe_shm_request(const decoded_message &message, const std::vector<int> &fds);
    void observe_pool_request(const decoded_message &message);
    void observe_surface_request(const decoded_message &message);
    void create_buffer(std::uint32_t pool_id, const std::vector<std::uint8_t> &payload);
    void commit_surface(std::uint32_t surface_id);

    std::map<std::uint32_t, std::shared_ptr<shm_pool_state>> m_pools;
    std::map<std::uint32_t, buffer_record> m_buffers;
    std::map<std::uint32_t, std::uint32_t> m_pending_buffer; // surface -> attached buffer
    std::map<std::uint32_t, captured_frame> m_frames;
    std::size_t m_copy_failures = 0;
};

} // namespace glintfx::test::wire_relay
