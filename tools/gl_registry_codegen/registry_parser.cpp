// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gl_registry_codegen/registry_parser.hpp"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <set>

#include "gl_registry_codegen/xml_reader.hpp"

// GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1, L-17): each
// function below is one atom of this file's single subject (gl.xml
// vocabulary -> plain domain model). See registry_parser.hpp's own
// header comment for the algorithm this mirrors (Khronos's own
// reference generator: apply every feature's <require> then <remove>,
// in ascending feature-number order, sequentially).

namespace glintfx::gl_codegen {

namespace {

// Parses "3.3" -> 3.3. gl.xml's own <feature number="..."> attribute is
// always a plain decimal with one digit on each side of the dot in
// every version this project reads (1.0 through 4.6) - std::from_chars
// handles that exactly, with no locale-dependent behavior (unlike
// std::stod, which reads the CURRENT LOCALE's decimal separator; this
// is a build-time tool that must give the same answer regardless of
// the machine's locale).
bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// Trims only the OUTER whitespace of a reconstructed C type/name
// string (e.g. "GLbitfield " from a <ptype>...</ptype> immediately
// followed by whitespace before <name>) - internal spacing, such as
// the one separating "GLubyte" from "*" in "const GLubyte *", is part
// of the original gl.xml text nodes and must survive verbatim.
// By std::string_view, not by value: substr() below always allocates a
// NEW string regardless, so there is nothing to move out of a by-value
// parameter here (cppcheck's own passedByValue finding, TESTES.md
// T15.0 stage 3 - a real cost, not decoration: the by-value form used
// to force a copy or move at every call site for zero benefit).
std::string trim_outer_whitespace(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() && is_whitespace(text[start])) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && is_whitespace(text[end - 1])) {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

double parse_version_number(std::string_view text) {
    double value = 0.0;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

// Parses the mixed text/element content of a SINGLE <proto> or
// <param> element - the reader is already positioned right after that
// element's own element_start event. Returns once the matching
// element_end is consumed. Everything that is NOT inside a nested
// <name> element is the "type" text (this includes text nested inside
// <ptype>, exactly like the type text living directly in <proto>/
// <param> itself - GODS_LAWS.md docs/api-conventions.md's own
// methodology section for this fatia measured this live against the
// real gl.xml: <ptype> never carries anything the type text does not
// already need verbatim).
struct proto_or_param_content {
    std::string type_text;
    std::string name_text;
};

proto_or_param_content read_proto_or_param_content(xml_reader &reader) {
    proto_or_param_content content;
    int depth = 1; // one element_start already consumed by the caller
    bool inside_name = false;
    xml_event event;
    while (depth > 0 && reader.next(event)) {
        switch (event.kind) {
        case xml_event_kind::element_start:
            if (!event.self_closing) {
                ++depth;
                if (event.name == "name") {
                    inside_name = true;
                }
            }
            break;
        case xml_event_kind::element_end:
            --depth;
            if (event.name == "name") {
                inside_name = false;
            }
            break;
        case xml_event_kind::text:
            (inside_name ? content.name_text : content.type_text) += event.text;
            break;
        case xml_event_kind::end_of_document:
            depth = 0;
            break;
        }
    }
    content.type_text = trim_outer_whitespace(content.type_text);
    content.name_text = trim_outer_whitespace(content.name_text);
    return content;
}

// Parses one <command>...</command> block, given that its element_start
// was already consumed by the caller. Skips every child that is not
// <proto> or <param> (<glx>, <alias>, <vecequiv> - none of them affect
// this project's GL 3.3 core signature table).
gl_command read_command(xml_reader &reader) {
    gl_command command;
    int depth = 1;
    xml_event event;
    while (depth > 0 && reader.next(event)) {
        if (event.kind == xml_event_kind::element_end) {
            --depth;
            continue;
        }
        if (event.kind == xml_event_kind::end_of_document) {
            break;
        }
        if (event.kind != xml_event_kind::element_start) {
            continue; // stray text directly inside <command> never happens
        }
        if (event.self_closing) {
            continue; // e.g. <glx type="render" opcode="..."/>
        }
        if (event.name == "proto") {
            const proto_or_param_content content = read_proto_or_param_content(reader);
            command.name = content.name_text;
            command.return_type = content.type_text;
        } else if (event.name == "param") {
            const proto_or_param_content content = read_proto_or_param_content(reader);
            command.params.push_back(gl_param{content.type_text, content.name_text});
        } else {
            ++depth; // an unexpected child element: track it, do not misread its close as ours
        }
    }
    return command;
}

} // namespace

std::vector<gl_command> parse_command_signatures(std::string_view xml_document) {
    std::vector<gl_command> commands;
    xml_reader reader(xml_document);
    xml_event event;
    bool inside_commands = false;
    int commands_depth = 0;

    while (reader.next(event)) {
        if (event.kind == xml_event_kind::end_of_document) {
            break;
        }
        if (!inside_commands) {
            if (event.kind == xml_event_kind::element_start && event.name == "commands" &&
                !event.self_closing) {
                inside_commands = true;
                commands_depth = 1;
            }
            continue;
        }

        // Inside <commands>: only <command> start events matter; every
        // other event at this level (whitespace text between siblings,
        // the closing </commands> itself) is either skipped or ends
        // the scan.
        if (event.kind == xml_event_kind::element_end) {
            --commands_depth;
            if (commands_depth == 0) {
                break; // reached </commands>
            }
            continue;
        }
        if (event.kind != xml_event_kind::element_start) {
            continue;
        }
        if (event.self_closing) {
            continue;
        }
        if (event.name == "command") {
            commands.push_back(read_command(reader));
        } else {
            ++commands_depth; // an unexpected direct child of <commands>
        }
    }

    return commands;
}

namespace {

// Applies one <require> or <remove> block's <command name="..."/>
// children to `names`, honoring the profile filter (absent or "core"
// contributes; any other value - "compatibility" - never does, this
// project only ever targets core).
void apply_command_block(xml_reader &reader, std::set<std::string> &names, bool is_require) {
    int depth = 1;
    xml_event event;
    while (depth > 0 && reader.next(event)) {
        if (event.kind == xml_event_kind::element_end) {
            --depth;
            continue;
        }
        if (event.kind == xml_event_kind::end_of_document) {
            break;
        }
        if (event.kind != xml_event_kind::element_start) {
            continue;
        }
        if (!event.self_closing) {
            ++depth; // e.g. a nested <enum>/<type> block this project never opens further
            continue;
        }
        if (event.name != "command") {
            continue; // <enum name="..."/>, <type name="..."/>: not this project's concern
        }
        const std::string name(find_attribute(event, "name"));
        if (is_require) {
            names.insert(name);
        } else {
            names.erase(name);
        }
    }
}

} // namespace

std::vector<std::string> resolve_core_profile_command_names(std::string_view xml_document,
                                                            double max_version) {
    std::set<std::string> names;
    xml_reader reader(xml_document);
    xml_event event;

    while (reader.next(event)) {
        if (event.kind == xml_event_kind::end_of_document) {
            break;
        }
        if (event.kind != xml_event_kind::element_start || event.self_closing) {
            continue;
        }
        if (event.name != "feature") {
            continue;
        }
        const bool is_gl_api = find_attribute(event, "api") == "gl";
        const double feature_number = parse_version_number(find_attribute(event, "number"));
        const bool in_scope = is_gl_api && feature_number <= max_version;

        // Whether or not this feature is in scope, its <require>/
        // <remove> children must still be CONSUMED (advancing the
        // reader past them) so the next sibling <feature> is not
        // misread as a child of this one.
        int depth = 1;
        xml_event inner;
        while (depth > 0 && reader.next(inner)) {
            if (inner.kind == xml_event_kind::element_end) {
                --depth;
                continue;
            }
            if (inner.kind == xml_event_kind::end_of_document) {
                break;
            }
            if (inner.kind != xml_event_kind::element_start) {
                continue;
            }
            if (inner.self_closing) {
                continue;
            }
            const bool is_require = inner.name == "require";
            const bool is_remove = inner.name == "remove";
            if (!is_require && !is_remove) {
                ++depth;
                continue;
            }
            const std::string_view profile = find_attribute(inner, "profile");
            const bool profile_applies = profile.empty() || profile == "core";
            if (in_scope && profile_applies) {
                apply_command_block(reader, names, is_require);
            } else {
                // Not in scope (or a non-core profile block): still
                // has to be consumed, empty target set thrown away.
                std::set<std::string> discarded;
                apply_command_block(reader, discarded, is_require);
            }
        }
    }

    return std::vector<std::string>(names.begin(), names.end());
}

std::vector<gl_command> build_gl_registry(std::string_view xml_document, double max_version) {
    const std::vector<gl_command> all_signatures = parse_command_signatures(xml_document);
    const std::vector<std::string> resolved_names =
        resolve_core_profile_command_names(xml_document, max_version);

    std::vector<gl_command> result;
    result.reserve(resolved_names.size());
    for (const std::string &name : resolved_names) {
        const auto it =
            std::find_if(all_signatures.begin(), all_signatures.end(),
                         [&name](const gl_command &command) { return command.name == name; });
        // A name a <feature> resolves to but that has no entry in
        // <commands> is a MALFORMED registry (GODS_LAWS.md L-27: this
        // is the checked fact - the real vendored gl.xml never hits
        // this, proven by main.cpp's own count-vs-resolved-names
        // check at build time). Asserting here, rather than silently
        // skipping, is the difference between a build that fails loud
        // at the exact file that is wrong and a loader silently
        // missing a function nobody ever notices until a consumer's
        // draw call segfaults on a null pointer.
        assert(it != all_signatures.end() &&
               "resolved GL command name has no signature in <commands>");
        if (it != all_signatures.end()) {
            result.push_back(*it);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const gl_command &a, const gl_command &b) { return a.name < b.name; });
    return result;
}

} // namespace glintfx::gl_codegen
