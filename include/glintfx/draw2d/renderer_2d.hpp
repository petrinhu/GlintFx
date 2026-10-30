// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/ellipse.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/draw_layer.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/image_resolver.hpp>
#include <glintfx/draw2d/renderer_2d_desc.hpp>
#include <glintfx/draw2d/stroke.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_2d_desc.hpp>
#include <glintfx/draw2d/texture_region.hpp>
#include <glintfx/draw2d/vertex_2d.hpp>
#include <glintfx/export.hpp>
#include <glintfx/platform/gl/context.hpp>

// draw2d/renderer_2d.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): filled rectangles and quadrilaterals, drawn into a
// gltfx_gl_context, gathered into as few draw calls as the order you
// asked for allows.
//
// ============================================================
// HOW A FRAME LOOKS
// ============================================================
//
//   renderer.begin_frame({});                  // clears to black
//   renderer.fill_rect({{10, 10}, {100, 50}}, red);        // pixels
//   renderer.begin_batch(camera);              // world from here on
//   renderer.fill_rect(tree, green, gltfx_draw_layer{2});
//   renderer.fill_quad(turned_sign, white);
//   auto report = renderer.finish_frame();     // the ONE report
//   // ...then present: gltfx_gl_context::swap_buffers(), or let
//   // gltfx_loop present after your on_render callback returns.
//
// ============================================================
// THE THREE DECISIONS OF THE PROJECT LEADER THIS HEADER CARRIES
// (19/09/2026, not to be reopened)
// ============================================================
//
//   1. POSITIONS ARE WORLD POSITIONS, plus ONE transform per batch.
//      There is no camera object with state inside the library: you
//      hand over a transform, it applies to every piece until the next
//      begin_batch(), flush() or finish_frame(). With no transform, a
//      world position IS a pixel position.
//   2. A PROBLEM IS REPORTED ONCE, when the frame ends. The drawing
//      calls return nothing. finish_frame() returns the report.
//   3. (For R2D-TEXTURE, not this header.) Images receive ready pixels.
//
// ============================================================
// PIXELS, frozen
// ============================================================
//
// A pixel position is a PHYSICAL pixel of the context's surface - the
// size the context reports, never a size you pass in. Origin at the top
// left corner, x grows to the right, y grows DOWN. The edge of a
// rectangle lies on the edge of a pixel; the center of the first pixel
// is (0.5, 0.5). Two rectangles that share an edge - the end of one and
// the start of the other written with the same coordinate - cover every
// pixel along it exactly once: no gap, no overlap. To draw in logical units
// on a scaled display, begin a batch whose transform scales by the
// window's scale factor.
// Proved by: draw2d_parity_test (two rectangles sharing an edge, read
// back pixel by pixel, on both systems).
//
// PRECISION, frozen: the transform is applied by the library in DOUBLE
// precision, to world positions that were never narrowed, and only the
// finished pixel positions become single precision. A world position
// keeps the precision of a double until that last step, far from the
// origin as much as near it (the reason core/vec2.hpp keeps world and
// screen apart).
// Proved by: quad_vertices_test (a camera at a position single
// precision cannot hold still lands the piece on the exact pixel).
//
// COLOR, frozen: gltfx_rgba is linear light with straight alpha
// (core/color.hpp). Blending happens with premultiplied alpha inside
// the library, so a half-transparent edge never grows a dark fringe.
// With the context option `srgb_framebuffer` on, blending happens in
// linear light; with it off, the color is encoded to sRGB before
// blending, which is what most 2D libraries do.
// Proved by: triangle_batch_test (the color stored for the graphics
// card is premultiplied).
// Proved by: draw2d_parity_test (a half-transparent edge, and the same
// piece with srgb_framebuffer on and off, read back as pixels).
//
// ============================================================
// WHAT THIS HEADER DOES NOT PROMISE (so nobody infers it from silence)
// ============================================================
//
//   - A number of frames per second, or of pieces per draw call. The
//     report counts draw calls for you to measure; nothing here
//     promises a count.
//   - Drawing from more than one thread. One renderer is used from one
//     thread, the same thread its context is used from.
//   - Surviving the loss of the GPU context by the driver. OpenGL 3.3
//     without the robustness extension cannot detect it.
//   - A color brighter than white on an 8-bit surface. A value above
//     1.0 is clipped by the surface, not by the type.

