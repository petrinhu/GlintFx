// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <glintfx/gfui/node_view.hpp>

#include "gfss/selector_ast.hpp"
#include "gfui/match_verdict.hpp"

// complex_match.hpp - GFSS-MATCH-COMBINE (TODO.md, GODS_LAWS.md
// L-17/L-19/L-20/L-22/L-28/L-40; docs/plano-w6-folha-de-estilo.md,
// fatia S-2): the entry point this whole fatia exists to ship -
// match_complex(), whether one WHOLE gfss_complex_selector (compounds
// glued by combinators, e.g. "button.primary #ok > a") matches ONE
// subject node, navigating other nodes as its own combinators demand.
// Everything else this fatia ships (combinator_step.hpp,
// deferred_simple_match.hpp) is an atom THIS file's own algorithm
// consumes, never a second public entry point of its own.
//
// DIRECTION AND SHAPE (D-W6-2, dossier SS5.1): right to left, with
// backtrack. `head combinator1 compound1 combinator2 compound2 ...` is
// read starting from the LAST compound (the "subject" - the node a
// style query is actually asking about) and walking BACKWARD toward
// `head`. `>` and `+` are single-step (D-MS-7's own D-W6-2 table: one
// candidate, no retry - the parent or the previous sibling, or the
// chain rejects outright). `descendant` (the whitespace combinator)
// and `~` iterate every ancestor/every earlier sibling in turn,
// stopping at the FIRST one for which the rest of the chain does not
// reject - `.a > .b .c` matching `.a > .b > .b > .c` is exactly this:
// the nearest `.b` leads to a dead end two levels up, and the search
// has to retry with the FARTHER `.b` (docs/plano-w6-folha-de-estilo.md
// dossier item 5.1, this file's own test proves it by counting calls,
// never by reading the source - F10 of the plan).
//
// WHY RIGHT TO LEFT (D-W6-2's own reasoning, unchanged here): the
// subject is what most style queries actually reject on - most nodes
// in a real tree do NOT match most rules - so judging it FIRST means a
// selector with zero chance of matching a given node costs nothing
// past the subject's own compound: complex_match.cpp's own test proves
// this too, with a fake_counting_tree.hpp fixture that shows ZERO
// calls into an ancestor's own facts once the subject alone already
// rejects.
//
// EACH COMPOUND ALONG THE WAY IS JUDGED BY THE SAME TWO-LEVEL RULE
// (compound_match.hpp's match_compound(), then, only if it answers
// `deferred`, deferred_simple_match.hpp's judge_deferred_simple_
// selectors()) - never re-implemented here. This file's own job is
// ONLY the combinator-driven walk and the three-value combination
// across compounds; every question about ONE node's own simple
// selectors is answered by those two files.
//
// `:where()` (D-W6-12, plano S-4) IS NOT PART OF THIS FATIA - see
// deferred_simple_match.hpp's own header comment for why nothing here
// has to change in advance of it.

