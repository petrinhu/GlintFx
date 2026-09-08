# API reference: Graphics Context

An OpenGL 3.3 core graphics context, opened on top of an already-open
window - the piece that lets a program actually draw. Same public API on
Linux and Windows, no platform-specific code needed at the call site.

Before reading this page, read [API Reference: Core](API-Core)'s "Error
handling" section and [API Reference: Window and Display](API-Window-and-Display) -
a graphics context is opened over a `gltfx_window`, and every fallible
call below returns the same `gltfx_rslt<T>` those pages already explain.

## Graphics context

[`context.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/platform/gl/context.hpp):

| Name | Effect |
|---|---|
| `gltfx_gl_context::open(window, desc)` | Opens an OpenGL 3.3 core context over an already-open `window`. Returns the open context or a `gltfx_err`. `window` must outlive the context. |
| `context.is_open()` | Whether the context is currently open. |
| `context.make_current()` | Makes this context the one the calling thread draws into. A context is single-threaded: do not call this from more than one thread over the context's lifetime. |
| `context.swap_buffers()` | Presents whatever was drawn. Returns a `gltfx_present_outcome` (below) - never blocks indefinitely, even when the window cannot currently be repainted. |
| `context.proc_address(name)` | Resolves an OpenGL function's address by name, the way you load any GL entry point beyond the fixed set this library declares by hand. Returns `nullptr` when this driver does not have that function - this is an ordinary lookup, not a `gltfx_err`. |
| `context.set_option(entry)` | Changes a `live` option (see "Options" below) after the context is already open. |
| `context.option(id)` | Reads back the current value of any option this context knows about. |
| `context.option_support(id)` | Whether *this* system actually honors option `id` right now - distinct from whether the library knows about the option in the abstract (`gltfx_gfx_option_describe()`, below). |
| `context.gpu()` | Which GPU this context is rendering on, as far as the system has told the library - see "GPU information" below. |

`gltfx_gl_context_desc` - what you pass to `open()`:

| Field | Effect |
|---|---|
| `options` | Pointer to an array of `gltfx_gfx_option_entry` (below) - the options you want set at open time. May be `nullptr` only when `option_count` is `0`. |
| `option_count` | How many entries `options` points to. |

`gltfx_present_outcome` - what `swap_buffers()` returns on success:

| Value | Meaning |
|---|---|
| `presented` | The frame actually reached the screen. |
| `skipped_hidden` | The window is not currently being repainted (minimized, or a compositor withholding its frame budget) - nothing was drawn, and this is not an error. |

**A second context opened over the same window must ask for the same
`open_only` options the first one got** (see "Options" below for what
`open_only` means) - a later, disagreeing request is refused with
`invalid_argument`, naming the option.

## Options

[`gfx_option.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/platform/gl/gfx_option.hpp) -
every knob a graphics context exposes is a row of one table, so new
options can be added later without changing `gltfx_gl_context_desc`'s
own shape.

`gltfx_gfx_option` - the options this build knows about today:

| Option | What it controls |
|---|---|
| `vsync` | Whether presenting waits for the display's own refresh. |
| `frame_rate_cap` | An upper bound on how often the context presents, in frames per second. |
| `gpu_preference` | Which GPU to prefer on a multi-GPU system. **Only `no_preference` is supported today** - this library does not yet let you actually choose a GPU. |
| `msaa_samples` | Multisample anti-aliasing sample count. |
| `srgb_framebuffer` | Whether the framebuffer is sRGB-encoded. |
| `preset` | A named bundle of the above, when the library picks one automatically. |
| `auto_choice_reason` | Why the library picked what it picked, when `preset` did the choosing. |
| `power_source` | Whether the machine is on battery or mains power, where the system reports it. |

Each option has a **kind** (`gltfx_gfx_option_kind`: `toggle` reads
0/1, `integer` reads a plain number, `choice` reads a value's numeric
id - never floating point) and a **when** (`gltfx_gfx_option_when`:
`open_only` - set only in the list passed to `open()`, fixed for the
context's life; `live` - changeable any time via `set_option()`;
`read_only` - written by the library itself, never by you).