namespace glintfx {

struct renderer_2d_impl;

// Internal access to the private layout, for the library's own code
// only - the same "friend struct" shape gltfx_gl_context uses. Never
// part of the public contract, never exported.
struct renderer_2d_internal_access {
    [[nodiscard]] static renderer_2d_impl *get(class gltfx_renderer_2d &renderer) noexcept;
};

// One handle per subject (GODS_LAWS.md L-19): this one draws 2D
// pieces. Opaque (PIMPL): the layout lives only inside the library.
class gltfx_renderer_2d {
  public:
    // Opens a renderer that draws into `context`. Fallible for real:
    // the drawing program may not compile on this driver, a GL function
    // it needs may be missing, memory may run out. Refusals, as a
    // gltfx_err, never an exception (docs/api-conventions.md R1, R3):
    //   - `invalid_argument`, rejected_value() "context": `context` is
    //     not open.
    //   - `invalid_argument`, rejected_value() "curve_max_error_pixels":
    //     that field of `desc` is not a finite number greater than zero
    //     (renderer_2d_desc.hpp). Checked right after the context, before
    //     anything is made or made current.
    //   - `unsupported`, rejected_value() naming the missing GL
    //     function (for example "glGenVertexArrays").
    //   - `platform_failure`, rejected_value() "vertex_shader",
    //     "fragment_shader" or "program_link": the driver refused the
    //     library's own drawing program. The driver's own message is
    //     sent to the log sink (core/log) as the field `driver_log` of
    //     the event `draw2d_program_rejected`; it is never put in the
    //     error, because it is a sentence in the driver's language.
    //   - `platform_failure`, rejected_value() "vertex_array_create":
    //     the graphics card did not give the renderer its vertex array
    //     or buffers (the GL error code, when there is one, is in
    //     os_error_code()).
    //   - `out_of_memory`: the room asked for by `desc` could not be
    //     reserved, or the graphics card itself ran out of memory while
    //     creating the renderer's objects (rejected_value()
    //     "vertex_array_create", the GL error code in os_error_code()).
    //
    // `context` MUST OUTLIVE the renderer - the same rule gltfx_window
    // sets for its display, gltfx_gl_context for its window and
    // gltfx_loop for its context. Moving the context handle while the
    // renderer is alive is harmless: the renderer holds what the handle
    // points to, not the handle. Several renderers may be opened over
    // the same context; each owns its own GPU objects. `desc` is read,
    // never stored.
    [[nodiscard]] GLINTFX_API static gltfx_rslt<gltfx_renderer_2d>
    open(gltfx_gl_context &context, const gltfx_renderer_2d_desc &desc) noexcept;

    // Move-only, like every handle in this library. Every method below
    // except is_open(), the destructor and the move operations must
    // never be called on a moved-from renderer (the same precondition
    // gltfx_loop states).
    gltfx_renderer_2d(const gltfx_renderer_2d &) = delete;
    gltfx_renderer_2d &operator=(const gltfx_renderer_2d &) = delete;
    GLINTFX_API gltfx_renderer_2d(gltfx_renderer_2d &&other) noexcept;
    GLINTFX_API gltfx_renderer_2d &operator=(gltfx_renderer_2d &&other) noexcept;

    // Releases the renderer's GPU objects in its context, then its
    // memory. Never refused.
    GLINTFX_API ~gltfx_renderer_2d();

    [[nodiscard]] GLINTFX_API bool is_open() const noexcept;

    // Starts a frame: reads the size of the context's surface, clears
    // it if `desc` asks for that, and opens the pixel-direct batch every
    // frame starts with (as if begin_batch() had been called). Called
    // while a frame is already open, it throws the open frame away,
    // undrawn, and counts it in the next report's frames_abandoned.
    //
    // begin_frame(), flush() and finish_frame() make this renderer's
    // context current on the calling thread before they touch GL, so a
    // program with two windows never draws into the wrong one. A
    // failure to make it current is reported by finish_frame().
    // create_texture() and update_texture() make it current too, before
    // they touch GL, and return the failure (see each one).
    // Proved by: draw2d_parity_test (two windows, each with its own
    // context, and the other window's context made current before every
    // begin_frame(), flush() and finish_frame(): each window's clear and
    // pieces read back as pixels in that window only, on both systems).
    GLINTFX_API void begin_frame(const gltfx_frame_2d_desc &desc) noexcept;

    // Starts a batch with no transform: from here on, a world position
    // is a pixel position.
    GLINTFX_API void begin_batch() noexcept;

    // Starts a batch whose pieces are placed by `world_to_pixel`: scale
    // first, then rotation, then translation (core/transform.hpp),
    // turning a world position into a pixel position. A camera looking
    // at world point C, zoomed by Z and showing C at pixel P, with no
    // rotation, is {.translation = P - Z * C, .rotation = {0},
    // .scale = {Z, Z}}. A y-up world is a negative y scale.
    //
    // Changing batch does not by itself cost a draw call: the transform
    // is applied before the graphics card sees the pieces.
    GLINTFX_API void begin_batch(gltfx_transform world_to_pixel) noexcept;

    // A filled axis-aligned rectangle (in world space; turned only if
    // the batch transform turns it). `rect.corner` is the corner with
    // the smallest x and y, `rect.size` its width and height. A
    // negative width or height is refused (counted in the report);
    // zero is drawn and covers nothing.
    GLINTFX_API void fill_rect(gltfx_rect_world rect, gltfx_rgba color,
                               gltfx_draw_layer layer = {}) noexcept;

    // A filled quadrilateral with four free corners (core/quad.hpp),
    // painted as the two triangles (top_left, top_right, bottom_right)
    // and (top_left, bottom_right, bottom_left). A quad that crosses
    // itself is painted as those two triangles, as they are; nothing is
    // refused for its shape, only for a corner that is not a finite
    // number.
    GLINTFX_API void fill_quad(gltfx_quad_world corners, gltfx_rgba color,
                               gltfx_draw_layer layer = {}) noexcept;

