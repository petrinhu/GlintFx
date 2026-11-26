# Changelog

All notable changes to glintfx are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), adapted to glintfx's own four-component version scheme (`vMAJOR.MINOR.PATCH.TWEAK`, not three-component Semantic Versioning). See the "Versioning and compatibility" section of [`README.md`](README.md) for what each component means, and [`PACKAGING.md`](PACKAGING.md) for how that maps onto the CMake package and pkg-config version.

**glintfx is pre-1.0.** There is no ABI or file-format compatibility guarantee across a `MAJOR` bump, and the public surface can still change between any two commits; see "Versioning and compatibility" in `README.md`.

## [Unreleased]

## [0.7.0.0] - 2026-11-26

This release is a `MINOR` bump while the project is pre-1.0, and `SOVERSION` stays 0: the 2D renderer now draws shapes, textures, images from an atlas and shadows, and four example programs show the public API at work. `find_package(glintfx 0.7)` finds this release; the installed CMake package refuses `find_package(glintfx 0.6)` against it (`SameMinorVersion`), so a consumer asks for `0.7`. A program built against `0.6` must be rebuilt: see "Changed".

This section covers what changed after `v0.6.1.0`.

### Added

- **Shapes in the 2D renderer** (`include/glintfx/draw2d/renderer_2d.hpp`). Besides rectangles and quadrilaterals, a frame can now hold triangles with a colour per corner (`fill_triangles()`, and `fill_indexed_triangles()` to share vertices), lines with a thickness and flat, square or rounded ends (`stroke_line()`), rectangle outlines centred on the edge, inside it or outside it (`stroke_rect()`), and filled or outlined ellipses, circles included, turned by any angle (`fill_ellipse()`, `stroke_ellipse()`). A thickness is in world units by default, so it follows the zoom of the batch transform, or in pixels, so it stays the same at every zoom. A half-transparent outline of a rectangle or of an ellipse covers each pixel once: no darker corner, no fold. The value types are in `include/glintfx/draw2d/vertex_2d.hpp`, `include/glintfx/draw2d/stroke.hpp`, `include/glintfx/core/segment.hpp` and `include/glintfx/core/ellipse.hpp`. Curves are drawn as straight segments, and the new field `curve_max_error_pixels` of `gltfx_renderer_2d_desc` (`include/glintfx/draw2d/renderer_2d_desc.hpp`) says how far from the true curve they may lie, in pixels: 0.25 by default; smaller is smoother and costs more triangles, and the number of segments of one curve has a ceiling whatever the value. `open()` refuses a value that is not a finite number greater than zero.
- **Textures** (`include/glintfx/draw2d/texture_2d.hpp`, `include/glintfx/draw2d/texture_2d_desc.hpp`, `include/glintfx/draw2d/texture_region.hpp`, `include/glintfx/core/image_view.hpp`). `create_texture()` makes a texture from pixels you already hold in memory, described by a `gltfx_image_view`: four 8-bit channels per pixel, sRGB-encoded or in linear light, with alpha straight or premultiplied, and rows that may be padded. The library reads the pixels only during the call and keeps its own copy; glintfx does not open or decode image files. Four calls draw with a texture: `draw_texture()` puts a region of it into a rectangle, mirrored if you ask and multiplied by a tint; `draw_texture_quad()` puts it into a free quadrilateral with projective interpolation, so the straight lines of the image stay straight inside a trapezoid; `fill_textured_triangles()` and `fill_indexed_textured_triangles()` draw triangles with a texel coordinate and a tint for each vertex. Texel coordinates have types of their own (`gltfx_vec2_texel`, `gltfx_rect_texel`), so a world rectangle passed where a texel rectangle is expected does not compile. You choose the `nearest` or the `linear` filter and how many reduced levels (mipmaps) a texture has, and the library computes those levels itself, in linear light. The linear filter blends neighbouring texels in linear light, and colours are kept premultiplied by alpha, so an edge between two colours does not come out darker and a transparent texel leaves no fringe. A texture made `updatable` can be changed in place with `update_texture()`, which also recomputes the reduced levels the change touches; it does not send the batch, so call `flush()` first if the pieces already submitted must keep the old content. A texture belongs to the renderer that made it and must be destroyed before that renderer; destroying it while a frame is open is safe, because the pieces already queued still draw with it.
- **A texture atlas** (`include/glintfx/draw2d/texture_atlas.hpp`, `include/glintfx/core/atlas_allocator.hpp`). `gltfx_texture_atlas` keeps many images in one texture. `add_image()` writes an image into a cell of its own and returns a receipt; `draw_atlas_image()` draws the image by that receipt, and images of one atlas drawn one after the other, with nothing between them, share one draw call; `region_of()` gives the place of the image for the other textured calls; `release_slot()` gives the cell back for a later image. The edge of every image is copied into a margin around it, so neither the linear filter nor the reduced levels make one image bleed into its neighbour, and `open()` refuses a margin too narrow for the reduced levels asked for. A receipt that is no longer current, or that belongs to another atlas, is refused by `draw_atlas_image()` and counted in the frame report, never drawn. Underneath, `gltfx_atlas_allocator` places rectangles inside a fixed area and takes the space back when they are released. It needs no graphics card, so you can also build a glyph cache or a sprite sheet of your own on it, and with the same version of the library the same calls give the same positions on every system and with every compiler. A full atlas answers with the new error code `no_free_space`, and a rectangle larger than the whole atlas with `invalid_argument`, so a loop that opens a new atlas whenever one is full never runs forever.
- **The contract for turning the images of a style sheet into textures** (`include/glintfx/draw2d/image_resolver.hpp`). glintfx never opens a file or decodes an image: when a style sheet names a picture with `url("...")`, it is your program that says which texture answers that text. You install the answer on a renderer with `set_image_resolver()`, as a `noexcept` function and a context of your own; the function receives the text exactly as the sheet wrote it, and returns a texture and a region of it, or no texture. Joining a relative address to the place of the sheet is up to you. Each renderer keeps its own resolver, and `set_image_resolver()` returns the one installed before. In this release no drawing call asks the resolver yet, so installing one changes nothing you can see.
- **Shadows and light for standing pieces** (`include/glintfx/core/light_2d.hpp`, `include/glintfx/core/projected_shadow.hpp`, `include/glintfx/core/light_tint.hpp`). `gltfx_project_directional_shadow()`, for a sun given by the direction its light comes from and its elevation, and `gltfx_project_point_shadow()`, for a lamp given by its position and its height, compute the shadow that an upright piece casts on the ground, as a quadrilateral whose corners are ordered so that the image of the piece lands on its shadow without swapping corners. They are pure functions that need no graphics card, and the same inputs give the same bits on the same binary. In the renderer, `fill_shadow()` draws such a quadrilateral as a shadow that darkens what is already drawn under it, and `draw_shadow_texture()` gives the shadow the silhouette of a texture, taken from its alpha. Between two barriers (`flush()` or `finish_frame()`), a pixel is darkened at most once, even where shadows overlap, until an opaque piece is drawn over it, and each ordinary run of pieces drawn after a shadow costs one more draw call. The colour of a shadow has no default: black with alpha `a` keeps `1 - a` of what is under it. Shadows use the stencil buffer of the surface: while the drawing between two barriers holds a shadow, that buffer is the renderer's, and on a surface with no stencil bits a shadow is refused by name. `gltfx_light_tint()` computes, for a list of points on the ground, the light that point lights (each with a linear or a smooth fall-off) and an ambient light put on them, for you to pass to the drawing as the colour or the tint of each vertex.
- **A fuller frame report** (`include/glintfx/draw2d/frame_2d_report.hpp`). Every new drawing call keeps the rule of the existing ones: it returns nothing, one call is one piece however many triangles it holds, and a refused call is refused whole and counted once, so one bad piece never stops the rest of the frame. `gltfx_draw_2d_refusal` has a new reason for each new way a call can be refused (for example `index_out_of_range`, `degenerate_quad` or `stale_region`), `gltfx_draw_2d_refusal_name()` gives the token of each, and `gltfx_frame_2d_report` has a counter for every reason; together they add up to `pieces_refused`. Two new counters show costs for you to measure, never promised as numbers: `texture_switches`, how many times the bound texture changed, and `shadow_segments`, how many times the stencil buffer was cleared for shadows. New values and counters are added at the end, never inserted or reordered.
- **Four example programs** (`examples/first_window/`, `examples/shapes/`, `examples/textures/`, `examples/sprite_shadows/`). Each one opens a window and draws one fixed scene in every frame through the public API alone, and the comment at the top of its source file is the lesson. `first_window` shows the window, its graphics context, the 2D renderer, the main loop and the draw layers; `shapes` shows every shape of one colour, the two units of thickness and the curve tolerance; `textures` shows the textured calls, both filters, reduced levels, transparency, a tint and an atlas; `sprite_shadows` shows a projected shadow, two overlapping shadows that darken once, and the silhouette of an image. They are built with the library when the new CMake option `GLINTFX_BUILD_EXAMPLES` is on. It is on by default only when glintfx is the top-level project, and off when you embed glintfx with `add_subdirectory` or `FetchContent`; the examples are never installed. Each program runs until its window is closed, or stops by itself after `N` drawn frames with `--frames N`. The scenes are placed in physical pixels and assume a display scale of 1.

