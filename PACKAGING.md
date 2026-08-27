# Packaging glintfx

This document is for **packagers and consumers** of glintfx — anyone
writing a distro package (RPM `.spec`, Debian `debian/rules`, an Arch
`PKGBUILD`, ...), a `Makefile`/Autotools build that links glintfx via
`pkg-config`, a CI script that installs glintfx and then builds
against it, or a project that vendors glintfx by source via CMake's
`add_subdirectory`/`FetchContent` instead of installing it. It is not
internal project documentation; if you are looking for how glintfx
itself is developed, start at `GODS_LAWS.md` and `CONTRACT.md` instead.

glintfx is distributed under AGPL-3.0-or-later, to a consumer base
that is public and unknown to the maintainers. Everything below is
written, and tested, against that assumption: no specific packaging
pipeline is treated as "the" pipeline.

This is reference material: what gets installed, what layouts are
supported, and how to link against it — not a walkthrough of writing
any one distro's package.

## What gets installed

- The library itself: `libglintfx.so.<A>.<B>.<C>.<D>` (shared, the
  default) or `libglintfx.a` (`-DBUILD_SHARED_LIBS=OFF`).
- Public headers under `<includedir>/glintfx/`.
- A CMake package (`find_package(glintfx)`), under
  `<libdir>/cmake/glintfx/`.
- A pkg-config module, `<libdir>/pkgconfig/glintfx.pc`.

`<libdir>` and `<includedir>` are `CMAKE_INSTALL_LIBDIR` and
`CMAKE_INSTALL_INCLUDEDIR` (from CMake's `GNUInstallDirs` module),
resolved at configure time.

**None of the above applies to embedding** (next section) — installing
is opt-in there, and off by default.

## Embedding via `add_subdirectory`/`FetchContent`

A consumer can vendor glintfx by source instead of installing it:

```cmake
add_subdirectory(glintfx)          # or FetchContent, which populates
                                    # a tree and calls add_subdirectory
                                    # on it the same way
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE glintfx::glintfx)
```

glintfx does **not** install its headers or CMake package into the
consumer's own install prefix as a side effect of this (`GLINTFX_INSTALL`
is off by default when embedded — see "What gets installed" above; a
consumer that *does* want glintfx installed alongside its own artifacts
can still set `-DGLINTFX_INSTALL=ON` explicitly).

### On Windows, shared (the default), glintfx places its own DLL next to your executable

This is the one thing embedding does for you automatically, and it
exists to close a real gap, not a hypothetical one: CMake gives every
target — glintfx's library and your executable — its own build
subdirectory by default. On Linux this is invisible, because CMake
auto-embeds a build-tree RPATH into your executable, so the loader
finds `libglintfx.so` wherever it actually is. **Windows has no
equivalent mechanism.** The default Windows DLL search order checks
your executable's own folder, not the rest of the build tree (see
[Dynamic-link library search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order)) —
so without help, `glintfx.dll` and `my_app.exe` land in different
directories, the loader cannot find the DLL, and `my_app.exe` fails to
start.

When glintfx is embedded, built shared, and on Windows, it places its
own `glintfx.dll` in the **outermost project's own default runtime
output directory** — the same place your own executable already lands
by default if you have not customized your layout. You do not need to
read this section to get a working `my_app.exe`; that is the point.

