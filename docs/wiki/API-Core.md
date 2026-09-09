# API reference: Core

The **core** module (`include/glintfx/core/`) has no dependency on any
operating system - no window, no file, no network. It is the vocabulary
every other part of glintfx is written in terms of: error handling, the
library's own version, 2D math, color, and time.

This page describes the **effect** of each public name - what happens
when you call it, from the caller's side. For *why* it is shaped this
way, the header itself is the source of truth; every function below
links to its file so you can read the full reasoning there. Signatures
here are simplified for reading; copy the exact one from the header
before compiling against it.

## Error handling

**Read this section first.** Every fallible function in glintfx -
everywhere, not just in `core` - returns one thing: `gltfx_rslt<T>` (or
`gltfx_rslt<void>` when there is no value to return on success). There is
no second form: no bare error code, no out-parameter, no exception
crossing a public function. [`err.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/err.hpp)

| Name | Effect |
|---|---|
| `rslt.has_value()` | `true` if the call succeeded. Always check this (or `has_error()`) before reading further. |
| `rslt.has_error()` | `true` if the call failed. |
| `rslt.value()` | The result. **Only valid when `has_value()` is true** - calling it otherwise is undefined behavior (a debug build aborts with a message naming the mistake; a release build does not check). |
| `rslt.err()` | The `gltfx_err` describing the failure. Only valid when `has_error()` is true, same rule as `value()`. |

**Example**, the pattern every glintfx call follows:

```cpp
auto result = glintfx::gltfx_display::open(); // any fallible call
if (!result.has_value()) {
    // result.err() is a glintfx::gltfx_err - see below
    return 1;
}
auto display = std::move(result.value());
```

`gltfx_err` ([`err.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/err.hpp)) is what a failed call hands back:

| Name | Effect |
|---|---|
| `err.code()` | Which kind of failure this is - a `gltfx_err_code` value (see the table below). |
| `err.path()`, `err.line()`, `err.column()`, `err.byte_offset()`, `err.rejected_value()`, `err.os_error_code()` | Extra detail, when the failing call chose to attach it. Reads back empty (for text) or zero (for numbers) when not set - never undefined behavior, so it is always safe to call these. |

`gltfx_err_code` ([`err_code.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/err_code.hpp)) - the seven kinds of failure glintfx reports today:

| Value | Meaning |
|---|---|
| `out_of_memory` | An allocation failed. |
| `io_failure` | Reading or writing something failed for a reason other than the two below. |
| `not_found` | Whatever was asked for (a file, an entry) does not exist. |
| `invalid_argument` | An argument you passed does not satisfy what the function requires. |
| `parse_failure` | Text or data being read is not in the expected shape. |
| `unsupported` | This system, or this build, does not support what was asked for. |
| `platform_failure` | The operating system reported a failure this list does not name more specifically. |

`gltfx_err_code_name(code)` returns the identifier above as text (e.g.
`"not_found"`) - stable, safe to log or match on.

`gltfx_err_fields(err)` ([`err_format.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/err_format.hpp))
turns a `gltfx_err` into a list of `(name, value)` text pairs - `code`
always first, then whichever diagnostic fields above were actually
attached. **glintfx never ships a pre-written error sentence, in any
language**: this function gives you the raw facts so you can build your
own message, in your own language and format.

```cpp
for (const auto &field : glintfx::gltfx_err_fields(err)) {
    std::cerr << field.name << "=" << field.value << " ";
}
// e.g.: code=not_found path=scene.rcss
```

## Version

[`version.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/version.hpp) -
the version of glintfx your program was actually built or linked
against, readable at runtime (see the wiki's [Pinning a Version](Pinning-a-Version)
page for why that matters).

| Name | Effect |
|---|---|
| `glintfx::runtime_version()` | Returns a `version{major_version, minor_version, patch_version, tweak_version}` value. |
| `glintfx::version_string()` | The same, already formatted as text. |

## 2D math and geometry

glintfx keeps two separate families of type for position and size, and
they do not implicitly convert into each other:

| Family | Precision | Used for |
|---|---|---|
| **World** (`gltfx_vec2_world`, `gltfx_rect_world`, `gltfx_transform`) | `double` | What you author and store - a large map keeps exact coordinates. |
| **Screen** (`gltfx_vec2_screen`, `gltfx_rect_screen`, `gltfx_mat3`) | `float` | What actually reaches the graphics card, built fresh once per frame. |

See [Concepts](Concepts) if "why two types for the same thing" is not
yet obvious - the short version is that OpenGL does not accept double
precision, so the narrowing has to happen somewhere, and glintfx makes
it an explicit, named step instead of a silent one.

[`vec2.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/vec2.hpp):

| Name | Effect |
|---|---|
| `gltfx_vec2_world{x, y}` | A position or size in world space (two `double`s). |
| `gltfx_vec2_screen{x, y}` | A position or size in screen space (two `float`s). |
| `gltfx_vec2_world_to_screen(v)` | Converts world to screen. Can round on very large coordinates (numbers above about 16.7 million lose exactness). |
| `gltfx_vec2_screen_to_world(v)` | Converts screen to world. Never loses anything - every `float` fits exactly in a `double`. |