### Changed

- **A program built against `0.6` must be rebuilt.** Two public structs gained fields at their end, so their size changed: `gltfx_renderer_2d_desc` (`include/glintfx/draw2d/renderer_2d_desc.hpp`) and `gltfx_frame_2d_report` (`include/glintfx/draw2d/frame_2d_report.hpp`). Before 1.0 the ABI can change in any release, which is what `SOVERSION` 0 means (see "Versioning and compatibility" in `README.md`). No public declaration was removed or renamed.
- **`fill_rect()` and `fill_quad()` refuse a piece that the batch transform takes out of range** (`include/glintfx/draw2d/frame_2d_report.hpp`). When a corner would land on a pixel coordinate larger in magnitude than the drawing program can hold (about 1.7e38), or when the batch transform itself holds a value that is not a finite number, the piece is refused and counted as `pixel_position_out_of_range`, as for every new drawing call. A piece that is only outside the surface is still drawn, and covers no pixel.
- **`gltfx_err_code` has a new value, `no_free_space`** (`include/glintfx/core/err_code.hpp`): a store of fixed size has no free place of the size asked for now, and releasing what it holds can make room. It is not a lack of memory, which stays `out_of_memory`. The texture atlas is the first to return it.
- **After `flush()` and `finish_frame()`, the renderer also leaves the stencil state that OpenGL starts with** (`include/glintfx/draw2d/renderer_2d.hpp`): the function `ALWAYS` with reference 0, the operation `KEEP`, and every bit writable, on both faces. If your own GL code uses the stencil buffer, write what you need again after a barrier that closed a drawing with shadows: that drawing cleared the buffer and left its marks there.
- **A frame presented with nothing drawn in it now has a written contract** (`include/glintfx/platform/gl/context.hpp`, item 2 of its top comment, and `include/glintfx/platform/loop/loop.hpp`). Its content is undefined, and the system may refuse to present it: some drivers do and some do not, so the same program can behave differently on two machines. A refusal is an ordinary `platform_failure` error, with `rejected_value()` naming the call that refused, and it is never reported as `presented`; the context stays usable, and `gltfx_loop::run()` ends with that error as it came. glintfx never draws on your behalf to avoid the refusal: draw in every frame you present, and a clear is enough.
- **`include/glintfx/platform/asset/file.hpp` and the headers under `include/glintfx/gfss/` are now marked experimental.** No function, type or layout in them changed. They are now documented as outside the compatibility contracts of the rest of the public API, so a later release may change or remove them without a `MAJOR` bump. One promise stays: a numeric property identity already published in `include/glintfx/gfss/property.hpp` keeps its meaning. See "Status" in `README.md`.

### Fixed

- **On Windows, the window now appears on the screen** (`include/glintfx/platform/gl/context.hpp`). In `0.6.1.0`, nothing in the library showed it: it stayed invisible, while `swap_buffers()` still answered `presented`. Now the first frame that is presented, and not skipped, shows the window once, as on Linux (Wayland), even when the program was started with a request to hide its window, and a window that was never shown is not taken for a hidden one. Whether it also gets the keyboard focus is the system's decision.
- **On Linux (Wayland), the window now acknowledges every request of the compositor**, such as a new size, fullscreen, or becoming the active window, in the frame that answers it. Before, only the first request was acknowledged: a compositor may then not treat the window as maximized or fullscreen, and a strict one may end the connection. A request that changes the size is acknowledged with the first frame drawn at the new size. No public function changed.
- **On Linux (Wayland), a failed request for the compositor's "ready for the next frame" signal no longer holds up the next frame.** When that request could not be made, because memory ran out inside `libwayland-client`, the next present waited for a signal that was never asked for until its time ran out, and came back as `skipped_hidden`. It now waits only for a signal that was requested, and the library sends the warning `frame_callback_request_failed` to the log sink, once for each run of such failures.

