# API reference: Main Loop

The **main loop** is the piece every consumer of glintfx organizes their own
frame around: it pumps the operating system's events, measures how much
time really passed, tells you whether this tick should draw, presents at
whatever cadence the display can sustain, honors a frame-rate cap if you
asked for one, and sleeps instead of spinning the CPU while the window is
not visible. Same public API on Linux and Windows.

**If you pinned a version older than `v0.6.1.0`:** from `v0.4.0.0` through `v0.6.0.0`, on Linux (Wayland), a window driven by this loop shows its first frame and then never shows another one. Windows was not affected. `v0.6.1.0` fixes it and changes nothing else; see the `[0.6.1.0]` section of [`CHANGELOG.md`](https://github.com/petrinhu/GlintFx/blob/main/CHANGELOG.md), and [Pinning a Version](Pinning-a-Version) for how to move to it.

Before reading this page, read [API Reference: Core](API-Core)'s "Error
handling" section, [API Reference: Window and Display](API-Window-and-Display)
and [API Reference: Graphics Context](API-Graphics-Context) - a loop is
opened over an already-open display, window and graphics context, and every
fallible call below returns the same `gltfx_rslt<T>` those pages already
explain.

## Two ways to use it

[`loop.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/platform/loop/loop.hpp):

| Style | How |
|---|---|
| **Let the loop drive** | Hand `run()` a small table of callbacks (below); it calls them itself, once per tick, until the window is asked to close or your own code says to stop. This is the shortest path for a program whose "loop" is the whole program. |
| **Drive it yourself** | Call `step()` to get one tick, then decide yourself whether to draw and call `present()`. Every guarantee below still applies; useful when you already have your own loop shape to fit into (an existing game engine embedding glintfx, for instance). |

## Opening a loop

| Name | Effect |
|---|---|
| `gltfx_loop::open(display, window, context)` | Opens a loop over an already-open display, window and graphics context. `window` must have been opened from `display`, and `context` from `window`, the same chain those two `open()` calls already require one level up. Returns the open loop or a `gltfx_err` naming which of the three (`"display"`, `"window"`) was not open. |
| `loop.is_open()` | Whether the loop is currently open. |
| *(destructor)* | Closes automatically when a `gltfx_loop` goes out of scope. Never closes the display, window or context themselves - a loop never owns them, and all three must outlive it. |

## One tick: `gltfx_frame_tick`

What `step()` (or one iteration of `run()`) hands back - what one tick of
your program actually looked like:

| Field | Effect |
|---|---|
| `elapsed` | Wall time since the previous tick. Never negative (a clock that appears to go backward reports zero), and clamped to a fixed ceiling (`k_gltfx_max_frame_elapsed`, 250 milliseconds) so that a machine suspend/resume or a debugger pause never hands your own game logic or physics a multi-second, "spiral of death" jump. Zero on the very first tick. |
| `now` | The clock reading that closed this tick. |
| `frame_index` | `1` on the first tick, `+1` every tick after that, with no gap - keeps counting even while the window is hidden, independent of `should_render` below. |
| `should_render` | Whether **this** tick should draw (`on_render` below, then present). Becomes `false` only while the window stays hidden; the moment it is shown again, this returns to `true` with nothing your own code has to do. |
| `last_present` | What the last `present()` call returned: `presented` (it reached the screen) or `skipped_hidden` (the window was not being repainted, which is not an error). |

## Handing the loop your callbacks

`gltfx_loop_callbacks` is five plain values, never `std::function` - the
compiler refuses, at your own assignment line, any callback that is not
declared `noexcept` (see [`docs/api-conventions.md`](https://github.com/petrinhu/GlintFx/blob/main/docs/api-conventions.md)
R10 for the full reasoning and what it does and does not protect you from):

| Field | Effect |
|---|---|
| `context` | An opaque pointer you own; handed back byte-identical to every callback below, never read or touched by glintfx itself. `nullptr` is legal if your callbacks need no state. |
| `on_frame` | Runs once per tick, before `on_render`. Return `false` to end the loop cleanly. **Required** - `run()` refuses a call with this empty. |
| `on_render` | Runs once per tick, only when `should_render` is `true`, right before presenting. **Required** - `run()` refuses a call with this empty. |
| `on_event` | Reserved for keyboard/mouse/gamepad input delivery, not built yet (see "What this page does not cover" below). **Must stay `nullptr` today** - `run()` refuses a call that fills this in, rather than accepting it and silently never calling it. |
| `destroy_context` | `nullptr` means `context` is **borrowed**: you keep owning it, and must keep it alive for as long as the loop can still call through it (see the table below for exactly how long that is). A non-null function means ownership is **handed to the loop**: it destroys `context` through that function, exactly once, once it is done with it. |

**Two different lifetimes for a handed-over `context`, chosen by which call
you make, not by a sixth field:**

| | `run(callbacks)` | `set_callbacks(callbacks)`, then `run()` |
|---|---|---|
| `context` lives until | that one `run(callbacks)` call returns, on **every** path out (including a refusal) | it is **replaced** by a later, successful `set_callbacks()`, or the loop itself is destroyed - never earlier |
| Run the same callbacks again? | No - a handed-over context is gone once the call returns; call again with a fresh (or still-borrowed) one | Yes, as many `run()` calls as you like in between |

Once a `context` is handed over, never touch it again yourself, from the
instant the call is made, whether or not it succeeds.

## Running

| Name | Effect |
|---|---|
| `loop.run(callbacks)` | Runs `callbacks` until the system asks to close the window, or `on_frame` returns `false`. Order each tick: `step()`, then `on_frame`, then, only if `should_render`, `on_render` followed by presenting. The tick that received the close request is still delivered in full before `run()` returns. Refuses (by name, `invalid_argument`) an empty `on_frame` (`"on_frame"`), an empty `on_render` (`"on_render"`), a filled-in `on_event` (`"on_event"`), a `destroy_context` set with no `context` (`"destroy_context"`), and a call made from **inside** your own `on_frame`/`on_render` (`"running"`). |
| `loop.set_callbacks(callbacks)` | Stores `callbacks` for this loop, replacing whatever was stored before, validated the same way `run(callbacks)` validates its own argument. Refused (`"running"`) if called from inside your own `on_frame`/`on_render`. |
| `loop.run()` | Runs whatever `set_callbacks()` last stored. Refused (`"callbacks"`) if nothing was ever stored. Otherwise identical to `run(callbacks)` above. |
| `loop.step()` | One tick: pumps the system's events, waits only when the window is hidden or a frame-rate cap says to, pumps again, reads the clock, returns the `gltfx_frame_tick`. Never calls `on_frame`/`on_render`/present itself - that is what `run()` is for. |
| `loop.present()` | Presents the current frame **through this loop** - the only way to present that feeds the next tick's own `last_present`/`should_render`. Calling the graphics context's `swap_buffers()` directly instead still works (see [API Reference: Graphics Context](API-Graphics-Context)), but the loop never learns about it. |

## Frame-rate cap

Set the `frame_rate_cap` option on the graphics context (see [API Reference:
Graphics Context](API-Graphics-Context)'s "Options" section) and the loop
honors it: it never presents more than that many times per second, measured
by a fixed deadline rather than by adding up how long each frame actually
took (so a few slow frames in a row do not make the next ones burst to
catch up). Changing the cap while the loop is running takes effect on the
very next tick, and it coexists with vsync: whichever of the two is slower
wins.

## What this page does not cover, stated so it is not assumed

- **No keyboard, mouse or gamepad input.** `on_event` above exists in the
  struct's layout already, but `run()` refuses it if you fill it in - this
  library does not yet promise to ever call it.
- **No automatic fixed-step simulation.** `elapsed` above is fine for
  animation or logic with no physics; anything that needs a fixed step runs
  through [`core/fixed_step.hpp`](https://github.com/petrinhu/GlintFx/blob/main/include/glintfx/core/fixed_step.hpp)'s
  own `gltfx_fixed_step`, which the loop never calls on your behalf.
- **Single-threaded.** The same restriction `gltfx_gl_context::make_current()`
  already documents.

## What is proven live, and what is only declared

Every promise above is built and tested without needing a real display,
window or graphics context first. On top of that, the identical tests run
through the public API alone, against a real display, window and graphics
context, on both Linux (inside an isolated container, against a real
compositor) and Windows - as of 2026-09-15, in [CI run `34975391524`](https://github.com/petrinhu/GlintFx/actions/runs/34975391524),
over commit `14427bc`, which the `v0.4.0.0` tag marks; all jobs green including
every Windows job and the Linux/Windows parity job. That run predates the
Linux frame fix of release `v0.6.1.0`.

**Proven live on both systems as of 2026-09-15, before the `v0.6.1.0` fix:** ticks arrive in an unbroken sequence
starting at 1; the very first tick reports zero elapsed time; a minimized
window is not drawn into while it stays hidden; `run()` ends cleanly the
moment your own `on_frame` says to; a moved-from loop handle is refused by
name.

**Proven live on Windows only, so far: recovering after a minimized window
is restored** - `SW_RESTORE` genuinely restores a minimized window with no
compositor to negotiate with, and the loop was measured drawing again
right afterward. This is the first time this project has measured that
recovery anywhere. On Linux, the isolated compositor this project's test
container uses never restores a minimized window at all, so recovery stays
structurally undemonstrated there - a limit of what that test environment
can provoke, not a gap left unaddressed.

**Still undemonstrated on either system:** occlusion by another window, and
focus loss - nothing in this project's test setup provokes either today.
The full, row-by-row account of every promise above, including exactly
which test proves each one and any caveat attached to it, is
[`docs/gl-loop-portability-matrix.md`](https://github.com/petrinhu/GlintFx/blob/main/docs/gl-loop-portability-matrix.md) -
read that table, not this paragraph, before depending on any one of them.

## Minimal working example

Opens a display, window and graphics context (see the two previous API
pages), then hands the loop a pair of callbacks that clear the screen to a
solid color every frame, for 120 frames, using no `context` pointer at all
(`nullptr` is a legal, empty one):

```cpp
#include <utility>

#include <glintfx/platform/gl/context.hpp>
#include <glintfx/platform/gl/gfx_option.hpp>
#include <glintfx/platform/loop/loop.hpp>
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

namespace {

using gl_clear_color_fn = void (*)(float, float, float, float);
using gl_clear_fn = void (*)(unsigned int);
gl_clear_color_fn g_gl_clear_color = nullptr;
gl_clear_fn g_gl_clear = nullptr;

bool on_frame(void * /*context*/, const glintfx::gltfx_frame_tick &tick) noexcept {
    // Stop after 120 ticks; a real program would check its own game state
    // instead (or nothing at all, letting the window's own close button
    // be the only way out).
    return tick.frame_index < 120;
}

void on_render(void * /*context*/, const glintfx::gltfx_frame_tick & /*tick*/) noexcept {
    constexpr unsigned int k_gl_color_buffer_bit = 0x00004000;
    g_gl_clear_color(0.1F, 0.1F, 0.1F, 1.0F);
    g_gl_clear(k_gl_color_buffer_bit);
}

} // namespace

int main() {
    auto display_result = glintfx::gltfx_display::open();
    if (!display_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_display display = std::move(display_result.value());

    glintfx::gltfx_window_desc window_desc{};
    window_desc.title = "Hello, glintfx loop";
    window_desc.application_id = "com.example.hello_loop";
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
    g_gl_clear_color =
        reinterpret_cast<gl_clear_color_fn>(context.proc_address("glClearColor"));
    g_gl_clear = reinterpret_cast<gl_clear_fn>(context.proc_address("glClear"));
    if (g_gl_clear_color == nullptr || g_gl_clear == nullptr) {
        return 1;
    }

    auto loop_result = glintfx::gltfx_loop::open(display, window, context);
    if (!loop_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_loop loop = std::move(loop_result.value());

    glintfx::gltfx_loop_callbacks callbacks{};
    callbacks.on_frame = on_frame;
    callbacks.on_render = on_render;
    // context left nullptr, destroy_context left nullptr: no state to own.

    auto run_result = loop.run(callbacks);
    return run_result.has_value() ? 0 : 1;
}
```

## See also

- [API Reference: Graphics Context](API-Graphics-Context) - opening the
  context a loop presents through, and the `frame_rate_cap`/`vsync` options
  this page's "Frame-rate cap" section refers to.
- [API Reference: Window and Display](API-Window-and-Display) - opening the
  display and window a loop is built on top of.
- [Getting Started](Getting-Started) - the full build-from-source
  walkthrough, extended to a program that opens a window, runs a loop and
  draws something.
