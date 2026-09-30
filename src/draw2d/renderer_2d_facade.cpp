// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/draw2d/renderer_2d.hpp>

#include <cassert>
#include <utility>

#include <glintfx/core/err_code.hpp>
#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>

#include "draw2d/gl_load_refusal.hpp"
#include "draw2d/renderer_2d_impl.hpp"
#include "gl_functions.hpp"
#include "platform/gl/gl_context_access.hpp"

// renderer_2d_facade.cpp - R2D-BATCH, fatia B4: the public handle gltfx_renderer_2d over the frame
// machine (renderer_2d_impl.hpp). The one place the drawing layer meets a real graphics context,
// and it does so only through gl_context_access.hpp (the interior of the context, D-API-06), never
// through the adapter of a platform. Every method but open() is a forward; open() is what this file
// is for.
namespace glintfx {

namespace {

// The host of the real context: what renderer_2d_host asks, answered by the interior of the
// context.
[[nodiscard]] gltfx_rslt<void> host_make_current(void *interior) noexcept {
    return gl_context_interior_make_current(interior);
}
[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> host_surface_size(void *interior) noexcept {
    return gl_context_interior_surface_size(interior);
}
[[nodiscard]] void *host_resolve(void *interior, const char *name) noexcept {
    return gl_context_interior_resolve(interior, name);
}

} // namespace

renderer_2d_impl *renderer_2d_internal_access::get(gltfx_renderer_2d &renderer) noexcept {
    return renderer.impl;
}

gltfx_rslt<gltfx_renderer_2d> gltfx_renderer_2d::open(gltfx_gl_context &context,
                                                      const gltfx_renderer_2d_desc &desc) noexcept {
    if (!context.is_open()) {
        return gltfx_rslt<gltfx_renderer_2d>::err(
            gltfx_err(gltfx_err_code::invalid_argument).with_rejected_value("context"));
    }
    // The interior, not the handle: moving the handle afterwards moves nothing this renderer holds.
    void *interior = gl_context_internal_access::get(context);
    const gltfx_rslt<void> current = gl_context_interior_make_current(interior);
    if (current.has_error()) {
        return gltfx_rslt<gltfx_renderer_2d>::err(current.err());
    }
    const auto table = render::load_gl_functions(&host_resolve, interior);
    if (table.has_error()) {
        return gltfx_rslt<gltfx_renderer_2d>::err(draw2d::gl_load_refusal(table.err()));
    }
    // The surface encoding of THIS context, as it is when the renderer is opened.
    const auto srgb = context.option(gltfx_gfx_option::srgb_framebuffer);
    const bool srgb_framebuffer = srgb.has_value() && srgb.value() != 0;

    const renderer_2d_host host{interior, &host_make_current, &host_surface_size};
    auto created =
        renderer_2d_impl::create(host, table.value(), srgb_framebuffer, desc.reserve_pieces);
    if (created.has_error()) {
        return gltfx_rslt<gltfx_renderer_2d>::err(created.err());
    }
    return gltfx_rslt<gltfx_renderer_2d>::ok(gltfx_renderer_2d(created.value()));
}

gltfx_renderer_2d::gltfx_renderer_2d(gltfx_renderer_2d &&other) noexcept
    : impl(std::exchange(other.impl, nullptr)) {}

gltfx_renderer_2d &gltfx_renderer_2d::operator=(gltfx_renderer_2d &&other) noexcept {
    if (this != &other) {
        renderer_2d_impl::destroy(impl);
        impl = std::exchange(other.impl, nullptr);
    }
    return *this;
}

gltfx_renderer_2d::~gltfx_renderer_2d() { renderer_2d_impl::destroy(impl); }

bool gltfx_renderer_2d::is_open() const noexcept { return impl != nullptr; }

// Precondition of every method below but is_open(), the destructor and the moves: this renderer was
// not moved-from (docs/api-conventions.md's precondition-violation category, the same shape
// display_facade.cpp documents). Debug catches it with a named message; Release has the guard
// compile away.
void gltfx_renderer_2d::begin_frame(const gltfx_frame_2d_desc &desc) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::begin_frame() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->begin_frame(desc);
}
void gltfx_renderer_2d::begin_batch() noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::begin_batch() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->begin_batch();
}
// cppcheck-suppress passedByValue ; reason: gltfx_transform is a VALUE type and value types go by
// value in the public API (docs/auditoria-api-draw2d.md B0-I10, B0-C1): the copy is a few doubles,
// and a const reference would make the caller's lifetime part of the contract.
void gltfx_renderer_2d::begin_batch(gltfx_transform world_to_pixel) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::begin_batch() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->begin_batch(world_to_pixel);
}
// cppcheck-suppress passedByValue ; reason: gltfx_rect_world is a VALUE type and value types go by
// value in the public API (docs/auditoria-api-draw2d.md B0-I10, B0-C1): the copy is a few doubles,
// and a const reference would make the caller's lifetime part of the contract.
void gltfx_renderer_2d::fill_rect(gltfx_rect_world rect, gltfx_rgba color,
                                  gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::fill_rect() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->fill_rect(rect, color, layer);
}
// cppcheck-suppress passedByValue ; reason: gltfx_quad_world is a VALUE type and value types go by
// value in the public API (docs/auditoria-api-draw2d.md B0-I10, B0-C1): the copy is a few doubles,
// and a const reference would make the caller's lifetime part of the contract.
void gltfx_renderer_2d::fill_quad(gltfx_quad_world corners, gltfx_rgba color,
                                  gltfx_draw_layer layer) noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::fill_quad() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    impl->fill_quad(corners, color, layer);
}
void gltfx_renderer_2d::flush() noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::flush() called on a moved-from renderer - the "
                              "object no longer owns its drawing state");
    impl->flush();
}
gltfx_rslt<gltfx_frame_2d_report> gltfx_renderer_2d::finish_frame() noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::finish_frame() called on a moved-from renderer - "
                              "the object no longer owns its drawing state");
    return impl->finish_frame();
}
gltfx_frame_2d_report gltfx_renderer_2d::last_frame_report() const noexcept {
    assert(impl != nullptr && "gltfx_renderer_2d::last_frame_report() called on a moved-from "
                              "renderer - the object no longer owns its drawing state");
    return impl->last_frame_report();
}

} // namespace glintfx