## [0.6.1.0] - 2026-10-10

This release is a `PATCH` bump: one fix, no new feature, no change to the API or the ABI. `find_package(glintfx 0.6)` keeps finding it, and `SOVERSION` stays 0.

### Fixed

- **On Linux (Wayland), the window no longer freezes on its first frame (`WL-FRAME-ORDER`).** Since `0.4.0.0`, a program that presents through a `gltfx_gl_context`, whether it calls `present()` itself or lets the main loop do it, showed its first frame and then never showed another one: every later present came back as `skipped_hidden` without reaching the screen, and with vsync on each of those presents spent the full 100 ms wait first. The library asked the compositor for its "ready for the next frame" signal only after that frame had already been handed over, so the signal never came. The request now travels with the frame it belongs to, and the wait ends as soon as that signal arrives, within the same 100 ms budget. Windows was not affected. If you use `0.6.0.0`, this release changes nothing else; if you pinned `0.4.x` or `0.5.x`, read the `[0.6.0.0]` section below before moving, because `find_package(glintfx 0.5)` no longer finds `0.6`.

## [0.6.0.0] - 2026-10-06

This release is a `MINOR` bump while the project is pre-1.0 (`SOVERSION` stays 0): the 2D drawing surface is new, and the installed CMake package now refuses `find_package(glintfx 0.5)` against it (`SameMinorVersion`), so a consumer asks for `0.6`.

This section covers what changed after `v0.5.2.0`. The sections for `0.4.1.0` through `0.5.2.0` below were written afterwards, on 2026-10-11; those releases shipped without an entry.

### Added

- **2D drawing: filled rectangles and quadrilaterals, drawn into a `gltfx_gl_context` (`R2D-BATCH`).** `include/glintfx/draw2d/renderer_2d.hpp` is the new public surface (five headers under `include/glintfx/draw2d/`). A frame is `begin_frame()`, any number of `fill_rect()` and `fill_quad()` calls, then `finish_frame()`. Pieces are gathered into as few draw calls as the order you asked for allows, and a draw layer lets you say which piece paints over which.
- **Positions are world positions with one transform per batch.** With no transform a world position is a physical pixel position (origin at the top left, y grows down); with `begin_batch(transform)` the transform applies to every piece until the next batch. Two rectangles that share an edge cover each pixel along it exactly once. The transform is applied in double precision, so a camera far from the origin does not make pieces shimmer.
- **A problem is reported once, when the frame ends.** The drawing calls return nothing; `finish_frame()` hands back one report of what was refused or failed, so a single bad piece never stops the rest of the frame.
- **A suggested graphics preset, handed over and never applied behind your back (`GFX-PRESET`).** Reading the new `suggested_preset` option tells you which of power saving, balanced or performance fits this machine right now; the new `auto_choice_reason` option says why (on battery, software renderer, shared GPU, dedicated GPU, unknown GPU), and the new `power_source` option reports mains, battery or unknown. Reading never changes anything. Setting `preset` applies every row of that preset at once, or none of them, and `automatic` is a shortcut for "apply the suggestion of this moment". `gltfx_gfx_preset_row_count()` and `gltfx_gfx_preset_row_at()` let you read the rows of a preset and decide on your own side what to apply. Named constants (`k_gltfx_preset_*`, `k_gltfx_vsync_*`, `k_gltfx_power_source_*`, `k_gltfx_auto_choice_reason_*`) mean no consumer has to write a bare number, and those numbers are part of the data contract: a number is never reused for another meaning.

### Fixed

- **The sRGB framebuffer now works on Linux (Wayland/EGL).** Asking for an sRGB surface had never taken effect there, because the request was made at a place the driver ignores it. It is now made on the surface itself, and read back live.
- **On Windows, an sRGB request is decided by what the system actually reports (`D-SRGB-2`).** The window's pixel format selection now reads the system's own extension list and refuses by name when sRGB is not available, instead of announcing a support that was not applied. A request the system cannot honor now says so.
- **`extension_token` tells upper case from lower case**, as the extension lists of the graphics drivers do; before, a differently cased token could be taken for another one.

### Changed

- **The `index` parameter of `gltfx_gfx_option_at()` and of `gltfx_gpu_list::at()` is renamed** (`index_in_table`, `enumeration_position`). Positional calls compile and behave as before; only the documentation reads differently.

### Infrastructure

- **A test with two windows in one process** proves that drawing goes to the context you meant even when another context was current just before each entry point.
- **The check that compares the published headers against their frozen text no longer fails on Windows**, where the checkout has CRLF line endings: line endings are normalized before comparing.
- **A registry-order check at compile time** rejects a removed or out-of-order option row, and the 2D drawing code was split so no function is above the project's size limit.
- **The local mirror of the CI (`tools/preci.sh`)** times every stage, compares each test against the last green run, and turns on `ccache` by itself with its state always printed. The CI itself is split per operating system.
- **The adversarial reviews of the sRGB work, of the 2D drawing and of the preset** (each executing mutants against the tests) are versioned under `docs/`.

## [0.5.2.0] - 2026-09-30

*Written on 2026-10-11, after the release, from the tag's own message and the commits it marks.*

This release is a `PATCH` bump: one fix you can observe, no new feature, no change to the API or the ABI.

### Fixed

- **On Linux (Wayland), a graphics context whose connection to the compositor has failed now refuses cleanly, every time (`EGL-DEAD-DISPLAY-GUARD`).** Once that connection has failed, `make_current()` and `swap_buffers()` return `platform_failure`, and keep returning the same error on every later call, without reaching the graphics driver. `os_error_code()` carries the system's error number. When the compositor raised a protocol error, `rejected_value()` names the protocol interface it rejected, such as `wl_surface`; otherwise it is empty. Destroying the context still works normally afterwards. Windows has no such connection and is not affected.

### Infrastructure

