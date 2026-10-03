<!-- Origem: auditoria-api-draw2d.md, md5 1006552f7bf0c982df6ee4a383320d72. Corpo copiado byte a byte; so este cabecalho e o apendice final foram acrescentados (D-FECH-3). -->

# Parecer B0: revisão de API dedicada do desenho 2D (`R2D-BATCH`)

**Quem:** revisor de API dedicado da W7-D (agente `api-review`), distinto do CTO que planejou (`cto-review`) e do implementador (`impl-da14`), pela L-12. **Data real:** 29/09/26 - 23:05 (`date`). **Árvore:** fixture montado do blob `e39fcb021bf08d3d12fd8e6e991cf2a55e4464ad` (`git archive`). **Nada no repositório foi editado.**

**Plano lido:** `/var/tmp/cto-w7d/PLANO.md` na versão **`e923192a5aaaf72a5f6792a486df5e86`** (o briefing esperava `892f16ce...`; a divergência foi avisada ao team-lead às 22:46). Às 23:03 o arquivo estava em **`bd73502a64fa2b4c41d338ce4c09de1e`**, com diferença só de redação em três linhas sobre a B3a do adendo, nada em §3 (D-W7D-18) nem §4.B.

**Fontes internas:** `docs/plano-w7d.md` §2.1, §4 inteira e §9 (D-W7D-08 a 16); `docs/plano-w7d-adendo-revalidacao.md` §3.B e §6 (D-W7D-R2, R8); a memória `project_decisoes_api_desenho_2d.md` (as três decisões do líder de 19/09, que NÃO reabro); `docs/api-conventions.md` R1 a R10; `GODS_LAWS.md` L-12, L-17, L-19, L-21, L-22, L-26, L-27, L-28, L-29, L-32, L-34; `include/glintfx/core/{mat3,transform,vec2,rect,color,angle}.hpp`; `include/glintfx/platform/gl/context.hpp`; `include/glintfx/platform/loop/loop.hpp` (P10); `TODO.md` linhas de `R2D-BATCH`, `R2D-TEXTURE`, `R2D-YSORT`, `R2D-TRANSFORM`, `R2D-BLEND-ADDITIVE`.

**Leis aplicadas e como:** as mesmas do parecer P0 (L-12, L-21, L-22/R3, L-27, L-29, L-34 com a emenda de 29/09, L-11 global, L-09), mais: L-19 (um handle por assunto, PIMPL; tipo de valor do núcleo tem layout visível e mora no núcleo); L-17 (um assunto por arquivo; as cinco perguntas não se aplicam a cabeçalho sem corpo, mas a pergunta do nome, sim, a cada tipo); L-28 (a pergunta "quem nunca viu entende lendo uma vez?" feita a cada nome); L-32 com a extensão de 27/08 (nada de R2D-SHAPES congela antes da demo).

**Bibliotecas lidas para aprender (L-29, licença conferida, nada copiado):** SDL3 (zlib), documentação de `SDL_FlushRenderer`, `SDL_RenderGeometry`, `SDL_DestroyRenderer`; raylib (zlib, declarada na primeira linha do `raylib.h`), só as declarações públicas de `Camera2D`, `BeginMode2D`, `DrawRectangleRec`, `DrawRectanglePro`, `DrawLineEx`, `GetWorldToScreen2D`; bgfx (BSD-2), documentação de `bgfx::Stats`/`getStats`; Cairo, só a página de manual de `cairo_matrix_t` (documentação, não código); MonoGame, só a página de documentação de `SpriteBatch.Begin`; a referência EGL de `eglSwapBuffers` (Khronos). **Tentadas e NÃO lidas:** a página `love.graphics.getStats` do LÖVE devolveu 403; o `sokol_gfx.h` veio truncado antes de `sg_frame_stats`. Nada do que eu digo abaixo se apoia nessas duas.

---

## 1. Veredito

**APROVADO COM EMENDAS, e com UM achado CRÍTICO que muda uma assinatura do rascunho** (B0-C1). Os nomes da v1 congelam no texto da seção 2, que já incorpora todas as emendas; B4 publica esse texto. O rascunho de `docs/plano-w7d.md` §4.2 NÃO pode ser publicado como estava: `begin_batch(const gltfx_mat3 &)` quebra a decisão de precisão congelada do líder.

**Contagem de achados:** 2 CRÍTICO, 10 IMPORTANTE, 4 COSMÉTICO.

**O que muda no plano por causa deste parecer (L-67: o plano muda antes do código; o commit de B0 corrige as linhas):**
1. B4 publica SEIS cabeçalhos, não quatro: `include/glintfx/core/quad.hpp` (não `draw2d/quad_world.hpp`, B0-I3), `include/glintfx/draw2d/{renderer_2d,renderer_2d_desc,frame_2d_desc,frame_2d_report,draw_layer}.hpp`.
2. B2a ganha uma célula com câmera em posição que a precisão simples NÃO representa (B0-C1).
3. B5 ganha a célula que lê o estado de GL DEPOIS de `flush()` (B0-I5).
4. D-W7D-13, na parte "erro com os campos de contagem", passa a se cumprir por `last_frame_report()` (B0-C2).

---

## 2. O texto congelado: os nomes da v1, com o texto inteiro dos cabeçalhos

**A fonte única é ESTE parecer, versionado em `docs/auditoria-api-draw2d.md`** (D-W7D-R8: `/var/tmp` não sobrevive nem é visto no servidor). O fechamento de `R2D-BATCH` (adendo §3.B item 4) compara o publicado com os blocos de código abaixo, por máquina; cada um traz o md5 do arquivo. O fixture de onde saíram (`/var/tmp/cto-w7d/api-review/fixture/include/`, que também foi a sonda do portão) é cópia de conveniência, byte a byte igual (conferido por script), e não é para commit. As linhas etiquetadas **[B5]** na seção 7 entram só no commit de B5; todo o resto entra em B4.

### `include/glintfx/core/quad.hpp`

md5 `f233cfc646685998cd1922af5853c67d`, 35 linhas.

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// core/quad.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md, B0): four
// corners in WORLD space, in double precision, with no rule that the
// sides be parallel to anything. It is the general shape a rectangle
// becomes once it is turned, slanted or tapered, and it lives here, in
// the core layer next to gltfx_rect_world, because it is pure geometry:
// it knows nothing about drawing, GL or the operating system (GODS_
// LAWS.md L-19, "value type of the core").
//
// THE CORNERS ARE NAMED BY ROLE, NOT BY POSITION ON SCREEN. `top_left`
// is the corner where the top-left of an image lands when an image is
// drawn onto this quad (R2D-TEXTURE); after a rotation of half a turn
// it sits at the bottom right of the screen and is still `top_left`.
// Walking top_left -> top_right -> bottom_right -> bottom_left goes
// once around the quad. Either direction of travel is accepted.
//
// Trivial aggregate; the layout IS the contract (GODS_LAWS.md L-19,
// L-26). Nothing here is validated: a quad whose corners are not finite
// numbers is a value like any other, and whoever consumes it decides
// what to do with it.

