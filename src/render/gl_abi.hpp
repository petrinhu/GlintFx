// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>

// gl_abi.hpp - GL-LOADER (TODO.md, GODS_LAWS.md L-07, L-19, L-31).
//
// Hand-written, NOT generated: the raw C ABI shapes the GL 3.3 core
// function pointer table (generated at build time from the vendored
// third_party/khronos/gl.xml, GODS_LAWS.md L-07 EXCECAO No 1) depends
// on. Single subject: exactly the base GL types and the calling
// convention the resolved GL 3.3 core commands use - measured, not
// guessed (see registry_parser.hpp's own commit for how the 344-
// function set was resolved; this file's 18 type aliases are the
// distinct <ptype> names that set actually references, enumerated by
// scanning it, not assumed from a system header).
//
// WHY THIS FILE EXISTS INSTEAD OF #include <GL/gl.h> OR <GL/glcorearb.h>:
// GODS_LAWS.md L-07 forbids depending on ANY system GL header for the
// SHIPPED library, even though such a header happens to exist on this
// machine (see TODO.md's GL-LOADER row) - a consumer on a machine
// without GL development headers installed (common: the OpenGL ABI is
// provided by the display driver, but the HEADERS are a separate,
// optional development package) would fail to build against glintfx.
// These aliases are internal (never under include/glintfx/), scoped
// to glintfx::render, so they never collide with a REAL <GL/gl.h> a
// consumer's own code might include elsewhere in the same program -
// different namespace, same names, C++ scoping makes both coexist.
//
// Each width is chosen to match the real GL ABI's own definition
// (found by reading glcorearb.h on this machine and cross-checking
// against gl.xml's <types> section), not the CONVENTIONAL name of the
// underlying C type - GLenum is `unsigned int` in every real GL
// header, and std::uint32_t is bit-and-alignment-identical to
// `unsigned int` on all five of this project's target platforms
// (LP64 Linux/macOS-style and LLP64 Windows both keep `int`/
// `unsigned int` at 32 bits) - a calling convention only cares about
// size and register class, never the C++ type name, so this does not
// need to be the EXACT same type, only the exact same shape.
namespace glintfx::render {

using GLenum = std::uint32_t;
using GLboolean = std::uint8_t;
using GLbitfield = std::uint32_t;
using GLbyte = std::int8_t;
using GLubyte = std::uint8_t;
using GLshort = std::int16_t;
using GLushort = std::uint16_t;
using GLint = std::int32_t;
using GLuint = std::uint32_t;
using GLsizei = std::int32_t;
using GLfloat = float;
using GLdouble = double;
using GLchar = char;
using GLintptr = std::intptr_t;
using GLsizeiptr = std::ptrdiff_t;
using GLint64 = std::int64_t;
using GLuint64 = std::uint64_t;

// Opaque handle (GLsync is `struct __GLsync *` in every real GL
// header - a pointer to a type whose layout the driver owns and this
// project never needs to see, matching GODS_LAWS.md L-19's opacity
// principle for exactly this reason).
struct gl_sync_opaque;
using GLsync = gl_sync_opaque *;

static_assert(sizeof(GLenum) == 4, "GLenum must match the real GL ABI's 32-bit unsigned int");
static_assert(sizeof(GLboolean) == 1, "GLboolean must match the real GL ABI's 8-bit unsigned char");
static_assert(sizeof(GLint) == 4, "GLint must match the real GL ABI's 32-bit int");
static_assert(sizeof(GLsync) == sizeof(void *),
              "GLsync must stay a bare pointer, never a fat handle");

// APIENTRY (GODS_LAWS.md L-31: WGL on Windows, EGL on Linux) - every
// real GL implementation uses __stdcall on 32-bit Windows and the
// platform default calling convention everywhere else (64-bit
// Windows has only one calling convention, so __stdcall there is a
// no-op annotation, not a behavior change). A GL function pointer
// whose type omits this on Windows corrupts the stack on any call -
// this one macro is the entire cross-platform ABI difference this
// project's GL 3.3 core loader has to account for.
#if defined(_WIN32)
#define GLINTFX_GL_APIENTRY __stdcall
#else
#define GLINTFX_GL_APIENTRY
#endif

} // namespace glintfx::render
