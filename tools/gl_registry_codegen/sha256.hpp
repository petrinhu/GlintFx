// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>

// sha256.hpp - GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1).
//
// Single subject: SHA-256, hand-written (no OpenSSL/libcrypto/third-
// party hash library - GODS_LAWS.md L-07 is dependency zero for THIS
// project's own tooling too, not only the shipped library; a build-
// time tool depending on a system crypto library would be exactly the
// kind of dependency the law exists to prevent, one exception already
// spent on the DATA this tool reads, not a licence to add a second one
// for a hash function).
//
// Used for exactly one thing (main.cpp): proving, on every build, that
// the vendored third_party/khronos/gl.xml is still byte-for-byte the
// file recorded in third_party/khronos/README.md - the permanent proof
// behind that README's "verbatim, not modified" claim (GODS_LAWS.md
// L-07 EXCECAO No 1, obligation 3).

namespace glintfx::gl_codegen {

// Returns the 64-character lowercase hex digest of `data`.
[[nodiscard]] std::string sha256_hex(std::string_view data);

} // namespace glintfx::gl_codegen
