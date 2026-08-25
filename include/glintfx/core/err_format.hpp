// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glintfx/core/err.hpp>
#include <glintfx/core/err_code.hpp>

// core/err_format.hpp - CE-5 of CORE-ERROR (TODO.md, GODS_LAWS.md
// L-17/L-22): enumerates a gltfx_err as (name, value) TOKENS, for a
// consumer to build ITS OWN message in ITS OWN language.
//
// DESIGN DECISION, NOT A v1 GAP - THIS NEVER CHANGES (decision of the
// leader, TODO.md CORE-ERROR row, 25/08/2026): the structured data
// below IS the whole translation story. glintfx NEVER ships a message
// catalog, in this slice or any future one - "code=parse_failure
// path=scene.rcss line=12" is the complete output; a consumer reading
// those tokens decides whether to render "Falha ao analisar
// scene.rcss na linha 12" or "Parse failed at scene.rcss:12" or a
// machine log line, in whatever language and format THEY choose.
// glintfx choosing the sentence would mean choosing the CONSUMER's
// language, which an aberto-e-desconhecido consumer base (LEI ZERO)
// makes impossible to get right even once, let alone for every locale.
//
// VOCABULARY, NOT PROSE: every `name` below is a stable identifier
// (the literal C++ field names from err.hpp: "code", "path", "line",
// ...), and the `code` field's own value is
// gltfx_err_code_name(err.code()) - CE-1's identifier lookup,
// literally built "safe to log or match on", reused here for exactly
// that. tests/err_format_test.cpp's own vocabulary case asserts on
// this: no name contains a space, and the `code` value never does
// either - the shape a natural-language sentence would have and an
// identifier token never does.
//
// ABSENT FIELDS ARE OMITTED, NOT EMPTY (GODS_LAWS.md L-22 read the
// other way): err.hpp's own CE-3 convention already answers "was this
// diagnostic field ever attached?" with 0/empty; this file reads that
// SAME signal to decide whether the token appears in the output AT
// ALL. A consumer scanning the returned vector never sees `line=0`
// for an error that never had a line - it sees no "line" token, which
// is a stronger, less error-prone signal to build a message from than
// a zero a caller could mistake for a real line number.
//
// HEADER-ONLY BY DESIGN, NOT OVERSIGHT (the CE-2 lesson, applied
// here): gltfx_err_fields() below is `inline`, calling only gltfx_err's
// ALREADY-EXPORTED plain accessors. If this were instead an EXPORTED
// function returning std::vector<gltfx_err_field> BY VALUE across the
// .so/.dll boundary, the vector's buffer and every gltfx_err_field's
// std::string buffer would be ALLOCATED inside the library and freed
// by the CONSUMER'S code when their local variable goes out of scope -
// on Windows, exactly the cross-CRT allocate-here/free-there heap
// corruption err.hpp's own "LIFECYCLE" paragraph exists to forbid for
// gltfx_err itself. Staying header-only means every allocation this
// file performs happens ENTIRELY inside the consumer's own compiled
// code, both ends, no boundary crossed at all.

namespace glintfx {

// Not ABI-frozen (GODS_LAWS.md L-19/L-26): unlike gltfx_err, this type
// never crosses the library boundary as a parameter or return of an
// EXPORTED function (see "HEADER-ONLY BY DESIGN" above) - it is
// constructed and destroyed entirely inside the consumer's own
// compiled code, so its layout is ordinary source/API compatibility,
// not the frozen-footprint contract gltfx_err's own static_assert
// enforces.
struct gltfx_err_field {
    std::string_view name;
    std::string value;
};

// Enumerates `err` as (name, value) tokens - `code` first, always
// present (a gltfx_err always carries one); every CE-3 diagnostic
// field absent by its own 0/empty convention is OMITTED, not included
// with an empty value. See the header comment above for why this
// shape is permanent, not a placeholder for a future sentence-
// producing overload.
[[nodiscard]] inline std::vector<gltfx_err_field> gltfx_err_fields(const gltfx_err &err) {
    std::vector<gltfx_err_field> out;
    out.push_back({"code", std::string(gltfx_err_code_name(err.code()))});

    // Each guard reads the SAME 0/empty "was this ever attached?"
    // signal err.hpp's own CE-3 accessors already establish - see the
    // "ABSENT FIELDS ARE OMITTED" paragraph above.
    if (!err.path().empty()) {
        out.push_back({"path", std::string(err.path())});
    }
    if (err.line() != 0) {
        out.push_back({"line", std::to_string(err.line())});
    }
    if (err.column() != 0) {
        out.push_back({"column", std::to_string(err.column())});
    }
    if (err.byte_offset() != 0) {
        out.push_back({"byte_offset", std::to_string(err.byte_offset())});
    }
    if (!err.rejected_value().empty()) {
        out.push_back({"rejected_value", std::string(err.rejected_value())});
    }
    if (err.os_error_code() != 0) {
        out.push_back({"os_error_code", std::to_string(err.os_error_code())});
    }

    return out;
}

} // namespace glintfx
