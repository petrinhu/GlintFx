# API reference: Texture atlas

A **texture atlas** is one big picture that holds many small pictures side
by side. Instead of keeping each small picture in its own texture, your
program writes them all into one texture and remembers where each one sits.
Later it draws any of them by pointing at its place.

glintfx has two parts for this:

- The **allocator** decides where each picture goes, and takes the space
  back when you no longer need it. It knows nothing about pictures or
  graphics cards. It only works with rectangles of numbers.
- The **atlas** owns the big texture, writes each picture into the place the
  allocator chose, and tells you where that place is.

**Status, as of 2026-11-20: in progress.** The atlas is part of the texture
API, which is still under review before its public names are called stable.
The names on this page are the ones in the public headers today. They can
still change before that review ends. The
[README's Status section](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
lists textures among the parts that are built: it says textures are drawn,
from images handed in as bytes. It does not name the atlas yet. The README
mentions the atlas only in its examples section. The [Home](Home) page lists
this page under "In progress", not with the finished areas.

## Words used on this page

- **Texture**: a picture kept in the graphics card's memory, so the card can
  draw it.
- **Card**: the graphics card, the part of the computer that draws pixels on
  the screen.
- **Texel**: one dot of a texture. A pixel is a dot on the screen. A texel is
  a dot of the picture stored on the card. Positions count from the top left
  corner of the texture, starting at zero.
- **Region**: a rectangle inside a texture, given as a corner and a size, in
  texels.
- **Filter**: the rule for choosing the colour of a screen dot when a picture
  is drawn at a size other than its own. `nearest` copies one texel. `linear`
  blends the neighbouring texels.
- **Margin**: a ring of extra texels around each picture. The ring holds
  copies of the picture's own edge (see [Why the margin exists](#why-the-margin-exists)).
- **Cell**: the place the allocator gives one picture. A cell is big enough
  for the picture and its margin. Each side of a cell is rounded up to a
  multiple of 2^k.
- **Reduced levels** (also called mipmaps): smaller copies of the texture,
  made ahead of time. Each one is half the size of the one above it. They
  make a picture drawn small look right.
- **2^k** (two to the power k): 2 multiplied by itself k times. So 2^0 is 1,
  2^1 is 2, 2^2 is 4 and 2^3 is 8.
- **Descriptor**: a plain bundle of named values that you fill in and pass to
  a function. Here it holds the size and the options of the atlas.
- **Tint**: a colour that multiplies the colours of a picture as it is drawn.
  White leaves the picture as it is.
- **Mirror**: a flip of the picture, left to right or top to bottom, when it
  is drawn.
- **Piece**: one thing you ask the renderer to draw, such as one picture or
  one rectangle.
- **Flush**: the moment the renderer sends the pieces it has collected to the
  card, in order. Nothing sent after a flush is drawn before what came earlier.
- **Frame report**: the summary that `finish_frame()` returns when a frame
  ends. It counts the pieces that were refused, by reason, and how many times
  the bound texture changed.
- **Slot**: the receipt that `add_image()` gives you for the cell that holds a
  picture. A slot has a number for the cell, and a generation that counts how
  many times that number was handed out. So an old slot never matches a newer
  cell that reuses the number. You keep the slot to draw the picture, to find
  its place, or to give the cell back.
- **Moved-from**: an object whose contents were handed to another object with
  a move. It still exists, but it holds nothing.
- **Thread**: one path of work inside a program. Two threads can run at the
  same time. An object used from two threads at once can be damaged.
- **Continuous integration (CI)**: a server that builds the library and runs
  its tests after every change, on each supported system.

## Where to read the exact rules

This page explains what each name does, from your side of the call. The exact
signatures and the full list of refusals live in the headers. The example
below is built by continuous integration on every supported system, so its
code stays correct.

- [`texture_atlas.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/texture_atlas.hpp):
  the atlas, its descriptor and each call.
- [`atlas_allocator.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/atlas_allocator.hpp):
  the allocator, the slot and its refusals.
- [`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp):
  the drawing calls, including the one that draws a picture of the atlas.
- [`examples/textures/textures.cpp`](https://github.com/petrinhu/GlintFx/blob/main/examples/textures/textures.cpp):
  a program that draws the same white picture from a plain texture and from
  an atlas, side by side, so you can compare the two.

## A short example

This is the atlas code of the example, shortened. The `picture` is a
`gltfx_image_view` over your own pixel bytes. The example gets its image from
a small helper. The view's fields are in
[`image_view.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/image_view.hpp).
The example draws the picture with `draw_texture()` and a region from
`region_of()`, the path for the textured calls that take a region. Step 4
shows the call that takes the slot directly.

```cpp
// 1. Open the atlas on a renderer that is already open.
auto opened = glintfx::gltfx_texture_atlas::open(
    renderer,
    {.width = 32, .height = 32, .margin_pixels = 1,
     .filter = glintfx::gltfx_texture_filter::linear});
if (opened.has_error()) { /* opened.err() says why */ }
glintfx::gltfx_texture_atlas atlas = std::move(opened.value());

// 2. Write a picture into a cell of its own. Its pixels are read only during this call.
auto slot = atlas.add_image(picture);
if (slot.has_error()) { /* slot.err() says why */ }

// 3. Ask where the picture sits inside atlas.texture(), in texels.
auto place = atlas.region_of(slot.value());
if (place.has_error()) { /* place.err() says why */ }

// 4. Draw it from the atlas texture into a rectangle on the screen.
renderer.draw_texture(atlas.texture(), {.region = {.source = place.value()}},
                      {{356.0, 316.0}, {64.0, 64.0}});
```

The example draws the same white picture twice: once from a plain texture,
and once from the atlas. The atlas copy keeps a clean white edge. The
example's header describes the scene. The pixels it expects are listed in
[`tests/fixtures/textures_probes.txt`](https://github.com/petrinhu/GlintFx/blob/main/tests/fixtures/textures_probes.txt).

## Step by step

### 1. Open the atlas

`open()` takes a renderer and a descriptor. The descriptor has these fields:

| Field | Default | Meaning |
|---|---|---|
| `width`, `height` | none; zero is refused | Size of the big texture, in texels. |
| `margin_pixels` | `1` | Texels of copied edge around each picture. See [Why the margin exists](#why-the-margin-exists). |
| `filter` | `linear` | How the texture is sampled. `nearest` keeps sharp edges, for pixel art. `linear` blends neighbouring texels. |
| `mipmap_levels` | `0` | Exactly how many reduced levels the texture gets. Zero means none. |

`open()` refuses a descriptor that breaks these rules:

- `width` and `height` must not be zero.
- `mipmap_levels` cannot be larger than the largest side allows. Halve the
  larger side again and again. The number of halvings that still leave at
  least one texel is the most levels you can ask for. A 32-texel side allows 5.
- The margin must be at least 2^k, where k is `mipmap_levels`. With no
  reduced levels, that means at least 1.
- With k reduced levels, `width` and `height` must both be multiples of 2^k.
- Twice the margin, plus one texel, must fit inside both `width` and `height`.

### 2. Add a picture

`add_image()` takes a `gltfx_image_view` and returns a slot, the receipt of
the cell that holds the picture. The atlas gives the picture a **cell**: its
own place, big enough for the picture and its margin. Each side of a cell is
rounded up to a multiple of 2^k, so every cell starts on a multiple of 2^k.

The library reads your pixels only during the call. You may free your bytes
right after `add_image()` returns.

`add_image()` is not a flush. It writes the picture into the texture at once,
but the pieces you already submitted, and have not yet sent, read the texture
when they are sent. If pieces submitted earlier in the same frame must keep
the old contents, call `renderer.flush()` before `add_image()`.

### 3. Find the picture

`region_of(slot)` returns the rectangle of the picture itself, not of its
cell, in texels of `atlas.texture()`. You need it for the textured calls that
take a region, such as `draw_texture()`, and for a part of a picture.
`draw_atlas_image()` takes the slot itself, so it does not need `region_of()`.

Always ask `region_of()` for the place. A later version of glintfx may place
pictures differently, so do not copy positions from one run into your code.

### 4. Draw the picture

To draw a whole picture from the atlas, pass the atlas and the slot to
`draw_atlas_image()`. The call draws the picture and nothing around it, into
the rectangle you give. The mirror is applied inside the picture, wherever
its cell is in the texture. The tint multiplies the picture's colours. The
tint you do not choose is opaque white, which leaves the picture as it is.

Include the header before you call it:

```cpp
#include <glintfx/draw2d/texture_atlas.hpp>
```

Then, between `renderer.begin_frame(...)` and `renderer.finish_frame()`:

```cpp
// The slot from add_image(), a whole picture, and a rectangle on the screen.
renderer.draw_atlas_image(atlas, {slot.value()}, {{356.0, 316.0}, {64.0, 64.0}});

// The same, mirrored left to right and with a pink tint.
renderer.draw_atlas_image(
    atlas,
    {.slot = slot.value(),
     .mirror = glintfx::gltfx_texture_mirror::flip_left_right,
     .tint = glintfx::gltfx_rgba{1.0F, 0.5F, 0.5F, 1.0F}},
    {{356.0, 316.0}, {64.0, 64.0}});
```

The example above is built by continuous integration, and it does not call
`draw_atlas_image()` yet. The first snippet has the form the library's own
tests use for this call. The second one names the mirror and the tint. Their
fields are declared in `gltfx_atlas_paint`, in
[`texture_atlas.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/texture_atlas.hpp).

Use `draw_atlas_image()` to draw one whole picture. Use `draw_texture()` with
a region from `region_of()` for a part of a picture, and for the other
textured calls of [`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp):
`draw_texture_quad()`, `fill_textured_triangles()` and
`fill_indexed_textured_triangles()`. This wiki has no page for those calls
yet.

`draw_atlas_image()` returns nothing and never throws. A piece it refuses is
not drawn. The refusal is counted in the frame report, under a counter named
for its reason, such as `pieces_refused_stale_region`. The reasons, in the
order they are checked:

- `not_a_number`, then `infinite`: a value of the rectangle or of the tint is
  not a finite number.
- `negative_size`: the rectangle has a negative width or height.
- `unknown_option`: the mirror is not one of the values of
  `gltfx_texture_mirror`.
- `texture_not_open`: the atlas was moved from.
- `texture_other_renderer`: the atlas was opened on another renderer.
- `region_other_atlas`: another atlas handed out the slot, or this atlas had
  the slot before it received another atlas by move assignment.
- `stale_region`: the slot was never handed out, as the default slot is, or
  its cell was released or changed since `add_image()` returned it.
- `pixel_position_out_of_range`: the batch transform takes a corner outside
  the range the drawing program can scale.

The slot is checked when the call runs. A piece drawn before `release_slot()`
and not yet sent still shows what the texture holds when it is sent. See
[step 2](#2-add-a-picture) for why `flush()` keeps earlier pieces as they were.

### 5. Give the place back

`release_slot(slot)` frees the cell. The texels stay in the texture until
another picture is written over them. After a release, do not draw the picture
from that slot or from its region. The place may now hold a different picture.
A slot that was already released is refused.

## How the allocator places pictures

The allocator cuts the area into horizontal strips, called shelves. A shelf is
as tall as the first picture that opened it, and holds pictures side by side.
For each new picture, the allocator tries these steps, in this order:

1. A shelf that already holds pictures, at least as tall as the new picture
   and at most one and a half times as tall, with room for it. The shortest
   such shelf wins.
2. The topmost free strip that is at least as tall as the picture. The
   picture opens a new shelf exactly as tall as itself.
3. Any shelf that holds pictures, is at least as tall as the picture, and has
   room for it.
4. Otherwise the call is refused as `no_free_space`.

Pictures of similar heights pack best. A picture much shorter than its shelf
leaves the space under it empty until the shelf is empty again.

Given the same calls in the same order, on an atlas of the same size, the
allocator gives the same places on every system. That is a promise for one
version of glintfx, not for all of them.

### The allocator on its own

The allocator is also a public class of its own, `glintfx::gltfx_atlas_allocator`.
It needs no renderer and no graphics card. It fits any layout of rectangles,
such as glyphs or the frames of a sprite sheet. `open(width, height)` returns a
`gltfx_rslt` with the allocator. `allocate(width, height)` returns the slot of a
new rectangle. `release(slot)` takes one back, and `release_all()` takes back
every slot at once. `width()`, `height()` and `is_open()` tell you the size and
whether the allocator holds anything. The full list of refusals, in order, is
in [`atlas_allocator.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/atlas_allocator.hpp).

## Why the margin exists

When a picture is drawn larger or smaller than its texels, or with the
`linear` filter, a screen pixel at the picture's edge can read texels just
outside the picture. In an atlas, those texels belong to a neighbouring
picture. Without a margin, the edge takes on the neighbour's colour.

The margin fixes this. The texels outside the picture are copies of the
nearest edge texel, so the edge keeps its own colour. The margin is made of
copies, not of blank space.

With reduced levels, each smaller level averages more texels, so the margin
must reach further: at least 2^k texels for k levels. That is why the rule
grows with `mipmap_levels`. With no reduced levels, a margin of 1 is enough.

## Refusals

The refusals of `open()`, `add_image()`, `region_of()` and `release_slot()` are
each a `gltfx_err` (see [API Reference: Core](API-Core)). The `rejected_value()`
names the part that was wrong, as a short token. The drawing call
`draw_atlas_image()` is different: it returns nothing, and it refuses a piece
and counts the refusal in the frame report (see [step 4](#4-draw-the-picture)).

| Call | Code | Token | Meaning and what to do |
|---|---|---|---|
| `open` | `invalid_argument` | `width`, `height`, `filter`, `mipmap_levels`, `margin_pixels` | A descriptor value breaks one of the rules above. Change that value. |
| `open` | `unsupported` | `width` or `height` | The card cannot hold a texture of this size. Use a smaller atlas. |
| `open` | `out_of_memory` | `atlas_open` | The library ran out of memory. |
| `open` | `out_of_memory` or `platform_failure` | `texture_create` | The card could not make the texture. Use a smaller atlas, or free memory. |
| `add_image` | `invalid_argument` | `atlas` | The atlas was moved from, and holds nothing. |
| `add_image` | `invalid_argument` | `width` or `height` | The picture has no size, or its cell is bigger than the whole atlas. No release can make a cell that big fit, so use a bigger atlas. |
| `add_image` | `invalid_argument` | `pixel_format`, `bytes_per_row` or `pixels` | The picture's bytes or format are not valid. |
| `add_image` | `no_free_space` | `atlas_full` | No free place of that size is left now. Release pictures you no longer need, or open a second atlas. |
| `add_image` | `out_of_memory` | `atlas_add_image` | The library ran out of memory. |
| `add_image` | `out_of_memory` | `atlas_allocate` | The library ran out of memory, or no number is left for a new cell. All 4294967295 cell numbers are in use or retired, and a number is retired once its generation reaches 4294967295. Release cells, or open a second atlas. |
| `add_image` | the errors of writing a texture | as they come | The card refused the picture's texels. The errors are named in [`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp) under `update_texture()`. |
| `region_of`, `release_slot` | `invalid_argument` | `atlas` | The atlas was moved from, and holds nothing. |
| `region_of`, `release_slot` | `invalid_argument` | `stale_slot` | The slot was already released, or was never given out. Use the slot `add_image()` returned, until you release it. |
| `region_of`, `release_slot` | `invalid_argument` | `slot_other_allocator` | The slot belongs to another atlas. Use a slot of this atlas. |

A refused `open()` leaves nothing behind. A refused `add_image()` takes no
cell. A refusal that comes before `out_of_memory` with `atlas_add_image`
changes nothing. A refusal from that point on may leave the memory the library
keeps between calls grown. If the card refuses the picture's data after its
cell was taken, the cell is given back, and what the card holds there is not
defined. Do not draw from that place until another picture is written there.
The number of that cell may later be given out again, with a larger
generation, so an old slot is refused rather than reused.

## Lifetime and threads

- The atlas belongs to the renderer it was opened on. Do not use it after
  that renderer is gone.
- An atlas cannot be copied. The copy constructor and the copy assignment are
  deleted, so a copy does not compile. Move the atlas with `std::move`, as
  step 1 does. `gltfx_atlas_allocator` cannot be copied either.
- A moved-from atlas holds nothing. `is_open()` returns `false`, and
  `add_image()`, `region_of()` and `release_slot()` are refused.
- An atlas that receives another atlas by move assignment first gives its own
  cells and texture back. The slots it handed out before are then refused by
  `draw_atlas_image()`, as in [step 4](#4-draw-the-picture). A slot follows its
  cell through a move, so the atlas moved into can still use it.
- Use one atlas from one thread at a time. The renderer itself is used from
  one thread too (see [API Reference: Draw 2D](API-Draw-2D)).

## Cost

With no reduced levels, the library keeps no copy of the texels in its own
memory. With k reduced levels, it keeps one copy of every level. That is at
least `width` x `height` x 4 bytes, and less than twice that. `add_image()`
also keeps a working block for the largest cell it has handled so far. The
exact numbers are in the `COST` section of
[`atlas_allocator.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/atlas_allocator.hpp)
and of
[`texture_atlas.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/texture_atlas.hpp).

## What this page does not promise

- A speed, or a number of draw calls. The frame report counts how many times
  the bound texture changed (`texture_switches` in
  [`frame_2d_report.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/frame_2d_report.hpp)).
  That is a number to measure, not a promise.
- That places stay the same in a later version of glintfx.
- That the textured calls other than `draw_atlas_image()` take a slot. They
  take a region, from `region_of()`.
- Drawing from two threads at the same time.

## Related

- [API Reference: Draw 2D](API-Draw-2D): the filled shapes and the frame
  report.
- [API Reference: Core](API-Core): the error pattern every refusal uses.
- [Concepts](Concepts): the order in which the parts of a program are opened.

---

Last reviewed: 2026-11-20, against the public headers and the README as they
stood that day.
Owner: the glintfx maintainers. To report an inaccuracy, open an
[issue](https://github.com/petrinhu/GlintFx/issues).
