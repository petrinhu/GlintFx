# API reference: Draw 2D

Filled rectangles and quadrilaterals, drawn into an OpenGL 3.3 core
context you already opened, gathered into as few draw calls as the order
you asked for allows. Same public API on Linux and Windows, no
platform-specific code at the call site.

Before reading this page, read [API Reference: Core](API-Core)'s "Error
handling" section and [API Reference: Graphics Context](API-Graphics-Context):
a renderer is opened over a `gltfx_gl_context`, and every fallible call
below returns the same `gltfx_rslt<T>` those pages already explain.

This page describes what a program can observe. For what has been measured
on each system, and what has not been proven equal yet, read
[draw2d-portability-matrix.md](https://github.com/petrinhu/GlintFx/blob/main/docs/draw2d-portability-matrix.md).

## A whole frame

```cpp
glintfx::gltfx_rslt<glintfx::gltfx_renderer_2d> opened =
    glintfx::gltfx_renderer_2d::open(context, glintfx::gltfx_renderer_2d_desc{});
if (opened.has_error()) { /* opened.err() says why */ }
glintfx::gltfx_renderer_2d renderer = std::move(opened.value());

renderer.begin_frame(glintfx::gltfx_frame_2d_desc{});   // clears to opaque black
renderer.fill_rect({{40, 40}, {80, 80}}, {1.0F, 0.0F, 0.0F, 1.0F});
renderer.fill_rect({{80, 80}, {80, 80}}, {0.0F, 0.0F, 1.0F, 1.0F},
                   glintfx::gltfx_draw_layer{1});        // painted on top
auto report = renderer.finish_frame();                   // does NOT present
context.swap_buffers();                                  // you present
```

A frame is `begin_frame()`, any number of `fill_rect()` and `fill_quad()`
calls, then `finish_frame()`. Drawing calls return nothing: a problem is
reported once, at the end of the frame, in the report.

## The renderer

[`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp):

| Name | Effect |
|---|---|
| `gltfx_renderer_2d::open(context, desc)` | Opens a renderer that draws into `context`. Returns the renderer or a `gltfx_err` (below). `context` must outlive the renderer. Moving the context handle while the renderer is alive is harmless. |
| `renderer.is_open()` | Whether the renderer holds its drawing state. False only for a renderer that was moved from. |
| `renderer.begin_frame(desc)` | Starts a frame: reads the size of the context's surface, clears it if `desc` asks for that, and opens the pixel-direct batch. Called while a frame is already open, it throws the open frame away, undrawn, and the next report counts it. |
| `renderer.begin_batch()` | Starts a batch with no transform: a world position is a pixel position. |
| `renderer.begin_batch(world_to_pixel)` | Starts a batch whose pieces are placed by a `gltfx_transform` (scale first, then rotation, then translation). Changing batch does not by itself cost a draw call. |
| `renderer.fill_rect(rect, color, layer)` | A filled axis-aligned rectangle. `layer` defaults to the default layer. |
| `renderer.fill_quad(corners, color, layer)` | A filled quadrilateral with four free corners, painted as two triangles. |
| `renderer.flush()` | A barrier: everything submitted so far is sent to the graphics card now, in its order. |
| `renderer.finish_frame()` | Ends the frame and returns its report. It does not present. |
| `renderer.last_frame_report()` | The report of the last `finish_frame()`, still readable when that call returned an error. |

`gltfx_renderer_2d_desc::reserve_pieces` is a hint, never a limit: roughly
how many pieces you expect in one frame, so the first frames do not grow
storage while you draw. Zero lets the renderer choose. Submitting more is
always allowed.

One renderer is used from one thread, the same thread its context is used
from. The renderer makes its own context current before it touches GL, so
a program with two windows never draws into the wrong one. Calling a
method on a renderer that was moved from is a precondition violation: a
debug build stops with a named message.

## Order: layers, then submission

Pieces are painted by layer, and on the same layer by the order you
submitted them, on every system. A lower layer is painted first, so a
higher layer lands on top. `gltfx_draw_layer{value}` is a strong type, so
a call site says what the number is. A piece drawn without a layer is on
layer zero.

The order does not cross `flush()`: a piece submitted after a flush is
painted over everything before it, whatever its layer.

## Coordinates, transform, precision

A piece is given in world space. With no transform a world position is a
pixel position, measured from the top left of the surface. With a
transform the library applies it in double precision, to world positions
that were never narrowed, and only the finished pixel positions become
single precision. The transform is applied when you submit the piece, so
changing batch afterwards moves nothing already submitted.

A camera looking at world point C, zoomed by Z, showing C at pixel P, with
no rotation, is `{.translation = P - Z * C, .rotation = {0}, .scale = {Z, Z}}`.
A y-up world is a negative y scale.

## Color

`gltfx_rgba` is linear light with straight alpha. Blending happens with
premultiplied alpha inside the library, so a half-transparent edge never
grows a dark fringe. With the context option `srgb_framebuffer` on,
blending happens in linear light and the surface encodes the result. With
it off, the color is encoded to sRGB before blending, which is what most
2D libraries do, and the clear color is encoded the same way.

## The frame report

[`frame_2d_report.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/frame_2d_report.hpp)
says what happened to the frame, counted. Every piece submitted while a
frame is open ends in exactly one of four places: drawn, refused,
dropped for lack of memory, or dropped because the graphics side failed.
The four counters add up to the pieces submitted.

| Field | Meaning |
|---|---|
| `pieces_submitted` | Pieces submitted while the frame was open. |
| `pieces_drawn` | Pieces that reached the graphics card. A piece with zero area is drawn: it covers no pixel. |
| `pieces_refused`, `first_refusal` | Pieces refused for a value that is not a finite number, or a rectangle with a negative width or height, and the token of the first reason. Not an error of the frame. |
| `pieces_dropped_out_of_memory` | Pieces dropped because the renderer could not get memory of its own. The frame then ends with an error. |
| `pieces_dropped_graphics_failure` | Pieces accepted that never reached the screen because the context could not be made current, or the card refused the frame's data. The frame then ends with an error. |
| `pieces_dropped_outside_frame` | Pieces submitted with no frame open, since the previous report. |
| `frames_abandoned` | Frames begun and never finished, since the previous report. |
| `batches`, `flushes` | How many times the transform changed, and how many barriers the frame had. |
| `draw_calls` | How many draw calls reached the card. Printed for you to measure, never promised as a number. |

## Errors

Nothing throws. Refusals are a `gltfx_err` whose `rejected_value()` is a
token, never a sentence.

`open()` can return:

| Code | Token | Meaning |
|---|---|---|
| `invalid_argument` | `context` | The context is not open. |
| `unsupported` | the GL function name | The driver lacks a function the renderer needs. |
| `platform_failure` | `vertex_shader`, `fragment_shader`, `program_link` | The driver refused the library's own drawing program. The driver's message goes to the log sink as the field `driver_log` of the event `draw2d_program_rejected`, never into the error. |
| `platform_failure` | `vertex_array_create` | The card did not give the renderer its vertex array or buffers. |
| `out_of_memory` | `vertex_array_create` or none | The room asked for could not be reserved, or the card itself ran out of memory while creating the renderer's objects. |

`finish_frame()` can return:

| Code | Token | Meaning |
|---|---|---|
| `out_of_memory` | none, or the step | Pieces were dropped for lack of memory, or the card itself ran out of memory (the GL error code is in `os_error_code()`). |
| `platform_failure` | `vertex_upload`, `index_upload`, `draw` | The card refused the frame's data. |
| `platform_failure` | the context's own | The context could not be made current. |
| `invalid_argument` | `frame` | No frame was open. |

When there is an error, the report is still there: read it with
`last_frame_report()`.

## What the renderer leaves in GL

The renderer trusts no GL state when it draws: it sets everything it
depends on. After `flush()` and `finish_frame()` it leaves GL as follows,
and promises nothing else: no program, vertex array, buffer or texture
bound and texture unit zero active; blending on with premultiplied
factors and the add equation; polygon mode fill; depth, stencil, scissor
and face culling off; color mask fully on; rasterizer discard, color
logic op and alpha-to-coverage off; the draw framebuffer zero; the
viewport over the whole surface; and `GL_FRAMEBUFFER_SRGB` on exactly when
`srgb_framebuffer` is on. Call `flush()` before you call GL yourself in
the middle of a frame.

## What this page does not promise

- A number of frames per second, or of pieces per draw call.
- Drawing from more than one thread.
- Surviving the loss of the GPU context by the driver: OpenGL 3.3 without
  the robustness extension cannot detect it.
- A color brighter than white on an 8-bit surface: a value above one is
  clipped by the surface, not by the type.
