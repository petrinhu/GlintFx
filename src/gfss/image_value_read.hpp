// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <glintfx/gfss/token.hpp>

// image_value_read.hpp - R2D-IMAGE-RESOLVER, fatia R1 (TODO.md; contrato
// docs/auditoria-api-image-resolver.md; plano de fechamento da W8, D-W8-373).
//
// Reads the VALUE of an image-bearing gfss property (`none`, `url(...)`, or a
// gradient that this slice does not yet paint) from the already-tokenized
// component values GFSS-TOKEN produced. Internal in this slice: the public
// surface that consumes it (the image reference handed to the resolver) is a
// later fatia; nothing here is a public header (GODS_LAWS.md L-19).
//
// THE TEXT IS VERBATIM (D-W8-373, GODS_LAWS.md Q-R06 / D-W8-329): the url text
// is returned exactly as it is written between the quotes or the parentheses.
// `url("a\"b.png")` yields the raw text `a\"b.png`, backslash included. The
// decoding of escapes is NOT done here; the consumer decodes, and that work is
// the INBOX item GFSS-URL-UNESCAPE.
//
// THE TEXT IS A VIEW, NEVER A COPY: `text` points INTO the lexeme of the token
// it came from, which points into the caller's source buffer. The caller's
// source must outlive the result (the same lifetime rule gltfx_gfss_token
// already carries).
//
// ERRORS ARE DIAGNOSABLE, NEVER SILENT: an `error` result carries the line and
// the column of the offending token and a `diagnostic.expected` identifier from
// the closed gfss vocabulary (diagnostic_vocabulary.hpp). A token that the
// tokenizer already flagged (bad_url, bad_string, or any token with a
// diagnostic attached) is reported with ITS OWN expected identifier.

namespace glintfx::style::detail {

enum class image_value_kind : std::uint8_t {
    none,                       // the keyword `none`: no image
    url,                        // `url(...)`: `text` holds the verbatim address
    gradient_not_yet_supported, // `linear-gradient(...)`: well-formed, out of this slice
    error,                      // malformed; `diagnostic` says where and what was expected
};

struct image_value_result {
    image_value_kind kind = image_value_kind::error;
    std::string_view text{};            // url only; a view into the token's lexeme, never a copy
    gltfx_gfss_diagnostic diagnostic{}; // error only; empty otherwise (R4)
};

// Reads ONE image value from `tokens`. Leading and trailing whitespace and the
// final <EOF-token> (as gltfx_gfss_tokenize() appends it) are ignored; whitespace
// between the tokens of `url( "a.png" )` is ignored too. Never throws, never
// allocates, never reads outside `tokens`.
[[nodiscard]] image_value_result
read_image_value(std::span<const gltfx_gfss_token> tokens) noexcept;

} // namespace glintfx::style::detail