    // A barrier: everything submitted so far is sent to the graphics
    // card NOW, in its order. Layer order does not cross a barrier: a
    // piece submitted after flush() is painted over everything before
    // it, whatever its layer. The current batch transform carries on.
    //
    // Use it before calling GL yourself in the middle of a frame (the
    // same contract SDL_FlushRenderer has). The renderer trusts NO GL
    // state when it draws again: it sets everything it depends on. And
    // after flush() and finish_frame() it leaves GL like this, and
    // nothing else is promised:
    //   - no program, no vertex array, no buffer and no texture bound
    //     (all zero), texture unit 0 active;
    //   - blending on, as (ONE, ONE_MINUS_SRC_ALPHA) for color and
    //     alpha, with the blend equation ADD for both;
    //   - the polygon mode FILL for both faces;
    //   - depth test, stencil test, scissor test and face culling off;
    //     color mask all on;
    //   - rasterizer discard, color logic op and sample-alpha-to-coverage
    //     off;
    //   - the DRAW framebuffer 0 (the surface itself);
    //   - the viewport covering the whole surface;
    //   - GL_FRAMEBUFFER_SRGB on exactly when the context option
    //     `srgb_framebuffer` is on.
    // Proved by: draw2d_parity_test (hostile state planted before the
    // frame - depth test on, blending swapped, a one-pixel scissor,
    // front faces culled, color mask off - and the pixels still right;
    // then every state above read back after flush()).
    GLINTFX_API void flush() noexcept;

    // Ends the frame: sends what is left to the graphics card and
    // returns the frame's report. It does NOT present: present after it
    // (gltfx_gl_context::swap_buffers(), or the loop does it for you).
    //
    // An error means the frame was not drawn as asked:
    //   - `out_of_memory`: pieces were dropped for lack of memory, or
    //     the graphics card itself ran out of memory (then
    //     rejected_value() names the step and os_error_code() holds the
    //     GL error code);
    //   - `platform_failure`: the graphics card refused the frame's data
    //     (rejected_value() "vertex_upload", "index_upload" or "draw",
    //     the GL error code in os_error_code()), or the context could
    //     not be made current (the context's own error, passed on);
    //   - `invalid_argument`, rejected_value() "frame": no frame was
    //     open.
    // Refused pieces are NOT an error: the frame is drawn without them,
    // and the report counts them. When there is an error, the report is
    // still there: read it with last_frame_report().
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_frame_2d_report> finish_frame() noexcept;

    // The report of the last finish_frame() call - the same value it
    // returned on success, and still readable when it returned an
    // error. All counters zero before the first frame.
    [[nodiscard]] GLINTFX_API gltfx_frame_2d_report last_frame_report() const noexcept;

    // Filled triangles, each corner with its own colour. `vertices` is read
    // only during this call (docs/api-conventions.md R5); the library copies
    // what it needs. `fill_triangles` reads the vertices in order, three at a
    // time. `fill_indexed_triangles` reads each three indices as three
    // vertices of the span. Two names, not one overload with an empty span:
    // `fill_triangles(v, {})` is ambiguous in g++ and clang++ (D-W8-282).
    //
    // One call is one piece, whatever the method (D-W8-287). A refused call is
    // refused WHOLE and counted once, never triangle by triangle. An empty
    // call (no vertices, or no indices) does nothing and is not a piece.
    // Refused, counted in the report, never an exception. The first rule
    // that applies names the reason:
    //   1. more than 2^32 vertices, which a 32-bit index cannot address:
    //      too_many_vertices;
    //   2. a vertex count (no indices) or an index count that is not a
    //      multiple of three: count_not_multiple_of_three;
    //   3. an index that is not a vertex of this span: index_out_of_range;
    //   4. a NaN in any position or colour: not_a_number; then an
    //      infinity in any of them: infinite;
    //   5. a position that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the
    //      vertex program would overflow: pixel_position_out_of_range.
    GLINTFX_API void fill_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                    gltfx_draw_layer layer = {}) noexcept;
    GLINTFX_API void fill_indexed_triangles(std::span<const gltfx_vertex_2d_world> vertices,
                                            std::span<const std::uint32_t> indices,
                                            gltfx_draw_layer layer = {}) noexcept;

