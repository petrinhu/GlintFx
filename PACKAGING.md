# Packaging glintfx

This document is for **packagers and consumers** of glintfx — anyone
writing a distro package (RPM `.spec`, Debian `debian/rules`, an Arch
`PKGBUILD`, ...), a `Makefile`/Autotools build that links glintfx via
`pkg-config`, or a CI script that installs glintfx and then builds
against it. It is not internal project documentation; if you are
looking for how glintfx itself is developed, start at `GODS_LAWS.md`
and `CONTRACT.md` instead.

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

Two scripts in the glintfx source tree back the layout and
static-linking claims above with an automated regression test, not
just prose:

- `tests/tools/check_pkgconfig.sh` exercises every layout listed
  under "Supported `CMAKE_INSTALL_LIBDIR` / `CMAKE_INSTALL_INCLUDEDIR`
  layouts" above, plus the static-linking behavior described under
  "Static linking" above, end-to-end: install, then resolve purely
  through `pkg-config` (no CMake, no `find_package`, no hand-written
  `-I`/`-L`/`-l` involved at all).
- `tests/tools/check_blank_install_dir_rejected.sh` exercises the
  blank-value rejection described under "NOT supported" above: it
  confirms configure fails with glintfx's own error message, naming
  the offending variable, rather than succeeding and only failing
  later or silently.

Both run on every push to `main` and on every pull request, across
the project's Linux CI jobs — Fedora, Ubuntu, Arch, and CachyOS. They
do **not** run on the Windows job: `pkg-config` is a Unix/Linux
packaging convention, and glintfx has no platform layer outside Unix
yet, so there is nothing Windows-specific for either script to cover
today.

If you hit a packaging layout this document does not cover, these
scripts are the right place to add a regression test alongside a
fix.
