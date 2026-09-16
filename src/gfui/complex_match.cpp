// SPDX-License-Identifier: AGPL-3.0-or-later
#include "complex_match.hpp"

#include <cstddef>
#include <optional>
#include <vector>

#include "gfui/combinator_step.hpp"
#include "gfui/compound_match.hpp"
#include "gfui/deferred_simple_match.hpp"
#include "gfui/node_query.hpp"

// complex_match.cpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-40; docs/plano-w6-folha-de-estilo.md, fatia S-2): the
// algorithm behind complex_match.hpp's own match_complex() - see that
// file's own header comment for direction, shape and the two-level
// compound rule.
//
// ITERATIVE, NOT RECURSIVE - ON PURPOSE, FOUND DURING REVIEW: the walk
// below is a right-to-left backtracking search over `selector`'s own
// compounds, one recursive call per compound naturally (the first
// draft of this file WAS exactly that: match_step()/match_single_
// step()/match_backtracking() calling each other). Measured against
// selector_parse.cpp's own parse_complex_selector(): the loop that
// builds `rest` (`for (;;) { ... complex_selector.rest.push_back(...)
// }`) has NO cap on how many compounds one complex selector can chain
// - unlike `:not()`'s own recursive argument (bounded by the shared
// k_max_nested_selector_list_depth, D-W6-8, because PARSING it is
// itself recursive), a plain "a a a a ... a" selector with an
// arbitrary number of descendant combinators parses successfully no
// matter how long it is, because building `rest` is a flat loop, not a
// recursive descent. LEI ZERO's own consumer base is open and
// unknown - a style sheet that size is exactly the shape of input a
// hostile or merely careless one can hand this library, and the
// research this wave read before writing any code (GODS_LAWS.md L-43)
// found real CSS engines patched for precisely this: unbounded
// selector-matching recursion turning a long, syntactically valid
// selector into a native call-stack overflow (undefined behavior, the
// class of bug a recursion cap with a silenced analyzer warning would
// only ever move the threshold of, never remove). match_complex()
// below still walks right to left with the SAME direction and backtracking semantics
// the recursive draft had (D-W6-2), but the walk is now a loop over an
// explicit, HEAP-allocated stack (`std::vector<frame>`) instead of the
// native call stack - a `std::vector` that grows past available memory
// still ends the process, but as a clean allocation failure, never as
// the memory-unsafe class of failure stack overflow is. `:not()`'s own
// recursion (match_complex() -> judge_deferred_simple_selectors() ->
// match_complex() again, for each of its arguments) is UNCHANGED and
// stays real recursion - it already has the shared, parser-enforced
// D-W6-8 budget (at most 10 levels), the exact "bounded and cited"
// shape selector_parse.cpp's own NOLINTNEXTLINE(misc-no-recursion)
// comments already use elsewhere in this codebase, so it needed no
// change here.
//
// ONE COMPOUND, TWO LEVELS, ONE HELPER: compound_verdict_at() below is
// the ONLY place this file calls compound_match()/judge_deferred_
// simple_selectors() - it answers "does THIS compound hold at THIS
// node", nothing about combinators, and every frame of the walk below
// calls it exactly once per (compound, node) pair.
//
// INDEXING (private to this file, never exposed): `head` is index 0,
// `rest[i]` is index i+1 - the walk starts at `selector.rest.size()`
// (the SUBJECT, D-W6-2's own "olha o sujeito primeiro") and proceeds
// DOWN toward index 0. The combinator immediately to the LEFT of
// compound i (connecting compound i-1 to compound i) is `rest[i-
// 1].combinator` for i >= 1, meaningless for i == 0 (compound 0 has
// nothing to its left, the exact reason gfss_complex_selector's own
// header comment gives for why `head` is a separate field, not
// `rest[0]`).