**What this does NOT promise:** if your project already has multiple
executables scattered across nested subdirectories with no unified
output directory of their own, glintfx's own DLL joins the outermost
directory, same as any of your own un-customized top-level targets —
it does not chase every executable in your tree. That is a general
CMake/Windows multi-target packaging problem no single dependency can
fully solve on your behalf; if you need it solved for your whole
project, the CMake-recommended tool is
[`$<TARGET_RUNTIME_DLLS:tgt>`](https://cmake.org/cmake/help/latest/manual/cmake-generator-expressions.7.html)
in a post-build step on each of your executables.

**If you already have your own layout convention:**

- **Set `CMAKE_RUNTIME_OUTPUT_DIRECTORY` yourself** (the CMake-blessed
  variable for unifying every target's runtime output, your own and
  glintfx's alike) — glintfx detects this and does not touch anything;
  your value already applies to `glintfx.dll` the same way it applies
  to your own un-customized targets.
- **Or turn the mechanism off entirely**: `-DGLINTFX_EMBEDDED_RUNTIME_COLOCATE=OFF`.
  This is the escape valve for any other custom layout (for example,
  per-target `RUNTIME_OUTPUT_DIRECTORY` properties set by hand instead
  of the shared variable) — with it off, glintfx's DLL lands exactly
  where it would have before this section existed, and colocating it
  is entirely your own responsibility.

This mechanism is off by default everywhere else — a standalone
glintfx build, a static build, a Linux build — because none of them
ever hit the failure it exists to close.

### Where embedding is tested

- `tests/embed/` proves configure/build/run of an embedded consumer,
  that glintfx's generated headers stay scoped under the embedded
  build's own subdirectory, and the `GLINTFX_INSTALL` opt-in described
  above, on Linux (`tests/tools/check_embed.sh`) and on Windows
  (`tools/ci/check-embed.ps1`) — the Windows script runs the produced
  executable for real and reads its actual exit code and stdout/stderr,
  not just whether a file exists.
- `tests/embed_dll_colocation/` proves the DLL-colocation **decision**
  described above — the escape valve, and the option's own default —
  on every one of the five supported platforms (it has no shell or OS
  dependency at all). It does **not**, on its own, prove the DLL ends
  up somewhere a real Windows loader can use: that is `check-embed.ps1`
  above, which runs the real produced executable on real Windows CI.

## Version numbers have FOUR components, not three

Both `find_package(glintfx)` and `glintfx.pc`'s `Version:` field use
`MAJOR.MINOR.PATCH.TWEAK` (e.g. `0.1.0.0`), not three-component
SemVer. `TWEAK` marks a build/packaging revision with no code change.
This is a deliberate, permanent choice, not a placeholder.

## `pkg-config --exists` does NOT validate content

This is worth stating plainly, because it is easy to assume otherwise:
`pkg-config --exists glintfx` (and a "successful" `--cflags`/`--libs`)
only means a `glintfx.pc` file was found somewhere on
`PKG_CONFIG_PATH` and parses as valid pkg-config syntax. **It does not
check that the directories it reports actually contain glintfx's
headers or library artifact.** A partially-removed install, a
half-finished packaging step, or a `glintfx.pc` copied without the
files it describes can all still report `--exists` as true.

If you need to verify an installation is genuinely usable, do not
stop at `--exists`: resolve `pkg-config --variable=includedir glintfx`
and `pkg-config --variable=libdir glintfx` and confirm those
directories actually contain a `glintfx/` header tree and a
`libglintfx.so*`/`libglintfx.a` artifact, or simply compile and run a
trivial program against the reported `-I`/`-L`/`-l` flags.

### glintfx already runs this check for you, at install time, on your own machine

You do not actually have to do any of the above by hand: `cmake
--install` already does it for you, automatically, every time you
install glintfx with `GLINTFX_INSTALL` at its default value (see
"Embedding via `add_subdirectory`/`FetchContent`" above for the one
case where nothing is installed at all, and this check does not run
either). Right after `glintfx.pc`, the public headers and the library
artifact are written to disk, an installation step runs a real
`pkg-config --exists glintfx`, resolves `--variable=includedir` and
`--variable=libdir`, and walks every `-I`/`-L` token a plain
(non-`--static`) `--cflags`/`--libs` query emits, the same query any
consumer runs first, confirming each path exists on disk and actually
contains a `glintfx/` header tree or a `libglintfx.so*`/`libglintfx.a`
artifact, against whatever real layout that particular `cmake
--install` invocation produced, including a `DESTDIR`-staged one.
This runs on **your** machine, as part of the `cmake --install` you
already run, not only in glintfx's own CI, which by construction can
only exercise the layouts its own maintainers thought to enumerate.
An install that resolves to nothing worth checking (an empty
`Cflags:`/`Libs:` line, for instance) counts as a failure too, not a
silent pass.

**What this does NOT check:** that `Libs.private` carries a linker
token genuinely load-bearing for a real static link (see "Static
linking" above; that claim is proven separately, against glintfx's own
CI layouts, not re-checked here); anything Windows-specific
(pkg-config has no role in glintfx's Windows story today); anything
target-architecture-specific under cross-compilation (every check here
reads the filesystem and runs `pkg-config`/`pkgconf`, a host tool
operating on text, and never executes target-arch code); or a
`--component`-scoped install (nothing glintfx installs today declares
a `COMPONENT`, so a `--component` install under any other name simply
installs nothing for glintfx in the first place, and this check is
skipped right along with it, for that same reason).

**How a failure shows up:** as a hard error from `cmake --install`
itself, not from a separate script you have to remember to run
afterward. This check is one more installation step appended right
after `glintfx.pc`'s own, so a real problem fails the whole install
invocation (nonzero exit), with a message naming the exact resolved
path and what was wrong with it: a directory that does not exist on
disk, one that exists but has no `glintfx/` subdirectory or no
`libglintfx.so*`/`libglintfx.a` artifact in it, or a `--cflags`/`--libs`
query that emitted no `-I`/`-L` token to check in the first place.

**If you need to skip it:** the same setting works two ways, both
named `GLINTFX_SKIP_PKGCONFIG_VALIDATION`. Pass
`-DGLINTFX_SKIP_PKGCONFIG_VALIDATION=ON` at configure time to turn it
off for the whole build directory, so every future `cmake --install`
run from it skips the check; or set the `GLINTFX_SKIP_PKGCONFIG_VALIDATION`
environment variable (to anything other than empty or `0`) for one
specific `cmake --install` invocation, with no reconfigure needed. This
second form is what a pipeline that configures once and installs
several times (for example, an RPM spec file's `%install` section
invoking `%cmake_install` more than once, against different staging
roots) needs. Either way, `glintfx.pc`, the headers and the library
are still installed exactly as they would be otherwise; only this
extra check is skipped. Reach for it when your own pipeline already
verifies the install some other way, when you are intentionally
staging a partial or non-standard layout you know this check has no
way to make sense of, or when you simply do not want `pkg-config` in
the loop of your `cmake --install` at all.

**Where this does not run at all:** when glintfx is embedded via
`add_subdirectory`/`FetchContent` and never installed in the first
place (`GLINTFX_INSTALL` defaults to off there, see "Embedding" above,
so `glintfx.pc`, the headers, the library and this check are all
equally absent from that build); and, on any machine where neither
`pkg-config` nor `pkgconf` is on `PATH`, where this degrades to a
warning instead of failing the install: `glintfx.pc` is still written,
there is simply no tool available to confirm it resolves.

## Supported `CMAKE_INSTALL_LIBDIR` / `CMAKE_INSTALL_INCLUDEDIR` layouts

All of the following are tested, on every push, against a real
install-then-resolve-via-pkg-config round trip (see "Where this is
tested" below for exactly which platforms):

- **Left unset.** `GNUInstallDirs` picks a sane per-platform default
  (e.g. `lib64` on Fedora, `lib` on many other Linux distros). This is
  the recommended default for most consumers.
- **A relative path**, including a Debian-multiarch-style path such as
  `lib/x86_64-linux-gnu`.
- **An absolute path**, independent of any install prefix (a packager
  staging directly into a fixed system location). `glintfx.pc` is
  emitted with that literal absolute path, correctly, with no prefix
  duplication.
- **A trailing slash, a doubled path separator, or a leading `./`** in
  either of the above — normalized before use, so a value like
  `lib64/` behaves identically to `lib64`.
- **`DESTDIR`-staged installs** — the format real RPM and DEB packages
  actually build under: `CMAKE_INSTALL_PREFIX` set at configure time
  (typically `/usr`), no explicit `CMAKE_INSTALL_LIBDIR` override, and
  `DESTDIR=<staging root> cmake --install <builddir>` at install time
  (equivalently, RPM's `%cmake`/`%cmake_install` macros, or DEB's
  `dh_auto_configure`/`dh_auto_install`). `DESTDIR` is fundamentally
  different from `cmake --install --prefix <path>`: `--prefix`
  rewrites *relative* install destinations only; `DESTDIR` prepends to
  *every* destination, relative or absolute, without changing any
  path baked into the installed files' own content. `glintfx.pc`
  reflects the real, final, post-staging location (e.g.
  `prefix=/usr`), not the temporary staging root — which is what lets
  the staged tree be moved onto the target system unchanged.

## NOT supported

- **`CMAKE_INSTALL_LIBDIR` or `CMAKE_INSTALL_INCLUDEDIR` set to an
  empty or whitespace-only value.** This is refused outright, with a
  `FATAL_ERROR` at configure time. Unlike every layout listed above,
  blank never names a real, intentional layout — it is always a
  mistake in how the value was passed (an unset shell variable
  interpolated into a `-D` flag, a stray blank line in a build
  script). Left unhandled, CMake concatenates the value directly into
  several install destinations, and an empty string collapses at
  least one of them to the filesystem root.
- **Moving an already-installed tree whose `CMAKE_INSTALL_LIBDIR` or
  `CMAKE_INSTALL_INCLUDEDIR` was set to an ABSOLUTE path.** A
  relative-layout install is relocatable — you can move the entire
  installed tree (or an entire `DESTDIR` staging tree) to a different
  location and `glintfx.pc` keeps resolving correctly, because its
  `prefix=` is expressed relative to the `.pc` file's own location.
  An absolute-layout install is not: `glintfx.pc` bakes the literal
  absolute path you configured with, by design (that is what "give me
  an absolute, fixed system location" means to `GNUInstallDirs`), so
  moving the tree afterward breaks it. This is documented CMake
  behavior, not something glintfx works around.

## Static linking

Use `pkg-config --cflags --libs --static glintfx`, not the plain
`--cflags --libs` line, when linking against `libglintfx.a`. On Linux,
`--static` is what makes the `Libs.private` field (currently
`-lwayland-client -lm`) appear in the output — glintfx's own Wayland
platform code needs those symbols, and a plain (non-static) pkg-config
query deliberately omits them, because they would be redundant noise
on a dynamic link (`libglintfx.so` already carries its own runtime
dependency on `libwayland-client.so`). Linking `libglintfx.a` without
`--static` can appear to succeed and only fail once your own code
actually exercises the Wayland-backed parts of glintfx, with undefined
references that have nothing obviously to do with glintfx itself.

## Where this is tested

Three scripts in the glintfx source tree back the layout,
static-linking and automatic-validation claims above with an
automated regression test, not just prose:

- `tests/tools/check_pkgconfig.sh` exercises every layout listed
  under "Supported `CMAKE_INSTALL_LIBDIR` / `CMAKE_INSTALL_INCLUDEDIR`
  layouts" above, plus the static-linking behavior described under
  "Static linking" above, end-to-end: install, then resolve purely
  through `pkg-config` (no CMake, no `find_package`, no hand-written
  `-I`/`-L`/`-l` involved at all).
- `tests/tools/check_pkgconfig_validate.sh` exercises the automatic
  install-time check described under "glintfx already runs this check
  for you, at install time, on your own machine" above: a real install
  with the library artifact deleted afterward, a real install with
  `glintfx.pc` itself missing, a hand-assembled `glintfx.pc` with
  empty `Cflags:`/`Libs:` lines, a `DESTDIR`-staged install in the
  format Fedora's own RPM macros produce, both forms of the escape
  hatch, and the warning-instead-of-failure behavior when
  `pkg-config`/`pkgconf` is absent from `PATH`.
- `tests/tools/check_blank_install_dir_rejected.sh` exercises the
  blank-value rejection described under "NOT supported" above: it
  confirms configure fails with glintfx's own error message, naming
  the offending variable, rather than succeeding and only failing
  later or silently.

All three run on every push to `main` and on every pull request,
across the project's Linux CI jobs — Fedora, Ubuntu, Arch, and
CachyOS. They do **not** run on the Windows job: `pkg-config` is a
Unix/Linux packaging convention, and glintfx has no platform layer
outside Unix yet, so there is nothing Windows-specific for any of the
three scripts to cover today.

If you hit a packaging layout this document does not cover, these
scripts are the right place to add a regression test alongside a
fix.
