# glintfx

> A 2D framework for C++23, built from scratch with zero dependencies beyond the standard library and the operating system's own APIs.

[![CI](https://github.com/petrinhu/GlintFx/actions/workflows/ci.yml/badge.svg)](https://github.com/petrinhu/GlintFx/actions/workflows/ci.yml)
[![License: AGPL-3.0-or-later](https://img.shields.io/badge/license-AGPL--3.0--or--later-blue.svg)](LICENSE)

## What glintfx is

glintfx is a reusable **library**, not a finished application. The goal is that a consumer gets window creation, the main loop, 2D rendering, input, gamepad, audio, fonts, asset loading, and 2D math from glintfx, and writes only their own game or app logic on top of it.

glintfx is public, on GitHub, under **AGPL-3.0-or-later**. It is written for a consumer base that is open and not known in advance, not for any single project. Every API, packaging, and versioning decision is made with that unknown consumer in mind.

## Status: pre-1.0, under active construction

This section states what actually exists in the source tree today, not what is planned. Planned scope (window, rendering, input, a style engine, a map format, and more) is real and tracked, but is **not** built yet unless listed below as "implemented."

**Implemented and tested today** (measured against the source tree on 2026-08-26, check `ls src/` and `ls include/glintfx/` yourself: this list rots the moment new code lands):

- **`glintfx::version`**: the library's own version, exposed as a value type with four components (see "Versioning" below).
- **`glintfx::gltfx_err` / `glintfx::gltfx_rslt<T>`**: the library's public error-handling types. Every fallible public function returns `gltfx_rslt<T>`; no exceptions cross the public API. Full contract: [`docs/api-conventions.md`](docs/api-conventions.md).
- **`glintfx::gltfx_rgba` / `glintfx::gltfx_rgba8`**: a frozen-layout color value type - four 32-bit floats (`red`, `green`, `blue`, `alpha`), alpha inside, straight (never premultiplied). The numbers are **linear light**, not the display-encoded value a monitor receives: this is a deliberate choice (see the header comment in `include/glintfx/core/color.hpp`), because it is the only convention under which adding two lights together gives a physically correct result, and it lets a future bloom or `oklch()` value go brighter than white without being clipped at birth. `gltfx_rgba8` is the display's own 8-bit-per-channel format, used only at the boundary with the outside world (a texture byte, a parsed color, a GPU vertex attribute). `gltfx_rgba_from_srgb8()` and `gltfx_rgba_to_srgb8()` round-trip between the two, and `gltfx_rgba_premultiplied()` computes the transient premultiplied form one interpolation needs. Parsing color text (`#rrggbb`, `rgb()`, `oklch()`, and the CSS named colors) is out of scope here; it belongs to the style-sheet track (`gfss`).
- A **Wayland protocol binding** (`xdg-shell`, generated at build time by `wayland-scanner`) linked into the library target on Linux. This is plumbing only: there is no window creation, no event loop, and no rendering yet built on top of it.

**Not implemented yet:** window creation, the main loop, 2D rendering, input (keyboard, mouse, gamepad), audio, font rendering, asset loading, the style/layout engine, and the map format. There is no working demo application yet. If you need any of the above today, glintfx is not ready for you. Watch [`TODO.md`](TODO.md) (in Portuguese, the maintainer's working language) for progress, or the [releases page](https://github.com/petrinhu/GlintFx/releases) for the first tagged version.

**No version has been tagged yet.** There is no ABI or file-format compatibility guarantee before the first `1.0.0.0` tag; see "Versioning" below for exactly what that means.

## Supported platforms

Five targets, each with its own entry in continuous integration: a green build on one does not imply another is supported.

| Platform | Role |
|---|---|
| **Fedora** | Primary target. Tracked via `fedora:latest` in CI since 27/08/2026 (previously pinned to `fedora:44`) - measured that day to be the same version, 44, as the maintainer's own machine, so CI still fails whenever it would; the pin was lifted so it keeps matching automatically as the maintainer upgrades, instead of needing someone to bump a fixed version by hand. |
| Ubuntu | Portability target. Tracked via `ubuntu:latest` in CI since 27/08/2026 (previously pinned to `ubuntu:24.04`); see `PACKAGING.md`'s "CMake version requirement" section for what that means if you specifically target Ubuntu 24.04. |
| Arch Linux | Portability target. |
| CachyOS | Portability target, **not** covered by the Arch job (different toolchain, repositories, and default flags). A green Arch build does not mean CachyOS is supported. |
| Windows | Portability target. |

On Linux, glintfx targets **Wayland only**. There is no X11 backend and none is planned: a session running X11 is not a supported target, by design, not by omission.

Both a shared library (the default) and a static library are built and tested on every push, on every platform above - the compiled artifact's name follows each platform's own convention, not one fixed pair of names: `libglintfx.so`/`libglintfx.a` on Linux, `glintfx.dll` (plus its `glintfx.lib` import library) for the shared build and `glintfx.lib` for the static build on Windows.

## Design principles that shape the public API

These are load-bearing decisions, not style preferences. They are why the API looks the way it does:

- **Zero dependencies.** Nothing beyond the C++23 standard library and the operating system's own API (Win32, Wayland, OpenGL, and so on). No third-party package manager, no vendored library. Image decoding, font rasterization, the GL loader, audio mixing, and gamepad decoding are all written in-house.
- **The public surface is opaque.** The library is shared by default, so any public class with a visible layout becomes part of the ABI contract. Stateful public types hide their implementation behind an opaque handle or PIMPL; only plain value types (like `version` and `gltfx_rgba`) expose a stable, visible layout on purpose.
- **No exception crosses the public API.** Internally, exceptions are allowed; every fallible public function returns `glintfx::gltfx_rslt<T>` instead. See [`docs/api-conventions.md`](docs/api-conventions.md) for the full rationale and the tests that prove it.
- **OpenGL 3.3 core**, on every platform that has a render backend, once one exists.

## How to consume glintfx

There are three ways to bring glintfx into your own project, and they behave differently. Read [`PACKAGING.md`](PACKAGING.md) for the full reference: what gets installed, supported install layouts, static linking, and what each claim is tested against.

1. **Install it** (via CMake's `install()`, then `find_package(glintfx)` or `pkg-config`), the way a distro package or a system-wide install would.
2. **Embed it by source**, with CMake's `add_subdirectory()` or `FetchContent`, without installing anything.
3. **Package it**, as a distro package (RPM `.spec`, Debian `debian/rules`, an Arch `PKGBUILD`, and so on). `PACKAGING.md` is written for packagers specifically, and every layout claim it makes is backed by an automated test, not just prose.

## Building from source and running the tests

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

To build the static library instead of the default shared one, add `-DBUILD_SHARED_LIBS=OFF` to the configure step.

Requirements: a C++23 compiler (GCC 14 or newer; GCC 13's partial C++23 support is not enough, measured against Ubuntu 24.04's default toolchain), **CMake 4.1 or newer** (glintfx's install-time pkg-config validator is built on CMake's own native pkg-config parser, `cmake_pkg_config()`, which needs that floor - see `PACKAGING.md`'s "CMake version requirement" section if your platform's package manager ships an older one; as of 27/08/2026 the `windows-latest` GitHub Actions runner does, measured, and so does Ubuntu 24.04's `apt` specifically if that is the exact release you target - `PACKAGING.md` explains why that no longer matches what this project's own CI tests), Ninja, and, on Linux, `libwayland-client` plus `wayland-protocols` development headers.

**On Linux**, the test suite currently has 54 registered cases in shared mode and 53 in static mode (the difference is `visibility_test`, which only makes sense against a shared object). These two numbers deliberately exclude `preci_selftest` (`tests/CMakeLists.txt`), a local lint self-test that only registers when clang-format, clang-tidy and cppcheck are all found on the machine that ran `cmake` - most developer machines have them and so does the dedicated `lint` CI job, but the other CI targets build with a plain toolchain and never register it. A raw `ctest -N` total is therefore a property of the machine, not of the repository: this project measured it both ways on the same commit (55/54 with the three tools installed, 54/53 without) and shipped documentation once that stated the raw total from a machine that had them, which the four Linux build jobs - which do not install them - correctly reproved. Run `ctest --test-dir build -N` yourself to see your own raw total: it will be one higher than the numbers above if your machine has clang-format/clang-tidy/cppcheck, and match them exactly if it does not. The CI gate that checks this sentence (`readme_test_count_test`, see `tests/tools/check_readme_test_count.sh`) makes the same subtraction before comparing, so it holds on every Linux target in this project's CI matrix (Fedora, Ubuntu, Arch, CachyOS), not just the one it was written from.

**On Windows, these two numbers do not apply, and nothing in CI enforces them there.** Most of this suite's gates in `tests/CMakeLists.txt` are `if(UNIX)`-only entries, because they are POSIX shell scripts (`tests/tools/*.sh`) that Windows has no default interpreter for - `readme_test_count_test` itself is one of them, so this very sentence is unchecked on that platform. A raw `ctest --test-dir build -N` on Windows registers a substantially smaller total than the Linux figures above: this project has not measured that exact number on a real Windows machine, so no figure is stated here rather than guessing one - see `.github/workflows/ci.yml`'s `windows` job for the dedicated PowerShell scripts (`tools/ci/check-embed.ps1` and the "Consumo instalado"/"Consumo embutido" steps) that cover, outside `ctest`, the same consumption claims the Unix-only gates prove through it.

## Versioning and compatibility

glintfx tags releases as **`vMAJOR.MINOR.PATCH.TWEAK`**, four components, not three. This is a deliberate choice, not an oversight:

| Component | Bumps when |
|---|---|
| **MAJOR** | An API break: code that used to compile against glintfx no longer does. |
| **MINOR** | A new, backward-compatible feature. |
| **PATCH** | A bug fix, with no new feature and no break. |
| **TWEAK** | A build or packaging revision, with no code change at all. |

`SOVERSION` tracks `MAJOR`. **Before 1.0, `SOVERSION` is `0`**: the conventional Unix signal that the ABI can break at any time, and it is honest, because nothing here is stable yet. Once glintfx reaches 1.0, the table above applies in full, and a break in `MAJOR` becomes a promise glintfx keeps, not just a number.

glintfx tracks **three separate compatibility contracts**, not two, because a binary is not the only thing a consumer can lose:

- **API**: does code that compiled before still compile.
- **ABI**: does a binary linked before still link and run.
- **File format**: does a file glintfx wrote before still load. This is the one a rebuild cannot fix: a `.so` break sends the consumer back to their compiler, but a broken save file is gone. Any file format glintfx ships follows the same MAJOR/MINOR discipline as API and ABI, and a new MAJOR reader keeps reading every format version it ever wrote.

## API reference

The public error-handling contract (the type every fallible function returns, why reading it wrong is a precondition violation rather than a recoverable error, and how the public surface is checked against name collisions on five platforms) is documented in [`docs/api-conventions.md`](docs/api-conventions.md). It is the template every later API review is checked against, not prose to be taken on faith: every rule cites the test that proves it.

## Changes

See [`CHANGELOG.md`](CHANGELOG.md). No version has shipped yet, so it is currently empty of releases: it exists so the first tag has somewhere to land.

## License

glintfx is licensed under the **GNU Affero General Public License v3.0 or later** (AGPL-3.0-or-later). See [`LICENSE`](LICENSE) for the full text.

## Third-party files

Almost everything in this repository is written from scratch under AGPL-3.0-or-later, with one narrow, explicitly recorded exception: [`third_party/khronos/gl.xml`](third_party/khronos/gl.xml), the Khronos Group's own machine-readable OpenGL API registry, vendored verbatim under its own Apache-2.0 license so this project's build-time code generator (`tools/gl_registry_codegen/`) can resolve the OpenGL 3.3 core function list on every one of this project's five target platforms. See [`third_party/khronos/README.md`](third_party/khronos/README.md) for the full provenance record (source URL, upstream commit, retrieval date, `sha256`) and why this one file needed an exception. Everything this generator produces at build time is glintfx's own code, licensed AGPL-3.0-or-later like the rest of the project.

## More documentation

- [`docs/api-conventions.md`](docs/api-conventions.md): the public error-handling contract, in depth.
- [`PACKAGING.md`](PACKAGING.md): reference for packagers and consumers: install layouts, embedding, static linking.
- [`CHANGELOG.md`](CHANGELOG.md): what changed, release by release.

The project's internal engineering documents (governance rules, the C++ style contract, the testing manual, audit reports, and the task table) are written in Portuguese, the maintainer's working language, and are not required reading to use glintfx as a library. They live at the repository root (`GODS_LAWS.md`, `CONTRACT.md`, `TESTES.md`, `AUDITORIAS.md`, `TODO.md`) for anyone curious how the project is run.

## Acknowledgments

glintfx is written by one maintainer, but not in a vacuum. The people below shaped the maintainer's work in general, not necessarily this library on its own; where a contribution reached glintfx directly, it is named as such below.

- **El [Iagows](https://github.com/iagows)** (the maintainer's brother), a computer engineer whose training shaped how the maintainer thinks about technology, and who has given advice on architecture and stack choices across the maintainer's projects.
- **Od Fuinha Minduim, Thiago MadDog and Thiago Arcanjo**, IT professionals who have given tips on testing, architecture, QA, security and CI over the years.
- **The #metaleiros-PE crew**, a longstanding friendship and a source of ongoing feedback.

### Special thanks: Gus Dragon, Revisor Adversarial de Design

Gus Dragon, the maintainer's son, holds the title his father gave him: **Revisor Adversarial de Design** (roughly, "adversarial design reviewer": someone who questions a system's structure and looks for how it will break, before it exists, not just someone who plays the finished thing). On 21/08/2026 he asked, naming glintfx by name: "GlintFx e Mapeditor façam blocos especiais pra isso" (make glintfx and the map editor build special blocks for that). That request became glintfx's generic tile-marking mechanism: each map cell carries an opaque, author-defined mark, checked against a per-query traversal mask, with no game-specific meaning ever known to the library itself. It is the one entry on this list with a direct, verifiable line into glintfx's own code, not a general influence on the maintainer's work.