    // A straight line from `segment.start` to `segment.end`, `style.stroke.thickness`
    // wide, with the end caps `style.cap`. The thickness is in the unit
    // `style.stroke.unit` (stroke.hpp): world by default, pixel on request. A
    // zero-length line (start equals end) draws the cap alone: nothing for `flat`,
    // a square for `square`, a disc for `rounded`, as SVG 2 defines it (D-W8-270).
    // D-W8-354: segment, style and colour are grouped; four parameters at most.
    //
    // One call is one piece. A line of zero thickness, or of zero length with
    // `flat`, is a piece of zero area: drawn, covering no pixel. The square of
    // a zero-length line is aligned with the axes of the space of its unit: the
    // world's (turned and scaled with the batch) or the screen's. A rounded cap
    // is half a disc of radius half the thickness: on the screen, half an
    // ellipse when a world-unit line is scaled unevenly. On the surface, its
    // edge is made of straight segments at most `curve_max_error_pixels`
    // (renderer_2d_desc.hpp, default 0.25) away from the true curve on the
    // screen, plus the rounding of their corners that fill_ellipse() states,
    // under the two conditions of fill_ellipse(): its larger radius on the
    // screen is at most 2^40 pixels, and the 1024 segments one cap may use
    // are enough for that distance. The part of a cap that lies off the
    // surface may be drawn coarser; no pixel on the surface changes for that.
    // Refused, counted in the report, never an exception.
    // The first rule that applies names the reason:
    //   1. a NaN in the start, the end, the thickness or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a negative thickness: negative_size;
    //   3. a thickness unit or a cap outside its enumeration: unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow, or a rounded cap whose larger radius on the
    //      screen passes the largest double: pixel_position_out_of_range.
    // Proved by: stroke_geometry_test (the outline of each cap and unit).
    // Proved by: stroke_line_check_test (the order of the refusals).
    // Proved by: stroke_line_pixels_test (caps in both units, under a
    // turned and unevenly scaled transform, of radius 1 to 2^40 pixels on
    // the screen and distance 1/16 to 8 pixels, swept on a surface of 800 by
    // 600 pixels with their float corners within the distance plus the
    // rounding, and the ends of every segment on it within their reach of the
    // band, the largest larger radius checked to pass 2^39; a cap of radius 50000
    // pixels; the distance read by the
    // cap; a cap just off the surface cut with no reach; the cap that cannot
    // be measured, refused).
    // Proved by: renderer_2d_impl_test (the distance given to open() sets the
    // segments of a cap).
    // Proved by: arc_parameters_test (the two conditions of the distance; the
    // half curve of a cap swept with a larger radius of 1 to 2^40 pixels, the
    // largest checked to pass 2^39.5, and distance 1/16 to 8 pixels, on
    // surfaces of 1920 by 1080 and 16384 by 16384 pixels, on its parameters,
    // in double, before any rounding to single precision; the coarser edge
    // off the surface).
    GLINTFX_API void stroke_line(gltfx_segment_world segment, gltfx_line_style style,
                                 gltfx_rgba color, gltfx_draw_layer layer = {}) noexcept;

    // The outline of an axis-aligned rectangle, grown by `style.align`: centered
    // on the edge (the default), inside the rectangle, or outside it. The
    // four sides do not overlap at the corners, so a half-transparent
    // outline has no darker corner. D-W8-354: four parameters at most.
    //
    // One call is one piece. The corners are square: each side meets the next
    // at a right angle. The thickness is in the unit `style.stroke.unit`
    // (stroke.hpp): in world unit the outline is the world outline carried by
    // the batch transform; in pixel unit it keeps its thickness on the screen
    // at every zoom. When the part of the outline that grows inward reaches
    // the middle of the rectangle's shorter side (the sides measured in the
    // unit of the thickness), there is no hole: the outline fills its outer
    // rectangle. Centered, that is a thickness of the whole shorter side or
    // more; inside, of half of it or more; outside, never. A rectangle
    // with no width or no height has no hole either, and the outline is, by
    // alignment: centered, the bar of the thickness around the segment;
    // inside, a piece of zero area, drawn, covering no pixel; outside, the
    // rectangle grown by the thickness, filled. A thickness of zero is a piece
    // of zero area: drawn, covering no pixel. Refused, counted in the report,
    // never an exception. The first rule that applies names the reason:
    //   1. a NaN in the corner, the size, the thickness or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a negative width, height or thickness: negative_size;
    //   3. a thickness unit or an alignment outside its enumeration:
    //      unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: stroke_rect_geometry_test (the outline of each alignment and unit).
    // Proved by: stroke_rect_check_test (the order of the refusals).
    // Proved by: draw2d_shapes_parity_test (the half-transparent corner).
    GLINTFX_API void stroke_rect(gltfx_rect_world rect, gltfx_rect_outline_style style,
                                 gltfx_rgba color, gltfx_draw_layer layer = {}) noexcept;

    // Makes a texture from `image`, on this renderer's context. The pixels
    // are read only during this call (docs/api-conventions.md R5): the
    // library converts them (core/image_view.hpp says to what) and keeps
    // its own copy on the graphics card (made updatable and with reduced
    // levels, one in the library's memory as well: see update_texture()).
    // Refused as gltfx_err, never an
    // exception (R1, R3), and the first refusal of this list wins:
    //   - invalid_argument: "width" or "height" is zero; "pixel_format" is
    //     `unspecified` or a value this build does not know; "bytes_per_row"
    //     or "pixels" does not hold the image (see image_view.hpp); "filter"
    //     is not a value of gltfx_texture_filter; "mipmap_levels" is more
    //     reduced levels than this size allows (texture_2d_desc.hpp).
    //   - the error of this renderer's context, as it came, when the
    //     context cannot be made current (nothing was touched).
    //   - unsupported, rejected_value() "width" or "height": larger than
    //     the card allows (the limit is read from the card, never assumed).
    //   - out_of_memory, rejected_value() "texture_create": the library ran
    //     out of memory (os_error_code() zero), or the card did
    //     (os_error_code() the GL code).
    //   - platform_failure, rejected_value() "texture_create": the card
    //     refused the texture for another reason (os_error_code() the GL
    //     code).
    // Like begin_frame(), this call makes this renderer's context current
    // before it touches GL. It sets the pixel-unpack state it reads with (no
    // unpack buffer, alignment 1, row length and skips zero) and leaves it
    // so, with no texture bound and texture unit 0 active.
    // Proved by: texture_desc_validation_test (each invalid_argument, by
    // name and in this order, in the check this call makes first).
    // Proved by: texture_upload_test (the creation this call forwards to,
    // over a fake GL: the context first, the card's limit, the two memory
    // failures, the unpack state set and left so, the stored texels
    // uploaded as sRGB, every reduced level).
    // Proved by: draw2d_parity_test (this call on a real context, over a
    // pixel-unpack buffer and unpack state left by the caller: the texture
    // opens with its size, the unpack state, the binding and the unit read
    // back as said above, and a zero width is refused by name).
    [[nodiscard]] GLINTFX_API gltfx_rslt<gltfx_texture_2d>
    create_texture(gltfx_image_view image, gltfx_texture_2d_desc desc = {}) noexcept;

