# API Reference: Draw 2D shapes

This page covers the filled and outlined shapes that glintfx draws into a
graphics context you already opened: triangles, lines, rectangles and
ellipses. For each call it says what appears on the screen, what you pass
in, and which values are refused. It builds on [Draw 2D](API-Draw-2D), which
explains the frame, the renderer and the frame report.

Only the shapes that are finished are on this page. Drawing pictures onto
shapes is still under review and is not covered here.

## Words used on this page

- **Pixel**: one dot of the surface the graphics card draws into. Without a
  transform, a position is a pixel position, counted from the top left
  corner, with y growing down.
- **World position**: a position in your own units, for example the units of
  your game world. A transform turns it into pixels.
- **Batch**: a group of drawing calls that share one transform. The transform
  moves, turns and scales every call in the group. See [Draw 2D](API-Draw-2D)
  for how batches work.
- **Layer**: a number that sets the painting order. A higher layer is painted
  over a lower one.
- **Vertex**: one corner point of a triangle. It has a position and a color.
- **Triangle**: a shape with three corners. The graphics card fills only
  triangles, so every shape on this page is built from them.
- **Index**: a whole number that points at one vertex in a list. Index 0 is
  the first vertex in the list.
- **Indexed triangles**: a list of vertices, plus a second list of indices.
  Each group of three indices names three vertices. A corner that several
  triangles share is written once in the vertex list.
- **Stroke**: an outline drawn as a band of a chosen thickness. The band can
  follow a line, the edge of a rectangle or the curve of an ellipse.
- **Thickness**: how wide the band of a stroke is.
- **Cap**: the shape at the end of a line. A flat cap stops at the exact end
  point. A square cap adds half the thickness past each end. A rounded cap
  adds a half disc at each end.
- **Alignment**: where a rectangle's outline grows: centered on its edge,
  inside the rectangle, or outside it.
- **Radius**: the distance from the center of an ellipse to its edge, along
  one of its two axes.
- **Curve**: a smooth edge, such as the edge of a circle. The graphics card
  draws only straight pieces, so a curve is drawn as many short straight
  segments. Cutting a curve into straight pieces is called tessellation.
- **Curve distance**: the largest gap, in pixels, between a true curve and the
  straight segments that stand in for it.
- **Alpha**: how opaque a color is. Alpha 1 is solid. Alpha 0.5 is half
  transparent, so the color behind shows through.
- **Refusal**: a call that is not drawn because one of its values is wrong.
  A refusal does not stop the frame. The frame still ends, and the frame
  report counts the refused call and names the first reason.
- **Frame report**: the counts that `finish_frame()` returns at the end of each
  frame.

## Where to read the exact rules

This page says what each call does for you. The exact signatures and every
refusal live in the headers. The continuous integration reads the pixels that
the example below draws, so the example stays correct on every system the
project supports.

- [`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp):
  the six shape calls on this page, each with its refusals.
- [`stroke.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/stroke.hpp):
  the thickness, the caps and the outline alignment.
