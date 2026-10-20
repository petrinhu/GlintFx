// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/image_view.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/segment.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/draw_layer.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
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
    // is made of straight segments at most 0.25 pixel away from the true half
    // circle on the screen while its radius there is at most 13280 pixels;
    // above that, the distance is not guaranteed. Refused, counted in the
    // report, never an exception.
    // The first rule that applies names the reason:
    //   1. a NaN in the start, the end, the thickness or the colour:
    //      not_a_number; then an infinity in any of them: infinite;
    //   2. a negative thickness: negative_size;
    //   3. a thickness unit or a cap outside its enumeration: unknown_option;
    //   4. a point of the outline that the batch transform turns into a pixel
    //      coordinate larger in magnitude than about 1.7e38, where the vertex
    //      program would overflow: pixel_position_out_of_range.
    // Proved by: stroke_geometry_test (the outline of each cap and unit).
    // Proved by: stroke_line_check_test (the order of the refusals).
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
    // corners on one line, so it is refused the same way).
    // D-W8-354: four parameters at most.
    // Proved by: renderer_2d_impl_test (degenerate_quad after the texture's
    // reasons; the projective weight of each corner reaches the vertex).
    // Proved by: draw2d_texture_parity_test (on a real context: a
    // checkerboard drawn into a trapezoid reads back, pixel by pixel, the
    // square the homography of the four corners puts there, including
    // pixels a drawing that ignored the projective weight would get wrong).
    GLINTFX_API void draw_texture_quad(const gltfx_texture_2d &texture, gltfx_texture_paint paint,
                                       gltfx_quad_world corners,
                                       gltfx_draw_layer layer = {}) noexcept;

  private:
    explicit gltfx_renderer_2d(renderer_2d_impl *interior) noexcept : impl(interior) {}
    friend struct renderer_2d_internal_access;
    renderer_2d_impl *impl = nullptr;
};

} // namespace glintfx
