// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdlib>
#include <limits>
#include <type_traits>

// core/atlas_memory.hpp - R2D-TEXTURE, the atlas allocator (core/atlas_allocator.hpp). The memory
// the allocator grows into, WITHOUT std::vector AND WITHOUT A FAILURE THAT THROWS (GODS_LAWS.md
// L-22, docs/api-conventions.md R3): growth goes through a realloc-shaped pair of functions (null
// on failure, the old block untouched), so nothing here can throw, and a failed growth leaves the
// array exactly as it was. A test hands in its own pair, armed to fail at a chosen request. NOT
// under include/glintfx/: internal, not frozen ABI.
//
// The same technique as draw2d/pod_buffer.hpp, which the core layer cannot include (the layers
// point down only, GODS_LAWS.md L-19). Two copies of one shape; a third would move it into the
// core for both (CONTRACT.md section 6, the rule of three).
namespace glintfx {

// realloc-shaped: reallocate(nullptr, n) allocates; a null return is a failure and leaves the old
// block untouched; release(nullptr) is harmless. Both noexcept.
struct atlas_memory {
    void *(*reallocate)(void *block, std::size_t bytes) noexcept = nullptr;
    void (*release)(void *block) noexcept = nullptr;
};

[[nodiscard]] inline atlas_memory default_atlas_memory() noexcept {
    return atlas_memory{[](void *block, std::size_t bytes) noexcept -> void * {
                            return std::realloc(block, bytes);
                        },
                        [](void *block) noexcept { std::free(block); }};
}

// A growable array of trivially copyable elements: {data, size, capacity}.
template <typename T> struct atlas_array {
    T *data = nullptr;
    std::size_t size = 0;
    std::size_t capacity = 0;
};

inline constexpr std::size_t k_atlas_array_first_capacity = 8;

// Makes room for `additional` more elements. True when the array can take them (already, or after
// growing); false when the memory refused or the size would overflow, and the array is then
// untouched. Growth doubles, or takes exactly what is needed when that is more than double.
template <typename Element>
[[nodiscard]] bool reserve_atlas_room(const atlas_memory &memory, atlas_array<Element> &array,
                                      std::size_t additional) noexcept {
    static_assert(std::is_trivially_copyable_v<Element>,
                  "atlas_array grows by realloc: the element must be trivially copyable");
    if (additional > std::numeric_limits<std::size_t>::max() - array.size) {
        return false;
    }
    const std::size_t needed = array.size + additional;
    if (needed <= array.capacity) {
        return true;
    }
    std::size_t grown = array.capacity == 0 ? k_atlas_array_first_capacity : array.capacity * 2;
    if (grown < needed) {
        grown = needed;
    }
    if (grown > std::numeric_limits<std::size_t>::max() / sizeof(Element)) {
        return false;
    }
    void *block = memory.reallocate(array.data, grown * sizeof(Element));
    if (block == nullptr) {
        return false; // the old block is untouched
    }
    array.data = static_cast<Element *>(block);
    array.capacity = grown;
    return true;
}

// Gives the array's block back and leaves it empty.
template <typename Element>
void free_atlas_array(const atlas_memory &memory, atlas_array<Element> &array) noexcept {
    memory.release(array.data);
    array = atlas_array<Element>{};
}

} // namespace glintfx
