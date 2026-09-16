// SPDX-License-Identifier: AGPL-3.0-or-later
#include "deferred_simple_match.hpp"

#include <vector>

#include "gfss/ascii_case.hpp"
#include "gfui/complex_match.hpp"
#include "gfui/node_query.hpp"

// deferred_simple_match.cpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-40; docs/plano-w6-folha-de-estilo.md, fatia S-2): the
// algorithm behind deferred_simple_match.hpp's own judge_deferred_
// simple_selectors() - see that file's own header comment for scope
// and what stays out of it.
//
// "REJEICAO VENCE ADIAMENTO", SAME ORDER compound_match.cpp'S OWN
// match_compound() ALREADY USES: a compound carrying `:scope` AND
// `:placeholder-shown` where `:scope` fails is rejected outright, the
// `:placeholder-shown` gap never even reached - the SAME short-circuit
// this whole track applies everywhere (GODS_LAWS.md L-40's own "a
// resposta certa custa o mínimo necessario para ser certa").

namespace glintfx::gfui::detail {

namespace {

// D-W6-5: with an explicit scope, `:scope` holds ONLY for that exact
// node (identity, both the node pointer AND the tree context must
// agree - two views from DIFFERENT trees that happen to share a raw
// node pointer are not the same node, node_view.hpp's own three-
// pointer shape). Without one (`scope.node == nullptr`), `:scope`
// falls back to CSS Selectors 4's own SS3.5 rule - "equivalent to
// :root" - which this eight-fact contract answers as "no parent at
// all" (fact 6's own null-means-root convention, node_query.hpp's own
// is_null(parent(...))).
[[nodiscard]] bool node_satisfies_scope(const gltfx_node_view &node,
                                        const gltfx_node_view &scope) noexcept {
    if (!is_null(scope)) {
        return node.node == scope.node && node.tree == scope.tree;
    }
    return is_null(parent(node));
}

// D-W6-2's own dossier answer (SS5.5): `:not(s1, s2, ...)` holds iff
// NONE of the list matches `node` AS SUBJECT, with the SAME scope this
// compound's own query carries (a `:scope` inside `:not()`'s own
// argument still anchors to the outer query's scope, never a fresh
// one - `:not()` narrows WHAT is judged, never WHERE the query
// started). Recurses through match_complex.hpp's own match_complex(),
// the two-way .cpp-level dependency match_verdict.hpp's own header
// comment already names - each nested selector runs the FULL two-
// level match (compound and combinator), never compound_match() alone
// (a nested selector can itself carry combinators, e.g. "div:not(.a
// > .b)"). `any_deferred` folds in a nested selector whose own answer
// is still open (it might carry ITS OWN placeholder-shown/pseudo-
// element), matching this whole file's "rejeicao vence adiamento,
// adiamento vence acerto silencioso" order.
[[nodiscard]] match_verdict
judge_not(const std::vector<style::detail::gfss_complex_selector> &not_selectors,
          const gltfx_node_view &node, const gltfx_node_view &scope) noexcept {
    bool any_deferred = false;
    for (const style::detail::gfss_complex_selector &nested : not_selectors) {
        const match_verdict nested_verdict = match_complex(nested, node, scope);
        if (nested_verdict == match_verdict::matched) {
            // One argument matched THIS node - ":not()" fails outright,
            // "rejeicao vence adiamento" applies even here.
            return match_verdict::rejected;
        }
        if (nested_verdict == match_verdict::deferred) {
            any_deferred = true;
        }
        // nested_verdict == rejected contributes nothing bad: this one
        // argument does not stand in the way of ":not()" holding.
    }
    return any_deferred ? match_verdict::deferred : match_verdict::matched;
}

} // namespace

match_verdict judge_deferred_simple_selectors(const style::detail::gfss_compound_selector &compound,
                                              const gltfx_node_view &node,
                                              const gltfx_node_view &scope) noexcept {
    bool any_deferred = false;
    for (const style::detail::gfss_simple_selector &simple : compound.simple_selectors) {
        switch (simple.kind) {
        case style::detail::gfss_simple_selector_kind::pseudo_class:
            if (glintfx::style::detail::ascii_case_insensitive_equal(simple.name, "scope")) {
                if (!node_satisfies_scope(node, scope)) {
                    return match_verdict::rejected;
                }
                break;
            }
            // Every other pseudo_class name this switch can see here is
            // `:placeholder-shown` (compound_match.cpp's own note_
            // pseudo_class_selector() is the ONLY thing that marks a
            // compound has_deferred_requirement for a pseudo_class, and
            // it only ever does so for the two names neither the state
            // table nor structural_match.hpp's own table claims) - docs/
            // node-view-and-matching.md's own gap 1, still unresolved by
            // product decision, still deferred through here too.
            any_deferred = true;
            break;
        case style::detail::gfss_simple_selector_kind::pseudo_function:
            if (glintfx::style::detail::ascii_case_insensitive_equal(simple.name, "not")) {
                const match_verdict not_verdict = judge_not(simple.not_selectors, node, scope);
                if (not_verdict == match_verdict::rejected) {
                    return match_verdict::rejected;
                }
                if (not_verdict == match_verdict::deferred) {
                    any_deferred = true;
                }
                break;
            }
            // Every other pseudo_function name reaching this switch is
            // owned elsewhere (the four An+B ones, GFSS-MATCH-STRUCT)
            // and never marks has_deferred_requirement in the first
            // place - compound_match() would have already judged it
            // (and possibly rejected before this function is even
            // called), so nothing to do here.
            break;
        case style::detail::gfss_simple_selector_kind::pseudo_element:
            // `::before`/`::after` (LAYOUT-PSEUDO-BOXES's own future
            // scope, never this fatia's) - always deferred, same as
            // compound_match.cpp's own PASS 1 already marks it.
            any_deferred = true;
            break;
        case style::detail::gfss_simple_selector_kind::type:
        case style::detail::gfss_simple_selector_kind::universal:
        case style::detail::gfss_simple_selector_kind::class_selector:
        case style::detail::gfss_simple_selector_kind::id_selector:
        case style::detail::gfss_simple_selector_kind::attribute:
            // Owned by compound_match.cpp itself - already judged
            // BEFORE this function was ever called (it only runs once
            // match_compound() has already returned `deferred` for the
            // SAME compound), nothing left to check here.
            break;
        }
    }
    return any_deferred ? match_verdict::deferred : match_verdict::matched;
}

} // namespace glintfx::gfui::detail