- The project's own test tooling now runs commands under a ceiling on how many processes they may create (`tools/contido.sh`), so a runaway test can no longer exhaust the machine.
- A comment in a public header that claims a behaviour is the same on both systems, or was measured, must now name the test that shows it; a gate rejects a claim without one.

## [0.5.1.0] - 2026-09-24

*Written on 2026-10-11, after the release, from the tag's own message and the commits it marks.*

This release is a `PATCH` bump: one fix, no new feature, no change to the API or the ABI.

### Fixed

- **On Linux (Wayland), the wait for the compositor now tells a closed connection apart from "nothing to read yet", so a compositor that goes away is reported as a connection failure when reading, not only when writing.** Before, the wait could take a closed connection for "nothing to read yet". The wait now checks the connection itself, not only that the process exists. Messages the compositor sent before closing are still delivered first, so the failure appears on a later call. This is proven by a unit test of that decision; a real compositor closing the connection does not reach that branch in the live test, so no end-to-end run shows it yet.

### Infrastructure

- When glintfx is cross-compiled (for example, built for Windows from Linux), the build-time OpenGL code generator is now built for the machine running the build, not for the target.
- The gate that keeps the library's layers apart now also covers the style-sheet code.
- Before a container test starts, the CI now waits until the compositor inside the container is ready, not only until its process exists.

## [0.5.0.0] - 2026-09-22

*Written on 2026-10-11, after the release, from the tag's own message and the commits it marks.*

This release is a `MINOR` bump while the project is pre-1.0 (`SOVERSION` stays 0). The installed CMake package uses `SameMinorVersion`, so `find_package(glintfx 0.4)` does not find this release: ask for `0.5`. No public header changed.

### Added

- **A complete reader for gfss style sheets, inside the library (`GFSS-SHEET-PARSE`).** It reads a whole sheet, recovers rule by rule from a broken rule instead of giving up on the sheet, and dispatches at-rules. It is not reachable through the public API yet: no public function returns a parsed sheet.
- A rule that uses the `:placeholder-shown` selector is now refused when a sheet is read, instead of being accepted with no way to ever match.

### Fixed

- **On Linux (Wayland), a slow write to the compositor no longer closes the connection (`WL-WRITE-TIMEOUT-NAO-FATAL`).** If the compositor was too busy to accept data within the time the library chose to wait, the library treated it as dead and closed the connection, which ended the window. It now keeps waiting, as the libwayland documentation asks and as GLFW and SDL do. Only a broken socket is still treated as fatal.

## [0.4.2.1] - 2026-09-19

*Written on 2026-10-11, after the release, from the tag's own message and the commits it marks.*

This release is a `TWEAK` bump: no product code changed, and the library you build is the same, byte for byte, as `0.4.2.0`.

### Infrastructure

- The PowerShell syntax check of the Windows scripts now runs on the CI server; before, it existed but was never executed there.
- A gate now watches the project's audit manual for stale statements, and another checks the README's Windows build instructions against the CI file they describe.

## [0.4.2.0] - 2026-09-19

*Written on 2026-10-11, after the release, from the tag's own message and the commits it marks.*

This release is a `PATCH` bump: a fix in how errors are copied, no new feature.

### Fixed

- **When memory runs out, glintfx returns an error instead of ending your program (`ERR-COPY-FIX`).** Copying a `gltfx_err` that carried diagnostic details, such as a path or a rejected value, allocated memory. Inside functions that promise never to throw, a failed allocation ended the consumer's process, on every platform and in Release builds too. The diagnostic details are now shared between copies instead of duplicated, so copying an error no longer allocates, and copy and assignment are now `noexcept`. The size of `gltfx_err` stays two pointers, and on Windows no exported symbol was renamed (checked there with `dumpbin`).

### Infrastructure

- A gate now reads, from the header itself, whether copying `gltfx_err` is `noexcept`, and rejects code that allocates inside a `noexcept` function without protection.

## [0.4.1.0] - 2026-09-18

*Written on 2026-10-11, after the release, from the tag's own message and the commits it marks.*

This release is a `PATCH` bump: behaviour fixes, no change to any public type, signature or layout.

### Fixed

- **Several public functions no longer end your program when memory runs out.** Among them: `gltfx_gl_context::open()`, `gltfx_gpu_enumeration::query()` on Windows, looking up an OpenGL function by name, and finding the graphics card's device on Linux. This affected every platform.

### Infrastructure

- The style engine's selector matching (specificity, all the combinators, and the shorthand properties of the first version) was completed inside the library. None of it is reachable through the public API yet.
- A gate that finds memory allocation inside `noexcept` functions without protection now runs in CI.

## [0.4.0.0] - 2026-09-15

### Changed (Breaking)

- **The main loop's callbacks (`gltfx_loop_callbacks::on_frame`/`on_render`/`on_event`) are now a plain `noexcept` function pointer plus an opaque `void *context`, never `std::function` (`LOOP-CALLBACK-THROW`).** Code that passed a lambda with captured state, or a function not declared `noexcept`, stops compiling at the exact line that assigns it - it never compiled correctly before either: `std::function` erased the `noexcept` guarantee, so a throwing callback had nothing in the type system stopping it, and the resulting exception crossing `gltfx_loop::run()`'s own `noexcept` boundary ended the consumer's process with no diagnostic from this library at all. See `docs/api-conventions.md` R10 for the full four-layer contract this closes, including what none of the four layers can do for a consumer.
- **This confesses something the project owes its consumers, not a new change:** the type change above already shipped, silently, inside `v0.3.8.0` (13/09/2026) - the commit that made it (`51af0d3`, `LOOP-CALLBACK-THROW` S1a) is an ancestor of that tag, and that tag's own `[0.3.8.0]` section below does not mention it (measured: zero occurrences of `callback`/`noexcept`/`breaking` in that section). A consumer who updated to `v0.3.8.0` and found their build broken was given no warning why. `v0.3.8.0` itself is not rewritten - a published tag never is (`GODS_LAWS.md` L-25) - this entry is the announcement that should have shipped with it, made here instead, on the first release afterward.

### Added