namespace glintfx::gfui::detail {

namespace {

[[nodiscard]] const style::detail::gfss_compound_selector &
compound_at(const style::detail::gfss_complex_selector &selector, std::size_t index) noexcept {
    if (index == 0) {
        return selector.head;
    }
    return selector.rest[index - 1].compound;
}

[[nodiscard]] match_verdict
compound_verdict_at(const style::detail::gfss_compound_selector &compound,
                    const gltfx_node_view &node, const gltfx_node_view &scope) noexcept {
    const match_verdict owned = match_compound(compound, node);
    if (owned != match_verdict::deferred) {
        return owned; // matched or rejected - nothing left this fatia owns to add
    }
    return judge_deferred_simple_selectors(compound, node, scope);
}

// Combines the LOCAL compound's own verdict with the UPSTREAM verdict
// the rest of the chain produced - both already known non-rejected by
// every caller below: `deferred` is infectious (either half still open
// makes the WHOLE chain still open, GODS_LAWS.md L-40's own "never a
// guessed matched"); otherwise both halves are `matched`, so the
// combination is too.
[[nodiscard]] match_verdict combine(match_verdict local, match_verdict upstream) noexcept {
    if (local == match_verdict::deferred || upstream == match_verdict::deferred) {
        return match_verdict::deferred;
    }
    return match_verdict::matched;
}

// One level of the explicit, heap-allocated stack the walk below
// pushes and pops instead of recursing - see this file's own header
// comment for why. `search_from` is the node whose own next candidate
// (combinator_next_candidate()) continues the search along this
// frame's own axis: it starts as `node` itself and, for a backtracking
// combinator, advances to the last candidate TRIED every time this
// frame is asked for another one - the SAME nearest-first walk the
// recursive draft's own match_backtracking() did, just resumable
// instead of held on the call stack. `single_step_tried` is the single-
// step equivalent: true once the ONE candidate `>`/`+` ever offer has
// already been tried, so a second ask for a candidate at this frame
// answers "none left" without re-deriving the same one (D-W6-2's own
// "sem retrocesso").
struct frame {
    std::size_t index = 0;
    match_verdict local = match_verdict::rejected;
    style::detail::gfss_combinator combinator = style::detail::gfss_combinator::descendant;
    bool backtracks = false;
    gltfx_node_view search_from;
    bool single_step_tried = false;
};

[[nodiscard]] frame make_frame(const style::detail::gfss_complex_selector &selector,
                               std::size_t index, const gltfx_node_view &node,
                               const gltfx_node_view &scope) noexcept {
    frame f{};
    f.index = index;
    f.local = compound_verdict_at(compound_at(selector, index), node, scope);
    f.search_from = node;
    if (index > 0) {
        f.combinator = selector.rest[index - 1].combinator;
        f.backtracks = (f.combinator == style::detail::gfss_combinator::descendant ||
                        f.combinator == style::detail::gfss_combinator::subsequent_sibling);
    }
    return f;
}

// The next candidate this frame's own axis offers, or nullopt when
// exhausted - `>`/`+` offer exactly one (D-W6-2's own "passo unico");
// `descendant`/`~` offer every ancestor/earlier-sibling in turn,
// nearest first, until combinator_next_candidate() itself returns a
// null view (the root, or the first sibling).
[[nodiscard]] std::optional<gltfx_node_view> next_candidate(frame &f) noexcept {
    if (!f.backtracks && f.single_step_tried) {
        return std::nullopt;
    }
    const gltfx_node_view candidate = combinator_next_candidate(f.combinator, f.search_from);
    if (is_null(candidate)) {
        return std::nullopt;
    }
    f.search_from = candidate;
    f.single_step_tried = true;
    return candidate;
}

} // namespace

match_verdict match_complex(const style::detail::gfss_complex_selector &selector,
                            const gltfx_node_view &node, const gltfx_node_view &scope) noexcept {
    std::vector<frame> stack;
    stack.reserve(selector.rest.size() + 1);
    stack.push_back(make_frame(selector, selector.rest.size(), node, scope));

    match_verdict pending = match_verdict::rejected;
    bool have_pending = false; // true when `pending` is a CHILD frame's already-decided answer,
                               // waiting to be consumed by what is now stack.back()

    while (!stack.empty()) {
        frame &top = stack.back();

        if (!have_pending) {
            // First visit to `top` (just pushed): "rejeicao vence
            // adiamento", the SAME order every compound-level
            // evaluator in this track already uses - a rejected
            // compound never even asks for a candidate. `index == 0`
            // (head reached, nothing rejected) is the chain-exhausted
            // base case: whatever `local` says (matched or still
            // deferred) is the final word for this branch.
            if (top.local == match_verdict::rejected) {
                pending = match_verdict::rejected;
                have_pending = true;
                stack.pop_back();
                continue;
            }
            if (top.index == 0) {
                pending = top.local;
                have_pending = true;
                stack.pop_back();
                continue;
            }
            // Falls through to "try a candidate" below - the SAME code
            // path a resumed frame with a rejected child takes.
        } else {
            have_pending = false;
            if (pending != match_verdict::rejected) {
                // A candidate's own chain worked out (matched or still
                // deferred) - combine with THIS frame's own local
                // verdict and propagate upward. Never tries a farther
                // candidate once one has already worked: D-W6-2's own
                // "nearest one that does not reject wins", proved by
                // the dossier's own ".a > .b .c" case.
                pending = combine(top.local, pending);
                have_pending = true;
                stack.pop_back();
                continue;
            }
            // The candidate just tried led to a dead end - falls
            // through to ask this SAME frame for another one (a no-op
            // for a single-step combinator, which has none left).
        }

        const std::optional<gltfx_node_view> candidate = next_candidate(top);
        if (!candidate.has_value()) {
            // No candidate left along this frame's own axis (single-
            // step already spent its one try, or backtracking walked
            // every ancestor/earlier sibling and none worked) -
            // rejected, propagated to whatever frame is below this one.
            pending = match_verdict::rejected;
            have_pending = true;
            stack.pop_back();
            continue;
        }
        // Descend: push the next frame and visit IT first (have_
        // pending stays false, so the loop's own "first visit" branch
        // runs for it next iteration). `top.index - 1` is read before
        // this call, so the reference above being invalidated by a
        // possible stack reallocation inside push_back() never matters
        // - every iteration re-fetches `stack.back()` fresh at its own
        // top, never carrying a `frame&` across a push or a pop.
        stack.push_back(make_frame(selector, top.index - 1, *candidate, scope));
    }

    return pending; // stack only ever empties via a pop that just set `pending`
}

} // namespace glintfx::gfui::detail
