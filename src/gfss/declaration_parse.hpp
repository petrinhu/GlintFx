// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <vector>

#include <glintfx/gfss/token.hpp>

#include "declaration_ast.hpp"

// declaration_parse.hpp - GFSS-DECL-PARSE, DP-7 (TODO.md wave W5,
// GODS_LAWS.md L-17/L-19/L-20/L-22/L-27/L-28/L-40, plan
// /var/tmp/glintfx-plan/gfss-decl-parse.md SS2 D-DP-8): reads ONE
// declaration candidate (a span declaration_split.hpp's own function
// already found, `;`-delimited) into either an ACCEPTED gfss_
// declaration or a REJECTED diagnostic - the orchestrator this fatia's
// own plan names ("e o orquestrador de uma declaracao - encaminha, nao
// decide"): every real decision (name resolution, the flag, the value's
// own shape) lives in the file that owns it; this one only sequences
// them, D-DP-8's own fixed order:
//
//   name -> (refused, hint? -> rejected) -> (shorthand accepted? ->
//   crude, segue para !important) -> registry id (desconhecido ->
//   rejected) -> ':' (ausente -> rejected) -> tokens do valor ate o fim
//   da fatia -> tirar !important do fim (sobrou '!' -> rejected) ->
//   valor vazio -> rejected -> universal sozinha? -> forma universal ->
//   contrato da propriedade: cor / simples / cru -> validar -> rejected
//   ou aceita -> aviso property_reserved se aplicavel.

namespace glintfx::style::detail {

struct declaration_parse_result {
    bool accepted = false;
    gfss_declaration declaration{};
    gltfx_gfss_diagnostic diagnostic{}; // valid iff !accepted
};

// NOT noexcept: `declaration` can hold std::vector<gltfx_gfss_value>/
// std::vector<gltfx_gfss_token>, built with push_back - the same reason
// declaration_value_check.hpp's own check_declaration_value() is not
// either.
[[nodiscard]] declaration_parse_result
parse_declaration(const std::vector<gltfx_gfss_token> &tokens, std::size_t begin, std::size_t end);

} // namespace glintfx::style::detail
