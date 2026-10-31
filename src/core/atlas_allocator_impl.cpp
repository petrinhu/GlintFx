// SPDX-License-Identifier: AGPL-3.0-or-later
#include "core/atlas_allocator_impl.hpp"

#include <atomic>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

#include <glintfx/core/err_code.hpp>

#include "core/atlas_request_check.hpp"

// core/atlas_allocator_impl.cpp - R2D-TEXTURE. The interior of gltfx_atlas_allocator;
// core/atlas_allocator_impl.hpp says what each function does.
namespace glintfx {

namespace {

// The process-wide count behind next_atlas_owner(). Only the count needs to be shared between
// threads, so relaxed order is enough; lock-free, checked below, so no thread ever waits on it.
std::atomic<std::uint32_t> g_last_atlas_owner{0};
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "the owner count is lock-free on every system this library builds for");

constexpr std::uint32_t k_atlas_last_owner = 0xFFFFFFFFU;

// The interior lives in a block from atlas_memory (realloc-shaped), so it must be a type such a
// block can hold without a constructor: an aggregate of trivially copyable members, whose object
// the allocation itself creates (an implicit-lifetime type, C++23 [intro.object]).
static_assert(std::is_trivially_copyable_v<atlas_allocator_impl>,
              "atlas_allocator_impl lives in a realloc-shaped block: trivially copyable");

template <typename T>
gltfx_rslt<T> refused_with(gltfx_err_code code, std::string_view name) noexcept {
    return gltfx_rslt<T>::err(gltfx_err(code).with_rejected_value(name));
}

atlas_identity identity_of(const atlas_allocator_impl *impl) noexcept {
    if (impl == nullptr) {
        return atlas_identity{};
    }
    return atlas_identity{.open = true,
                          .width = impl->geometry.width,
                          .height = impl->geometry.height,
                          .owner = impl->owner};
}

// The interior's own block, from `memory`, with nothing open in it yet; nullptr when the memory
// refused.
atlas_allocator_impl *new_interior(atlas_memory memory) noexcept {
    void *block = memory.reallocate(nullptr, sizeof(atlas_allocator_impl));
    if (block == nullptr) {
        return nullptr;
    }
    auto *impl = static_cast<atlas_allocator_impl *>(block);
    *impl = atlas_allocator_impl{.memory = memory};
    return impl;
}

} // namespace

std::uint32_t atlas_owner_after(std::uint32_t last) noexcept {
    return last == k_atlas_last_owner ? 1U : last + 1U;
}

std::uint32_t next_atlas_owner() noexcept {
    std::uint32_t last = g_last_atlas_owner.load(std::memory_order_relaxed);
    std::uint32_t next = atlas_owner_after(last);
    while (!g_last_atlas_owner.compare_exchange_weak(last, next, std::memory_order_relaxed)) {
        next = atlas_owner_after(last);
    }
    return next;
}

gltfx_rslt<atlas_allocator_impl *>
make_atlas_allocator_impl(std::uint32_t width, std::uint32_t height, atlas_memory memory) noexcept {
    const gltfx_rslt<void> size = check_atlas_size(width, height);
    if (size.has_error()) {
        return gltfx_rslt<atlas_allocator_impl *>::err(size.err());
    }
    atlas_allocator_impl *impl = new_interior(memory);
    if (impl == nullptr) {
        return refused_with<atlas_allocator_impl *>(gltfx_err_code::out_of_memory, "atlas_open");
    }
    impl->slots.memory = memory;
    if (!open_atlas_geometry(impl->geometry, memory, width, height)) {
        destroy_atlas_allocator_impl(impl);
        return refused_with<atlas_allocator_impl *>(gltfx_err_code::out_of_memory, "atlas_open");
    }
    impl->owner = next_atlas_owner();
    return gltfx_rslt<atlas_allocator_impl *>::ok(impl);
}

void destroy_atlas_allocator_impl(atlas_allocator_impl *impl) noexcept {
    if (impl == nullptr) {
        return;
    }
    close_atlas_geometry(impl->geometry);
    close_atlas_slot_table(impl->slots);
    const atlas_memory memory = impl->memory;
    memory.release(impl);
}

void move_atlas_allocator_impl(atlas_allocator_impl *&target,
                               atlas_allocator_impl *&source) noexcept {
    if (&target == &source) {
        return;
    }
    destroy_atlas_allocator_impl(target);
    target = std::exchange(source, nullptr);
}

gltfx_rslt<gltfx_atlas_slot> allocate_in_atlas(atlas_allocator_impl *impl, std::uint32_t width,
                                               std::uint32_t height) noexcept {
    const gltfx_rslt<void> request = check_atlas_request(identity_of(impl), width, height);
    if (request.has_error()) {
        return gltfx_rslt<gltfx_atlas_slot>::err(request.err());
    }
    if (!find_atlas_spot(impl->geometry, width, height).found) {
        return refused_with<gltfx_atlas_slot>(gltfx_err_code::no_free_space, "atlas_full");
    }
    if (!reserve_atlas_number(impl->slots)) {
        return refused_with<gltfx_atlas_slot>(gltfx_err_code::out_of_memory, "atlas_allocate");
    }
    // place_in_atlas() cannot answer no_room here: find_atlas_spot() found a spot on this same
    // geometry above, and reserve_atlas_number() does not touch the geometry. What reaches the
    // branch below is the memory refused for a new run (no_memory).
    const atlas_placement placement = place_in_atlas(impl->geometry, width, height);
    if (placement.outcome != atlas_place_outcome::placed) {
        return refused_with<gltfx_atlas_slot>(gltfx_err_code::out_of_memory, "atlas_allocate");
    }
    const gltfx_atlas_rect rect{
        .x = placement.x, .y = placement.y, .width = width, .height = height};
    return gltfx_rslt<gltfx_atlas_slot>::ok(hand_out_atlas_number(impl->slots, rect, impl->owner));
}

gltfx_rslt<void> check_current_atlas_slot(const atlas_allocator_impl *impl,
                                          const gltfx_atlas_slot &slot) noexcept {
    gltfx_rslt<void> request = check_atlas_release(identity_of(impl), slot);
    if (request.has_error()) {
        return request;
    }
    if (!is_current_atlas_slot(impl->slots, slot)) {
        return refused_with<void>(gltfx_err_code::invalid_argument, "stale_slot");
    }
    return gltfx_rslt<void>::ok();
}

gltfx_rslt<void> release_from_atlas(atlas_allocator_impl *impl,
                                    const gltfx_atlas_slot &slot) noexcept {
    gltfx_rslt<void> current = check_current_atlas_slot(impl, slot);
    if (current.has_error()) {
        return current;
    }
    // is_current_atlas_slot() compared the whole rect with the one handed out, so slot.rect starts
    // a taken run of this geometry and free_in_atlas() never ignores it here.
    free_in_atlas(impl->geometry, slot.rect);
    take_back_atlas_number(impl->slots, slot.id);
    return gltfx_rslt<void>::ok();
}

void release_everything_in_atlas(atlas_allocator_impl *impl) noexcept {
    if (impl == nullptr) {
        return;
    }
    clear_atlas_geometry(impl->geometry);
    take_back_every_atlas_number(impl->slots);
}

} // namespace glintfx