namespace glintfx::gfui::detail {

// `selector` is the whole complex selector (`head` plus `rest`, in
// SOURCE order left to right - selector_ast.hpp's own gfss_complex_
// selector shape); `node` is the SUBJECT candidate - the node a query
// asks "does this selector match YOU"; `scope` is the query's own
// scoping root for `:scope` (`scope.node == nullptr` means "no
// explicit scope", D-W6-5's own second context - `:scope` then behaves
// as `:root`, holding only for a node with no parent). Answers in ALL
// FOUR match_verdict.hpp values (match_verdict.hpp's own header
// comment): match_compound() itself only ever answers in THREE of them
// (matched/rejected/deferred - it allocates nothing, compound_match.cpp's
// own header comment, so it can never reach the fourth); this function
// shares that same three-value reasoning on those three - a chain
// carrying `:placeholder-shown` or a pseudo-element anywhere along it
// is honestly `deferred`, never a guessed `matched`/`rejected`
// (GODS_LAWS.md L-40) - and adds the fourth, `resource_exhausted`, of
// its own (see below). `noexcept`, but NOT allocation-free: this
// function walks an explicit, HEAP-ALLOCATED stack (`std::vector<
// frame>`, one frame per compound still open) instead of recursing -
// see complex_match.cpp's own header comment for why (selector_parse.
// cpp's own compound-chain loop has no depth cap of its own, unlike
// `:not()`'s D-W6-8 budget, so native recursion here would have been
// an unbounded call-stack overflow).
//
// THE FOURTH VALUE, `resource_exhausted` (GFUI-VERDICT-RESOURCE-
// EXHAUSTED, ESCOPO.md "Ordem de produto de 15/09/2026" Decisao 8,
// 16/09/2026), IS WHY `noexcept` IS HONEST HERE, NOT A DANGER IT
// HIDES: an earlier draft of this fatia (S-2) shipped this function
// `noexcept` while allocating and let a failed allocation's
// `std::bad_alloc` escape - a `noexcept` function that lets an
// exception escape calls `std::terminate()` immediately, ending the
// CONSUMER's whole process with no chance to react. A SECOND draft of
// this same fatia narrowed the fix to "the ONE allocation this
// function's own stack ever needs is its initial `reserve()` +
// `push_back()`, sized exactly to the deepest chain this selector's
// own combinators can ever walk, so wrapping just that pair in `try`/
// `catch` is enough" - true of what the C++ standard itself guarantees
// (`push_back()` past a sufficient `reserve()` never reallocates), but
// WRONG about what a real Windows Debug build does: MSVC's own C++
// Standard Library allocates a debug container-proxy object at
// CONSTRUCTION time whenever `_ITERATOR_DEBUG_LEVEL` is nonzero (2 by
// default in Debug builds), independent of `reserve()` - measured live
// on this project's own Windows CI runner (GATE-DEBUG job), where the
// bare `std::vector<frame> stack;` declaration, sitting BEFORE that
// narrower try/catch even started, escaped a forced allocation
// failure and called `std::terminate()` (GODS_LAWS.md L-04's own
// "comportamento igual em todo sistema, provado em cada um" - a
// platform this project's own matrix promises to cover, invisible to
// every Linux job because libstdc++ never allocates on a `std::vector`'s
// own default construction). A THIRD draft widened the `try` to span
// this whole function's body and stopped there - STILL WRONG, proved by
// a minimal repro against the real cl.exe/link.exe: `vector()`, the
// plain DEFAULT constructor, is itself `noexcept` by the language's own
// rule (`noexcept(noexcept(Allocator()))`, and `std::allocator<T>`'s own
// constructor is unconditionally `noexcept`), so an exception thrown by
// its own debug-proxy allocation calls `std::terminate()` at THAT
// constructor's own boundary - no enclosing `try` in the CALLER, however
// widely drawn, ever gets a chance to run. The fix that actually works,
// confirmed by the same repro: construct `stack` through the SIZED
// constructor (`vector(size_type, const Allocator& = Allocator())`,
// called with `0`) instead - NOT `noexcept` by the standard, so the
// identical debug-proxy allocation failure is caught normally through
// it. complex_match.cpp's own match_complex() header comment spells out
// why "simplifying" that construction back to `std::vector<frame>
// stack;` would silently reintroduce the crash. With BOTH pieces in
// place - the sized construction AND the `try`/`catch (const std::bad_
// alloc&)` spanning this function's own entire body (construction,
// `reserve()`, and every `push_back()` the walk reaches) - no exception
// ever actually crosses this function's own boundary any more, on any
// platform. A failure there, or one reached through `:not()`'s own recursive call
// back into this SAME function (deferred_simple_match.cpp's own
// judge_not(), each with its own independent stack and its own
// independent allocation point), now surfaces as
// `match_verdict::resource_exhausted` - an honest "could not finish
// judging this", propagated by every caller ABOVE every other verdict
// (complex_match.cpp's own combine(), deferred_simple_match.cpp's own
// judge_not()/judge_deferred_simple_selectors()), never silently
// downgraded to a guessed `matched`/`rejected`/`deferred`. GODS_LAWS.
// md L-22's own "no exception crosses the public API" was already
// true before this fatia and remains true now - what changed is that
// it is ALSO now true of this internal `noexcept` boundary, which it
// was not.
[[nodiscard]] match_verdict match_complex(const style::detail::gfss_complex_selector &selector,
                                          const gltfx_node_view &node,
                                          const gltfx_node_view &scope) noexcept;

} // namespace glintfx::gfui::detail
