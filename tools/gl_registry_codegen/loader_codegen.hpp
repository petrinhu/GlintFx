// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "gl_registry_codegen/registry_parser.hpp"

// loader_codegen.hpp - GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO
// No 1, L-17, L-19, L-31). Single subject: turns the resolved GL 3.3
// core command list (registry_parser.hpp's own domain model) into the
// TEXT of the two generated files - never writes to disk itself, that
// is main.cpp's job, so this stays trivially testable with plain
// string assertions against small fixtures.
//
// The three-line attribution GODS_LAWS.md L-07 EXCECAO No 1 requires
// on every generated file (generated at build time, Khronos
// attribution, generator itself AGPL-3.0-or-later) is rendered by
// BOTH functions below, not shared through a helper: the header and
// the source each need their own SPDX line as their VERY FIRST line
// (a build tool, clang-format, or a human opening either file in
// isolation, has to see it immediately, not after a #include).

namespace glintfx::gl_codegen {

// GL-CODEGEN-HOST-TOOL (TODO.md, GODS_LAWS.md L-17/L-40, D-11 of
// docs/plano-fecho-w7b.md): the "aperto de mao de versao" a cross
// build's configure step reads via `gl_registry_codegen --codegen-abi`
// (main.cpp) BEFORE trusting a host-provided binary pointed at by
// GLINTFX_GL_CODEGEN_EXECUTABLE. The dor this guards against is
// documented, not hypothetical: protobuf's own tool-path variable
// silently ignored in one build mode
// (https://github.com/protocolbuffers/protobuf/issues/14576), and a
// tool built from a different checkout generating incompatible code
// (https://github.com/microsoft/onnxruntime/issues/8413).
//
// A LITERAL, not derived from anything else, on purpose - the SAME
// double-key pattern src/render/CMakeLists.txt already uses for
// third_party/khronos/gl.xml's own sha256 (GODS_LAWS.md L-07 EXCECAO
// No 1, obligation 3): cmake/GlintfxGlCodegenHostTool.cmake carries
// its OWN copy of this exact number, hardcoded there too. If this
// generator's output FORMAT ever changes in a way an older/newer tool
// binary could not safely stand in for, bump BOTH copies in the SAME
// commit - a value derived from one side automatically would defeat
// the whole point of a handshake between two SEPARATE binaries that
// were not necessarily built from the same source tree.
inline constexpr int codegen_abi_version = 1;

// Renders the generated PRIVATE header (never installed, never under
// include/glintfx/ - GODS_LAWS.md L-19: the public surface does not
// move for an internal loading detail): the PFNGL*PROC typedef and
// gl_function_table struct field for every command, plus the
// load_gl_functions() declaration.
[[nodiscard]] std::string render_header(const std::vector<gl_command> &commands);

// Renders the generated .cpp defining load_gl_functions(): calls
// glintfx::render::try_assign_gl_function_pointer() once per command,
// tracks the first missing name, and returns
// glintfx::gltfx_rslt<gl_function_table> (docs/api-conventions.md R1:
// one envelope, no second form) - err() carries
// glintfx::gltfx_err_code::not_found with with_rejected_value() set to
// the first function name that failed to resolve.
//
// `header_include_path` is the #include line's own argument (e.g.
// "generated/render/gl_functions.hpp") - a parameter, not a hardcoded
// literal, so a test can point it at a throwaway name without coupling
// to the real build-tree path main.cpp uses.
[[nodiscard]] std::string render_source(const std::vector<gl_command> &commands,
                                        std::string_view header_include_path);

} // namespace glintfx::gl_codegen