- **`gltfx_loop::set_callbacks()` plus a no-argument `run()` (`LOOP-CONTEXT-OWNERSHIP`):** a second way to hand the loop its callbacks, alongside the existing `run(callbacks)`, so the SAME callbacks can survive across repeated `run()` calls instead of being destroyed at the end of every one.
- **Optional ownership of the callbacks' own context (`LOOP-CONTEXT-OWNERSHIP`):** `destroy_context`, a field of `gltfx_loop_callbacks` that was already frozen in the ABI, is now honored - nullptr keeps the context **borrowed** (the consumer owns it, as before), a non-null function means the context is **handed over** and this library destroys it, through that function, exactly once, on every path out (including error and refusal).
- **`include/glintfx/platform/loop/loop_bind.hpp` (`LOOP-CALLBACK-BIND`):** `gltfx_bind_loop_callbacks<&Type::on_frame, &Type::on_render>(object)` (borrow) and `gltfx_adopt_loop_callbacks<&Type::on_frame, &Type::on_render>(heap_object)` (hand ownership over) - header-only, zero-cost helpers so a consumer with an object-and-method pair never has to hand-write the two thunks themselves. Both refuse, at compile time, a method that is not `noexcept`, or a temporary object.
- **`include/glintfx/platform/loop/loop_context_mark.hpp` (`LOOP-CONTEXT-MARK`):** an opt-in base class a consumer's own callback-context type can inherit from to get a liveness mark, checked by `loop_bind.hpp`'s own thunks in a build without `NDEBUG` (Debug), that stops the process deterministically, with a message naming the mistake, instead of calling through an object whose destructor already ran. Zero cost in Release (`-DNDEBUG`, this project's own default), proven by size.

### Fixed

- **A consumer that VETOES the system's close request no longer loses the frame-rate cap and the hidden-window sleep, forever (`LOOP-CLOSE-LATCH-SPIN`).** Once a close was requested, every subsequent `step()` spun without ever waiting again - burning battery and CPU on the consumer's own machine for as long as the process ran - because the request is a one-way latch (`window_state.hpp`), and the loop used to gate both waits on it. The read that caused this is gone; ending the loop on the non-veto path was already handled elsewhere.
- The first tick a loop delivers now reports `elapsed == 0`, as the header always promised; before, it reported the wall time between `gltfx_loop::open()` and the first `step()` - the consumer's own loading time - clamped to 250 ms, which reached the consumer's first physics integration as one large step. Found live on 13/09/2026 by `loop_parity_test` (`LOOP-FIRST-TICK-ELAPSED`).
- **`frame_rate_cap` now actually holds past the first tick, on both platforms.** `frame_cap_schedule::plan()` could round a sub-millisecond remainder down to 0 - the same value it uses to mean "deadline reached" - so a consumer's cap held for one tick and then the loop ran free for the rest of the session. Found live on Windows on 15/09/2026, the first time this project ever measured `frame_rate_cap` against a real, visibly-drawing window; fixed at the one place the ambiguity lived, and proven fixed live on that same system (`gh run view 34975391524`).
- **A minimized window's reported CPU cost on Windows was the instrument, not the loop.** `loop_hidden_test` had measured it with `std::clock()`, which the Microsoft C runtime documents as wall-clock time on that compiler, not processor time; a hidden window was already sleeping correctly. Replaced with `GetProcessTimes()`, gated behind a calibration cell so a future blind instrument cannot pass silently again.

### Infrastructure

- **`loop_engine_test`:** the main loop's own engine (`loop_step()`/`loop_present()`/`loop_run()`, `src/platform/loop/loop_engine.hpp`) gets its first callers in this repository, exercised directly against test-only doubles, needing no operating system at all.
- **`tests/parity/loop_parity_test.cpp` and `tests/loop_hidden_test.cpp` (`LOOP-RUN` fatia 8):** the first tests that call `gltfx_loop::open()`/`step()`/`present()`/`run()`/`set_callbacks()` through the public API alone, against a real display, window and graphics context - run against a real compositor inside this project's isolated Linux container, and executed by a server as a Windows `ctest` target for the first time on 15/09/2026 (`gh run view 34975391524`).
- **`tests/tools/check_loop_callback_truths.py`:** a gate that a documented truth about the callback contract (`docs/api-conventions.md` R10) actually appears, verbatim, in the comment immediately above the symbol it describes - a passage rewritten for readability cannot silently drop what it promised.

## [0.3.8.0] - 2026-09-13

### Added

- A memory-allocation leak counter for the eighteen test fixtures that run inside the isolated Wayland container (`CONTAINER-LEAK-COUNTER`), closing a gap where the container's own memory-safety instrumentation (AddressSanitizer's leak detector) is disabled there and fails silently: a classifier that decides, purely by the calling stack frame, whether a live allocation came from this library's own code or from a third-party library (`tests/container/alloc_counter_classify.{hpp,cpp}`); a hook that replaces the global `operator new`/`operator delete` inside each fixture binary and reports the result at process exit (`tests/container/alloc_counter_hook.cpp`); and the script that applies the pass/fail criterion against that report - zero live blocks attributed to this library's own code, third-party blocks measured and published but not yet judged (`tests/container/check_alloc_report.sh`). Two new steps run in both legs (`plain`, `asan`) of the `wayland-container` CI job: a check that every fixture binary actually carries the counting symbol before any fixture runs, and the report-verdict check after all eighteen have run. The gate was proven catching a real regression on the CI server itself (a fixture rebuilt without the counting hook), not only in a local rehearsal.
- `tests/container/exec_fixture.sh`: the atom the `wayland-container` job now uses to run every one of its fixtures, printing the exit code and, when the process died by signal, the signal name, before any of the fixture's own captured output reaches the log - a fixture killed by a signal no longer hides its own verdict behind unrelated driver noise printed earlier in the same output.

### Infrastructure

- The `wayland-container` CI job gained a second leg (`asan`) that runs the same eighteen fixtures under AddressSanitizer/UndefinedBehaviorSanitizer: the only fixtures in this project that exercise a real compositor, window, and graphics context now run with memory-safety instrumentation too. Total CI jobs: 22 to 23.

### Fixed

- The library's own declared version (`project(VERSION)` in `CMakeLists.txt`, and everything generated from it: the version macro a consumer reads at compile time, and the packaging files a packager reads) now matches the latest published tag. From v0.3.1.0 through v0.3.7.0 the declaration itself had stayed at `0.3.0.0`, so a consumer building against any of those seven tags was told `0.3.0.0` by the library, never the real version it actually received. The declaration is bumped again here, from `0.3.7.0` to `0.3.8.0`, to keep matching this tag.

