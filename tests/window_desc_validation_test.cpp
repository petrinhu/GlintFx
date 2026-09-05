// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/window/window_desc_validation.hpp"

// window_desc_validation_test.cpp - W-C' (docs/plano-w6a-janela.md
// fatia 4, D-W6a-23): the four cases the plan itself names for this
// slice - NUL, invalid UTF-8, an empty field accepted, and a valid
// accented title accepted. Proves validate_window_text_field() catches
// BOTH reasons a backend could otherwise diverge on the same input
// (this file's own header comment, and window_desc_validation.hpp's,
// explain why silence here is not an option) - it never decodes UTF-8
// itself (utf8_validation_test.cpp already owns that atom).

GLINTFX_TEST(embedded_nul_byte_is_rejected) {
    // "app" + NUL + "hidden" as ONE field, four characters after the
    // NUL a caller might expect to be part of the same title -
    // std::string_view carries the length explicitly, so the embedded
    // NUL does not truncate the VIEW itself, only whichever backend's
    // C-string API reads it later. See window_desc_validation.hpp's
    // header comment for why that divergence is exactly what this
    // check exists to catch before either backend ever sees it.
    const std::string_view value{"app\0hidden", 10};

    const glintfx::gltfx_rslt<void> result =
        glintfx::platform::validate_window_text_field("title", value);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view{"title"});
}

GLINTFX_TEST(invalid_utf8_is_rejected) {
    // Same truncated-sequence byte pattern utf8_validation_test.cpp's
    // own truncated_sequence_is_invalid case uses - this test proves
    // the FIELD-LEVEL wiring (field name comes back in rejected_
    // value(), not the malformed bytes), not the decoding itself.
    const std::string_view value{"euro \xe2\x82", 7};

    const glintfx::gltfx_rslt<void> result =
        glintfx::platform::validate_window_text_field("application_id", value);

    GLINTFX_CHECK(result.has_error());
    GLINTFX_CHECK(result.error().code() == glintfx::gltfx_err_code::invalid_argument);
    GLINTFX_CHECK(result.error().rejected_value() == std::string_view{"application_id"});
}

GLINTFX_TEST(empty_field_is_accepted) {
    // v1 never requires a title (window_desc_validation.hpp's own
    // header comment): an empty field is not a malformed one.
    const glintfx::gltfx_rslt<void> result =
        glintfx::platform::validate_window_text_field("title", std::string_view{});

    GLINTFX_CHECK(!result.has_error());
}

GLINTFX_TEST(valid_accented_title_is_accepted) {
    const glintfx::gltfx_rslt<void> result = glintfx::platform::validate_window_text_field(
        "title", "Configura\xc3\xa7\xc3\xa3o"); // "Configuração"

    GLINTFX_CHECK(!result.has_error());
}