    // Overwrites the part of `texture` that starts at texel (x, y) with
    // `image`, on this renderer's context. The pixels are read only during
    // this call, as create_texture() reads them. Only a texture made with
    // gltfx_texture_2d_desc::updatable can be changed. Refused as gltfx_err,
    // never an exception, and the first refusal of this list wins:
    //   - invalid_argument, rejected_value() "texture": the texture is not
    //     open, or belongs to another renderer;
    //   - invalid_argument, rejected_value() "updatable": the texture was
    //     made without gltfx_texture_2d_desc::updatable;
    //   - invalid_argument: "pixel_format", "bytes_per_row" or "pixels" as
    //     for create_texture(); "rect": the rectangle from (x, y), of the
    //     image's size, reaches past the texture (an image with no width may
    //     start at x equal to the texture's width, and likewise for y);
    //   - the error of this renderer's context, as it came, when the
    //     context cannot be made current (nothing was touched);
    //   - out_of_memory, rejected_value() "texture_update": the library ran
    //     out of memory (os_error_code() zero), or the card did
    //     (os_error_code() the GL code);
    //   - platform_failure, rejected_value() "texture_update": the card
    //     refused the writing for another reason (os_error_code() the GL
    //     code).
    // Every GL error the writing raised is read by this call, so none is
    // left for a later call to report. When the library runs out of
    // memory, nothing was touched. When the card refuses (out_of_memory or
    // platform_failure with a GL code), the library's memory is given back
    // exactly as it was before the call, so no refused texel reaches the
    // card through a later update; on the card, what the rectangle and the
    // reduced texels it touches hold is unspecified until an update that
    // covers the rectangle succeeds (OpenGL ignores the write it refuses,
    // not the writes before it). After out_of_memory from the card, OpenGL
    // leaves its whole state undefined: the texture's content on the card
    // is unspecified, and the way back is to make the texture again.
    //
    // An image with no width or no height is an empty update: once the
    // checks above pass, the call succeeds and does nothing (no texel
    // written, no GL call, the context not made current).
    //
    // The texture is READ at the send: updating it changes what the pieces
    // not yet sent show. This call does NOT send the batch. To keep the
    // previous content in those pieces, call flush() before it. The barrier
    // belongs to the consumer and is explicit.
    //
    // A texture with reduced levels has the reduced texels the change
    // touches recomputed, exactly as create_texture() computes them. For
    // that, an updatable texture with reduced levels keeps the texels of
    // every level in the library's memory too, from its creation to its
    // destruction: at least width x height x 4 bytes and less than twice
    // that (gltfx_texture_2d_desc::updatable gives the exact count); any
    // other texture keeps none.
    // Like create_texture(), this call makes this renderer's context current
    // before it touches GL, sets the pixel-unpack state it reads with (no
    // unpack buffer, alignment 1, row length and skips zero) and leaves it
    // so, with no texture bound and texture unit 0 active.
    // Proved by: texture_update_region_test (each invalid_argument, by name
    // and in this order, and the empty update).
    // Proved by: texture_update_test (the writing, over a fake GL: the
    // context first, the memory, the rectangle of each level, the reduced
    // texels equal to those of a texture made from the whole new image, the
    // unpack state set and left so; a writing the card refuses gives the
    // library's memory back, leaves no GL error behind, and never reaches
    // the card through a later update; the texels kept, counted).
    // Proved by: texture_update_kept_test (the rectangle each level
    // touches; the same swap twice gives the kept texels back, byte for
    // byte).
    // Proved by: renderer_2d_impl_test (an update in an open frame sends
    // nothing; an empty update makes no GL call).
    // Proved by: draw2d_parity_test (this call on a real context, over a
    // hostile unpack state: the state read back as said above; a rectangle
    // past the texture and a texture made without updatable refused by
    // name; an empty update succeeds).
    [[nodiscard]] GLINTFX_API gltfx_rslt<void> update_texture(gltfx_texture_2d &texture,
                                                              std::uint32_t x, std::uint32_t y,
                                                              gltfx_image_view image) noexcept;

