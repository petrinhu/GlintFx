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

Two supported ways to bring glintfx into a build. Both are documented in
full, with every flag and edge case, in [`PACKAGING.md`](PACKAGING.md) -
this section is the short version.

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

**No version has been tagged yet.** There is no `v1.0.0.0`, and no
`v0.x.y.z` either - do not write a `GIT_TAG` that names one, it does not
exist. The only correct mechanism today is to pin to an exact Git commit:

```bash
git ls-remote https://github.com/petrinhu/GlintFx.git main
```

Take the 40-character commit hash that prints, and put it in `GIT_TAG`
above. Re-run that command and update the value yourself whenever you
deliberately want a newer glintfx - nothing does this automatically,
which is exactly the point.

Once glintfx tags its first release, `vMAJOR.MINOR.PATCH.TWEAK` becomes
the thing to pin to, and the version number itself tells you what
changed - full rules in the README's ["Versioning and compatibility"](README.md#versioning-and-compatibility)
section (a human-oriented walkthrough of the same material lives in the
project's [wiki](https://github.com/petrinhu/GlintFx/wiki/Pinning-a-Version)).

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