## [0.3.7.0] - 2026-09-10

### Changed

- **Breaking:** `gltfx_rslt<T>::error()` and `gltfx_rslt<void>::error()` are renamed to **`err()`** (decision of the project leader, 2026-09-09). A translation unit that defines `ERROR_CONTEXT_MACROS` before including `<attr/error_context.h>` (libattr, Linux) turns `error` into a function-like macro - `r.error()` was reachable by it; `err()` is a different token that macro never touches. The static factory `gltfx_rslt<T>::err(gltfx_err)` is unaffected - it is now overloaded by arity with the renamed accessor (`err(gltfx_err)` versus `err()`), the same pairing shape Rust's `Result::err()`/`Err(...)` already uses. Every `gltfx_err error` parameter that carried the same word is renamed to `failure` alongside it. See `docs/api-conventions.md` R6 for the full rationale and the rule this adds: a public method or parameter name never coincides with a system macro.

### Fixed

- **Windows raw input no longer competes with the consumer for the single process-wide registration slot.** Previously, a consuming application that registered its own raw input silently disabled this library's own device-change notification, and closing this library's window silently disabled the consumer's raw input; two windows opened by this library in the same process could also steal the registration from each other. Replaced by a per-window device-interface notification, verified on a real Windows machine.
- **A defect that could crash the consuming process, on both Windows and Linux, is fixed.** Nine internal objects that hand their own address to the operating system for a later callback (the display, window, seat, and graphics-context adapters on both platforms) could still be moved in memory afterward, leaving the operating system holding a dangling address into freed memory. They are now pinned in place from construction onward. Verified by an adversarial review that deliberately reintroduced the mutation past both gates guarding it and confirmed both catch it; the public API surface is unchanged.
- On Windows, `gltfx_gl_context::swap_buffers()` could report a stale system error code left behind by an earlier, unrelated, successful call, instead of the real cause of a failure (or no error at all). All nine fallible Win32/WGL calls in the graphics-context adapter now clear the system error code immediately before the call, matching a convention the window and display adapters already followed.

## [0.3.6.0] - 2026-09-09

### Infrastructure

- The style-sheet declaration-block reader (`gfss` internal groundwork; there is still no public styling/layout API yet): reads a block of style declarations with per-declaration recovery on invalid input, covering the value contract of all 104 registered properties, eleven accepted shorthand properties, ten rejected shorthands that name the longhand properties replacing them, and the three CSS-wide keywords (`initial`, `inherit`, `unset`).

### Fixed

- 102 of those 104 properties had no real test protection: the test meant to guard them compared the parser's output against the same table that fed the parser, so a wrong value placed in that table would agree with itself forever. Replaced with an independent oracle read from the versioned specification document.

## [0.3.5.0] - 2026-09-09

### Added

- `glintfx::gltfx_log_severity`, `glintfx::gltfx_log_value`, `glintfx::gltfx_log_field`, `glintfx::gltfx_log_event`, `glintfx::gltfx_log_sink` / `gltfx_log_set_sink()` / `gltfx_log_get_sink()` (`include/glintfx/core/log/`): the structured-log receiver. A consumer registers one callback (a plain function pointer plus an opaque context, never `std::function` - it can be replaced from any thread at any time) and receives events made of a numeric severity, a stable category, a stable event name, and typed `(name, value)` fields - never a pre-formatted sentence in any language. No sink registered means silence: nothing is written anywhere by default. The library's own first real emission, `gpu_kind_resolved` (`kind`/`path` fields), fires once when the OpenGL context adapter (both Wayland/EGL and Win32/WGL) resolves which kind of GPU it is running on, proved end to end against a real Wayland compositor.

### Infrastructure

- A local CI-mirror gate now links all 14 Win32 build targets against the real Microsoft `cl.exe`, with the target list itself derived from CMake rather than kept by hand, and was proved red against a known-broken commit before being trusted.

## [0.3.4.0] - 2026-09-09

### Infrastructure

- The style-sheet property registry (`gfss` internal groundwork; no public API yet): 104 CSS properties recognized by name, each entry cross-checked by hand against the external W3C specification that names them. A new property is always appended at the end, never renumbered - a rule proved by four deliberate sabotage attempts that reordered two existing entries, all four caught.

### Fixed

- Documentation had described this registry as holding "~57 properties"; corrected to the real count of 104 (the registry itself was already right, the prose describing it was not).

## [0.3.3.0] - 2026-09-08

### Fixed

- **Critical, visible to any consumer using the style-sheet selector parser:** the CSS `:nth-child(odd)` keyword was translated to the coefficients that match only the very first element, instead of every second element starting at the first (CSS Syntax Module Level 3, section 6.2). A style sheet using `:nth-child(odd)` selected one element instead of half of them, with no warning. The neighboring `:nth-child(even)` keyword was already correct; this was an isolated mistake, not a systemic one. Found and fixed by test-driven development: the two pre-existing tests were seen failing red before the one-line fix, one of them because it had recorded the same wrong value the implementation produced as its own expected value, the other because it only exercised the two positions where the wrong and right answers happen to coincide.

## [0.3.2.0] - 2026-09-08

### Fixed

- On Windows, the packaging-validation step that talks to a real `pkg-config` reader had been silently failing with an empty error message: Strawberry Perl installs a bare Perl script and its `.bat` wrapper under the same name in the same directory, and CMake's `find_program()` never tries the `.bat` suffix on Windows, so it found the script that cannot run directly and the reader process never started. The search now also tries `pkg-config.bat` / `pkgconf.bat` first on Windows. This step still only warns, it never fails the build; the project leader retains the decision to make it fatal.

### Infrastructure

- A test that used to only ever expect the warning above now measures the real reader's answer on whichever host runs it. `pkgconfig_test`, which was taking 91 to 115 seconds against a shared 120-second timeout on every Linux target, was given its own measured timeout.

## [0.3.1.0] - 2026-09-08

*This tag exists because v0.3.0.0 was published while wave W6b was still open, using a green CI run as the closing criterion when the wave's own plan defined a different one; the project leader ordered the real closeout marked separately, without retracting v0.3.0.0.*

### Fixed

