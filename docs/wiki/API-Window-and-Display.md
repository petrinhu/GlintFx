# API reference: Window and Display

This is the area a consumer touches first: it is the only part of
glintfx that is complete, tested, and proven to behave the same way on
Linux and Windows today (see [`docs/window-portability-matrix.md`](https://github.com/petrinhu/GlintFx/blob/main/docs/window-portability-matrix.md)
for the exact, row-by-row proof). It lets you connect to the display
server and open a real, visible window - nothing is drawn inside it yet
(see the main README's ["Status"](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
section for what comes next).

Before reading this page, read [API Reference: Core](API-Core)'s "Error
handling" section - every fallible function below returns a
`gltfx_rslt<T>`, and this page assumes you already know how to read one.

## Display

[`display.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/platform/window/display.hpp) -
your program's one connection to the display server (Wayland on Linux,
the equivalent on Windows). You need exactly one of these before you can
open a window.

| Name | Effect |
|---|---|
| `gltfx_display::open()` | Connects to the display server. Returns a working `gltfx_display` on success, or a `gltfx_err` on failure - never a half-open connection. |
| `display.is_open()` | Whether the connection is currently open. |
| `display.pump_events()` | Lets the display server tell your program about anything that happened (a window being resized, the user requesting to close it, and so on). Call this once per frame - it never blocks waiting for something to happen. |
| *(destructor)* | Closing happens automatically when a `gltfx_display` goes out of scope - there is no separate `close()` to remember to call. |

**A `gltfx_display` must outlive every `gltfx_window` opened from it** -
destroying the display first is undefined behavior.

## Window

[`window.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/platform/window/window.hpp) -
a real, visible window, opened against an already-open display.

| Name | Effect |
|---|---|
| `gltfx_window::open(display, desc)` | Opens a window described by a `gltfx_window_desc` (below), against `display`. Returns the open window or a `gltfx_err`. |
| `window.is_open()` | Whether the window is currently open. |
| `window.logical_size()` | The window's size in the window system's own units - a `gltfx_window_size{width, height}`. |
| `window.pixel_size()` | The window's size in actual framebuffer pixels - the number your renderer needs. **This differs from `logical_size()` on a HiDPI display** - always read this one when deciding how many pixels to draw into, never the logical size. |
| `window.state(bit)` | Whether one of `gltfx_window_state_bit::{active, maximized, fullscreen, suspended}` is currently true. |
| `window.close_requested()` | `true` once the user has asked to close the window (clicked the close button, for instance). This never resets back to `false` - check it once per frame and end your loop when it turns true. |
| `window.set_title(title)` | Changes the window's title after it is already open. An empty title is valid, and clears it. |

`gltfx_window_desc` - what you fill in before calling `open()`:

| Field | Effect |
|---|---|
| `title` | The window's initial title text. |
| `application_id` | An identifier for your application (used by the desktop environment - a taskbar icon lookup key, for instance). |
| `logical_size` | The window's starting size, as a `gltfx_window_size{width, height}`. **Both dimensions must be non-zero** - `open()` refuses a zero width or height before either platform's own window system ever sees the request. |

## Minimal working example

The shortest path from nothing to a real, visible window that stays open
until the user closes it - the same walkthrough as the wiki's [Getting
Started](Getting-Started) page, repeated here because a reference page
should not send you away empty-handed. If you change one, change the
other.

```cpp
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

int main() {
    auto display_result = glintfx::gltfx_display::open();
    if (!display_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_display display = std::move(display_result.value());

    glintfx::gltfx_window_desc desc{};
    desc.title = "Hello, glintfx";
    desc.application_id = "com.example.hello";
    desc.logical_size = {800, 600};

    auto window_result = glintfx::gltfx_window::open(display, desc);
    if (!window_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_window window = std::move(window_result.value());

    while (!window.close_requested()) {
        if (!display.pump_events().has_value()) {
            break;
        }
    }

    return 0;
}
```

## What is not here yet

Nothing in this window is drawn on - that needs a graphics context (see
[`docs/gl-loop-portability-matrix.md`](https://github.com/petrinhu/GlintFx/blob/main/docs/gl-loop-portability-matrix.md)
for exactly how far that part has landed), and there is no keyboard,
mouse or gamepad input delivery yet. Do not write code that assumes
either exists.

## See also

- [API Reference: Core](API-Core) - the error-handling pattern and the
  math types used throughout this page.
- [Getting Started](Getting-Started) - the full build-from-source
  walkthrough this example is drawn from.
