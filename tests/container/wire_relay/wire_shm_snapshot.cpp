// SPDX-License-Identifier: AGPL-3.0-or-later
#include "wire_shm_snapshot.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <optional>

namespace glintfx::test::wire_relay {

// Owns the duplicate of the client's pool descriptor for as long as
// ANY wl_buffer made from the pool is still known (buffer_record
// shares it), so destroying the wl_shm_pool never invalidates it.
struct shm_pool_state {
    shm_pool_state(int duplicated_fd, std::uint64_t declared_size)
        : fd(duplicated_fd), size(declared_size) {}
    shm_pool_state(const shm_pool_state &) = delete;
    shm_pool_state &operator=(const shm_pool_state &) = delete;
    ~shm_pool_state() { (void)::close(fd); }

    int fd;
    std::uint64_t size; // the declared pool size: the bound every buffer is checked against
};

namespace {

constexpr std::uint16_t shm_create_pool_opcode = 0;
constexpr std::uint16_t pool_create_buffer_opcode = 0;
constexpr std::uint16_t pool_destroy_opcode = 1;
constexpr std::uint16_t pool_resize_opcode = 2;
constexpr std::uint16_t buffer_destroy_opcode = 0;
constexpr std::uint16_t surface_attach_opcode = 1;
constexpr std::uint16_t surface_commit_opcode = 6;

// Bounds-checked wire word reader: a payload shorter than the
// request's real argument list yields nullopt, never a read past the
// end (the sender may be lying about the opcode's shape).
std::optional<std::uint32_t> payload_u32(const std::vector<std::uint8_t> &payload,
                                         std::size_t index) {
    const std::size_t offset = index * sizeof(std::uint32_t);
    if (offset + sizeof(std::uint32_t) > payload.size()) {
        return std::nullopt;
    }
    std::uint32_t value = 0;
    std::memcpy(&value, payload.data() + offset, sizeof(value));
    return value;
}

// Bytes a frame of this layout spans; 0 means "not a copyable frame"
// (empty, or over shm_frame_size_cap).
std::uint64_t frame_byte_count(const frame_layout &layout) {
    const std::uint64_t bytes = static_cast<std::uint64_t>(layout.stride) * layout.height;
    return bytes > shm_frame_size_cap ? 0 : bytes;
}

bool lies_inside_pool(const shm_pool_state &pool, std::uint32_t offset,
                      const frame_layout &layout) {
    const std::uint64_t bytes = frame_byte_count(layout);
    return bytes != 0 && layout.width != 0 &&
           static_cast<std::uint64_t>(offset) + bytes <= pool.size;
}

// pread() on the duplicate: independent of any mapping, so a pool
// that grew needs no remap. Short read = failure, never a half frame.
std::optional<std::vector<std::uint8_t>> read_region(const shm_pool_state &pool,
                                                     std::uint32_t offset, std::uint64_t length) {
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t got = ::pread(pool.fd, bytes.data() + done, bytes.size() - done,
                                    static_cast<off_t>(offset + done));
        if (got <= 0) {
            return std::nullopt;
        }
        done += static_cast<std::size_t>(got);
    }
    return bytes;
}

} // namespace

void shm_snapshot::observe(const decoded_message &message, known_interface source,
                           const std::vector<int> &fds) {
    switch (source) {
    case known_interface::wl_shm:
        observe_shm_request(message, fds);
        break;
    case known_interface::wl_shm_pool:
        observe_pool_request(message);
        break;
    case known_interface::wl_buffer:
        if (message.header.opcode == buffer_destroy_opcode) {
            m_buffers.erase(message.header.object_id);
        }
        break;
    case known_interface::wl_surface:
        observe_surface_request(message);
        break;
    default:
        break; // every other interface is none of this atom's business
    }
}

