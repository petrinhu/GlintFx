# Changelog

All notable changes to glintfx are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), adapted to glintfx's own four-component version scheme (`vMAJOR.MINOR.PATCH.TWEAK`, not three-component Semantic Versioning). See the "Versioning and compatibility" section of [`README.md`](README.md) for what each component means, and [`PACKAGING.md`](PACKAGING.md) for how that maps onto the CMake package and pkg-config version.

**glintfx is pre-1.0.** There is no ABI or file-format compatibility guarantee across a `MAJOR` bump, and the public surface can still change between any two commits; see "Versioning and compatibility" in `README.md`.

## [Unreleased]

### Fixed

- The library's own declared version (`project(VERSION)` in `CMakeLists.txt`, and everything generated from it: the version macro a consumer reads at compile time, and the packaging files a packager reads) now matches the latest published tag. From v0.3.1.0 through v0.3.7.0 the declaration itself had stayed at `0.3.0.0`, so a consumer building against any of those seven tags was told `0.3.0.0` by the library, never the real version it actually received.

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

[Unreleased]: https://github.com/petrinhu/GlintFx/compare/v0.3.7.0...HEAD
[0.3.7.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.6.0...v0.3.7.0
[0.3.6.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.5.0...v0.3.6.0
[0.3.5.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.4.0...v0.3.5.0
[0.3.4.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.3.0...v0.3.4.0
[0.3.3.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.2.0...v0.3.3.0
[0.3.2.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.1.0...v0.3.2.0
[0.3.1.0]: https://github.com/petrinhu/GlintFx/compare/v0.3.0.0...v0.3.1.0
[0.3.0.0]: https://github.com/petrinhu/GlintFx/compare/v0.2.0.0...v0.3.0.0
[0.2.0.0]: https://github.com/petrinhu/GlintFx/releases/tag/v0.2.0.0