- The public "current status" section of the documentation is rewritten to describe effects rather than implementation details, and is now checked by its own gate so it cannot drift out of date again.
- A mutation test against the window `ack_configure` handshake, run against a real Wayland compositor in two isolated containers, produced a result opposite to what was expected: neither a newly written test nor the pre-existing one catches a reintroduced defect there. The pre-existing test's claim of coverage is not retired; it is now tracked as a high-priority open item instead.

### Known limitations

- The main loop (`gltfx_loop`) still has no implementation on the trunk; a facade for it exists on a branch, not yet through adversarial review.
- The default graphics option is accepted by the public API but silently has no effect yet. This is now stated in the public documentation itself, with a tracked item to close the gap.

## [0.3.0.0] - 2026-09-08

### Added

- `glintfx::version`: the library's own version, as a value type with four components (`major_version`, `minor_version`, `patch_version`, `tweak_version`), matching the `vMAJOR.MINOR.PATCH.TWEAK` tag scheme.
- `glintfx::gltfx_err` / `glintfx::gltfx_rslt<T>`: the public error-handling types. Every fallible public function returns `gltfx_rslt<T>` (`gltfx_rslt<void>` when there is no success value); no exception crosses the public API. Full contract, including the read-wrong-value precondition and the name-collision audit, in [`docs/api-conventions.md`](docs/api-conventions.md).
- `glintfx::gltfx_rgba` / `glintfx::gltfx_rgba8`: the color value type, frozen at four 32-bit floats (`red`, `green`, `blue`, `alpha`), alpha inside, straight (never premultiplied), with the canonical numbers being linear light rather than the display-encoded byte a monitor receives. `gltfx_rgba8` is the display's own 8-bit-per-channel format. `gltfx_rgba_from_srgb8()` and `gltfx_rgba_to_srgb8()` round-trip between the two; `gltfx_rgba_premultiplied()` computes the transient premultiplied form. See the header comment in `include/glintfx/core/color.hpp` for the six binding decisions this type answers to, and why the linear-light choice is the one a consumer is most likely to get wrong silently (it changes what a rendered image looks like, with no compile error to catch it).
- `glintfx::gltfx_display` / `glintfx::gltfx_window` (`include/glintfx/platform/window/`): a real, working connection to the display server and a real, visible window opened on top of it, on both Linux (Wayland) and Windows (Win32), through the exact same public API on both. A consumer can open a display, open a window against it, read its size two different ways (window-system units and framebuffer pixels), read whether it is active, maximized, fullscreen or suspended, change its title, and pump the system's own events once per frame. There is still no way to draw into that window, and no input event delivery yet. Exactly which effects are proven identical on both platforms today, and which are still only measured, is tracked row by row in [`docs/window-portability-matrix.md`](docs/window-portability-matrix.md).
- `glintfx::gltfx_gl_context` (`include/glintfx/platform/gl/context.hpp`, `gfx_option.hpp`, `gpu.hpp`): the public shape of an OpenGL 3.3 core graphics context opened over a window, backed by a real platform adapter on both systems (`src/platform/wayland/egl_context_adapter.cpp`, `src/platform/win32/wgl_context_adapter.cpp`). `open()`, `make_current()`, `swap_buffers()`, `proc_address()`, `set_option()`/`option_support()` and `gpu()` are exercised for real, through the public API alone, on both platforms, by `tests/parity/gl_context_parity_test.cpp`. Exactly which effects that test asserts equal on both systems, which it only measures without asserting, and which are not yet exercised at all, is tracked row by row in [`docs/gl-loop-portability-matrix.md`](docs/gl-loop-portability-matrix.md).
- `glintfx::gltfx_loop` (`include/glintfx/platform/loop/loop.hpp`): the public shape of the main loop every consumer will eventually organize a frame around (one call that pumps events, ticks time and presents; or three separate calls for a consumer with its own loop shape) is frozen, with its guarantees written down in the header itself. **Not implemented yet** - the header declares the surface, not a working implementation, so calling it does not yet produce a running program.
- `glintfx::gltfx_vec2` / `gltfx_vec2f`, `gltfx_rect` / `gltfx_rectf`, `gltfx_transform`, `gltfx_angle`, `gltfx_mat3`, `gltfx_lerp()` (`include/glintfx/core/{vec2,rect,transform,angle,mat3,interpolate}.hpp`): the library's 2D math value types, frozen against four decisions the project leader ratified on 05/09/2026. World positions and measures (`gltfx_vec2`, `gltfx_rect`, `gltfx_transform`) use double precision, so a large map's coordinates keep their exactness; only the one matrix that reaches the graphics card (`gltfx_mat3`) narrows to single precision, built once per frame by `gltfx_mat3_from_transform()` (scale, then rotation, then translation). An angle is its own type (`gltfx_angle`), never a bare number, so mixing degrees and radians is a compiler error instead of a silently crooked picture. A rectangle is a corner plus a size, never two opposite corners. `gltfx_lerp()` uses the interpolation form that is exact at `t == 1`, unlike the naive `a + t * (b - a)`.
- `glintfx::gltfx_duration` (`include/glintfx/core/time.hpp`): elapsed time and duration reported as an exact integer count of a tiny unit that never accumulates error, alongside a ready-made conversion to seconds - the project leader's decision of 27/08/2026, chosen over decimal seconds as the primary form specifically because that drifts over the hours-long runtime a game sits in.
- `glintfx::gltfx_fixed_step` (`include/glintfx/core/fixed_step.hpp`): the optional fixed-timestep accumulator a consumer reaches for when it wants a constant physics/game-rule step instead of the loop's own raw, variable frame time. Deliberately not something the loop itself imposes: which `dt` the physics uses is an application rule, not a framework concern, and forcing an accumulator on every consumer would tax one that only animates a UI and never wanted it.
- `glintfx::gltfx_load_file_bytes()` (`include/glintfx/platform/asset/file.hpp`): the "path to bytes" atom. Reads a file, resolves and classifies the path, and reports failure through `gltfx_rslt<T>`, with no cache of its own, on purpose - choosing how to cache a loaded asset would impose this project's own policy on every consumer.
- The `glintfx::gfui` node-view contract (`include/glintfx/gfui/node_view.hpp`): the eight-fact contract a consumer implements so the future style/layout engine can see their own UI tree, with nothing renamed or reordered from what it asks. Documented in depth in [`docs/node-view-and-matching.md`](docs/node-view-and-matching.md).

### Infrastructure

