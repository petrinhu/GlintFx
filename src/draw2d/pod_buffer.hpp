// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <type_traits>

// draw2d/pod_buffer.hpp - R2D-BATCH (docs/plano-w7d.md sec. 4.3, D-W7D-18; extracted from
// triangle_batch in B4, GODS_LAWS.md L-33: the growable block of trivially copyable elements was
// the same three-line shape for the vertices, the indices and the runs of the batch, and a FOURTH
// copy - the pending pieces of the frame - is what made it the place for it). Memory WITHOUT
// std::vector AND WITHOUT A FAILURE THAT THROWS (L-22, R3): growth goes through a realloc-shaped
// allocator (null on failure, the old block untouched), so nothing here can throw, and a failed
// growth leaves the buffer exactly as it was.
namespace glintfx::draw2d {

// realloc-shaped: reallocate(nullptr, n) allocates; a null return is a failure and leaves the old
// block untouched; release(nullptr) is harmless. Both noexcept. A test arms its own to fail at a
// chosen point.
struct batch_allocator {
    void *(*reallocate)(void *block, std::size_t bytes) noexcept;
    void (*release)(void *block) noexcept;
};

[[nodiscard]] inline batch_allocator default_batch_allocator() noexcept {
    return batch_allocator{[](void *block, std::size_t bytes) noexcept -> void * {
                               return std::realloc(block, bytes);
                           },
                           [](void *block) noexcept { std::free(block); }};
}

// A growable block of trivially copyable elements: {data, size, capacity}.
template <typename T> struct pod_buffer {
    T *data = nullptr;
    std::size_t size = 0;
    std::size_t capacity = 0;
};

inline constexpr std::size_t k_pod_buffer_first_capacity = 16;

// Makes room for `additional` more elements in a buffer of `Element`s. True when the buffer can
// take them (already, or after growing); false when the allocator refused or the size would
// overflow - the buffer is then untouched. Growth doubles, or takes exactly what was asked when
// that is more than double (`grown = needed`).
template <typename Element>
[[nodiscard]] bool ensure_room(const batch_allocator &allocator, pod_buffer<Element> &buffer,
                               std::size_t additional) noexcept {
    // The block is grown by realloc, which is defined only for trivially copyable elements: the
    // compiler refuses anything else HERE, the one place every buffer grows.
    static_assert(std::is_trivially_copyable_v<Element>,
                  "pod_buffer grows by realloc: the element must be trivially copyable");
    if (additional > std::numeric_limits<std::size_t>::max() - buffer.size) {
        return false;
    }
    const std::size_t needed = buffer.size + additional;
    if (needed <= buffer.capacity) {
        return true;
    }
    std::size_t grown = buffer.capacity == 0 ? k_pod_buffer_first_capacity : buffer.capacity * 2;
    if (grown < needed) {
        grown = needed;
    }
    if (grown > std::numeric_limits<std::size_t>::max() / sizeof(Element)) {
        return false;
    }
    void *block = allocator.reallocate(buffer.data, grown * sizeof(Element));
    if (block == nullptr) {
        return false; // the old block is untouched
    }
    buffer.data = static_cast<Element *>(block);
    buffer.capacity = grown;
    return true;
}

} // namespace glintfx::draw2d