- [`examples/shapes/shapes.cpp`](https://github.com/petrinhu/GlintFx/blob/main/examples/shapes/shapes.cpp):
  one program that draws every shape on this page in one scene.

The curve field that the section [How smooth curves are](#how-smooth-curves-are)
explains sits in the renderer's descriptor, in `renderer_2d_desc.hpp`.

## fill_triangles

### On screen

Each triangle is filled. Each corner has its own color, and the colors blend
smoothly across the triangle. The example draws one triangle with a red, a
green and a blue corner.

### What you pass in

- `vertices`: a list of vertices, read three at a time. Every three vertices
  make one triangle. Each vertex has a position in world units and a color.
- `layer`: optional. It defaults to layer zero.

The library copies what it needs during the call, so you can reuse or free
your list as soon as the call returns. One call counts as one piece in the
frame report, however many triangles it holds.

### What is refused

A refused call is dropped as a whole, never one triangle at a time. The frame
report names the first rule that applies:

- `too_many_vertices`: more vertices than a 32-bit index can address.
- `count_not_multiple_of_three`: the number of vertices is not a multiple of
  three.
- `not_a_number`, then `infinite`: a position or a color is not a finite
  number.
- `pixel_position_out_of_range`: a corner is so far away that the graphics
  card would overflow.

An empty list does nothing, and it does not count as a piece.

## fill_indexed_triangles

### On screen

The same filled triangles, but the shape is described by two lists. The
example draws a yellow hexagon. Its center corner is listed once, and all six
triangles around it share that corner.

### What you pass in

- `vertices`: the list of corners, as in `fill_triangles`.
- `indices`: a list of whole numbers. Each group of three names three entries
  of `vertices`, in order.
- `layer`: optional.

The two calls have two names on purpose. An empty list passed as the second
argument can then never be ambiguous to the compiler.

### What is refused

The same reasons as `fill_triangles`, and also:

- `count_not_multiple_of_three`: the number of indices is not a multiple of
  three.
- `index_out_of_range`: an index does not name a vertex in the list.

An empty list of vertices or of indices does nothing.

## stroke_line

### On screen

A straight line of the thickness you chose, with the end cap you chose. The
example draws the same white line three times, once per cap. The flat line
stops at its end. The square line reaches half its thickness past each end.
The rounded line has a half disc at each end.

A line with zero length draws only its cap: nothing for a flat cap, a square
for a square cap, and a disc for a rounded cap. A line with zero thickness
covers no pixel.

### What you pass in

- `segment`: the start and the end, in world positions.
- `style`: the thickness, its unit, and the cap. See
  [Line thickness, caps and outlines](#line-thickness-caps-and-outlines).
- `color`: the color, with its alpha.
- `layer`: optional.

### What is refused

- `not_a_number`, then `infinite`: the start, the end, the thickness or the
  color is not a finite number.
- `negative_size`: the thickness is below zero.
- `unknown_option`: the unit or the cap is not one of the named values.
- `pixel_position_out_of_range`: a point is so far away that the graphics card
  would overflow.

## stroke_rect

### On screen

The outline of an axis-aligned rectangle, with square corners. The alignment
decides where the band grows. By default it is centered on the edge. Inside
keeps the whole band inside the rectangle. Outside keeps the whole band
outside it. The example draws the same square three times, once per
alignment.

The four sides do not overlap at the corners. So a half-transparent outline
shows one even color all the way around, with no darker corner.

If the band is thick enough to reach the middle of the rectangle's shorter
side, the hole closes and the outline fills its outer rectangle. Centered, the
thickness has to reach the whole shorter side. Inside, half of it is enough.
Outside, the hole never closes.

### What you pass in

- `rect`: a corner (the one with the smallest x and y) and a size (width and
  height), in world positions.
- `style`: the thickness, its unit and the alignment. See
  [Line thickness, caps and outlines](#line-thickness-caps-and-outlines).
- `color`: the color, with its alpha.
- `layer`: optional.

### What is refused

- `not_a_number`, then `infinite`: the corner, the size, the thickness or the
  color is not a finite number.
- `negative_size`: the width, the height or the thickness is below zero.
- `unknown_option`: the unit or the alignment is not one of the named values.
- `pixel_position_out_of_range`: a point of the outline is so far away that
  the graphics card would overflow.

## fill_ellipse

### On screen

A filled ellipse. Its edge looks smooth, because the library draws the curve
as many short straight segments, each one close to the true curve (see
[How smooth curves are](#how-smooth-curves-are)). The example draws a green
circle.

An ellipse with a zero radius is drawn, but it covers no pixel.

### What you pass in

- `ellipse`: the center, a radius along each axis, and a turn. A positive turn
  goes from the first axis toward the second. With y growing down, that turn
  is clockwise on the screen.
- `color`: the color, with its alpha.
- `layer`: optional.

The smoothness of the curve is not an argument of this call. The renderer
sets it once, when it is opened.

### What is refused

- `not_a_number`, then `infinite`: the center, a radius, the turn or the color
  is not a finite number.
- `negative_size`: a radius is below zero.
- `pixel_position_out_of_range`: a point of the edge is so far away that the
  graphics card would overflow.

## stroke_ellipse

### On screen

The outline of an ellipse: a band centered on its curve, with half of the
thickness on each side. The example draws a blue outline, turned by 30
degrees. An ellipse has no alignment, because it has no inside or outside
edge that differs from its curve.

If half the thickness reaches the shorter radius, the hole in the middle
closes, and the outline fills the ellipse out to its outer edge.

A radius of zero is not refused. With both radii at zero, the outline is a
disc. With one radius at zero, it is a bar with rounded ends.

### What you pass in

- `ellipse`: as in `fill_ellipse`.
- `stroke`: the thickness and its unit. See
  [Line thickness, caps and outlines](#line-thickness-caps-and-outlines).
- `color`: the color, with its alpha.
- `layer`: optional.

With the unit in world units, the outline follows the batch transform. A
stretched batch makes the outline thicker along one axis. With the unit in
pixels, the outline keeps its thickness on the screen at any zoom.

### What is refused

- `not_a_number`, then `infinite`: the center, a radius, the turn, the
  thickness or the color is not a finite number.
- `negative_size`: a radius or the thickness is below zero.
- `unknown_option`: the unit is not one of the named values.
- `pixel_position_out_of_range`: a point of the outline is so far away that
  the graphics card would overflow.

## Line thickness, caps and outlines

Three small types give a shape its look. Each one is a plain set of named
values that you fill in and pass to a call.

### gltfx_stroke

- `thickness`: how wide the band is. The default is 1. Zero draws nothing
  visible. A negative value is refused as `negative_size`.
- `unit`: how the thickness is measured. `gltfx_stroke_unit::world` is the
  default. The thickness is in your world units, so it grows and shrinks with
  the zoom of the batch. `gltfx_stroke_unit::pixel` keeps the thickness at the
  same number of screen pixels at any zoom.

### gltfx_line_style

Used by `stroke_line`.

- `stroke`: a `gltfx_stroke`, as above.
- `cap`: a `gltfx_line_cap`. The default is `flat`. The three values are:
  - `flat`: the line ends at its exact end point.
  - `square`: the line reaches half its thickness past each end.
  - `rounded`: each end is closed with a half disc.

### gltfx_rect_outline_style

Used by `stroke_rect`.

- `stroke`: a `gltfx_stroke`, as above.
- `align`: a `gltfx_stroke_align`. The default is `centered`, with half of the
  thickness on each side of the edge. `inside` puts the whole band inside the
  rectangle. `outside` puts the whole band outside it.

The alignment is a field of the rectangle's style only. Lines and ellipses
have no inside or outside, so they do not take it.

## How smooth curves are

The edge of an ellipse and the rounded caps of a line are curves. The graphics
card draws straight pieces only, so the library cuts each curve into short
straight segments. The setting that controls how short they are is
`curve_max_error_pixels`. It belongs to the renderer's descriptor, and you
pass it to `open()` when you make the renderer. Every curve that renderer
draws uses it.

- **What you see:** `curve_max_error_pixels` is the largest gap, in pixels of
  the surface, between a true curve and the straight segments in its place. A
  smaller value gives a smoother curve, and it costs more triangles.
- **Default:** 0.25 pixels. At this value a circle looks round.
- **Refused:** `open()` refuses any value that is not a finite number greater
  than zero. The refusal is `invalid_argument`, and it names
  `curve_max_error_pixels`. Nothing is made.
- **Ceiling:** the cost has a limit whatever the value. One ellipse fill uses
  at most 1024 straight segments. The outline of one ellipse uses at most 2048
  points on each of its two edges. Each rounded cap uses at most 1024
  segments. A value so small that the limit is reached is still drawn, but
  with a gap larger than the one you asked for.
- **Example:** the example opens a second renderer with a distance of 12
  pixels. The same circle comes out with five straight sides, so it looks like
  a pentagon.

## A short example

[`examples/shapes/shapes.cpp`](https://github.com/petrinhu/GlintFx/blob/main/examples/shapes/shapes.cpp)
draws every shape on this page in one fixed scene, on every frame. Read it
from top to bottom:

- **Top row, filled shapes:** a red square, a green diamond, a triangle with a
  red, a green and a blue corner, and a yellow hexagon drawn with indexed
  triangles.
- **Middle row, outlines:** the same square drawn three times, with a white
  half-transparent outline centered on the edge, a magenta outline inside, and
  a cyan outline outside. A blue ellipse outline, turned, sits to the right.
- **Bottom row:** three lines, one per cap. A yellow line drawn in a batch that
  zooms by 2 shows the difference between thickness in world units and in
  pixels. A green circle uses the default curve distance, and a second
  renderer draws the same circle with a coarse distance.
- **On top:** thin black lines show where each outline grows and how far each
  cap reaches past its end.

Each call in the example is one line of the same form. This fragment draws the
centered outline:

```cpp
renderer.stroke_rect({{30.0, 190.0}, {100.0, 100.0}},
                     {.stroke = ten, .align = glintfx::gltfx_stroke_align::centered},
                     k_half_white);
```

The program is built with the project's CMake, and it is called `shapes`. Run
it without arguments and it draws until you close its window. Run
`shapes --frames N`, with N from 1 to 1000000, and it stops by itself after N
drawn frames. It exits with 0 when it finishes, with 1 when a library step
fails or a frame loses pieces, and with 2 when the command line is wrong.

The example is a demonstration. It uses only the public API, and nothing in it
becomes a rule of the library.

## What this page does not promise

- A speed, or a number of pieces per draw call. The frame report counts the
  draw calls, and that count is for you to measure.
- That a curve is exact. Its edge stays within the distance you chose, unless
  the limit on segments is reached first.
- That the pixels the example draws are a rule of the library.
- A color brighter than white on an 8-bit surface. A value above 1.0 is
  clipped by the surface.
- Drawing from more than one thread at a time.
- Surviving the loss of the graphics context by the driver.
- Pictures drawn onto shapes. That is not on this page yet.

## Related

- [API Reference: Draw 2D](API-Draw-2D): the renderer, the frame and the frame
  report that these shapes are drawn with.
- [API Reference: Core](API-Core): the error pattern every refusal on this
  page follows.
- [Home](Home): the list of finished areas.