    // Draws the region of `paint.region` from `texture` into the axis-aligned
    // rectangle `rect` (world space, like fill_rect), multiplied by `paint.tint`
    // as texture_region.hpp says. Mirroring is applied to the source, not to
    // the rectangle. Refused as ONE piece, counted in the report, never an
    // exception, for the first of these reasons, in this order:
    //   - not_a_number, then infinite: a value of the rectangle, of the
    //     source region or of the tint that is not a finite number;
    //   - negative_size: the rectangle or the source region with a negative
    //     width or height;
    //   - unknown_option: the mirror is a value outside its enumeration;
    //   - texture_not_open: the texture is released or was never opened;
    //   - texture_other_renderer: the texture belongs to another renderer;
    //   - region_out_of_range: the source rectangle is outside the texture;
    //   - pixel_position_out_of_range: the batch transform takes a corner
    //     outside the range the drawing program can scale, as for fill_rect.
    // A textured piece is painted in the same order as every other piece
    // (layer, then submission); a change of texture never reorders. In a
    // flush the graphics card accepts, each texture the library binds to
    // draw counts once in texture_switches: a change of texture between two
    // draw calls, a change to or from no texture, and the first texture of
    // every flush (the count starts again from no texture at every flush).
    // A flush the card refuses counts none (frame_2d_report.hpp).
    // D-W8-354: texture, paint and rect with the layer; four parameters at most.
    // Proved by: renderer_2d_impl_test (each reason in this order, over a
    // fake GL; the corners read the region by role and mirror; a texture
    // between two untextured pieces is three draw calls in paint order and
    // two switches; a flush the card refuses adds no switch).
    // Proved by: draw2d_texture_parity_test (on a real context, in both
    // values of srgb_framebuffer: textures A, B, A painted in that order
    // with three switches; a solid, a textured and a solid piece with two;
    // the four mirrors read back texel by texel; the tint over black).
    GLINTFX_API void draw_texture(const gltfx_texture_2d &texture, gltfx_texture_paint paint,
                                  gltfx_rect_world rect, gltfx_draw_layer layer = {}) noexcept;

    // Draws the region of `paint.region` from `texture` into a free
    // quadrilateral, with PROJECTIVE interpolation: a straight line of the
    // texture stays a straight line on the screen, even when the quad is a
    // trapezoid. The corner roles of gltfx_quad_world say where the texture's
    // top-left lands. Refused as ONE piece, counted in the report, for the
    // reasons of draw_texture (negative_size only for the source region: a
    // quadrilateral has no size), in its order, with `degenerate_quad` just
    // before pixel_position_out_of_range: the corners do not define a
    // projective mapping (three of them on one line, a quad whose edges
    // cross, or a corner that points inward; a quad of zero area has its
    // corners on one line, so it is refused the same way). Whether the
    // corners define a mapping is decided exactly, for any finite corners,
    // at any size, place and mix of magnitudes: no rounding enters that
    // answer. Quads that do define a mapping are refused as
    // `degenerate_quad` in one case only: when the diagonals cross so close
    // to one corner (closer than about 2^-64 of the diagonal's length) that
    // a projective weight of the quad would be 2^63 or more. Every quad
    // that does define a mapping and is drawn carries its four weights to
    // the graphics card multiplied by one common power of two, which leaves
    // the mapping as it is, so that each weight lies between 2^-32 and 2^32
    // (OpenGL 3.3 core profile specification, section 2.1.1: the largest
    // magnitude a texture coordinate can hold must be at least 2^32).
    // D-W8-354: four parameters at most.
    // Proved by: renderer_2d_impl_test (degenerate_quad after the texture's
    // reasons; the projective weight of each corner reaches the vertex).
    // Proved by: textured_quad_corners_test (a quad whose largest weight is
    // 2^63 or more, refused, the one past the largest float included; one
    // of 2^64/3, carried with the others below 2^32 by one power of two).
    // Proved by: projective_quad_test (the decision for quads of mixed
    // magnitudes, from slivers a few multiples of the smallest double tall
    // to half widths of 2^1023, and for every small whole-number quad; a
    // quad whose weight would pass the largest double, refused).
    // Proved by: draw2d_texture_parity_test (on a real context: a
    // checkerboard drawn into a trapezoid reads back, pixel by pixel, the
    // square the homography of the four corners puts there, including
    // pixels a drawing that ignored the projective weight would get wrong).
    GLINTFX_API void draw_texture_quad(const gltfx_texture_2d &texture, gltfx_texture_paint paint,
                                       gltfx_quad_world corners,
                                       gltfx_draw_layer layer = {}) noexcept;

