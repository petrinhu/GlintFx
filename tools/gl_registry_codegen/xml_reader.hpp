// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// xml_reader.hpp - GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1).
//
// Single subject: turns a byte stream into a flat sequence of
// element/text events. This is a minimal, non-validating pull reader
// for the narrow XML subset gl.xml actually uses (no third-party XML
// library - the exception vendors ONE data file, not a parsing
// dependency). It knows NOTHING about gl.xml's own vocabulary
// (<feature>, <command>, <proto>...); registry_parser.hpp owns that,
// on top of this reader's events.
//
// A self-closing element (<command name="x"/>) is reported as a
// SINGLE element_start event with self_closing = true - the caller
// never waits for a matching element_end for it, because every
// self-closing element this project reads (<command name=.../>,
// <enum name=.../>, <alias name=.../>, <glx .../>) has no children to
// look inside. This is a deliberate simplification, not an
// oversight: a general-purpose reader would need a one-event lookahead
// buffer to split "start" and "end" into two calls; this project never
// needs that split, so it is not built.
//
// What this reader does NOT support, because gl.xml (in the sections
// this project reads: <commands>, <feature>) never uses it: CDATA
// sections, XML namespaces, and a DOCTYPE internal subset containing
// '>' inside brackets. <!DOCTYPE ...> and <?xml ...?> are skipped
// wholesale (found by their opening marker, discarded up to the next
// '>' or "?>"), and <!-- ... --> comments are skipped and never
// surface as events.

namespace glintfx::gl_codegen {

enum class xml_event_kind { element_start, element_end, text, end_of_document };

struct xml_attribute {
    std::string name;
    std::string value;
};

struct xml_event {
    xml_event_kind kind = xml_event_kind::end_of_document;
    std::string name;                      // element_start / element_end
    std::vector<xml_attribute> attributes; // element_start only, empty otherwise
    std::string text;                      // text only, empty otherwise
    bool self_closing = false;             // element_start only
};

// Reads exactly one attribute's value out of an element_start event, or
// an empty string_view when the attribute was not present. Never
// undefined behavior on a missing attribute (GODS_LAWS.md docs/
// api-conventions.md R4's spirit: absent reads back empty, not UB) -
// this is a build-time tool, not a public API, so it returns a plain
// string_view rather than a gltfx_rslt<T>.
[[nodiscard]] std::string_view find_attribute(const xml_event &event,
                                              std::string_view attribute_name);

// Minimal non-validating pull reader. See the file header comment
// above for the exact subset it supports.
class xml_reader {
  public:
    explicit xml_reader(std::string_view document) : m_document(document) {}

    // Fills `out` with the next event and returns true. Returns an
    // end_of_document event exactly once when the input is exhausted,
    // then returns false on every call after that - a caller that
    // keeps calling past the end gets a clear "stop" signal instead of
    // silently re-reporting end_of_document forever.
    [[nodiscard]] bool next(xml_event &out);

  private:
    std::string_view m_document;
    std::size_t m_pos = 0;
    bool m_reported_end = false;
};

} // namespace glintfx::gl_codegen
