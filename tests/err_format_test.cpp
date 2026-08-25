// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>
#include <glintfx/core/err_format.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// err_format_test.cpp - CE-5 of CORE-ERROR (TODO.md, GODS_LAWS.md
// L-20/L-40): proves gltfx_err_fields() emits ONLY the fields that
// were actually attached (absent is OMITTED, never present-with-a-
// zero-or-empty-value), that attached fields round-trip to the
// correct textual token, and - the design decision the brief calls
// out explicitly - that the vocabulary is IDENTIFIER TOKENS, never a
// natural-language sentence.

namespace {

bool field_named(const std::vector<glintfx::gltfx_err_field> &fields, std::string_view name) {
    for (const auto &f : fields) {
        if (f.name == name) {
            return true;
        }
    }
    return false;
}

const glintfx::gltfx_err_field *find_field(const std::vector<glintfx::gltfx_err_field> &fields,
                                           std::string_view name) {
    for (const auto &f : fields) {
        if (f.name == name) {
            return &f;
        }
    }
    return nullptr;
}

} // namespace

GLINTFX_TEST(code_only_error_emits_a_single_code_token) {
    const glintfx::gltfx_err err(glintfx::gltfx_err_code::not_found);
    const auto fields = glintfx::gltfx_err_fields(err);

    GLINTFX_CHECK(fields.size() == 1);
    GLINTFX_CHECK(!fields.empty() && fields[0].name == std::string_view{"code"});
    GLINTFX_CHECK(!fields.empty() && fields[0].value == "not_found");
}

GLINTFX_TEST(attached_fields_appear_with_correct_textual_value) {
    glintfx::gltfx_err err(glintfx::gltfx_err_code::parse_failure);
    err.with_path("assets/scene.rcss")
        .with_position(12, 5)
        .with_byte_offset(4096)
        .with_rejected_value("#ffgg00")
        .with_os_error_code(-2);

    const auto fields = glintfx::gltfx_err_fields(err);

    const auto *code = find_field(fields, "code");
    GLINTFX_CHECK(code != nullptr && code->value == "parse_failure");

    const auto *path = find_field(fields, "path");
    GLINTFX_CHECK(path != nullptr && path->value == "assets/scene.rcss");

    const auto *line = find_field(fields, "line");
    GLINTFX_CHECK(line != nullptr && line->value == "12");

    const auto *column = find_field(fields, "column");
    GLINTFX_CHECK(column != nullptr && column->value == "5");

    const auto *byte_offset = find_field(fields, "byte_offset");
    GLINTFX_CHECK(byte_offset != nullptr && byte_offset->value == "4096");

    const auto *rejected = find_field(fields, "rejected_value");
    GLINTFX_CHECK(rejected != nullptr && rejected->value == "#ffgg00");

    const auto *os_error = find_field(fields, "os_error_code");
    GLINTFX_CHECK(os_error != nullptr && os_error->value == "-2");

    // code + the six CE-3 diagnostic fields, all attached: exactly
    // seven tokens, nothing more.
    GLINTFX_CHECK(fields.size() == 7);
}

GLINTFX_TEST(only_touched_fields_appear_the_rest_stay_omitted) {
    glintfx::gltfx_err err(glintfx::gltfx_err_code::io_failure);
    err.with_path("only/this/one.txt");

    const auto fields = glintfx::gltfx_err_fields(err);

    GLINTFX_CHECK(field_named(fields, "code"));
    GLINTFX_CHECK(field_named(fields, "path"));
    // Never attached: must NOT appear at all - not with a zero/empty
    // value, absent entirely (the "campo ausente nao aparece na
    // saida" requirement, GODS_LAWS.md L-22 read for the formatter).
    GLINTFX_CHECK(!field_named(fields, "line"));
    GLINTFX_CHECK(!field_named(fields, "column"));
    GLINTFX_CHECK(!field_named(fields, "byte_offset"));
    GLINTFX_CHECK(!field_named(fields, "rejected_value"));
    GLINTFX_CHECK(!field_named(fields, "os_error_code"));

    GLINTFX_CHECK(fields.size() == 2);
}

GLINTFX_TEST(vocabulary_is_identifier_tokens_never_a_sentence) {
    glintfx::gltfx_err err(glintfx::gltfx_err_code::unsupported);
    err.with_path("x")
        .with_position(1, 1)
        .with_byte_offset(1)
        .with_rejected_value("y")
        .with_os_error_code(1);
    const auto fields = glintfx::gltfx_err_fields(err);

    std::size_t checked = 0;
    for (const auto &f : fields) {
        // A NAME is always one of this file's own literal identifiers
        // (name field, e.g. "path", "line"): never a sentence, so it
        // never contains a space.
        GLINTFX_CHECK(f.name.find(' ') == std::string_view::npos);
        ++checked;
    }

    // The `code` VALUE specifically is CE-1's own identifier
    // (gltfx_err_code_name()), "safe to log or match on" by that
    // header's own words - not a sentence either.
    const auto *code = find_field(fields, "code");
    GLINTFX_CHECK(code != nullptr && code->value.find(' ') == std::string::npos);

    // L-40: the count checked is printed even when everything passes.
    std::println("err_format_test: {} token name(s) checked, none contains a space", checked);
}