[`angle.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/angle.hpp):

| Name | Effect |
|---|---|
| `gltfx_angle{radians}` | An angle. Always stored in radians; a plain number cannot be passed where an angle is expected (mixing up degrees and radians is a classic, silent bug this type makes impossible to compile). |
| `gltfx_angle_from_degrees(d)` | Builds an angle from a degree measure. No range limit - 720 degrees stays 720 degrees, it is not folded into a single turn. |
| `gltfx_angle_to_degrees(a)` | Reads an angle back as degrees. |

[`transform.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/transform.hpp):

| Name | Effect |
|---|---|
| `gltfx_transform{translation, rotation, scale}` | Where something sits, how it is turned, and how big it is - all world-space. Applied in this fixed order: scale, then rotation, then translation. |

[`mat3.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/mat3.hpp):

| Name | Effect |
|---|---|
| `gltfx_mat3{column_major}` | The 3x3 matrix format OpenGL itself expects (nine `float`s, column by column). |
| `gltfx_mat3_from_transform(t)` | Builds that matrix from a world-space transform. Call this **once per frame**, not once per object. |

[`rect.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/rect.hpp):

| Name | Effect |
|---|---|
| `gltfx_rect_world{corner, size}` / `gltfx_rect_screen{corner, size}` | A rectangle: a corner plus a size (never two opposite corners). A negative size means an empty rectangle, not an "inverted" one. |

[`interpolate.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/interpolate.hpp):

| Name | Effect |
|---|---|
| `gltfx_lerp(a, b, t)` | Linear interpolation from `a` to `b`. Exact at both ends (`t=0` gives exactly `a`, `t=1` gives exactly `b`). Any non-finite input (infinity, NaN) gives NaN back, on purpose - it never fabricates a plausible-looking wrong answer. |

**Example**, a moving, rotating object's matrix for this frame:

```cpp
glintfx::gltfx_transform t{
    .translation = {.x = 100.0, .y = 50.0},
    .rotation = glintfx::gltfx_angle_from_degrees(90.0),
    .scale = {.x = 1.0, .y = 1.0},
};
glintfx::gltfx_mat3 frame_matrix = glintfx::gltfx_mat3_from_transform(t);
```

## Color

[`color.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/color.hpp) -
glintfx's canonical color is **linear light**, not what a monitor
displays - see [Concepts](Concepts) if that sounds surprising.

| Name | Effect |
|---|---|
| `gltfx_rgba{red, green, blue, alpha}` | The canonical color: four `float`s, linear light, straight (never premultiplied) alpha. Values above 1.0 are valid - they represent light brighter than white. |
| `gltfx_rgba8{red, green, blue, alpha}` | The display's 8-bit-per-channel format - what a texture byte or a `#rrggbb` value actually is. Only used at the boundary with the outside world. |
| `gltfx_rgba_from_srgb8(encoded)` | Converts a display-encoded `gltfx_rgba8` into the canonical linear `gltfx_rgba`. |
| `gltfx_rgba_to_srgb8(color)` | The inverse. Out-of-range input is clamped, not rejected. |
| `gltfx_rgba_premultiplied(color)` | The transient premultiplied form (`red`/`green`/`blue` each multiplied by `alpha`) that a correct interpolation needs. |

## Time

[`time.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/time.hpp):

| Name | Effect |
|---|---|
| `gltfx_now()` | One reading from glintfx's own monotonic clock, as a `gltfx_time_point`. Only meaningful compared against another reading from this same function. |
| `gltfx_duration_between(earlier, later)` | The elapsed `gltfx_duration` (an exact nanosecond count) between two `gltfx_now()` readings. |
| `gltfx_duration_to_seconds(d)` | The same span, as a convenient (but rounding) `double` of seconds. |
| `gltfx_duration_from_seconds(s)` | The inverse - build a duration from a seconds value you authored yourself. |

[`fixed_step.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/fixed_step.hpp) -
an optional helper for physics or any simulation that needs to run at a
fixed rate, instead of the variable time a frame actually took:

| Name | Effect |
|---|---|
| `gltfx_fixed_step{dt, max_steps}` | Describes the fixed step size you want, and an optional cap on how many steps to report at once (protects against a runaway "spiral of death" after a stall). |
| `gltfx_fixed_step_accumulate(step, elapsed)` | Feed it how much wall time just passed; it tells you how many `dt`-sized steps to run right now, plus a leftover fraction (`alpha`) for smoothly interpolating what is drawn between steps. |

## See also

- [Getting Started](Getting-Started) - if you have not built glintfx yet.
- [API Reference: Window and Display](API-Window-and-Display) - the next
  thing most consumers reach for, and the one place these math types
  and the error-handling pattern above are put to use together.
