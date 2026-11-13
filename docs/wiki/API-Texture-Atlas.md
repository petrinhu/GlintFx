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

**Status, as of 2026-11-13: in progress.** The atlas is part of the texture
API, which is still under review before its public names are called stable.
The names on this page are the ones in the public headers today. They can
still change before that review ends. The
[README's Status section](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
is the dated list of what is finished, and it does not mention the texture
API yet. The [Home](Home) page lists this page under "In progress", not with
the finished areas.

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
- **Slot**: the receipt the allocator gives you when it places a picture. You
  keep it to find the picture later, or to give its place back.
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
- [`examples/textures/textures.cpp`](https://github.com/petrinhu/GlintFx/blob/main/examples/textures/textures.cpp):
  a program that draws the same white picture from a plain texture and from
  an atlas, side by side, so you can compare the two.

## A short example

This is the atlas code of the example, shortened. The `picture` is a
`gltfx_image_view` over your own pixel bytes. The example gets its image from
a small helper. The view's fields are in
[`image_view.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/image_view.hpp).

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

`add_image()` takes a `gltfx_image_view` and returns a slot. The atlas gives
the picture a **cell**: its own place, big enough for the picture and its
margin. Each side of a cell is rounded up to a multiple of 2^k, so every
cell starts on a multiple of 2^k.

The library reads your pixels only during the call. You may free your bytes
right after `add_image()` returns.

`add_image()` is not a flush. It writes the picture into the texture at once,
but the pieces you already submitted, and have not yet sent, read the texture
when they are sent. If pieces submitted earlier in the same frame must keep
the old contents, call `renderer.flush()` before `add_image()`.

### 3. Find the picture

`region_of(slot)` returns the rectangle of the picture itself, not of its
cell, in texels of `atlas.texture()`. Always ask `region_of()` for the place.
A later version of glintfx may place pictures differently, so do not copy
positions from one run into your code.

### 4. Draw the picture

A drawing call takes a **region**, never a slot. Pass `atlas.texture()` and a
region made from `region_of()`. The options for the tint and the mirror are in
[`texture_region.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/texture_region.hpp).
The drawing calls themselves are in
[`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp).
This wiki has no page for the textured drawing calls yet.

### 5. Give the place back

`release_slot(slot)` frees the cell. The texels stay in the texture until
another picture is written over them. After a release, do not draw from that
region. The place may now hold a different picture. A slot that was already
released is refused.

## How the allocator places pictures

The allocator cuts the area into horizontal strips, called shelves. A shelf is
as tall as the first picture that opened it, and holds pictures side by side.
A new picture goes onto a shelf that fits it, or opens a new shelf. If no
place is free, the call is refused as `no_free_space`.

Pictures of similar heights pack best. A picture much shorter than its shelf
leaves the space under it empty until the shelf is empty again.

Given the same calls in the same order, on an atlas of the same size, the
allocator gives the same places on every system. That is a promise for one
version of glintfx, not for all of them.

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

Every refusal is a `gltfx_err` (see [API Reference: Core](API-Core)). The
`rejected_value()` names the part that was wrong, as a short token.

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
| `add_image` | `out_of_memory` | `atlas_add_image` or `atlas_allocate` | The library ran out of memory. |
| `add_image` | the errors of writing a texture | as they come | The card refused the picture's texels. The errors are named in [`renderer_2d.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/draw2d/renderer_2d.hpp) under `update_texture()`. |
| `region_of`, `release_slot` | `invalid_argument` | `atlas` | The atlas was moved from, and holds nothing. |
| `region_of`, `release_slot` | `invalid_argument` | `stale_slot` | The slot was already released, or was never given out. Use the slot `add_image()` returned, until you release it. |
| `region_of`, `release_slot` | `invalid_argument` | `slot_other_allocator` | The slot belongs to another atlas. Use a slot of this atlas. |

A refused `open()` leaves nothing behind. A refused `add_image()` changes
nothing, with two exceptions. Memory the library keeps between calls may stay
grown. And if the card refuses the picture's data after its cell was taken,
the cell is given back, and what the card holds there is not defined. Do not
draw from that place until another picture is written there.

## Lifetime and threads

- The atlas belongs to the renderer it was opened on. Do not use it after
  that renderer is gone.
- A moved-from atlas holds nothing. `is_open()` returns `false`, and
  `add_image()`, `region_of()` and `release_slot()` are refused.
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
- That a drawing call takes a slot. It takes a region.
- Drawing from two threads at the same time.

## Related

- [API Reference: Draw 2D](API-Draw-2D): the filled shapes and the frame
  report.
- [API Reference: Core](API-Core): the error pattern every refusal uses.
- [Concepts](Concepts): the order in which the parts of a program are opened.

---

Last reviewed: 2026-11-13, against the public headers as they stood that day.
Owner: the glintfx maintainers. To report an inaccuracy, open an
[issue](https://github.com/petrinhu/GlintFx/issues).
