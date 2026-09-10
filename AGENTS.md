# AGENTS.md

This file is for AI agents (and the humans directing them) that need to
work with glintfx: using it as a dependency in another project, or
contributing to this repository directly. Two different jobs - read the
section that matches yours.

## What this project is, in three lines

glintfx is a **library**, not a finished application: a C++23 codebase
that gives a 2D game or app a window, a main loop, rendering, input,
audio and similar building blocks, so the consumer only writes their own
logic on top. It is **pre-1.0** and under active construction: large
parts of the above do not exist yet, and the parts that do exist can
still change shape without notice. It is public on GitHub, under the
**GNU AGPL-3.0-or-later** license, which imposes real obligations on
anyone who distributes a program built with it (see "Pinning a version"
below, and the license itself, before assuming you can use it freely in
a closed-source product).

## Consuming glintfx from another project

Three supported ways to bring glintfx into a build. All three are
documented in full, with every flag and edge case, in
[`PACKAGING.md`](PACKAGING.md) - this section is the short version.

**1. Embed it by source** (no separate install step, works today, no
tagged version needed):

```cmake
include(FetchContent)
FetchContent_Declare(
  glintfx
  GIT_REPOSITORY https://github.com/petrinhu/GlintFx.git
  GIT_TAG        <commit SHA you pinned - see "Pinning a version" below>
)
FetchContent_MakeAvailable(glintfx)

target_link_libraries(my_app PRIVATE glintfx::glintfx)
```

**2. Use an installed copy** (built and installed once, then referenced
like any other system library):

```cmake
find_package(glintfx REQUIRED)
target_link_libraries(my_app PRIVATE glintfx::glintfx)
```

This is what `cmake --install` produces: the compiled library, public
headers under `<includedir>/glintfx/`, a CMake package under
`<libdir>/cmake/glintfx/`, and `glintfx.pc` under
`<libdir>/pkgconfig/`, on every one of the five supported platforms,
Windows included.

**3. Package it**, as a distro package (RPM `.spec`, Debian
`debian/rules`, an Arch `PKGBUILD`, or any pipeline that installs
glintfx once and builds against it afterward). `PACKAGING.md` is
written for this audience specifically - what gets installed, every
supported `CMAKE_INSTALL_LIBDIR`/`CMAKE_INSTALL_INCLUDEDIR` layout,
`DESTDIR`-staged installs, static linking, and the Windows-specific
detail of pkg-config there - and every layout claim it makes is backed
by an automated regression test, not prose.

Whichever of the three you use, `pkg-config --exists glintfx`
returning true is not proof the install is usable by itself; see
`PACKAGING.md`'s "`pkg-config --exists` does NOT validate content"
section before trusting it as a health check in your own script.

Requirements for building glintfx at all (compiler, CMake floor, Linux
packages) are listed in the main [`README.md`](README.md#building-from-source-and-running-the-tests),
measured against the project's own CI - do not assume a version number
from memory, read that section.

## Pinning a version

**Read this before writing a build file against glintfx - it is the
single most consequential decision a consumer makes, and getting it
wrong fails silently.**

Pinning means locking your build to one exact, unchanging copy of
glintfx, instead of whatever its `main` branch looks like at build time.
**glintfx is pre-1.0: its interface can change between any two commits,
with no deprecation warning.** A build that does not pin will, sooner or
later, silently start compiling against a different glintfx than the one
that was tested - the failure mode ranges from a compiler error to a
runtime bug with no compile-time signal at all.

**Nine versions have been tagged so far; the most recent is
`v0.3.7.0`, cut 10/09/2026** (`v0.2.0.0` through `v0.3.7.0` - run
`git tag --list 'v*'` against a clone to see all of them). None is
accompanied by a formal GitHub Release (checked directly against
the repository: no entry exists on the releases page for any tag as
of this writing) - each tag's own annotated message (`git show
<tag>` on a clone) is the authoritative description of what it
marks, not a release-notes page. Its own `CHANGELOG.md` has fallen
behind again: its most recent dated section is `[0.3.0.0]`, and the
seven tags published after it (`v0.3.1.0` through `v0.3.7.0`) have
no entry there yet, a known gap tracked in `TODO.md`, not silently
accepted as done.

Pin `GIT_TAG` to the tag name directly:

```bash
GIT_TAG v0.3.7.0
```

A raw commit hash still works in the same field (find one with
`git ls-remote https://github.com/petrinhu/GlintFx.git main`), and is
the only option if you need a point between two tags, but prefer the
tag name once it covers the commit you want: it tells the next person
what was tested without anyone looking up a hash.

`vMAJOR.MINOR.PATCH.TWEAK` is the thing to pin to now, and the version
number itself tells you what changed - full rules in the README's
["Versioning and compatibility"](README.md#versioning-and-compatibility)
section (a human-oriented walkthrough of the same material lives in the
project's [wiki](https://github.com/petrinhu/GlintFx/wiki/Pinning-a-Version)).
**Every tag published so far still carries the pre-1.0 caveat above:
`MAJOR` is `0`, `SOVERSION` is `0`, and there is no cross-version
compatibility promise until `1.0.0.0`.**

## Working inside this repository

If you are modifying glintfx itself rather than consuming it, the rules
that govern that work are **not restated here** - a copy would drift from
the original the first time it changes. Read the source instead:

| Question | Where the answer lives |
|---|---|
| Coding standards, architecture, C++ rules | [`CONTRACT.md`](CONTRACT.md) |
| Testing, static analysis, sanitizers | [`TESTES.md`](TESTES.md) |
| Product and process rules specific to this project | [`GODS_LAWS.md`](GODS_LAWS.md) (written in Portuguese, the maintainer's working language) |
| What actually runs in continuous integration | [`.github/workflows/ci.yml`](.github/workflows/ci.yml) |
| The same checks, runnable on your own machine before committing | `tools/preci.sh` |
| Open work and its current state | [`TODO.md`](TODO.md) |

## The prohibitions that cost the most here

- **No third-party dependency**, beyond the C++ standard library and the
  operating system's own APIs. Why: this is a library other projects
  link against - a dependency glintfx pulled in would become every
  consumer's problem too, silently, so the project writes that surface
  itself instead of importing it.
- **A test that opens a window, reads input, or touches a real display
  never runs directly against the machine that started it - only inside
  an isolated container with its own, separate compositor.** Why: a
  misbehaving window or input test can otherwise take over whichever
  desktop session actually ran it, not just the intended target.
- **No vendored third-party source outside one single, explicitly
  documented exception, and no secret or credential of any kind.** Why:
  this is a public repository under a copyleft license - undeclared
  third-party code is a license problem the moment anyone else builds
  from it, and a leaked credential is compromised the instant it is
  pushed, not when someone notices.
