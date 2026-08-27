# third_party/khronos/

This directory is `glintfx`'s **only** vendored third-party file. Everything
else in this repository is AGPL-3.0-or-later, written from scratch
(`GODS_LAWS.md` L-01/L-07). This directory exists because of a single,
narrow, explicitly recorded exception.

## `gl.xml` - provenance

| Field | Value |
|---|---|
| Source | `KhronosGroup/OpenGL-Registry`, `xml/gl.xml` |
| URL | `https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/xml/gl.xml` |
| Upstream commit | `0eab18f308d29bb54aa4667013ae505d73a72194` |
| Obtained | 2026-08-26 |
| `sha256` | `fba2eaa6262cededdba0dd3cd1e3b1806c24899a7c5df8158467e41c19969426` |
| License | `Apache-2.0` (SPDX identifier, confirmed in the file's own header comment) |

**Verbatim, not modified.** This file is redistributed byte-for-byte
identical to the upstream commit above: not one character was edited, not
even the copyright comment at the top. A build-time gate
(`tools/gl_registry_codegen`, see its own header comment) proves this
mechanically: it recomputes the `sha256` of the vendored file and compares
it against the value recorded in this table, so a silent edit fails the
build instead of drifting unnoticed.

## Why this file needed an exception, and the Wayland precedent did not

`cmake/GlintfxWaylandProtocols.cmake` reads `xdg-shell.xml` from wherever the
`wayland-protocols` package already installed it on the machine that builds
`glintfx`: it is never copied into this repository, so nothing is
redistributed and no obligation is created. `gl.xml` is different: it is not
a package on any of this project's five target platforms (verified
2026-08-26, see `TODO.md`, item `GL-LOADER`), so there is no "already
installed" copy to read at build time. Vendoring it is the only way to
resolve the OpenGL 3.3 core function list on all five platforms with one
mechanism. Redistributing is exactly what creates the licensing obligation
the Wayland file never triggers.

This is `GODS_LAWS.md` L-07's **EXCEPTION Number 1**, opened by the project
leader on 2026-08-26, after choosing this path (vendor + generate) over the
other two considered: hand-transcribing 344 function signatures (rejected,
human transcription error that only surfaces as a runtime crash), or reading
the platform header (`GL/glcorearb.h` on Linux) directly (rejected, no
equivalent exists on Windows, which would mean maintaining two mechanisms).

## The four license obligations, and where each is met

1. **The full Apache-2.0 text travels with the file**: `LICENSE-APACHE-2.0.txt`,
   in this same directory. The repository's own `LICENSE` at the project
   root is **not touched** and stays AGPL-3.0-or-later only; the two license
   texts are never merged into one file.
2. **The copyright header inside `gl.xml` stays intact**: never stripped,
   never "cleaned up". Read it yourself: the first lines of the file.
3. **The file is vendored verbatim and never edited**, which removes the
   obligation to mark modifications, because there are none by construction.
   The `sha256` gate above is the permanent proof of this.
4. **This README records the provenance**: URL, upstream commit, date of
   retrieval, `sha256`, and the "verbatim, not modified" statement, all four
   are in this file.

There is no `NOTICE` file in the upstream repository (verified 2026-08-26);
none is fabricated here.

## What the generated output owes this file, and what it does not

`tools/gl_registry_codegen/` reads `gl.xml` at build time and emits a header
and source file that never enter version control (build-tree only, same
pattern as `cmake/GlintfxWaylandProtocols.cmake` uses for the Wayland
binding). Those generated files are **not** Apache-2.0: each one is
`glintfx`'s own code (a mechanical restatement of a public, standardized
function list, not a copy of any creative expression in `gl.xml` itself),
licensed AGPL-3.0-or-later like the rest of this project. Every generated
file carries three lines at the top recording exactly this: that it is
generated at build time, the Khronos attribution, and that the generator
itself is AGPL-3.0-or-later. This mirrors what Khronos's own official
generator does with the same input (its own output is labeled MIT, a
different and more permissive license than the Apache-2.0 input): the
copyright holder of the registry does not treat generated output as bound to
the registry's own license.

**Where this analysis stops:** this is the project's own reading of the
license text and the CLO's technical orientation, not a legal opinion. It
does not, and cannot, affirm that the generated output carries zero legal
risk in every jurisdiction; that would require a lawyer, for a question
(API-shape derivation) that has no settled answer in general. See
`GODS_LAWS.md` L-07's "EXCECAO No 1" (its own EXCEPTION Number 1) for the
CLO's full note on this limit.