void shm_snapshot::observe_shm_request(const decoded_message &message,
                                       const std::vector<int> &fds) {
    if (message.header.opcode != shm_create_pool_opcode || fds.empty()) {
        return;
    }
    const std::optional<std::uint32_t> pool_id = payload_u32(message.payload, 0);
    const std::optional<std::uint32_t> size = payload_u32(message.payload, 1);
    // wl_shm.create_pool's size is an int32: a negative one is a lie.
    if (!pool_id || !size || static_cast<std::int32_t>(*size) <= 0) {
        return;
    }
    const int duplicated = ::fcntl(fds[0], F_DUPFD_CLOEXEC, 0);
    if (duplicated < 0) {
        return;
    }
    m_pools[*pool_id] = std::make_shared<shm_pool_state>(duplicated, *size);
}

void shm_snapshot::observe_pool_request(const decoded_message &message) {
    const std::uint32_t pool_id = message.header.object_id;
    if (message.header.opcode == pool_create_buffer_opcode) {
        create_buffer(pool_id, message.payload);
    } else if (message.header.opcode == pool_destroy_opcode) {
        m_pools.erase(pool_id); // buffers already made keep their own share
    } else if (message.header.opcode == pool_resize_opcode) {
        const auto pool = m_pools.find(pool_id);
        const std::optional<std::uint32_t> size = payload_u32(message.payload, 0);
        if (pool != m_pools.end() && size && *size > pool->second->size) {
            pool->second->size = *size; // the protocol only lets a pool grow
        }
    }
}

void shm_snapshot::create_buffer(std::uint32_t pool_id, const std::vector<std::uint8_t> &payload) {
    const auto pool = m_pools.find(pool_id);
    const std::optional<std::uint32_t> buffer_id = payload_u32(payload, 0);
    if (pool == m_pools.end() || !buffer_id) {
        return;
    }
    buffer_record record;
    record.pool = pool->second;
    record.offset = payload_u32(payload, 1).value_or(0);
    record.layout.width = payload_u32(payload, 2).value_or(0);
    record.layout.height = payload_u32(payload, 3).value_or(0);
    record.layout.stride = payload_u32(payload, 4).value_or(0);
    record.layout.format = payload_u32(payload, 5).value_or(0);
    // A buffer that does not fit the pool is not recorded (an id the
    // client reuses must not keep a stale record either): the commit
    // that names it is then a counted copy failure.
    m_buffers.erase(*buffer_id);
    if (payload.size() >= 6 * sizeof(std::uint32_t) &&
        lies_inside_pool(*record.pool, record.offset, record.layout)) {
        m_buffers[*buffer_id] = record;
    }
}

void shm_snapshot::observe_surface_request(const decoded_message &message) {
    const std::uint32_t surface_id = message.header.object_id;
    if (message.header.opcode == surface_attach_opcode) {
        const std::optional<std::uint32_t> buffer_id = payload_u32(message.payload, 0);
        if (buffer_id && *buffer_id != 0) {
            m_pending_buffer[surface_id] = *buffer_id;
        } else if (buffer_id) {
            m_pending_buffer.erase(surface_id); // null attach: nothing to present
        }
    } else if (message.header.opcode == surface_commit_opcode) {
        commit_surface(surface_id);
    }
}

void shm_snapshot::commit_surface(std::uint32_t surface_id) {
    const auto pending = m_pending_buffer.find(surface_id);
    if (pending == m_pending_buffer.end()) {
        return; // a commit with no new buffer presents no new frame
    }
    const std::uint32_t buffer_id = pending->second;
    m_pending_buffer.erase(pending);
    const auto buffer = m_buffers.find(buffer_id);
    if (buffer == m_buffers.end()) {
        ++m_copy_failures;
        return;
    }
    const buffer_record &record = buffer->second;
    auto bytes = read_region(*record.pool, record.offset, frame_byte_count(record.layout));
    if (!bytes) {
        ++m_copy_failures;
        return;
    }
    m_frames[surface_id] = captured_frame{record.layout, std::move(*bytes)};
}

} // namespace glintfx::test::wire_relay
