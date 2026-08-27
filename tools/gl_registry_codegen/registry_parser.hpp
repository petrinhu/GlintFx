// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

// registry_parser.hpp - GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO
// No 1). Single subject: turns gl.xml's own vocabulary (<commands>,
// <feature>, <require>, <remove>) into a plain domain model - the
// ordered list of GL commands a given core profile feature set
// resolves to, each with its C return-type text and parameter list.
// Built on top of xml_reader.hpp's generic events; this file is the
// only place in the tool that knows what a <feature> or a <command>
// MEANS.

namespace glintfx::gl_codegen {

struct gl_param {
    std::string type; // C type text, e.g. "const GLchar *const*"
    std::string name; // e.g. "string"
};

struct gl_command {
    std::string name;        // e.g. "glActiveTexture"
    std::string return_type; // e.g. "void", "const GLubyte *"
    std::vector<gl_param> params;
};

// Extracts every <command> declared inside gl.xml's <commands> section
// (the name -> signature table), regardless of which <feature> ever
// references it. Order is document order; registry_parser does not
// sort here, that is a decision resolve_core_profile() makes for its
// OWN caller (determinism of GENERATED output, not of this table).
[[nodiscard]] std::vector<gl_command> parse_command_signatures(std::string_view xml_document);

// Resolves the set of command NAMES that belong to a core-profile
// feature set: every <feature api="gl" number="N"> with N <=
// max_version contributes its <require> commands (profile absent or
// "core") in feature-number order, and each <remove> (profile absent
// or "core") subtracts, applied in the SAME feature-number order - the
// exact algorithm Khronos's own reference generator uses, so a command
// removed by an earlier feature and never re-required by the same or a
// later one in scope stays removed (GODS_LAWS.md L-27: this is the
// checked fact, not the "674 required minus 350 removed" shortcut,
// which double-subtracts a name re-required later and was rejected
// after producing a different, wrong total in this project's own
// verification).
[[nodiscard]] std::vector<std::string>
resolve_core_profile_command_names(std::string_view xml_document, double max_version);

// Convenience composing the two functions above: resolves the command
// NAMES for the given max_version, looks each one up in the signature
// table, and returns them sorted alphabetically by name (determinism
// of the GENERATED output - two runs of this tool against the same
// gl.xml must byte-for-byte agree, independent of any hash-map
// iteration order upstream). A name resolved by
// resolve_core_profile_command_names() that has no entry in the
// signature table is a malformed registry, not a silent skip - see
// build_gl_registry()'s own .cpp comment for why this is asserted
// rather than degraded.
[[nodiscard]] std::vector<gl_command> build_gl_registry(std::string_view xml_document,
                                                        double max_version);

} // namespace glintfx::gl_codegen
