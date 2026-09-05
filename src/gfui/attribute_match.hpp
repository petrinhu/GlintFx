// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfui/node_view.hpp>

#include "gfss/selector_ast.hpp"

// attribute_match.hpp - GFSS-MATCH-ATTR (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-40; docs/node-view-and-matching.md's own
// "What is not judged yet" section, item "Attribute selectors"): the
// evaluator behind compound_match.cpp's own switch, filling in the
// `attribute` case that section names as deferred until this fatia
// existed. Decides whether ONE attribute simple selector ("[foo]",
// "[foo=bar]", ...) holds against ONE node - never looks at any other
// node, the SAME single-node contract compound_match.hpp's own
// match_compound() already documents for its callers.
//
// SEPARATE FILE FROM compound_match.cpp/.hpp - deliberate (GODS_LAWS.md
// L-17: "cada fatia distinta com sua fronteira propria"): this file
// answers ONE question ("does this attribute requirement hold"), never
// how a whole compound assembles its verdict from several requirements
// (id/state/tag/classes/attribute/structural) - that assembly stays in
// compound_match.cpp, which calls into this file the same way it
// already calls into gfui/node_query.hpp and gfui/state_pseudo_class_
// table.hpp for its own owned requirements.
//
// SIX OPERATORS, NOT SEVEN (GODS_LAWS.md L-27, fact vs inference,
// selector_ast.hpp's own header comment on GFSS-SEL-PARSE-ATTR):
// TODO.md's own GFSS-MATCH-ATTR row says "os 7 operadores" and then
// names exactly six by symbol (=, ~=, |=, ^=, $=, *=), with bare
// presence named SEPARATELY in the same sentence - gfss_attribute_
// operator (selector_ast.hpp) holds exactly those six, and presence is
// its own bool (gfss_simple_selector::has_attribute_value), never a
// fabricated seventh operator. This file evaluates PRESENCE alone
// (has_attribute_value == false) plus the six named operators - seven
// evaluated OUTCOMES, six operator VALUES, and the discrepancy in the
// service order's own wording is the fatia's to report, not to resolve
// (GODS_LAWS.md L-18's own "fato separado de inferencia").
//
// CASE POLICY, AND WHY NAME AND VALUE DIFFER (docs/node-view-and-
// matching.md's own "case-sensitivity rule": "a class name and an id
// are compared exactly, byte for byte - they are identifiers the
// document's author chose"): the attribute NAME is never compared by
// this file at all - it is handed, byte for byte, to the consumer's
// own gltfx_node_facts::attribute() callback (via gfui/node_query.hpp's
// own attribute() forwarder), which does whatever lookup the
// consumer's own tree considers correct for ITS OWN name convention.
// This file has no opinion on attribute-NAME case, by construction: it
// never touches the name once it hands it off, and comparing it a
// second time here would just as easily disagree with whatever the
// consumer's own lookup already decided. The attribute VALUE is the
// other half of the pair, and IS compared here, by this file's own
// code - and it follows the SAME "author-chosen identifier" reasoning
// compound_match.cpp already applies to class/id (D-MS-5): an
// attribute's value is content the document's author wrote, not
// vocabulary the leaf grammar defines, so every operator below compares
// it EXACT, byte for byte. gfss's own documentation (read under
// GODS_LAWS.md L-28/L-29) specifies no case-insensitivity flag for
// attribute selectors (unlike CSS Selectors Level 4's own optional
// "i"/"s" suffix), and selector_ast.hpp's own gfss_simple_selector
// carries no such flag either - so there is nothing here to opt into
// even if a future fatia wanted to.

namespace glintfx::gfui::detail {

// `simple` MUST have `simple.kind == gfss_simple_selector_kind::
// attribute` - compound_match.cpp's own collect_requirements() is the
// only caller, and it only ever routes attribute-kind simple selectors
// here (the SAME "caller already filtered by kind" precondition
// structural_match.hpp's own two evaluators carry). noexcept: no
// allocation anywhere in this evaluator (plan's own zero-allocation
// discipline for the whole matcher), the same guarantee every
// gltfx_node_facts callback already carries across the ABI boundary.
[[nodiscard]] bool attribute_selector_holds(const style::detail::gfss_simple_selector &simple,
                                            const gltfx_node_view &node) noexcept;

} // namespace glintfx::gfui::detail