| Name | Effect |
|---|---|
| `gltfx_gfx_option_entry{id, value}` | One `(option, value)` pair - what every options list and `set_option()` call is made of. |
| `gltfx_gfx_option_info{id, name, kind, when, min_value, max_value, default_value}` | The static facts about one option, known without any open context. |
| `gltfx_gfx_option_count()` | How many options this build's table has. |
| `gltfx_gfx_option_at(index)` | The option at `index` (`0` to `gltfx_gfx_option_count() - 1`). Out of range degrades to an empty `gltfx_gfx_option_info`, never undefined behavior. |
| `gltfx_gfx_option_describe(id)` | The static facts about `id`, with no system in the picture - what the *library* knows, not what a real driver actually supports (that is `context.option_support(id)`, above). |
| `gltfx_gfx_option_by_name(name)` | Looks up an option's id by its saved-settings-file name (e.g. `"vsync"`). Fails with `not_found` for an unrecognized name - useful for reading a consumer's own settings file back. |

## GPU information

[`gpu.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/platform/gl/gpu.hpp) -
glintfx *reports* which GPU is in use; it does not yet let you *choose*
one (see `gpu_preference` above).

| Name | Effect |
|---|---|
| `gltfx_gpu_kind` | `unknown`, `software`, `shared` (integrated), or `dedicated`. `unknown` is the honest default when the system never said - never a guess. |
| `gltfx_gpu_info{kind, name, enumeration_index}` | What `context.gpu()` returns: the kind, the driver's own name string (for a support log, never parsed for logic), and this GPU's position in a `gltfx_gpu_enumeration` query, if one was made. |
| `gltfx_gpu_enumeration::query()` | Lists every GPU the system reports, deduplicated. Fails with `unsupported` when this platform has no enumeration mechanism, rather than returning an empty list. |
| `enumeration.count()` | How many GPUs `query()` found. |
| `enumeration.at(index)` | The GPU at `index`. Out of range degrades to a default `gltfx_gpu_info`, never undefined behavior. |

## Minimal working example

Opens a display and window (see [API Reference: Window and Display](API-Window-and-Display)),
opens a graphics context with vsync on, clears the screen to a solid
color once, and presents it:

```cpp
#include <utility>

#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

int main() {
    auto display_result = glintfx::gltfx_display::open();
    if (!display_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_display display = std::move(display_result.value());

    glintfx::gltfx_window_desc window_desc{};
    window_desc.title = "Hello, glintfx GL";
    window_desc.application_id = "com.example.hello_gl";
    window_desc.logical_size = {800, 600};

    auto window_result = glintfx::gltfx_window::open(display, window_desc);
    if (!window_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_window window = std::move(window_result.value());

    glintfx::gltfx_gfx_option_entry vsync_on{.id = glintfx::gltfx_gfx_option::vsync, .value = 1};
    glintfx::gltfx_gl_context_desc context_desc{.options = &vsync_on, .option_count = 1};

    auto context_result = glintfx::gltfx_gl_context::open(window, context_desc);
    if (!context_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_gl_context context = std::move(context_result.value());

    if (!context.make_current().has_value()) {
        return 1;
    }

    // A GL function beyond what this library declares by hand is
    // resolved through proc_address(), never linked directly.
    using gl_clear_color_fn = void (*)(float, float, float, float);
    using gl_clear_fn = void (*)(unsigned int);
    auto *gl_clear_color =
        reinterpret_cast<gl_clear_color_fn>(context.proc_address("glClearColor"));
    auto *gl_clear = reinterpret_cast<gl_clear_fn>(context.proc_address("glClear"));
    if (gl_clear_color == nullptr || gl_clear == nullptr) {
        return 1;
    }

    constexpr unsigned int k_gl_color_buffer_bit = 0x00004000;
    gl_clear_color(0.1F, 0.1F, 0.1F, 1.0F);
    gl_clear(k_gl_color_buffer_bit);

    auto present_result = context.swap_buffers();
    return present_result.has_value() ? 0 : 1;
}
```

## What is not decided yet

- **You cannot choose a GPU.** `gpu_preference` only accepts
  `no_preference` today - `context.gpu()` and `gltfx_gpu_enumeration`
  let you *see* what is running, not pick it.
- **Whether a given option is actually honored is a per-system
  question**, answered by `context.option_support(id)` on a real, open
  context - `gltfx_gfx_option_describe(id)` only tells you what the
  library's table says in the abstract, not what your machine's driver
  does with it.
- **A context is single-threaded.** Calling `make_current()` from more
  than one thread over its lifetime is not supported.

## See also

- [API Reference: Window and Display](API-Window-and-Display) - opening
  the window a graphics context is built on top of.
- [API Reference: Core](API-Core) - the error-handling pattern used
  throughout this page.
