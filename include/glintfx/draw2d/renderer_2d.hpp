// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/color.hpp>
#include <glintfx/core/err.hpp>
#include <glintfx/core/quad.hpp>
#include <glintfx/core/rect.hpp>
#include <glintfx/core/transform.hpp>
#include <glintfx/draw2d/draw_layer.hpp>
#include <glintfx/draw2d/frame_2d_desc.hpp>
#include <glintfx/draw2d/frame_2d_report.hpp>
#include <glintfx/draw2d/renderer_2d_desc.hpp>
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

  private:
    explicit gltfx_renderer_2d(renderer_2d_impl *interior) noexcept : impl(interior) {}
    friend struct renderer_2d_internal_access;
    renderer_2d_impl *impl = nullptr;
};

} // namespace glintfx