- CMake build (shared library by default, static via `-DBUILD_SHARED_LIBS=OFF`), an install target with a CMake package (`find_package(glintfx)`) and a pkg-config module (`glintfx.pc`), and support for embedding by source (`add_subdirectory`/`FetchContent`). Reference: [`PACKAGING.md`](PACKAGING.md).
- Continuous integration across five platforms (Fedora, Ubuntu, Arch Linux, CachyOS, Windows), each built and tested in both shared and static mode. The Windows job now runs at the same depth as the Linux jobs: license-header coverage, public-header hygiene, static analysis (`clang-tidy` against `src/platform/win32/`), a dedicated AddressSanitizer run, and inspection of the real compiled binary's own imports and exports.
- A `parity` CI job that unions the real `ctest -N` inventory every Linux and Windows job produced and fails on any test name present on one system and missing on the other, unless it is a declared, tracked exception (`tests/parity_aliases.txt`, `tests/parity_exceptions.txt`).
- A Wayland protocol binding (`xdg-shell`, generated by `wayland-scanner` at build time), linked into the library target on Linux. Internal plumbing only: no window, event loop, or rendering is built on it yet.
- A CI gate that fails the build when a tracked source or build file is missing its `SPDX-License-Identifier: AGPL-3.0-or-later` header, so the license stated in [`LICENSE`](LICENSE) actually covers every file a consumer receives, not just the ones a human remembered to mark.
- A pkg-config validation step that runs on every `cmake --install`, not only in this project's own CI: it confirms the installed `glintfx.pc`'s `includedir`, `libdir`, and every `-I`/`-L` token it emits actually resolve on the machine doing the install, catching a broken packaging layout before a packager's own consumer ever tries to build against it. Built on CMake's own native pkg-config format parser (`cmake_pkg_config()`), which raised this project's declared CMake floor to 4.1. `glintfx.pc` is now installed on Windows too, not only on Unix; see "Packaging on Windows" in `PACKAGING.md` for what is and is not guaranteed there specifically.
- A dedicated CI job builds and links glintfx with Clang, alongside the existing GCC and MSVC jobs. Unit tests only for now: two test-only consumption gates (not glintfx's own source or headers) make a GCC-specific assumption that does not hold under Clang; tracked as open findings in [`TODO.md`](TODO.md).
- A GL 3.3 core function loader (`glintfx::render::load_gl_functions`), generated at build time from the OpenGL API registry rather than a hand-maintained list of prototypes. Internal plumbing only: nothing in this loader is part of the public API. See the "Third-party" section below for the one exception this required to the project's usual zero-dependency policy.
- A style-sheet tokenizer and closed-vocabulary token/value/property/keyword scanner for a future style/layout engine (`include/glintfx/gfss/`). Internal groundwork only: there is no public API for styling or layout yet, other than the `gfui` node-view contract listed above.
- A dedicated container image with the real Microsoft `cl.exe` (`tools/msvc-container/`), used to cross-check claims about MSVC behavior against the actual compiler rather than assumption.

### Third-party

- `third_party/khronos/gl.xml`: the Khronos Group's own OpenGL API registry, vendored verbatim under its own Apache-2.0 license (this project's only vendored third-party file). See [`third_party/khronos/README.md`](third_party/khronos/README.md) for the full provenance record and why this one case needed an exception to glintfx's own zero-dependency policy.

## [0.2.0.0] - 2026-09-05

The first tagged version of glintfx. It marks the point where all five supported platforms were first proved equal: 21 of 21 jobs green on Fedora, Ubuntu, CachyOS, Arch, and Windows, in both build modes, plus static analysis, the sanitizer run, debugger checks, a second compiler, an isolated container test, and the license-header gate.

### Fixed

- On Windows, reading a file could report success while handing back a truncated buffer if the read failed partway through: a consumer asked for a file, received half of it, and was told everything went fine.
- The staged install-verification check constructed a path layout impossible on the platform it ran on, rejecting a correct installation and suggesting a packager disable its own validation.
- Mixing an absolute directory with a relative one produced a descriptor that sent a consumer's own compiler looking for headers in an empty location.

### Infrastructure

- The Windows test suite grew from 46 to 70 cases, closing the gap against the other four platforms from nineteen down to ten (the remaining ten named one by one in `README.md`). Every gate that only scans the tree now runs on all five supported systems.

### Notes

- glintfx is pre-1.0: the public surface can still change at any time, and any compatibility promise made today only holds within the same second version component.

[Unreleased]: https://github.com/petrinhu/GlintFx/compare/v0.7.0.0...HEAD
[0.7.0.0]: https://github.com/petrinhu/GlintFx/compare/v0.6.1.0...v0.7.0.0
[0.6.1.0]: https://github.com/petrinhu/GlintFx/compare/v0.6.0.0...v0.6.1.0
[0.6.0.0]: https://github.com/petrinhu/GlintFx/compare/v0.5.2.0...v0.6.0.0
[0.5.2.0]: https://github.com/petrinhu/GlintFx/compare/v0.5.1.0...v0.5.2.0
[0.5.1.0]: https://github.com/petrinhu/GlintFx/compare/v0.5.0.0...v0.5.1.0
[0.5.0.0]: https://github.com/petrinhu/GlintFx/compare/v0.4.2.1...v0.5.0.0
[0.4.2.1]: https://github.com/petrinhu/GlintFx/compare/v0.4.2.0...v0.4.2.1
[0.4.2.0]: https://github.com/petrinhu/GlintFx/compare/v0.4.1.0...v0.4.2.0
[0.4.1.0]: https://github.com/petrinhu/GlintFx/compare/v0.4.0.0...v0.4.1.0
[0.4.0.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.8.0...v0.4.0.0
[0.3.8.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.7.0...v0.3.8.0
[0.3.7.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.6.0...v0.3.7.0
[0.3.6.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.5.0...v0.3.6.0
[0.3.5.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.4.0...v0.3.5.0
[0.3.4.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.3.0...v0.3.4.0
[0.3.3.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.2.0...v0.3.3.0
[0.3.2.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.1.0...v0.3.2.0
[0.3.1.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.0.0...v0.3.1.0
[0.3.0.0]: https://github.com/petrinhu/GlintFx/compare/v0.2.0.0...v0.3.0.0
[0.2.0.0]: https://github.com/petrinhu/GlintFx/tree/v0.2.0.0