namespace glintfx {

struct gltfx_quad_world {
    gltfx_vec2_world top_left;
    gltfx_vec2_world top_right;
    gltfx_vec2_world bottom_right;
    gltfx_vec2_world bottom_left;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/draw_layer.hpp`

md5 `e2664cfdf2bdc69c971d7ae716563ba3`, 32 linhas.

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/draw_layer.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): the OPTIONAL order key of one drawn piece (the leader's decision
// of 27/08/2026, TODO.md line R2D-BATCH).
//
// THE ORDER, frozen: inside one frame, between two flush() barriers,
// pieces are painted by (layer, order of submission). A lower layer is
// painted first, so a higher layer lands on top. Two pieces on the SAME
// layer keep the order you submitted them in, every frame, on every
// system. A piece drawn without a layer is on layer 0, so a program
// that never names a layer gets plain submission order.
// Proved by: draw_order_test (the closed set of eight cells {no layer,
// equal, lower, higher} x {submitted A then B, B then A}, and a
// thousand pieces on one layer kept in submission order).
// Proved by: draw2d_parity_test (the same eight cells read back as
// pixels from a real surface, on both systems).
//
// WHY A TYPE AND NOT A BARE INTEGER: `gltfx_draw_layer{3}` at a call
// site says what the 3 is. A bare 3 does not, and a bare integer
// converts silently from a size, a count or a coordinate.

namespace glintfx {

struct gltfx_draw_layer {
    std::int32_t value = 0;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/renderer_2d_desc.hpp`

md5 `d9a59e2caa865b918c3e64cfdf050353`, 28 linhas.

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

// draw2d/renderer_2d_desc.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what gltfx_renderer_2d::open() needs besides the context.
//
// LAYOUT IS THE CONTRACT (GODS_LAWS.md L-19): adding a field is an ABI
// change. Before 1.0 (SOVERSION 0, GODS_LAWS.md L-26) that is allowed
// and is announced by the version; from 1.0 on, a new knob arrives as a
// new descriptor type with its own open() overload, never as a field
// appended here.

namespace glintfx {

struct gltfx_renderer_2d_desc {
    // A HINT, never a limit and never a promise: roughly how many pieces
    // (one call of fill_rect() or fill_quad() is one piece) you expect
    // to submit in one frame. The renderer reserves room for that many
    // at open(), so the first frames do not grow storage while you draw.
    // Zero lets the renderer choose. Submitting more than this is always
    // allowed: storage grows, and if growing fails the pieces that did
    // not fit are counted in the frame report (never a crash).
    std::size_t reserve_pieces = 0;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/frame_2d_desc.hpp`

md5 `b715c5ba2e2bf40539225c52fe6aada8`, 31 linhas.

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/core/color.hpp>

// draw2d/frame_2d_desc.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what gltfx_renderer_2d::begin_frame() needs.
//
// LAYOUT IS THE CONTRACT: the same growth rule as
// gltfx_renderer_2d_desc (see that header).
//
// WHY THE FRAME IS CLEARED BY DEFAULT: after a buffer swap, what is left
// in the buffer you draw into is undefined unless the
// surface was created to preserve it (the EGL reference for
// eglSwapBuffers says so in those words). A frame that does not clear
// shows whatever the driver left there. Leaving it out is a choice you
// make on purpose, by writing std::nullopt.

namespace glintfx {

struct gltfx_frame_2d_desc {
    // The color the whole surface is filled with before anything is
    // drawn in this frame, or std::nullopt to keep what is there.
    // Linear light, straight alpha, like every gltfx_rgba. Default:
    // opaque black.
    std::optional<gltfx_rgba> clear_color = gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F};
};

} // namespace glintfx
````

### `include/glintfx/draw2d/frame_2d_report.hpp`

md5 `df24f6b88a8d7c23994a2856bee4339e`, 86 linhas.

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// draw2d/frame_2d_report.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what happened to one frame, counted. It is the ONE place a frame
// reports anything (the leader's decision of 19/09/2026: drawing calls
// return nothing; a problem is reported once, at the end of the frame).
//
// LAYOUT IS THE CONTRACT: the same growth rule as
// gltfx_renderer_2d_desc (see that header). New counters are appended
// at the end, never inserted, never reordered.
//
// WHAT A "PIECE" IS: one call of fill_rect() or fill_quad(). Every piece
// submitted while a frame is open ends in exactly one of three places:
//
//   pieces_submitted == pieces_drawn + pieces_refused
//                       + pieces_dropped_out_of_memory
//
// Proved by: frame_report_tally_test (a piece holding a non-number is
// refused with the token of the FIRST reason; an empty frame reports
// zero; running out of memory becomes a count and one error at the
// end, never a thrown exception).
// A piece submitted while NO frame is open (before begin_frame(), or
// after finish_frame()) is not part of any frame: it is dropped and
// counted in pieces_dropped_outside_frame of the NEXT report, never in
// pieces_submitted.

namespace glintfx {

// Why the FIRST refused piece of a frame was refused. The token of each
// value (gltfx_draw_2d_refusal_name()) is an identifier, never a
// sentence (docs/api-conventions.md R7). APPEND-ONLY: a value is never
// renumbered and never reused.
enum class gltfx_draw_2d_refusal : std::uint8_t {
    none = 0,
    not_a_number = 1,
    infinite = 2,
    negative_size = 3,
};

// The token for `reason`: "none", "not_a_number", "infinite",
// "negative_size". A value this build does not know (a newer glintfx
// crossing the library boundary) reads "unknown" (docs/api-
// conventions.md R4). The returned text lives as long as the program.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal reason) noexcept;

struct gltfx_frame_2d_report {
    // Pieces submitted while this frame was open.
    std::uint64_t pieces_submitted = 0;
    // Pieces that reached the graphics card. A piece whose area is zero
    // is drawn: it simply covers no pixel.
    std::uint64_t pieces_drawn = 0;
    // Pieces refused because a value was not a finite number, or a
    // rectangle had a negative width or height. The frame goes on
    // without them; this is not an error of the frame.
    std::uint64_t pieces_refused = 0;
    // Why the first of those was refused; `none` when pieces_refused is
    // zero.
    gltfx_draw_2d_refusal first_refusal = gltfx_draw_2d_refusal::none;
    // Pieces dropped because the renderer could not get memory to store
    // them. When this is not zero, finish_frame() returns an error.
    std::uint64_t pieces_dropped_out_of_memory = 0;
    // Pieces submitted with no frame open, since the previous report.
    std::uint64_t pieces_dropped_outside_frame = 0;
    // Frames that were begun and never finished (begin_frame() called
    // while a frame was open throws the open one away, undrawn), since
    // the previous report. The pieces of an abandoned frame are not
    // counted in any other field of any report.
    std::uint64_t frames_abandoned = 0;
    // How many times the transform changed (begin_batch()), counting
    // the pixel-direct batch every frame starts with.
    std::uint64_t batches = 0;
    // How many flush() barriers this frame had.
    std::uint64_t flushes = 0;
    // How many draw calls reached the graphics card. Printed for you to
    // measure, never promised as a number.
    std::uint64_t draw_calls = 0;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/renderer_2d.hpp`

md5 `1dbbb02c5b333c41c6e88adfa34a31ce`, 252 linhas.

````cpp
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
    //   - `out_of_memory`: the room asked for by `desc` could not be
    //     reserved.
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
    //     alpha;
    //   - depth test, stencil test, scissor test and face culling off;
    //     color mask all on;
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
    //   - `out_of_memory`: pieces were dropped for lack of memory;
    //   - `platform_failure`: the graphics card refused the frame's data
    //     (rejected_value() "vertex_upload" or "draw"), or the context
    //     could not be made current (the context's own error, passed
    //     on);
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
````


---

## 3. As regras de `docs/api-conventions.md`, uma a uma

| Regra | Veredito | Por quê |
|---|---|---|
| R1, envelope único | **CONFORME** | Os dois pontos falíveis devolvem `gltfx_rslt`: `open()` e `finish_frame()`. As chamadas de desenho devolvem `void` por decisão do líder de 19/09 (a falha é acumulada e relatada no fim), não por serem falíveis sem envelope: elas não falham sozinhas, alimentam o relatório. |
| R2, `[[nodiscard]]` estrutural | **CONFORME** | Vem do `gltfx_rslt`. `last_frame_report()` e `gltfx_draw_2d_refusal_name()` levam `[[nodiscard]]` explícito, como as funções totais das irmãs. |
| R3, `noexcept` medido | **CONFORME, com condição de B2c/B4** | Tudo é `noexcept`. As chamadas de desenho ALOCAM quando o armazenamento cresce: degrau 2 da R3 (captura dentro do `try`, a peça vira contagem em `pieces_dropped_out_of_memory`). `noexcept_alloc_test` sem linha nova é a prova; a célula (e) de B2 ("alocador armado falha no meio, nada lança") é o teste de comportamento. |
| R4, ausência nunca é comportamento indefinido | **CONFORME** | `gltfx_draw_2d_refusal_name()` fora da tabela lê `"unknown"`; `last_frame_report()` antes do primeiro quadro lê tudo zero; peça fora de quadro é contada, não perdida em silêncio; `finish_frame()` sem quadro aberto é recusado por nome (`"frame"`). |
| R5, alocar e liberar do mesmo lado | **CONFORME** | Nenhum contêiner atravessa. `gltfx_frame_2d_report` e `gltfx_quad_world` são triviais (`static_assert(std::is_trivially_copyable_v<...>)` compilado no TU da seção 6.4). O `std::optional<gltfx_rgba>` de `gltfx_frame_2d_desc` não aloca. |
| R6, nome sem colisão | **CONFORME, metade Windows NÃO EXECUTADA** | Seção 6: 0 colisão real no texto congelado; `flush` e `layer`, que o plano marcava como suspeitos, limpos; `clear` evitado de propósito (B0-K2). Windows: lista de R6 conferida à mão, nenhum nome novo casa; a prova real é `header_hygiene_test` no trabalho Windows do servidor em B4. |
| R7, token, nunca frase | **CONFORME COM EMENDA (B0-I8)** | Recusas e erros são tokens (`not_a_number`, `vertex_shader`, `vertex_upload`, `frame`, `context`, nome da função GL). O rascunho punha "o registro do driver como campo" do erro, e o `gltfx_err` não tem campo de texto livre (FATO: seus campos são `path`, `line`, `column`, `byte_offset`, `rejected_value`, `os_error_code`, `docs/api-conventions.md`, auditoria de `gltfx_err`); a mensagem do driver é frase. Emenda: vai ao registro (R9). |
| R8, retorno de chamada | **NÃO SE APLICA** | Nenhum. |
| R9, evento de registro | **CONFORME (acréscimo)** | Um evento novo, `draw2d_program_rejected`, com o campo de texto `driver_log`. Nome de evento e nome de campo são identificadores (R9). |
| R10, as quatro camadas | **NÃO SE APLICA**, com uma nota | O desenhador é chamado de dentro do `on_render` do laço; o cabeçalho diz que `finish_frame()` NÃO apresenta e que o laço apresenta depois do `on_render`. |
| Disciplina de citação | **CONFORME, com a ordem de entrada da seção 7** | Simulado por máquina (seção 6.5), com o limite do portão medido (B0-I7). |

---

## 4. As perguntas que o plano manda a B0

**O plano não numera "as seis perguntas".** Medido: `grep -rn 'seis perguntas'` acha só as duas linhas que as mencionam (adendo §3.B, PLANO §4.B), sem lista. **INF, declarada:** reconstruí pelas decisões de `docs/plano-w7d.md` §9 que mandam um ponto a B0, uma por decisão, e são exatamente seis; mais os três pontos da linha de B0 (`plano-w7d.md:222`) que não são decisão. Respondo as nove.

| # | Pergunta, e de onde vem | Resposta congelada |
|---|---|---|
| 1 | Tempo de vida: o desenhador segura o interior do contexto ou mover o contexto é proibido? (`plano-w7d.md:203`) | Segura o interior; mover o handle do contexto é inofensivo. O contexto TEM de viver mais que o desenhador: a mesma pré-condição da família (D-API-06). |
| 2 | Onde é o "ao apresentar o quadro" (D-W7D-08, `:385`), e o quadro não terminado | `finish_frame()`, chamado pelo consumidor, devolve o relatório; não apresenta. `begin_frame()` com quadro aberto joga o aberto fora e o conta em `frames_abandoned` do próximo relatório. |
| 3 | `flush()` é pública? (D-W7D-11, `:391`) | Sim, com a lista FECHADA do estado de GL em que o desenhador deixa o contexto (B0-I5). |
| 4 | Peça inválida e falta de memória (D-W7D-13, `:395`) | Inválida é recusada, contada, com o motivo da PRIMEIRA; o quadro segue e não é erro. Falta de memória descarta, conta e faz `finish_frame()` devolver `out_of_memory`; as contagens ficam em `last_frame_report()` (B0-C2). "Quad degenerado" deixa de ser recusa (B0-I4). |
| 5 | O nome do diretório público (D-W7D-14, `:397`) | `include/glintfx/draw2d/` fica. O tipo de geometria pura sai dele e vai para `core/` (B0-I3). |
| 6 | `fill_quad` na v1? (D-W7D-16, `:401`) | Sim, com os cantos nomeados por papel e a regra de triangulação escrita (B0-I4). |
| 7 | R6 de `flush` e `layer` (`:222`) | Medido: limpos (seção 6). |
| 8 | Legibilidade L-28 (`:222`) | Respondida nome a nome na seção 5 (cada decisão diz por que o nome escolhido se lê de primeira). |
| 9 | O ponto único de relato (`:222`) | Igual à pergunta 2, mais `last_frame_report()`, que é LEITURA do mesmo relatório, não um segundo ponto de relato. |

---

## 5. Achados

### CRÍTICO

**B0-C1. `begin_batch(const gltfx_mat3 &world_to_pixel)` estreita a câmera para precisão simples ANTES de a biblioteca subtraí-la, o contrário da decisão de precisão do líder.**
- FATO: `gltfx_mat3` é `std::array<float, 9>` (`core/mat3.hpp`, `struct gltfx_mat3`), e o próprio cabeçalho diz que ele é "the SCREEN side of the two precision families", construído UMA vez por quadro a partir de `gltfx_transform`, "the narrowing from world precision happens in exactly one place". D-W7D-09 (`plano-w7d.md:387`) escolheu transformar na CPU em double exatamente para "o mundo nunca ser estreitado antes de a câmera ser subtraída".
- FATO medido por aritmética: com a matriz em float, uma câmera em x = 16 777 217 vira 16 777 216 (2^24 + 1 não cabe em 24 bits de mantissa); uma peça em x = 16 777 218 sai no pixel **2**, e o certo é **1**.
- FATO, e é o que tornava o defeito invisível: a célula de estreia de B2a escolhe câmera em -16 777 216 = -2^24, que É representável em float. O teste do plano passaria com a assinatura errada.
- Resolução congelada: `begin_batch(gltfx_transform world_to_pixel)`, tipo double que já existe e já está congelado (`core/transform.hpp`). D-API-01.
- Correção de B2a (o plano muda): acrescentar a célula "câmera em 16 777 217, peça em 16 777 218, pixel esperado 1,0"; o mutante "matriz em float" tem de dar 2,0.

**B0-C2. D-W7D-13 promete "erro `out_of_memory` com os campos de contagem", e o envelope não comporta as duas coisas.**
- FATO: `gltfx_rslt<T>` guarda valor OU erro (`std::variant`, `docs/api-conventions.md` R1); `gltfx_err` não tem campo de contagem (campos listados na linha R7 acima). O desenho do rascunho perde as contagens exatamente no quadro que falhou, quando elas mais importam.
- Resolução congelada: `last_frame_report()`, leitura do relatório do último `finish_frame()`, que existe também quando ele devolveu erro. D-API-02.

### IMPORTANTE

**B0-I1. `reserve_quads` nomeia o mecanismo antigo.** FATO: D-W7D-18 troca o agrupador para triângulo indexado, e R2D-SHAPES traz peças que não são quadrilátero. Resolução: `reserve_pieces`, na mesma unidade do relatório ("peça" = uma chamada de desenho), e o cabeçalho diz que é dica, nunca limite. D-API-09.

**B0-I2. O rascunho diz que os descritores "crescem só por acréscimo" como se isso fosse compatível.** FATO: L-19 diz que layout de tipo de valor é contrato, e mudá-lo é mudança de ABI assumida. Acrescentar campo a uma `struct` passada por referência ou devolvida por valor é quebra de ABI. Resolução congelada: os três cabeçalhos de valor dizem a regra verdadeira (antes da 1.0 pode, anunciado pela versão, L-26; da 1.0 em diante, descritor novo com sobrecarga nova). D-API-09.

**B0-I3. `gltfx_quad_world` é geometria pura e estava planejado dentro de `draw2d/`.** FATO: o plano o põe em `include/glintfx/draw2d/quad_world.hpp` (PLANO §4.B, B4); `gltfx_rect_world`, o irmão dele, mora em `core/rect.hpp`; L-19 põe "tipos" no núcleo puro. Resolução congelada: `include/glintfx/core/quad.hpp`. Porta de mão única: o caminho de `#include` congela (R6). D-API-05.

**B0-I4. `gltfx_quad_world` não fixava a ordem dos cantos, a diagonal, nem o que é "degenerado".** Resolução congelada: cantos nomeados por PAPEL (onde cai o canto de uma imagem, o que R2D-TEXTURE vai precisar), triangulação `(top_left, top_right, bottom_right)` e `(top_left, bottom_right, bottom_left)`, nenhuma recusa pela forma (só por número não finito), área zero é desenhada e não cobre pixel. D-API-05.

**B0-I5. A promessa do estado de GL na SAÍDA não tinha teste.** FATO: as células de B5 (PLANO §4.B e `plano-w7d.md:227`) plantam estado hostil na ENTRADA; nenhuma lê o estado depois de `flush()`. O cabeçalho promete uma lista fechada. Resolução: a linha de prova [B5] do bloco de `flush()` exige as duas metades, e B5 ganha a célula que lê cada item da lista depois de `flush()` (o plano muda).

**B0-I6. A camada como sobrecarga dobra a superfície de cada primitiva.** FATO: o rascunho dá duas sobrecargas por forma (`plano-w7d.md:195`); com R2D-SHAPES seriam oito funções exportadas em vez de quatro. Resolução congelada: um parâmetro `gltfx_draw_layer layer = {}` com argumento padrão, e o tipo forte `gltfx_draw_layer`. D-API-04.

**B0-I7. (do instrumento, gêmeo de P0-I5) O portão de citação aceita um bloco se UMA citação dele existe.** FATO medido (seção 6.5): no estado de B4, com as linhas [B5] presentes, só o bloco de `flush()` reprovou; os blocos de `draw_layer.hpp` e do topo de `renderer_2d.hpp` passaram, porque cada um tem também uma citação válida de teste de B2. A separação B4/B5 só é garantida pela comparação do texto publicado com este parecer. Recomendação: linha na onda `INFRA-CI`, "cada citação do bloco tem de existir".

**B0-I8. A mensagem do driver não cabe no erro.** FATO e resolução na linha R7 da tabela. D-API-10.

**B0-I9. De quem é o contexto corrente.** FATO: `gltfx_gl_context::make_current()` existe e é por linha de execução (`context.hpp`); o rascunho não dizia se o desenhador confia no contexto corrente. Com duas janelas, confiar desenha na janela errada. Resolução congelada: `begin_frame()`, `flush()` e `finish_frame()` tornam corrente o contexto DO desenhador antes de tocar em GL; falha disso sai em `finish_frame()`. D-API-07.

**B0-I10. Passagem de argumento incoerente com a família.** FATO: as funções públicas existentes recebem tipo de valor POR VALOR (`gltfx_mat3_from_transform(gltfx_transform t)`, `mat3.hpp`; `gltfx_rgba_premultiplied(gltfx_rgba color)`, `color.hpp`; `set_option(gltfx_gfx_option_entry entry)`, `context.hpp`) e descritor por `const &` (`open(gltfx_window &, const gltfx_gl_context_desc &)`). O rascunho usava `const &` para `gltfx_rect_world`, `gltfx_quad_world`, `gltfx_rgba`, `gltfx_mat3`. Resolução congelada: valor por valor, descritor por `const &`, uma regra para a família toda.

### COSMÉTICO

**B0-K1. `m_impl` da família contra a L-21.** FATO: a L-21 diz "sem prefixo `m_`"; a árvore tem 128 usos de `m_impl` (`grep -rhoE '\bm_[a-z_]+' include src`). O texto congelado segue a LEI (`impl`, parâmetro `interior` para não sombrear, `-Wshadow` limpo). A divergência da família é do líder decidir (L-67: canon que contradiz a árvore sobe a ele); nome de membro privado não é contrato, então nada aqui congela.

**B0-K2. `clear` evitado de propósito.** FATO: `/usr/include/curses.h:1307` define `clear` (o portão o dá como neutralizado, define e `#undef` no mesmo arquivo). O campo se chama `clear_color` e usa `std::optional`, que diz "limpar ou não" e "com que cor" num nome só.

**B0-K3. Primeiro `std::optional` como campo de tipo público.** FATO: `grep -rn 'std::optional' include/` só acha menções em comentário de `err.hpp`. Não aloca e é trivialmente copiável para `gltfx_rgba`; registro, não defeito.

**B0-K4. Os nomes de teste citados nas linhas de prova seguem o PLANO (B2a a B2d): `quad_vertices_test`, `draw_order_test`, `triangle_batch_test`, `frame_report_tally_test`.** O adendo usava `draw_run_merge_test`. Se o implementador nomear diferente, a linha de prova muda junto, no mesmo commit, e o fechamento vê a diferença.

---

## 6. Estreia: o portão de colisão, e a separação B4/B5 por máquina

As mesmas rodadas do parecer P0 (é um fixture só): contidas, `TMPDIR=/var/tmp`, depois de `pgrep` vazio, nada de janela, GPU ou entrada.

**6.1 Controle positivo:** `major` plantado, rc=1 (`sys/sysmacros.h:60`), na segunda tentativa (a primeira, com o `enum` numa linha só, passou por defeito da minha sonda: o extrator lê enumeradores nas linhas seguintes à abertura).

**6.2 Sonda de parâmetros e de R2D-SHAPES** (fora do texto congelado): `context desc world_to_pixel rect color corners layer reason preset row_index interior other index clear fill_triangles stroke_line stroke_rect fill_ellipse vertices indices thickness center radii from to gltfx_vertex_2d_world position segments stroke fill draw batch frame report pixel quad triangle`. Resultado: rc=1 só por `index` (`X11/Xos.h:67`, não usado no texto de B0); `clear` neutralizado; **todos os nomes de R2D-SHAPES limpos**.

**6.3 Texto congelado:** rc=0, `366 nome(s) publico(s) verificados contra 15367 arquivo(s) de sistema (6 diretorio(s)), 0 colisao real (frontend: gcc)`. 55 nomes novos em relação ao blob (P0 e B0 juntos). Os de B0: `gltfx_renderer_2d`, `gltfx_renderer_2d_desc`, `gltfx_frame_2d_desc`, `gltfx_frame_2d_report`, `gltfx_quad_world`, `gltfx_draw_layer`, `gltfx_draw_2d_refusal`, `gltfx_draw_2d_refusal_name`, `renderer_2d_impl`, `renderer_2d_internal_access`, `begin_frame`, `begin_batch`, `fill_rect`, `fill_quad`, `flush`, `finish_frame`, `last_frame_report`, `impl`, `layer`, `reserve_pieces`, `clear_color`, `top_left`, `top_right`, `bottom_right`, `bottom_left`, `pieces_submitted`, `pieces_drawn`, `pieces_refused`, `first_refusal`, `pieces_dropped_out_of_memory`, `pieces_dropped_outside_frame`, `frames_abandoned`, `batches`, `flushes`, `draw_calls`, `not_a_number`, `infinite`, `negative_size`.

**6.4 Compilação:** TU que usa todo nome novo, depois de `sys/sysmacros.h`/`sys/types.h`, `-std=c++23 -fsyntax-only -Wall -Wextra -Wshadow -Wpedantic -Werror`, mais `static_assert` de trivialidade de `gltfx_quad_world` e `gltfx_frame_2d_report` e de agregado de `gltfx_frame_2d_desc`: **g++ rc=0, clang++ rc=0**.

**6.5 Portão de citação, por estado de commit** (cópias do repositório; os testes de B2 e o de B5 simulados por `add_test` na cópia):

| Estado | rc | Leitura |
|---|---|---|
| hoje, texto inteiro | 1 | 5 blocos sem citação válida: todos os testes citados ainda não existem |
| B4 (testes de B2 registrados), sem as linhas [B5] | **0** | 0 sem citação |
| B4 com as linhas [B5] (controle) | 1 | **só** o bloco de `flush()` reprova; os outros dois blocos mistos passam: é B0-I7 |
| B5, texto inteiro, `draw2d_parity_test` registrado | **0** | |

**NÃO executado:** metade Windows do portão de colisão; `header_hygiene_test` e `visibility_test`/`exports_win_test` reais (são de B4, na árvore); nenhum teste de produto (não existem).

---

## 7. Ordem de entrada das linhas (D-W7D-R2)

**[B4]** todo o texto da seção 2, MENOS os quatro blocos abaixo. **[B5]** estes quatro, inseridos byte a byte no commit de B5, junto com o registro de `draw2d_parity_test`:

1. `draw2d/draw_layer.hpp`:
```
// Proved by: draw2d_parity_test (the same eight cells read back as
// pixels from a real surface, on both systems).
```
2. `draw2d/renderer_2d.hpp`, bloco PIXELS:
```
// Proved by: draw2d_parity_test (two rectangles sharing an edge, read
// back pixel by pixel, on both systems).
```
3. `draw2d/renderer_2d.hpp`, bloco COLOR:
```
// Proved by: draw2d_parity_test (a half-transparent edge, and the same
// piece with srgb_framebuffer on and off, read back as pixels).
```
4. `draw2d/renderer_2d.hpp`, dentro do comentário de `flush()`:
```
    // Proved by: draw2d_parity_test (hostile state planted before the
    // frame - depth test on, blending swapped, a one-pixel scissor,
    // front faces culled, color mask off - and the pixels still right;
    // then every state above read back after flush()).
```
As linhas [B4] que citam testes de B2 (`draw_order_test`, `quad_vertices_test`, `triangle_batch_test`, `frame_report_tally_test`) exigem que B2a a B2d estejam commitados antes de B4, o que a ordem do PLANO §5 já garante.

---

## 8. Decisões no formato da L-34 (confirmar retroativamente; no modo autônomo quem decide divergência é o CTO)

Cada uma segue a ordem da emenda de 29/09: o que a comunidade mais quer, como as bibliotecas semelhantes fazem, e só então a escolha pelo mais completo, com as leis do projeto vencendo.

**D-API-01. O argumento de `begin_batch` é `gltfx_transform` (double), não `gltfx_mat3` (float).**
- Pergunta que iria ao líder: "A transformação única do lote chega à biblioteca em que tipo, dado que o mundo é double e a matriz da placa é float?"
- Opções: (a) `gltfx_mat3` (o rascunho); (b) `gltfx_transform`, escala, rotação, translação, em double; (c) tipo novo de matriz afim em double, geral (com cisalhamento); (d) valor em forma de câmera (alvo, deslocamento na tela, rotação, zoom).
- Fontes: `core/mat3.hpp` e `core/transform.hpp` (o próprio projeto já separou os dois e disse onde o estreitamento acontece); a decisão 1 do líder de 19/09 (posição de mundo, UMA transformação por lote, nenhuma câmera com estado); raylib, `Camera2D { Vector2 offset; Vector2 target; float rotation; float zoom; }` e `BeginMode2D(Camera2D)` (a forma de câmera mais conhecida da comunidade, e em float, que é exatamente o defeito); Cairo, `cairo_matrix_t` com seis `double` (a biblioteca 2D madura que escolheu double na transformação); MonoGame, `SpriteBatch.Begin(..., Matrix? transformMatrix)` (matriz geral, float).
- Escolha: **(b)**.
  - (a) quebra a decisão de precisão (B0-C1): proibida pela lei, qualquer que seja a comunidade.
  - (d) é a mais pedida, mas é "câmera" com nome e semântica próprios, perto demais do que o líder recusou ("nenhuma câmera com estado"), e a forma do raylib ainda é float. O cabeçalho entrega a RECEITA de câmera em uma linha, para o consumidor não errar.
  - (c) é a mais completa, mas é tipo novo de núcleo: escopo novo antes da demo (L-32, extensão de 27/08), e `transform.hpp` já diz que a composição mora "in whichever later slice needs it, in a type that can actually hold the result". Quando esse tipo existir (R2D-TRANSFORM, W10), `begin_batch(<matriz double>)` entra como SOBRECARGA, acréscimo compatível.
  - (b) é o único que honra a lei hoje, sem tipo novo, e deixa (c) aberto sem reabrir nada.
- Perda declarada: rotação combinada com escala NÃO uniforme em que a escala vem DEPOIS da rotação não se escreve em (b) (a ordem congelada é escala, rotação, translação); o espelhamento de y (mundo com y para cima) se escreve, com rotação de sinal trocado.
- Porta de mão única: **sim**. Custo de reverter: alto depois de B4; (c) depois é barato.

**D-API-02. O relatório do quadro que falhou continua legível: `last_frame_report()`.**
- Pergunta: "Quando o quadro falha, o consumidor perde as contagens?"
- Opções: (a) perde (só o erro); (b) `finish_frame()` devolve `gltfx_rslt<void>` e o relatório só por acessor; (c) `gltfx_rslt<relatório>` e o acessor para o caso de erro; (d) `finish_frame()` nunca falha e o relatório carrega o erro dentro.
- Fontes: R1 (envelope único; (d) seria segunda forma de erro); decisão 8 do líder, "Devolve erro; o aplicativo decide" (`ESCOPO.md`); bgfx, `getStats()` devolve as estatísticas por um caminho separado do envio, válidas até o próximo quadro.
- Escolha: **(c)**. É o único que dá o erro no envelope único E não perde as contagens; (b) obriga uma segunda chamada até no caso feliz.
- Porta de mão única: **sim**. Custo de reverter: alto depois de B4.

**D-API-03. O relatório tem campos com nome, não contadores por chave.**
- Pergunta: "Como o relatório cresce quando `R2D-TEXTURE` e `R2D-SHAPES` trouxerem contagens novas?"
- Opções: (a) `struct` com campos nomeados; (b) contadores num vetor de capacidade fixa, indexados por uma enumeração que só cresce, como a tabela de opções; (c) campos nomeados com espaço reservado.
- Fontes: bgfx `Stats` (campos nomeados: `numDraw`, `numCompute`, ...); L-28 (legibilidade é requisito: `report.draw_calls` se lê de primeira); L-19 (tipo de valor tem layout como contrato, e mudança é de ABI, anunciada pela versão); L-26 (antes da 1.0, `SOVERSION 0`, nada de estabilidade prometida).
- Escolha: **(a)**. (b) é o mais durável para ABI, mas troca legibilidade por um mecanismo que a própria lei de tipo de valor já cobre; (c) é o estilo de 1995 do Win32 e congela um tamanho arbitrário. O cabeçalho diz a regra de crescimento verdadeira (B0-I2); a revisão da 1.0 decide se (b) entra.
- Porta de mão única: **sim**. Custo de reverter: médio antes da 1.0 (quebra de ABI permitida), alto depois.

**D-API-04. Camada: tipo forte com argumento padrão.**
- Pergunta: "A chave de ordem entra como sobrecarga e como inteiro nu?"
- Opções: (a) sobrecarga com `std::int32_t` (o rascunho); (b) sobrecarga com tipo forte; (c) argumento padrão com `std::int32_t`; (d) argumento padrão com tipo forte.
- Fontes: a decisão do líder de 27/08 (chave opcional; sem chave, a submissão manda); L-28 (`gltfx_draw_layer{2}` diz o que é o 2); L-17 (superfície que dobra a cada forma nova é o monolito da fachada, "é só mais um método").
- Escolha: **(d)**. Sem chave = camada 0 é exatamente "a submissão manda", então o padrão não muda semântica; uma função exportada por forma.
- Porta de mão única: **sim** (o valor padrão é compilado no consumidor e nunca muda). Custo de reverter: alto depois de B4.

**D-API-05. O quadrilátero mora em `core/`, com cantos nomeados por papel.**
- Pergunta: "Onde mora `gltfx_quad_world`, e o que são os cantos?"
- Opções: (a) `draw2d/quad_world.hpp`, cantos numerados; (b) `core/quad.hpp`, vetor de quatro; (c) `core/quad.hpp`, cantos nomeados por papel.
- Fontes: L-19 (tipo de valor do núcleo); `core/rect.hpp` (o irmão); SDL3 `SDL_RenderTextureAffine(renderer, texture, srcrect, origin, right, down)`, em que `origin` é "a point indicating where the top-left corner of srcrect should be mapped to" (três pontos nomeados pelo canto da IMAGEM, lido em 29/09) e raylib `DrawRectanglePro` (origem e rotação), que mostram que a comunidade dá PAPEL aos cantos; `R2D-TEXTURE` (W8) precisa saber onde cai o canto da imagem.
- Escolha: **(c)**. Nome por papel sobrevive à rotação e serve à textura sem reabrir o tipo. Nenhuma recusa pela forma: quem cruza os lados recebe os dois triângulos como são.
- Porta de mão única: **sim** (caminho de `#include` e layout). Custo de reverter: alto depois de B4.

**D-API-06. O contexto tem de viver mais que o desenhador.**
- Pergunta: "O que acontece se o consumidor destruir o contexto antes do desenhador?"
- Opções: (a) pré-condição, como toda a família; (b) o desenhador guarda uma marca de vida do contexto e passa a recusar tudo, e o destrutor só libera memória.
- Fontes: `context.hpp` ("`window` MUST OUTLIVE the returned context"); `loop.hpp` P10; SDL3, `SDL_DestroyRenderer`: "This should be called before destroying the associated window" (a mesma pré-condição).
- Escolha: **(a)**. Uma regra só em toda a família é mais fácil de aprender; e (b) pode entrar depois sem quebrar ninguém, porque trocar comportamento indefinido por comportamento definido é compatível. O contrário não é.
- Porta de mão única: não. Custo de reverter: baixo (só num sentido, o de (b)).

**D-API-07. O desenhador torna corrente o PRÓPRIO contexto.** Pergunta e fontes em B0-I9. Opções: (a) exigir que o consumidor o faça (pré-condição); (b) o desenhador faz. Escolha **(b)**: com duas janelas, (a) desenha na errada em silêncio. Porta de mão única: sim (comportamento prometido). Custo de reverter: alto.

**D-API-08. O quadro é limpo por padrão.**
- Opções: (a) limpar sempre; (b) nunca limpar, o consumidor chama algo; (c) limpar por padrão, com `std::nullopt` para não limpar.
- Fontes: referência EGL de `eglSwapBuffers` ("The contents of the color buffer are undefined if the value of the `EGL_SWAP_BEHAVIOR` attribute of surface is not `EGL_BUFFER_PRESERVED`"); raylib e SDL pedem limpeza explícita (`ClearBackground`, `SDL_RenderClear`).
- Escolha: **(c)**: o padrão seguro, e a exceção é escrita de propósito. Porta de mão única: sim. Custo de reverter: alto (muda pixel observável).

**D-API-09. Os descritores dizem a regra de crescimento verdadeira, e a dica de capacidade conta peças.** Achados B0-I1 e B0-I2. Opções para a dica: `reserve_quads` (o rascunho), `reserve_vertices`/`reserve_indices` (expõe o formato interno, que D-W7D-18 manteve interno), `reserve_pieces`. Escolha: **`reserve_pieces`**. Porta de mão única: sim. Custo: alto depois de B4.

**D-API-10. A mensagem do driver vai ao registro, não ao erro.** Opções: (a) no `rejected_value` (viola R7: é frase do driver); (b) campo novo no `gltfx_err` (reabre um tipo congelado em CORE-ERROR); (c) evento `draw2d_program_rejected` com campo `driver_log` no registro (R9). Escolha: **(c)**. Porta de mão única: sim (nome de evento e de campo). Custo: baixo.

---

## 9. R2D-SHAPES: os quatro nomes vistos só para coerência, NÃO congelados

Pela D-W7D-18, `fill_triangles`, `stroke_line`, `stroke_rect` e `fill_ellipse` foram olhados junto com a v1 para a família nascer coerente. **Nada disto congela aqui.**

| Nome | Coerência com a v1 | O que a revisão dele terá de decidir (registro, não decisão) |
|---|---|---|
| `fill_triangles` | Verbo `fill_` igual; o par do `SDL_RenderGeometry` (vértices, índices opcionais: "if NULL all vertices will be rendered in sequential order") | Um tipo PÚBLICO de vértice (`gltfx_vertex_2d_world`, posição de mundo em double e cor em alfa reto) separado do formato interno de 32 bytes, que continua interno; a passagem por `std::span` (primeiro `span` público: lido só durante a chamada, R5 limpo) |
| `stroke_line` | `stroke_` é o vocabulário de Cairo e dos padrões de interface para contorno, e o de raylib é `DrawLineEx(..., float thick, ...)` | A espessura é em unidade de mundo (escala com o zoom) ou em pixel? É a pergunta mais importante da forma, e não tem resposta óbvia |
| `stroke_rect` | Igual a `fill_rect`, mesmo argumento `gltfx_rect_world` | Contorno por dentro, por fora ou centrado na borda |
| `fill_ellipse` | Segmentos derivados do raio NA TELA (D-W7D-18): possível, porque a transformação do lote é conhecida antes | O raio é um par (elipse) ou um escalar com escala do lote (círculo) |

Todos levam `gltfx_draw_layer layer = {}` como último parâmetro, pela D-API-04, e ficam em uma função exportada cada. Os nomes e os parâmetros prováveis foram varridos pela sonda da seção 6.2: nenhuma colisão.

---

## 10. FATO e INFERÊNCIA, separados

- **FATO:** tudo o que traz `arquivo:linha`, saída de comando (seções 6.1 a 6.5) ou citação verbatim de documentação com URL lida nesta sessão.
- **INF:** a reconstrução das "seis perguntas" (seção 4); que (d) de D-API-01 fica "perto demais" do que o líder recusou (leitura minha da decisão 1); que a revisão da 1.0 é o lugar certo para rever D-API-03; que a espessura de linha é "a pergunta mais importante" de `stroke_line`.
- **Nenhuma decisão aqui usa o que um consumidor específico precisa ou deixa de precisar** (L-21 global, frase-guarda). O GusWorld não aparece em nenhum argumento.
<!-- APENDICE-INICIO: saida de tools/auditoria_texto_congelado.py, nunca editar a mao -->

## Apêndice: texto final congelado

Gerado a partir das cercas da seção 2, com as emendas aplicadas (E3, E4 (open), E4 (finish_frame), D-B4-1 (regra dos destinos), D-B4-1 (campo)). Cada hunk de diff aparece como o texto final (contexto e linhas acrescentadas, sem as removidas).

### `include/glintfx/core/quad.hpp` (final)

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/core/vec2.hpp>

// core/quad.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md, B0): four
// corners in WORLD space, in double precision, with no rule that the
// sides be parallel to anything. It is the general shape a rectangle
// becomes once it is turned, slanted or tapered, and it lives here, in
// the core layer next to gltfx_rect_world, because it is pure geometry:
// it knows nothing about drawing, GL or the operating system (GODS_
// LAWS.md L-19, "value type of the core").
//
// THE CORNERS ARE NAMED BY ROLE, NOT BY POSITION ON SCREEN. `top_left`
// is the corner where the top-left of an image lands when an image is
// drawn onto this quad (R2D-TEXTURE); after a rotation of half a turn
// it sits at the bottom right of the screen and is still `top_left`.
// Walking top_left -> top_right -> bottom_right -> bottom_left goes
// once around the quad. Either direction of travel is accepted.
//
// Trivial aggregate; the layout IS the contract (GODS_LAWS.md L-19,
// L-26). Nothing here is validated: a quad whose corners are not finite
// numbers is a value like any other, and whoever consumes it decides
// what to do with it.

namespace glintfx {

struct gltfx_quad_world {
    gltfx_vec2_world top_left;
    gltfx_vec2_world top_right;
    gltfx_vec2_world bottom_right;
    gltfx_vec2_world bottom_left;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/draw_layer.hpp` (final)

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>

// draw2d/draw_layer.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): the OPTIONAL order key of one drawn piece (the leader's decision
// of 27/08/2026, TODO.md line R2D-BATCH).
//
// THE ORDER, frozen: inside one frame, between two flush() barriers,
// pieces are painted by (layer, order of submission). A lower layer is
// painted first, so a higher layer lands on top. Two pieces on the SAME
// layer keep the order you submitted them in, every frame, on every
// system. A piece drawn without a layer is on layer 0, so a program
// that never names a layer gets plain submission order.
// Proved by: draw_order_test (the closed set of eight cells {no layer,
// equal, lower, higher} x {submitted A then B, B then A}, and a
// thousand pieces on one layer kept in submission order).
// Proved by: draw2d_parity_test (the same eight cells read back as
// pixels from a real surface, on both systems).
//
// WHY A TYPE AND NOT A BARE INTEGER: `gltfx_draw_layer{3}` at a call
// site says what the 3 is. A bare 3 does not, and a bare integer
// converts silently from a size, a count or a coordinate.

namespace glintfx {

struct gltfx_draw_layer {
    std::int32_t value = 0;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/renderer_2d_desc.hpp` (final)

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

// draw2d/renderer_2d_desc.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what gltfx_renderer_2d::open() needs besides the context.
//
// LAYOUT IS THE CONTRACT (GODS_LAWS.md L-19): adding a field is an ABI
// change. Before 1.0 (SOVERSION 0, GODS_LAWS.md L-26) that is allowed
// and is announced by the version; from 1.0 on, a new knob arrives as a
// new descriptor type with its own open() overload, never as a field
// appended here.

namespace glintfx {

struct gltfx_renderer_2d_desc {
    // A HINT, never a limit and never a promise: roughly how many pieces
    // (one call of fill_rect() or fill_quad() is one piece) you expect
    // to submit in one frame. The renderer reserves room for that many
    // at open(), so the first frames do not grow storage while you draw.
    // Zero lets the renderer choose. Submitting more than this is always
    // allowed: storage grows, and if growing fails the pieces that did
    // not fit are counted in the frame report (never a crash).
    std::size_t reserve_pieces = 0;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/frame_2d_desc.hpp` (final)

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <optional>

#include <glintfx/core/color.hpp>

// draw2d/frame_2d_desc.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what gltfx_renderer_2d::begin_frame() needs.
//
// LAYOUT IS THE CONTRACT: the same growth rule as
// gltfx_renderer_2d_desc (see that header).
//
// WHY THE FRAME IS CLEARED BY DEFAULT: after a buffer swap, what is left
// in the buffer you draw into is undefined unless the
// surface was created to preserve it (the EGL reference for
// eglSwapBuffers says so in those words). A frame that does not clear
// shows whatever the driver left there. Leaving it out is a choice you
// make on purpose, by writing std::nullopt.

namespace glintfx {

struct gltfx_frame_2d_desc {
    // The color the whole surface is filled with before anything is
    // drawn in this frame, or std::nullopt to keep what is there.
    // Linear light, straight alpha, like every gltfx_rgba. Default:
    // opaque black.
    std::optional<gltfx_rgba> clear_color = gltfx_rgba{0.0F, 0.0F, 0.0F, 1.0F};
};

} // namespace glintfx
````

### `include/glintfx/draw2d/frame_2d_report.hpp` (final)

````cpp
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// draw2d/frame_2d_report.hpp - R2D-BATCH (docs/auditoria-api-draw2d.md,
// B0): what happened to one frame, counted. It is the ONE place a frame
// reports anything (the leader's decision of 19/09/2026: drawing calls
// return nothing; a problem is reported once, at the end of the frame).
//
// LAYOUT IS THE CONTRACT: the same growth rule as
// gltfx_renderer_2d_desc (see that header). New counters are appended
// at the end, never inserted, never reordered.
//
// WHAT A "PIECE" IS: one call of fill_rect() or fill_quad(). Every piece
// submitted while a frame is open ends in exactly one of four places:
//
//   pieces_submitted == pieces_drawn + pieces_refused
//                       + pieces_dropped_out_of_memory
//                       + pieces_dropped_graphics_failure
//
// Proved by: frame_report_tally_test (a piece holding a non-number is
// refused with the token of the FIRST reason; an empty frame reports
// zero; running out of memory becomes a count and one error at the
// end, never a thrown exception).
// A piece submitted while NO frame is open (before begin_frame(), or
// after finish_frame()) is not part of any frame: it is dropped and
// counted in pieces_dropped_outside_frame of the NEXT report, never in
// pieces_submitted.

namespace glintfx {

// Why the FIRST refused piece of a frame was refused. The token of each
// value (gltfx_draw_2d_refusal_name()) is an identifier, never a
// sentence (docs/api-conventions.md R7). APPEND-ONLY: a value is never
// renumbered and never reused.
enum class gltfx_draw_2d_refusal : std::uint8_t {
    none = 0,
    not_a_number = 1,
    infinite = 2,
    negative_size = 3,
};

// The token for `reason`: "none", "not_a_number", "infinite",
// "negative_size". A value this build does not know (a newer glintfx
// crossing the library boundary) reads "unknown" (docs/api-
// conventions.md R4). The returned text lives as long as the program.
[[nodiscard]] GLINTFX_API std::string_view
gltfx_draw_2d_refusal_name(gltfx_draw_2d_refusal reason) noexcept;

struct gltfx_frame_2d_report {
    // Pieces submitted while this frame was open.
    std::uint64_t pieces_submitted = 0;
    // Pieces that reached the graphics card. A piece whose area is zero
    // is drawn: it simply covers no pixel.
    std::uint64_t pieces_drawn = 0;
    // Pieces refused because a value was not a finite number, or a
    // rectangle had a negative width or height. The frame goes on
    // without them; this is not an error of the frame.
    std::uint64_t pieces_refused = 0;
    // Why the first of those was refused; `none` when pieces_refused is
    // zero.
    gltfx_draw_2d_refusal first_refusal = gltfx_draw_2d_refusal::none;
    // Pieces dropped because the renderer could not get memory of its own
    // to store them. When this is not zero, finish_frame() returns an
    // error.
    std::uint64_t pieces_dropped_out_of_memory = 0;
    // Pieces accepted while the frame was open that never reached the
    // screen because the graphics side failed: the context could not be
    // made current, or the graphics card refused the frame's data (the
    // card running out of ITS memory counts here, not in
    // pieces_dropped_out_of_memory). When this is not zero,
    // finish_frame() returns an error, and the error says why.
    std::uint64_t pieces_dropped_graphics_failure = 0;
    // Pieces submitted with no frame open, since the previous report.
    std::uint64_t pieces_dropped_outside_frame = 0;
    // Frames that were begun and never finished (begin_frame() called
    // while a frame was open throws the open one away, undrawn), since
    // the previous report. The pieces of an abandoned frame are not
    // counted in any other field of any report.
    std::uint64_t frames_abandoned = 0;
    // How many times the transform changed (begin_batch()), counting
    // the pixel-direct batch every frame starts with.
    std::uint64_t batches = 0;
    // How many flush() barriers this frame had.
    std::uint64_t flushes = 0;
    // How many draw calls reached the graphics card. Printed for you to
    // measure, never promised as a number.
    std::uint64_t draw_calls = 0;
};

} // namespace glintfx
````

### `include/glintfx/draw2d/renderer_2d.hpp` (final)

````cpp
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
````

<!-- APENDICE-FIM -->
