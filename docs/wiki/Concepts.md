# Concepts

The words and ideas below come up throughout glintfx's documentation. Each
one is explained once, here, so no other page has to stop and re-explain
it.

## Library vs. application

An **application** (or "app") is a finished program you run directly - a
game, a text editor, a web browser. A **library** is not run directly: it
is a collection of ready-made building blocks that an application's own
source code calls into, so the application's author does not have to write
those blocks from scratch. glintfx is a library, not an application: on
its own it does not do anything visible; it exists to be used from inside
someone else's program. (Its test programs, and a future demo, are
exceptions built only to prove the library itself works - they are not
"the product".)

## Compiling, linking, and "building"

Computers do not run the C++ source code (`.cpp`/`.hpp` files) you or a
library's author writes directly. Two steps turn it into something they
can run:

1. **Compiling**: a program called a compiler translates source code into
   machine instructions, one source file at a time.
2. **Linking**: another program (often part of the same toolchain)
   combines all of those translated pieces - your own program's, plus
   every library it uses, including glintfx - into one final, runnable
   program.

"Building" a project means running both steps, usually many times over
(once per source file), in the right order. A **build system** (glintfx
uses CMake and Ninja) is the tool that works out that order and runs the
compiler and linker for you, so nobody has to type each of those commands
by hand.

## Header vs. source file

A C++ library like glintfx is made of two kinds of files:

- **Header files** (ending in `.hpp`), which describe *what* a piece of
  the library offers - the names of its functions and types - without
  necessarily containing the actual logic. A program that wants to use
  glintfx includes (`#include`s) the headers it needs.
- **Source files** (ending in `.cpp`), which contain the actual logic
  that makes those functions work. These get compiled once, when the
  library itself is built, and a consumer never needs to look inside them.

glintfx's public headers, the ones a consumer is meant to `#include`, live
under `include/glintfx/` in the source tree; everything under `src/` is
the library's own internal implementation.

## API and ABI

**API** ("Application Programming Interface") is the set of functions,
types, and rules a library promises to a programmer writing source code
against it - names, parameters, what each call does. **ABI** ("Application
Binary Interface") is a lower-level promise: whether an already-compiled
program can keep working against a newer, already-compiled copy of the
library *without being recompiled*. glintfx tracks both separately (see
the main README's ["Versioning and compatibility"](https://github.com/petrinhu/GlintFx#versioning-and-compatibility)
section) because a change can break one without breaking the other.

## Dependency, and "zero dependencies"

A **dependency** is another piece of software a project needs in order to
build or run - a library it calls into, a tool it needs at build time.
glintfx deliberately has **zero dependencies** beyond the C++ standard
library (the parts of C++ that ship with every compiler) and the operating
system's own interfaces (Wayland on Linux, Win32 on Windows, and so on).
This is a real, load-bearing design decision, not an accident: it means
image decoding, font rendering, audio mixing and similar work are all
written from scratch inside glintfx itself, rather than pulled in from
elsewhere.

## Pre-1.0, and what "0.x" means

Version numbers with a leading `0` (like `0.3.1.0`) are a long-standing
convention across software: they mean "this is not finished, and the way
you call it can still change without warning between releases." Once a
project reaches version `1.0`, that changes - see the main README's
["Versioning and compatibility"](https://github.com/petrinhu/GlintFx#versioning-and-compatibility)
section for exactly what glintfx promises before and after that point.
glintfx is `0.x` today: parts of it that exist can still change shape, and
large parts of what it aims to do do not exist yet at all. The README's
own ["Status"](https://github.com/petrinhu/GlintFx#status-pre-10-under-active-construction)
section is the up-to-date, dated statement of exactly what is real right
now.

## AGPL-3.0-or-later, in plain language

glintfx is licensed under the **GNU Affero General Public License v3.0 or
later**. A software license is the legal document that says what you are
allowed to do with a piece of software, and what you must do in exchange.
In plain language, the AGPL says: you may use, study, and change glintfx
freely, but if you distribute a program built on it, or run it as a
service other people connect to over a network, you must also offer the
*complete source code* of your own program, under the same license, to
whoever receives it. This is a **copyleft** license: it is specifically
designed to prevent someone from taking a free, open project like glintfx
and building a closed, proprietary product on top of it without giving
anything back. If you are considering glintfx for something that will not
itself be open source, read the [full license text](https://github.com/petrinhu/GlintFx/blob/main/LICENSE)
carefully, ideally with someone who can advise you on what it requires in
your specific case - this wiki page is an introduction, not legal advice.

## Where the deeper documentation lives

This wiki stays intentionally small and introductory. Once these concepts
make sense, the source of truth for everything else lives in the
repository itself, and this wiki links to it rather than repeating it:

- [`README.md`](https://github.com/petrinhu/GlintFx#readme) - what exists
  today, supported platforms, how to consume the library, licensing.
- [`docs/api-conventions.md`](https://github.com/petrinhu/GlintFx/blob/main/docs/api-conventions.md) -
  the error-handling contract every glintfx function follows.
- [`PACKAGING.md`](https://github.com/petrinhu/GlintFx/blob/main/PACKAGING.md) -
  for anyone packaging glintfx for a Linux distribution, or embedding it
  by source in their own build.
- [`CHANGELOG.md`](https://github.com/petrinhu/GlintFx/blob/main/CHANGELOG.md) -
  what changed, release by release, once releases exist.
