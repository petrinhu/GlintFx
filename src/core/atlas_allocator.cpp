// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/atlas_allocator.hpp>

#include <cstdint>
#include <utility>

#include "core/atlas_allocator_impl.hpp"
#include "core/atlas_memory.hpp"

// core/atlas_allocator.cpp - R2D-TEXTURE: the public handle gltfx_atlas_allocator. Each member is
// a forward to core/atlas_allocator_impl.hpp, with the library's own memory (std::realloc and
// std::free). A moved-from handle owns nothing and gives nothing back.
namespace glintfx {

gltfx_rslt<gltfx_atlas_allocator> gltfx_atlas_allocator::open(std::uint32_t width,
                                                              std::uint32_t height) noexcept {
    gltfx_rslt<atlas_allocator_impl *> made =
        make_atlas_allocator_impl(width, height, default_atlas_memory());
    if (made.has_error()) {
        return gltfx_rslt<gltfx_atlas_allocator>::err(made.err());
    }
    return gltfx_rslt<gltfx_atlas_allocator>::ok(gltfx_atlas_allocator(made.value()));
}

gltfx_atlas_allocator::gltfx_atlas_allocator(gltfx_atlas_allocator &&other) noexcept
    : impl(std::exchange(other.impl, nullptr)) {}

gltfx_atlas_allocator &gltfx_atlas_allocator::operator=(gltfx_atlas_allocator &&other) noexcept {
    move_atlas_allocator_impl(impl, other.impl);
    return *this;
}

gltfx_atlas_allocator::~gltfx_atlas_allocator() { destroy_atlas_allocator_impl(impl); }

bool gltfx_atlas_allocator::is_open() const noexcept { return impl != nullptr; }

std::uint32_t gltfx_atlas_allocator::width() const noexcept {
    return impl == nullptr ? 0U : impl->geometry.width;
}

std::uint32_t gltfx_atlas_allocator::height() const noexcept {
    return impl == nullptr ? 0U : impl->geometry.height;
}

gltfx_rslt<gltfx_atlas_slot> gltfx_atlas_allocator::allocate(std::uint32_t width,
                                                             std::uint32_t height) noexcept {
    return allocate_in_atlas(impl, width, height);
}

// cppcheck-suppress passedByValue ; reason: gltfx_atlas_slot is a VALUE type and value types go by
// value in the public API (docs/auditoria-api-draw2d.md B0-I10): the copy is seven 32-bit integers,
// and a const reference would make the caller's lifetime part of the contract.
gltfx_rslt<void> gltfx_atlas_allocator::release(gltfx_atlas_slot slot) noexcept {
    return release_from_atlas(impl, slot);
}

void gltfx_atlas_allocator::release_all() noexcept { release_everything_in_atlas(impl); }

} // namespace glintfx
