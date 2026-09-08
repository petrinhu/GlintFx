# Getting started

This page walks you from an empty folder to a running program that opens a
real window using glintfx, explaining every tool and every word along the
way. It assumes nothing except that you have a terminal (a text-based
window where you type commands) open.

If a word here is unfamiliar and this page does not explain it, check
[Concepts](Concepts) first.

## What you need before you start

| Tool | What it is, in plain language |
|---|---|
| **git** | A program that downloads ("clones") a copy of a project's source code and its full history onto your computer. |
| **A C++23 compiler** | The program that turns human-readable source code (`.cpp`/`.hpp` files) into a real, runnable program. glintfx needs a recent one: GCC 14 or newer on Linux, or Visual Studio 2022 on Windows. |
| **CMake**, version 4.1 or newer | A "build system generator": a tool that reads a project's own build instructions and produces the actual commands your compiler needs, so the project does not have to hand-write those commands for every operating system separately. |
| **Ninja** | The program that actually runs the compiler, many times, in the right order, once CMake has told it what to do. |

On Linux, you also need the Wayland development files (`libwayland-client`,
`wayland-protocols`) and an EGL/OpenGL development package - see the
"Requirements" line in the [main README](https://github.com/petrinhu/GlintFx#building-from-source-and-running-the-tests)
for the exact package names on your distribution, since those differ by
distro and that list is the one kept up to date, not this page.

## Step 1: get the source code

Open a terminal and run:

```bash
git clone https://github.com/petrinhu/GlintFx.git
cd GlintFx
```

The first command downloads the project into a new folder called `GlintFx`;
the second one moves your terminal into that folder, so every command
below runs from inside it.

## Step 2: configure the build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

This tells CMake: read the project in the current folder (`-S .`), write
everything it generates into a new folder called `build` (`-B build`), use
Ninja to actually compile (`-G Ninja`), and build an optimized ("Release")
version rather than one meant for debugging. Nothing is compiled yet - this
step only prepares the instructions.

## Step 3: compile it

```bash
cmake --build build
```

This is the step that actually turns the source code into a real library
file on your disk (`libglintfx.so` on Linux, `glintfx.dll` on Windows).
Depending on your computer, this can take anywhere from under a minute to
several minutes.

## Step 4: run the test suite

```bash
ctest --test-dir build --output-on-failure
```

glintfx ships its own automated tests: small programs that check that the
library actually behaves the way it promises to. Running them is the fast
way to confirm your own build is healthy before you try to use it for
anything - if every test passes, your copy of glintfx works correctly on
your machine.

**On Windows**, prepare the compiler environment first (open a "Developer
Command Prompt for VS 2022", or run `vcvarsall.bat x64`) before Step 2 -
see the main README's own building section for exactly why.

## Step 5: your first program

Everything above proves the library builds and works; it does not yet show
you glintfx *doing* anything. The two pieces that exist and are proven to
work today, on both Linux and Windows, are opening a **display** (the
connection to your screen and window manager) and opening a **window** on
top of it - see [Concepts](Concepts) for what those two words mean here.

Save this as `hello.cpp`, in a small separate project of your own that
links against the glintfx you just built (the main README's ["How to
consume glintfx"](https://github.com/petrinhu/GlintFx#how-to-consume-glintfx)
section, and [`PACKAGING.md`](https://github.com/petrinhu/GlintFx/blob/main/PACKAGING.md),
explain the two supported ways to do that):

```cpp
#include <glintfx/platform/window/display.hpp>
#include <glintfx/platform/window/window.hpp>

int main() {
    // Connect to the display server. This can fail (no display available,
    // for instance), so open() hands back either a working display or an
    // error - never a half-working one.
    auto display_result = glintfx::gltfx_display::open();
    if (!display_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_display display = std::move(display_result.value());

    // Describe the window we want, then ask for it.
    glintfx::gltfx_window_desc desc{};
    desc.title = "Hello, glintfx";
    desc.application_id = "com.example.hello";
    desc.logical_size = {800, 600};

    auto window_result = glintfx::gltfx_window::open(display, desc);
    if (!window_result.has_value()) {
        return 1;
    }
    glintfx::gltfx_window window = std::move(window_result.value());

    // Keep the window alive and responsive until the user closes it.
    // pump_events() is what lets the operating system tell the window
    // about things like "the user clicked the close button".
    while (!window.close_requested()) {
        display.pump_events();
    }

    return 0;
}
```

This program opens a window and keeps it open until you close it - it does
not draw anything inside the window yet, because 2D drawing is not built
yet (see the README's "Status" section for the current, dated list of what
exists). `has_value()`/`value()` is the pattern every fallible glintfx
function uses to report success or failure without exceptions; the full
rules are in [`docs/api-conventions.md`](https://github.com/petrinhu/GlintFx/blob/main/docs/api-conventions.md).

## If something does not work

Compare what you measured (the exact error message, the exact command you
ran) against the [main README](https://github.com/petrinhu/GlintFx#readme)
and [`TODO.md`](https://github.com/petrinhu/GlintFx/blob/main/TODO.md) for
known, currently-open issues, before assuming your setup is at fault. If
you believe you found a new problem, open an issue on the
[GitHub repository](https://github.com/petrinhu/GlintFx/issues) describing
exactly what you ran and what happened.
