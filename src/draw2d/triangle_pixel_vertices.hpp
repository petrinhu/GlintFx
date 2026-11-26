// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>

#include "draw2d/pod_buffer.hpp"
#include "draw2d/quad_vertices.hpp"
#include "draw2d/texture_extent.hpp"
#include "draw2d/triangle_batch.hpp"

// draw2d/triangle_pixel_vertices.hpp - R2D-SHAPES, S2b-fix1 (N1 review of 00cdbb4e, finding I-8):
// the pixel vertices of ONE triangle call, turned from world position by the batch transform, in
// a buffer reused call after call. It came out of renderer_2d_impl, where the triangle path had
// entered as a second subject (GODS_LAWS.md L-17; L-19, armadilha 1). It knows nothing of GL, of
// the frame or of the report: the frame machine asks, and counts what it is told. Pure apart from
// the allocator it is given, so a test arms its own to fail (no window, no GL).
namespace glintfx::draw2d {

// What convert() did with one call.
enum class triangle_pixels_outcome : std::uint8_t {
    converted,     // converted() holds the call's vertices, in pixels
    out_of_memory, // the buffer could not grow: converted() is empty
    // A vertex lands on a pixel position out of the range the vertex program can scale
    // (pixel_position_in_range(), quad_vertices.hpp; S2b-fix1, I-7): converted() is empty. Asked
    // BEFORE any memory, so the same call gets the same answer whatever the memory.
    pixel_position_out_of_range,
};

class triangle_pixel_vertices {
  public:
    explicit triangle_pixel_vertices(batch_allocator allocator) noexcept;
    ~triangle_pixel_vertices();
    triangle_pixel_vertices(const triangle_pixel_vertices &) = delete;
    triangle_pixel_vertices &operator=(const triangle_pixel_vertices &) = delete;
    triangle_pixel_vertices(triangle_pixel_vertices &&) = delete;
    triangle_pixel_vertices &operator=(triangle_pixel_vertices &&) = delete;

    // Turns `vertices` into pixel vertices by `affine`, each colour kept as given. Only this call's
    // vertices are held afterwards: the previous call's are gone, whatever the outcome.
    [[nodiscard]] triangle_pixels_outcome convert(std::span<const gltfx_vertex_2d_world> vertices,
                                                  const pixel_affine &affine) noexcept;

    // The same for a TEXTURED call (R2D-TEXTURE T5, D-W8-675): each position turned by `affine`,
    // each tint kept as given as the vertex's colour, and each texel coordinate divided by the size
    // of the texture in texels, so reads() holds u and v in 0..1 across it, one per vertex (the
    // division in double, then narrowed to float). The pixel range is asked first, as in convert().
    // `extent` has no zero side (a texture of zero size is never made).
    [[nodiscard]] triangle_pixels_outcome
    convert_textured(std::span<const gltfx_textured_vertex_2d> vertices, const pixel_affine &affine,
                     texture_extent extent) noexcept;

    // The vertices of the last convert() or convert_textured(); empty when it did not answer
    // `converted`.
    [[nodiscard]] std::span<const colored_pixel_vertex> converted() const noexcept;

    // The reads of the last convert_textured(), one per vertex (T5); empty after convert(), and
    // when it did not answer `converted`.
    [[nodiscard]] std::span<const triangle_texture_read> reads() const noexcept;

  private:
    batch_allocator allocator;
    pod_buffer<colored_pixel_vertex> buffer;
    pod_buffer<triangle_texture_read> read_buffer;
};

} // namespace glintfx::draw2d