    // Filled triangles textured from `texture`, each vertex with its own position (world space,
    // like fill_triangles), tint and texel coordinate (gltfx_textured_vertex_2d,
    // texture_region.hpp). `vertices` and `indices` are read only during this call
    // (docs/api-conventions.md R5). `fill_textured_triangles` reads the vertices in order, three at
    // a time; `fill_indexed_textured_triangles` reads each three indices as three vertices of the
    // span (two names, as fill_triangles and fill_indexed_triangles). The texture is read
    // AFFINELY across each triangle, with the filter of the texture (draw_texture_quad() is the
    // projective reading), and each texel is multiplied by the tint of its vertex, interpolated
    // across the triangle, as texture_region.hpp says.
    //
    // Texel (0, 0) is the top-left corner of the texture: the outer corner of the first texel of
    // its first row, the first row of the image it was made from. x grows along the row and y
    // grows DOWN, from one row to the next. The edges of a texel lie on whole numbers, so the
    // center of the first texel is (0.5, 0.5), as PIXELS says for the surface.
    // A triangle is drawn whatever the order of its three vertices, clockwise or counter-clockwise
    // on the screen: none is dropped for its orientation, because face culling is off whenever the
    // renderer draws (flush() lists the state it sets).
    // The triangles are blended over what the surface already holds as every piece is: the color
    // premultiplied, (ONE, ONE_MINUS_SRC_ALPHA), as COLOR and flush() say.
    // The call checks each vertex and each index a fixed number of times and copies the vertices,
    // their texel coordinates and the indices into the frame's storage; its work and the memory it
    // takes grow linearly with the number of vertices and indices, and the checking and the
    // copying happen in this call, before the frame is sent.
    //
    // One call is one piece. An empty call (no vertices, or no indices) does nothing and
    // is not a piece, whatever the texture. Refused WHOLE, counted once in the report, never an
    // exception, for the first of these reasons, in this order:
    //   1. more than 2^32 vertices, which a 32-bit index cannot address: too_many_vertices;
    //   2. a vertex count (no indices) or an index count that is not a multiple of three:
    //      count_not_multiple_of_three;
    //   3. an index that is not a vertex of this span: index_out_of_range;
    //   4. a NaN in any position, tint or texel coordinate: not_a_number; then an infinity in
    //      any of them: infinite;
    //   5. texture_not_open, then texture_other_renderer, as for draw_texture();
    //   6. a texel coordinate outside 0..width by 0..height of the texture (its edges are inside):
    //      region_out_of_range. A coordinate past an edge is refused, never clamped nor repeated;
    //   7. a position that the batch transform turns into a pixel coordinate larger in magnitude
    //      than about 1.7e38, where the vertex program would overflow: pixel_position_out_of_range.
    // A textured triangle piece is painted in the same order as every other piece and counts its
    // texture in texture_switches as draw_texture() says; right after a textured piece of the same
    // texture, it is drawn in the same draw call.
    // Proved by: renderer_2d_impl_test (each reason in this order but too_many_vertices, over a
    // fake GL; an empty call asks nothing of the texture; each vertex reaches the card with its
    // texel coordinate divided by the texture's size, q = 1 and the tint premultiplied; a
    // rectangle and a triangle of one texture are one draw call).
    // Proved by: triangle_list_check_test (too_many_vertices, from a count handed to the check
    // and not from a span: the counts alone refuse 2^32 + 1 vertices, before the multiple of
    // three; an infinity in a position, a tint or either texel coordinate is infinite; and,
    // within one call, a NaN before an infinity).
    // Proved by: draw2d_texture_parity_test (on a real context, in both values of
    // srgb_framebuffer: two indexed triangles of opposite orientations put each texel of a 2 x 2
    // texture in its quarter, the first texel of the first row at the top left; a vertex tint of
    // alpha 0.5 over black reads the value the tint of draw_texture() is held to; a rectangle and
    // a triangle of one texture are one draw call and one switch).
    // Proved by: draw2d_parity_test (front faces culled before the frame, and face culling read
    // back off after flush()).
    // Proved by: piece_list_test and triangle_store_test (a textured triangle piece keeps its
    // texture and one read per vertex in the frame's storage; an untextured one grows no read
    // storage).
    GLINTFX_API void fill_textured_triangles(const gltfx_texture_2d &texture,
                                             std::span<const gltfx_textured_vertex_2d> vertices,
                                             gltfx_draw_layer layer = {}) noexcept;
    GLINTFX_API void fill_indexed_textured_triangles(
        const gltfx_texture_2d &texture, std::span<const gltfx_textured_vertex_2d> vertices,
        std::span<const std::uint32_t> indices, gltfx_draw_layer layer = {}) noexcept;

    // A filled ellipse (core/ellipse.hpp; a circle when its two radii are
    // equal): `ellipse.radius_x` and `ellipse.radius_y` turned by
    // `ellipse.rotation` about `ellipse.center`, in the world, then placed by
    // the batch transform like every other piece. D-W8-354: four parameters
    // at most.
    //
    // One call is one piece, painted as a fan of triangles from the centre.
    // On the surface, its edge is made of straight segments at most
    // `curve_max_error_pixels` (renderer_2d_desc.hpp, default 0.25) away from
    // the true edge on the screen, plus the rounding of their corners, while
    // two conditions hold: the larger radius on the screen is at most 2^40
    // pixels, and the 1024 segments one call may use are enough for that
    // distance. A very small distance on a very large surface can need more,
    // and then the distance drawn is larger than asked for. The part of the
    // edge that lies off the surface may be drawn coarser; no pixel on the
    // surface changes for that. THE ROUNDING OF THE CORNERS: every position
    // becomes single precision last (PRECISION, above), and rounding moves a
    // corner by at most 2^-24 of its distance from the origin of the screen,
    // or by 2^-149 pixel, whichever is more.
    // So the library ends every segment that reaches the surface within 1024
    // pixels of the BAND, the surface grown by one pixel on every side (or
    // within 652 times the distance, when that is more), unless the 1024
    // segments are enough for the distance but not for ending them that
    // near: then the segments are cut for the distance alone, and their
    // corners are rounded where they lie. Where the ends lie that near, the
    // rounding added to the distance is at most 2^-24 of the distance from
    // the origin of the farthest point the band and that reach allow: with
    // the default distance, less than 1/4000 of a pixel on a surface of 1920
    // by 1080 pixels, and less than 1/600 on one of 16384 by 16384. A radius
    // of zero is not refused: with one or
    // both radii zero, the filled ellipse is a piece of zero area, drawn,
    // covering no pixel. Refused, counted in the report, never an exception.
    // The first rule that applies names the reason:
    //   1. a NaN in the centre, a radius, the rotation or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a radius below zero (-0.0 is zero): negative_size;
    //   3. a point of the edge that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: ellipse_check_test (the order of the refusals).
    // Proved by: ellipse_pixels_test (the edge within the distance on the
    // surface; 400 ellipses of a larger radius of 64 to 2^39.5 pixels on the
    // screen, under a turned and unevenly scaled transform, at distances of
    // 1/16 to 8 pixels, swept on a surface of 800 by 600 pixels with their
    // float corners within the distance plus the rounding, and the ends of
    // every segment on it within their reach of the band, the largest larger
    // radius, measured on the true ellipse, checked to pass 2^39; the
    // rounding of a corner by the library's one narrowing, at most 2^-24 of
    // its distance from the origin, and all of it at a tie).
    // Proved by: arc_parameters_test (the two conditions of the distance; the
    // coarser edge off the surface; the ends of every segment on the surface
    // within their reach of the band, swept on 400 whole and half curves of a
    // larger radius of 1 to 2^40 pixels, the largest checked to pass 2^39.5,
    // at distances of 1/16 to 8 pixels, with reaches up to 4096 pixels, on
    // surfaces of 1920 by 1080 and 16384 by 16384 pixels; the segments cut
    // for the distance alone when the 1024 are too few to end them near, on
    // a curve built for it).
    // Proved by: renderer_2d_impl_test (one call is one piece; the distance
    // given to open() sets the segments).
    // Proved by: draw2d_shapes_parity_test (a filled circle read back as
    // pixels; a distance of 8 pixels draws the circle as a pentagon).
    GLINTFX_API void fill_ellipse(gltfx_ellipse_world ellipse, gltfx_rgba color,
                                  gltfx_draw_layer layer = {}) noexcept;

