// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gl_registry_codegen/xml_reader.hpp"

// GL-LOADER (TODO.md, GODS_LAWS.md L-07 EXCECAO No 1, L-17): each
// function below is one atom of the single subject this file owns
// (turning bytes into events) - see xml_reader.hpp's own header
// comment for the supported subset and the self-closing-element
// simplification.

namespace glintfx::gl_codegen {

namespace {

bool is_whitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_whitespace(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_whitespace(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

// Replaces the five predefined XML entities. gl.xml's <commands> and
// <feature> sections never use a numeric character reference
// (&#NN;/&#xNN;), so none is implemented here - a future need is a
// clear, isolated addition to this one function, not a redesign.
std::string decode_entities(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size();) {
        if (raw[i] == '&') {
            if (raw.compare(i, 5, "&amp;") == 0) {
                out += '&';
                i += 5;
                continue;
            }
            if (raw.compare(i, 4, "&lt;") == 0) {
                out += '<';
                i += 4;
                continue;
            }
            if (raw.compare(i, 4, "&gt;") == 0) {
                out += '>';
                i += 4;
                continue;
            }
            if (raw.compare(i, 6, "&quot;") == 0) {
                out += '"';
                i += 6;
                continue;
            }
            if (raw.compare(i, 6, "&apos;") == 0) {
                out += '\'';
                i += 6;
                continue;
            }
        }
        out += raw[i];
        ++i;
    }
    return out;
}

// Parses `name="value"` / `name='value'` pairs out of the text between
// an element's name and its closing '>' (or the '/' of a self-closing
// tag, already stripped by the caller). Whitespace-separated, no
// escaping inside the tag content itself other than the entities
// decode_entities() already handles inside each value.
std::vector<xml_attribute> parse_attributes(std::string_view tag_content) {
    std::vector<xml_attribute> attributes;
    std::size_t i = 0;
    while (i < tag_content.size()) {
        while (i < tag_content.size() && is_whitespace(tag_content[i])) {
            ++i;
        }
        std::size_t name_start = i;
        while (i < tag_content.size() && tag_content[i] != '=' && !is_whitespace(tag_content[i])) {
            ++i;
        }
        if (i == name_start) {
            break; // no more attributes
        }
        std::string name(tag_content.substr(name_start, i - name_start));
        while (i < tag_content.size() && (is_whitespace(tag_content[i]) || tag_content[i] == '=')) {
            ++i;
        }
        if (i >= tag_content.size() || (tag_content[i] != '"' && tag_content[i] != '\'')) {
            break; // malformed, nothing more to recover
        }
        char quote = tag_content[i];
        ++i;
        std::size_t value_start = i;
        while (i < tag_content.size() && tag_content[i] != quote) {
            ++i;
        }
        std::string value = decode_entities(tag_content.substr(value_start, i - value_start));
        if (i < tag_content.size()) {
            ++i; // consume closing quote
        }
        attributes.push_back(xml_attribute{std::move(name), std::move(value)});
    }
    return attributes;
}

// Splits the raw tag content (everything between '<' and '>', name
// plus attributes) into the element name and its attribute list.
void parse_element_head(std::string_view tag_content, std::string &name_out,
                        std::vector<xml_attribute> &attributes_out) {
    std::size_t name_end = 0;
    while (name_end < tag_content.size() && !is_whitespace(tag_content[name_end])) {
        ++name_end;
    }
    name_out = std::string(tag_content.substr(0, name_end));
    attributes_out = parse_attributes(tag_content.substr(name_end));
}

} // namespace

std::string_view find_attribute(const xml_event &event, std::string_view attribute_name) {
    for (const xml_attribute &attribute : event.attributes) {
        if (attribute.name == attribute_name) {
            return attribute.value;
        }
    }
    return {};
}

bool xml_reader::next(xml_event &out) {
    if (m_reported_end) {
        return false;
    }

    // Skips every non-content construct (comment, prolog, doctype)
    // between the cursor and the next real event, so the loop below
    // never has to special-case them at the call site.
    for (;;) {
        if (m_pos >= m_document.size()) {
            out = xml_event{};
            out.kind = xml_event_kind::end_of_document;
            m_reported_end = true;
            return true;
        }

        if (m_document[m_pos] != '<') {
            break; // text content starts here
        }
        if (m_document.compare(m_pos, 4, "<!--") == 0) {
            const std::size_t close = m_document.find("-->", m_pos + 4);
            m_pos = (close == std::string_view::npos) ? m_document.size() : close + 3;
            continue;
        }
        if (m_document.compare(m_pos, 2, "<?") == 0) {
            const std::size_t close = m_document.find("?>", m_pos + 2);
            m_pos = (close == std::string_view::npos) ? m_document.size() : close + 2;
            continue;
        }
        if (m_document.compare(m_pos, 2, "<!") == 0) {
            const std::size_t close = m_document.find('>', m_pos + 2);
            m_pos = (close == std::string_view::npos) ? m_document.size() : close + 1;
            continue;
        }
        break; // a real element start or end tag starts here
    }

    if (m_pos >= m_document.size()) {
        out = xml_event{};
        out.kind = xml_event_kind::end_of_document;
        m_reported_end = true;
        return true;
    }

    if (m_document[m_pos] != '<') {
        // Text content: everything up to the next '<'.
        const std::size_t next_lt = m_document.find('<', m_pos);
        const std::size_t text_end =
            (next_lt == std::string_view::npos) ? m_document.size() : next_lt;
        out = xml_event{};
        out.kind = xml_event_kind::text;
        out.text = decode_entities(m_document.substr(m_pos, text_end - m_pos));
        m_pos = text_end;
        return true;
    }

    if (m_pos + 1 < m_document.size() && m_document[m_pos + 1] == '/') {
        // End tag: </name>
        const std::size_t close = m_document.find('>', m_pos + 2);
        const std::size_t name_end = (close == std::string_view::npos) ? m_document.size() : close;
        out = xml_event{};
        out.kind = xml_event_kind::element_end;
        out.name = std::string(trim(m_document.substr(m_pos + 2, name_end - (m_pos + 2))));
        m_pos = (close == std::string_view::npos) ? m_document.size() : close + 1;
        return true;
    }

    // Start tag, possibly self-closing: <name attr="v" .../> or <name attr="v" ...>
    const std::size_t close = m_document.find('>', m_pos + 1);
    const std::size_t content_end = (close == std::string_view::npos) ? m_document.size() : close;
    std::string_view tag_content = m_document.substr(m_pos + 1, content_end - (m_pos + 1));
    const bool self_closing = !tag_content.empty() && tag_content.back() == '/';
    if (self_closing) {
        tag_content.remove_suffix(1);
    }

    out = xml_event{};
    out.kind = xml_event_kind::element_start;
    out.self_closing = self_closing;
    parse_element_head(trim(tag_content), out.name, out.attributes);
    m_pos = (close == std::string_view::npos) ? m_document.size() : close + 1;
    return true;
}

} // namespace glintfx::gl_codegen
