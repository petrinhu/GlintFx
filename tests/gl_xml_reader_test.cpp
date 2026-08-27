// SPDX-License-Identifier: AGPL-3.0-or-later
#include "harness/check.hpp"
#include "harness/test_registry.hpp"

#include "gl_registry_codegen/xml_reader.hpp"

// gl_xml_reader_test.cpp - GL-LOADER (TODO.md, GODS_LAWS.md L-20).
//
// Proves the minimal XML pull reader tools/gl_registry_codegen/
// xml_reader.hpp/.cpp against the exact shapes gl.xml's own <commands>
// and <feature> sections use: nested elements, mixed text/element
// content, quoted attributes, self-closing elements, and the two
// non-content constructs (comment, prolog) that must be skipped
// without ever surfacing as events.

using glintfx::gl_codegen::find_attribute;
using glintfx::gl_codegen::xml_event;
using glintfx::gl_codegen::xml_event_kind;
using glintfx::gl_codegen::xml_reader;

namespace {

std::vector<xml_event> read_all(std::string_view document) {
    xml_reader reader(document);
    std::vector<xml_event> events;
    xml_event event;
    while (reader.next(event)) {
        events.push_back(event);
        if (event.kind == xml_event_kind::end_of_document) {
            break;
        }
    }
    return events;
}

} // namespace

GLINTFX_TEST(plain_element_with_text_reports_start_text_end) {
    const auto events = read_all("<name>glActiveTexture</name>");
    GLINTFX_CHECK_EQ(events.size(), 4u); // start, text, end, end_of_document
    GLINTFX_CHECK(events[0].kind == xml_event_kind::element_start);
    GLINTFX_CHECK_EQ(events[0].name, "name");
    GLINTFX_CHECK(!events[0].self_closing);
    GLINTFX_CHECK(events[1].kind == xml_event_kind::text);
    GLINTFX_CHECK_EQ(events[1].text, "glActiveTexture");
    GLINTFX_CHECK(events[2].kind == xml_event_kind::element_end);
    GLINTFX_CHECK_EQ(events[2].name, "name");
}

GLINTFX_TEST(mixed_text_and_element_content_reports_text_around_child) {
    // The exact shape a <proto> or <param> element takes in gl.xml:
    // literal text, a child element, more literal text.
    const auto events = read_all("<param>const <ptype>GLchar</ptype> *const*<name>string</name></param>");
    // start param, text, start ptype, text, end ptype, text, start name,
    // text, end name, end param, end_of_document.
    GLINTFX_CHECK_EQ(events.size(), 11u);
    GLINTFX_CHECK(events[0].kind == xml_event_kind::element_start);
    GLINTFX_CHECK_EQ(events[0].name, "param");
    GLINTFX_CHECK(events[1].kind == xml_event_kind::text);
    GLINTFX_CHECK_EQ(events[1].text, "const ");
    GLINTFX_CHECK(events[2].kind == xml_event_kind::element_start);
    GLINTFX_CHECK_EQ(events[2].name, "ptype");
    GLINTFX_CHECK(events[3].kind == xml_event_kind::text);
    GLINTFX_CHECK_EQ(events[3].text, "GLchar");
    GLINTFX_CHECK(events[4].kind == xml_event_kind::element_end);
    GLINTFX_CHECK_EQ(events[4].name, "ptype");
    GLINTFX_CHECK(events[5].kind == xml_event_kind::text);
    GLINTFX_CHECK_EQ(events[5].text, " *const*");
    GLINTFX_CHECK(events[6].kind == xml_event_kind::element_start);
    GLINTFX_CHECK_EQ(events[6].name, "name");
    GLINTFX_CHECK(events[7].kind == xml_event_kind::text);
    GLINTFX_CHECK_EQ(events[7].text, "string");
}

GLINTFX_TEST(attributes_are_parsed_with_double_and_single_quotes) {
    const auto events = read_all(R"(<feature api="gl" name='GL_VERSION_3_3' number="3.3">)");
    GLINTFX_CHECK_EQ(events[0].attributes.size(), 3u);
    GLINTFX_CHECK_EQ(std::string(find_attribute(events[0], "api")), "gl");
    GLINTFX_CHECK_EQ(std::string(find_attribute(events[0], "name")), "GL_VERSION_3_3");
    GLINTFX_CHECK_EQ(std::string(find_attribute(events[0], "number")), "3.3");
}

GLINTFX_TEST(missing_attribute_reads_back_empty_never_undefined_behavior) {
    const auto events = read_all(R"(<feature api="gl">)");
    GLINTFX_CHECK(find_attribute(events[0], "profile").empty());
}

GLINTFX_TEST(self_closing_element_reports_a_single_start_event) {
    const auto events = read_all(R"(<command name="glActiveTexture"/>)");
    GLINTFX_CHECK_EQ(events.size(), 2u); // start, end_of_document
    GLINTFX_CHECK(events[0].kind == xml_event_kind::element_start);
    GLINTFX_CHECK(events[0].self_closing);
    GLINTFX_CHECK_EQ(std::string(find_attribute(events[0], "name")), "glActiveTexture");
}

GLINTFX_TEST(comment_is_skipped_and_never_surfaces_as_an_event) {
    const auto events = read_all("<a><!-- a comment with <fake> tags --></a>");
    GLINTFX_CHECK_EQ(events.size(), 3u); // start a, end a, end_of_document
    GLINTFX_CHECK(events[0].kind == xml_event_kind::element_start);
    GLINTFX_CHECK(events[1].kind == xml_event_kind::element_end);
}

GLINTFX_TEST(xml_prolog_is_skipped_and_never_surfaces_as_an_event) {
    const auto events = read_all("<?xml version=\"1.0\" encoding=\"UTF-8\"?><registry></registry>");
    GLINTFX_CHECK_EQ(events.size(), 3u);
    GLINTFX_CHECK(events[0].kind == xml_event_kind::element_start);
    GLINTFX_CHECK_EQ(events[0].name, "registry");
}

GLINTFX_TEST(entities_are_decoded_in_text_and_attribute_values) {
    const auto events = read_all(R"(<a x="1 &amp; 2">less &lt; more &gt; both</a>)");
    GLINTFX_CHECK_EQ(std::string(find_attribute(events[0], "x")), "1 & 2");
    GLINTFX_CHECK_EQ(events[1].text, "less < more > both");
}

GLINTFX_TEST(reading_past_end_of_document_returns_false) {
    xml_reader reader("<a/>");
    xml_event event;
    GLINTFX_CHECK(reader.next(event)); // element_start
    GLINTFX_CHECK(reader.next(event)); // end_of_document
    GLINTFX_CHECK(event.kind == xml_event_kind::end_of_document);
    GLINTFX_CHECK(!reader.next(event)); // nothing more, ever
}
