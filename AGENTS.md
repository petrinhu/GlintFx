# AGENTS.md

This file is for an AI agent that just cloned this repository and has no other context. Read `README.md` first; this file complements it and does not repeat what it already says.

## What glintfx is

glintfx is a 2D framework for C++23: window creation, the main loop, rendering, input, gamepad, audio, fonts, asset loading and 2D math, built from scratch with zero dependencies beyond the standard library and the operating system's own APIs. It is a **library**, not a finished application; a consumer writes their own game or app logic on top of it. It is public, on GitHub, under **AGPL-3.0-or-later**, written for a consumer base that is open and unknown in advance, never for any one specific integrator.

## Consuming glintfx from another project

There are three supported paths, in the order most consumers reach for them. `PACKAGING.md` is the authoritative reference for all three (install layouts, static linking, Windows specifics, what each claim is tested against); this section is only the quick map.

**1. Embed by source** (no install step, closest to zero-friction for a first try):

```cmake
add_subdirectory(glintfx)   # or FetchContent, which populates a tree
                             # and calls add_subdirectory on it the same way
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE glintfx::glintfx)
```

Needs CMake 4.1 or newer (see "Fixing a version" below for why that floor exists) and, on Linux, `libwayland-client`, `wayland-protocols` and EGL development headers.

**2. Install it**, the way a distro package or a system-wide install would, then consume it with `find_package(glintfx)` or `pkg-config --cflags --libs glintfx`. This is what `cmake --install` produces: the compiled library, public headers under `<includedir>/glintfx/`, a CMake package under `<libdir>/cmake/glintfx/`, and `glintfx.pc` under `<libdir>/pkgconfig/`, on every one of the five supported platforms, Windows included.

**3. Package it** as a distro package (RPM `.spec`, Debian `debian/rules`, an Arch `PKGBUILD`). `PACKAGING.md` is written for this audience specifically, and every layout claim it makes is backed by an automated test, not prose.

Whichever path is used, `pkg-config --exists glintfx` returning true is not proof the install is usable by itself; see `PACKAGING.md`'s "`pkg-config --exists` does NOT validate content" section before trusting it as a health check in a script.

## Fixing a version

**No version has been tagged yet.** The project is pre-1.0: there is no API, ABI, or file-format compatibility guarantee before the first `1.0.0.0` tag, and the public surface can still change between commits. Do not treat anything here as a stable dependency to pin against for production use yet; if a consumer needs that guarantee today, glintfx is not ready for it.

Once a version exists, it is tagged `vMAJOR.MINOR.PATCH.TWEAK`, four components, not three:

| Component | Bumps when |
|---|---|
| MAJOR | An API break: code that used to compile no longer does |
| MINOR | A new, backward-compatible feature |
| PATCH | A bug fix, no new feature, no break |
| TWEAK | A build or packaging revision, no code change |

`find_package(glintfx)` and `glintfx.pc`'s `Version:` field both use this same four-component scheme, so pin against it the same way you would pin a `MAJOR.MINOR.PATCH` dependency elsewhere, just with the extra component. `SOVERSION` tracks `MAJOR`, and before 1.0 it is fixed at `0`, the conventional Unix signal that the ABI can break at any time.

## Working inside this repository

If you are contributing code here rather than only consuming the library, read the rule files directly instead of relying on this summary, since they change independently of this one:

- `GODS_LAWS.md`, at the repository root, holds the maintainer's binding rules and takes precedence over every other document here.
- `CONTRACT.md` sets the coding standard (SOLID, layering, C++ rules) and, in its section 9, the exact Conventional Commits format this project requires (`feat`, `fix`, `refactor`, `docs`, `test`, `chore`, `perf`, `style`, `revert`, each with a `(scope)` and a description).
- `TESTES.md` governs how tests, static analysis, fuzzing and sanitizers are planned and run here.
- Before committing, run `tools/preci.sh` (or `tools/preci.sh --fast` for a documentation-only change): it is the local mirror of continuous integration, and a change that fails it will fail CI too.
- Every source file (C++ and the Python/shell tooling under `tests/tools/`) carries an `SPDX-License-Identifier: AGPL-3.0-or-later` header; a new file needs one too.

## Costly mistakes to avoid here

- **Do not add a dependency.** Nothing beyond the C++23 standard library and the operating system's own API is allowed, not even by `FetchContent` or vendoring; image decoding, font rasterization, the GL loader, audio mixing and gamepad decoding are all written in-house on purpose. A pull request that adds a third-party library will be rejected regardless of how small it is.
- **Do not run a test that opens a window, moves a cursor, or reads a keyboard on a real machine.** Every such test runs inside a container with its own nested, virtual Wayland compositor; running one directly can affect the maintainer's own live session (this has happened before, hence the rule). If you cannot run the containerized suite yourself, say so and let a maintainer run it.
- **Do not target X11.** On Linux this project speaks Wayland directly; there is no X11 backend and none is planned, so an X11-based example or fix from elsewhere in the ecosystem does not apply here.
- **Do not commit a secret, a credential, or a token.** A dedicated CI job scans the full git history for one on every push, but the containing commit is already in history at that point; keep it out before it is ever staged.