    // The outline of an ellipse, `stroke.thickness` wide, centred on its
    // curve: half the thickness on each side. There is no alignment for an
    // ellipse (D-W8-281). D-W8-354: four parameters at most.
    //
    // One call is one piece. The thickness is in the unit `stroke.unit`
    // (stroke.hpp): in world unit the outline is the world outline carried by
    // the batch transform (a non-uniform scale makes it thicker on one axis);
    // in pixel unit it is built on the screen, half the thickness away from
    // the screen ellipse across its curve, at every zoom. On the surface, its
    // two edges are within `curve_max_error_pixels` of their true curves,
    // plus the rounding of their corners that fill_ellipse() states, under
    // the two conditions of fill_ellipse(). Its band is the surface grown by
    // one pixel and by the reach of half the thickness on the screen (half
    // the thickness in pixel unit; in world unit, half the thickness times
    // the largest stretch of the batch transform), and the corners lie that
    // reach or less off the curve, so their rounding is at most that of a
    // point twice the reach farther out. Where the curve bends
    // tighter than half the thickness, the inner edge is the true boundary of
    // the points farther than half the thickness from the curve, so it never
    // folds over itself, and a half-transparent outline covers each pixel
    // once. When half the thickness reaches the shorter half axis of the
    // ellipse (measured in the unit of the thickness), there is no hole: the
    // outline fills its outer edge. A radius of zero is not refused: the
    // outline is the one the same thickness gives every other ellipse,
    // continued (a disc of radius half the thickness when both radii are
    // zero, a bar with rounded ends when one is). A thickness of zero is a
    // piece of zero area: drawn, covering no pixel. Refused, counted in the
    // report, never an exception. The first rule that applies names the
    // reason:
    //   1. a NaN in the centre, a radius, the rotation, the thickness or the
    //      colour: not_a_number; then an infinity in any of them: infinite;
    //   2. a radius or a thickness below zero (-0.0 is zero): negative_size;
    //   3. a thickness unit outside its enumeration: unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: ellipse_check_test (the order of the refusals).
    // Proved by: ellipse_outline_test (the inner edge within the true
    // boundary; the ring turning with the outline; no hole once half the
    // thickness reaches the shorter half axis).
    // Proved by: ellipse_pixels_test (the outline of each unit; the shapes of
    // radius zero).
    // Proved by: renderer_2d_impl_test (one call is one piece).
    // Proved by: arc_parameters_test (the ends of every segment of the curve
    // on the surface within their reach of the band, the half thickness
    // included, swept with reaches up to 4096 pixels, in double). The
    // corners of the outline in single precision are not swept: their
    // rounding is the one of the library's narrowing that
    // ellipse_pixels_test pins.
    // Proved by: draw2d_shapes_parity_test (a half-transparent outline of a
    // long ellipse, bent tighter than half its thickness, reads the same
    // value along its band and black in its hole).
    GLINTFX_API void stroke_ellipse(gltfx_ellipse_world ellipse, gltfx_stroke stroke,
                                    gltfx_rgba color, gltfx_draw_layer layer = {}) noexcept;

    // Installs the resolver of this renderer. Returns the one installed before,
    // for whoever wants to put it back. Not [[nodiscard]]: almost everyone
    // installs once and does not use the previous one (the same criterion as
    // gltfx_log_set_sink, core/log/sink.hpp). A null function removes it. The
    // swap is valid from the next paint call. A call that reenters the resolver
    // (the resolver asks a paint, or installs another resolver) is a refusal,
    // counted in the frame report, and it is born with the first paint bridge.
    GLINTFX_API gltfx_image_resolver set_image_resolver(gltfx_image_resolver resolver) noexcept;

  private:
    explicit gltfx_renderer_2d(renderer_2d_impl *interior) noexcept : impl(interior) {}
    friend struct renderer_2d_internal_access;
    renderer_2d_impl *impl = nullptr;
};

} // namespace glintfx
