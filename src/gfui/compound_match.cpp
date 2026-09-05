// SPDX-License-Identifier: AGPL-3.0-or-later
#include "compound_match.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "gfss/ascii_case.hpp"
#include "gfui/attribute_match.hpp"
#include "gfui/node_query.hpp"
#include "gfui/state_pseudo_class_table.hpp"
#include "gfui/structural_match.hpp"

// compound_match.cpp - GFSS-MATCH-SIMPLE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-22/L-27/L-40; /var/tmp/glintfx-plan/gfss-match-simple-
// plano.md SS3): the algorithm behind compound_match.hpp's own match_
// compound() - see that file's own header comment for scope and the
// three-value verdict, and the plan's own SS3.1/SS4 for the full cost
// table and case-policy this file implements.
//
// GROWN TWICE SINCE THE PLAN FIRST SHIPPED (GFSS-MATCH-ATTR and
// GFSS-MATCH-STRUCT, both TODO.md wave W5): this file no longer only
// judges id/type/state/class - it now also judges every attribute
// selector (by delegating to gfui/attribute_match.hpp) and every
// structural pseudo-class (by delegating to gfui/structural_match.hpp)
// - see compound_match.hpp's own header comment for the up-to-date
// scope line and what STILL defers. The two-pass, cost-ordered shape
// below is unchanged; the new work slots in as its own step, after
// classes and before the deferred check, for the reason PASS 2's own
// header comment on attribute_and_structural_selectors_hold() gives.
//
// TWO PASSES, ZERO ALLOCATION (plan SS3.1): PASS 1 (collect_
// requirements() below) walks the compound's own simple_selectors ONCE
// and never touches `node` - it only classifies what the AUTHOR wrote.
// PASS 2 (match_compound() below) then queries `node` from the
// cheapest, most selective requirement to the most expensive, stopping
// at the FIRST rejection (id -> state -> tag -> classes -> attribute ->
// structural, D-MS-7 extended by GFSS-MATCH-ATTR/GFSS-MATCH-STRUCT): a
// node has at most one id, so a wrong id rejects the most nodes for
// the fewest calls; state and tag each cost exactly one call; classes
// cost 1 + up to k calls; attribute costs one attribute() call per
// requirement; structural costs a sibling WALK per requirement (linear
// by design, structural_match.hpp's own header comment) - so it goes
// last, as the most expensive step this file owns. Rejection is
// checked BEFORE has_deferred_requirement is ever read (plan's own
// "#nope:first-child contra id=one -> rejected com UMA chamada" case,
// still true today even though :first-child is no longer deferred -
// the id check still short-circuits before ANY later step runs): the
// deferred half of a compound never costs anything when the owned half
// already settles the answer.
//
// POLICY OF COMPARISON (plan SS4, decisions D-MS-4/D-MS-5): tag name
// and pseudo-class name are ASCII case-insensitive (they are
// VOCABULARY the language defines, not something the leaf's author
// chose - the exact line HTML Standard/RmlUi/Servo all draw the same
// way, see the plan's own SS2/SS4 sources); class and id are compared
// EXACT, byte for byte (they are IDENTIFIERS the leaf's author chose,
// and every CSS author already expects that distinction).

namespace glintfx::gfui::detail {

namespace {

// Up to this many class_selector requirements of ONE compound are
// checked in a SINGLE enumeration of the node's own classes (plan
// SS3.5, D-MS-6): one for_each_class() call plus at most k visits,
// each visit compared against every requirement not yet satisfied,
// with a bit per requirement (never a counter - a counter would double
// count a class the consumer's own contract is free to repeat, plan
// SS3.5's own "armadilha"). Above this count, all_classes_present()
// below falls back to one has_class() call per requirement - correct
// either way, only the cost differs (plan SS3.5's own closing
// paragraph).
inline constexpr std::size_t k_class_requirement_bitmask_capacity = 64;

// PASS 1's own output: what the compound's author wrote, sorted by
// KIND, without a single call to `node` (plan SS3.1's own "coletar
// requisitos, sem tocar o no, zero chamadas"). Default member
// initializers, not a user-declared constructor - the same cppcheck
// uninitMemberVarNoCtor fix named_colors.hpp's own named_color_entry
// already applies, so aggregate init at every construction site below
// still sets every field.
struct compound_requirements {
    bool id_seen = false;
    std::string_view required_id;
    bool id_conflict = false; // a second id_selector with a DIFFERENT id ("#a#b")

    bool type_seen = false;
    std::string_view required_type;
    bool type_conflict = false; // a second type selector with a DIFFERENT tag

    gltfx_node_state required_state = gltfx_node_state::none; // OR of every state pseudo-class bit

    std::array<std::string_view, k_class_requirement_bitmask_capacity> class_requirements{};
    std::size_t class_requirement_count = 0; // may EXCEED the array's own capacity above

    bool has_deferred_requirement = false; // at least one simple selector this fatia does not own
};

// One id_selector noted (plan table SS3.1, `required_id`): the FIRST
// one wins the name; any LATER one with a DIFFERENT id (byte for byte
// - id is an author identifier, D-MS-5) marks a conflict a real node
// can never satisfy ("#a#b" never matches anything, by construction).
void note_id_selector(compound_requirements &out, std::string_view name) noexcept {
    if (!out.id_seen) {
        out.id_seen = true;
        out.required_id = name;
        return;
    }
    if (name != out.required_id) {
        out.id_conflict = true;
    }
}

// One type selector noted, same shape as note_id_selector() above but
// ASCII case-insensitive (tag is language vocabulary, D-MS-4) - "div"
// and a later "DIV" in the same compound name the SAME requirement,
// never a conflict.
void note_type_selector(compound_requirements &out, std::string_view name) noexcept {
    if (!out.type_seen) {
        out.type_seen = true;
        out.required_type = name;
        return;
    }
    if (!glintfx::style::detail::ascii_case_insensitive_equal(name, out.required_type)) {
        out.type_conflict = true;
    }
}

// One class_selector noted. Requirements past the bitmask's own
// capacity still COUNT (class_requirement_count keeps growing) but are
// not stored - all_classes_present() below falls back to re-reading
// the compound's own simple_selectors directly once the count crosses
// that capacity (plan SS3.5's own closing paragraph, "correto nos dois
// caminhos, so o custo difere").
void note_class_selector(compound_requirements &out, std::string_view name) noexcept {
    if (out.class_requirement_count < k_class_requirement_bitmask_capacity) {
        out.class_requirements[out.class_requirement_count] = name;
    }
    ++out.class_requirement_count;
}

// One pseudo_class noted: one of the five STATE names folds into the
// OR mask this fatia owns directly; one of the seven argument-less
// STRUCTURAL names (first-child, last-child, only-child, first-of-
// type, last-of-type, only-of-type, empty - structural_match.hpp's own
// closed table) is owned too, since GFSS-MATCH-STRUCT (TODO.md, wave
// W5) - but NOT here: PASS 2's own attribute_and_structural_selectors_
// hold() re-reads `compound` directly to evaluate it, the same "parse
// here (PASS 1), judge there (PASS 2)" split note_class_selector()
// above already uses for classes, because judging a structural
// pseudo-class needs `node` (sibling navigation) and this pass never
// touches it. So a structural name here contributes nothing at all -
// neither a requirement to store nor a deferred flag. Only the two
// names left over (:placeholder-shown, :scope) still mark this
// compound deferred: neither has an owner yet (docs/node-view-and-
// matching.md's own "two gaps" for the first, the combinator
// dependency GFSS-MATCH-COMBINE still owns for the second).
void note_pseudo_class_selector(compound_requirements &out, std::string_view name) noexcept {
    const std::optional<gltfx_node_state> state_bit = state_bit_for_pseudo_class(name);
    if (state_bit.has_value()) {
        out.required_state = static_cast<gltfx_node_state>(
            static_cast<std::uint8_t>(out.required_state) | static_cast<std::uint8_t>(*state_bit));
        return;
    }
    if (structural_simple_kind_for_pseudo_class(name).has_value()) {
        return;
    }
    out.has_deferred_requirement = true;
}

// One pseudo_function noted: "not" is combinator work (docs/node-view-
// and-matching.md's own "depends on the combinator work above") and
// stays deferred - GFSS-MATCH-COMBINE's own future job, never this
// file's. Every OTHER functional pseudo-class name recognized by the
// parser (selector_pseudo_vocabulary.hpp's own closed list has exactly
// five: the four An+B ones plus "not") is one of the four GFSS-MATCH-
// STRUCT owns (nth-child, nth-last-child, nth-of-type, nth-last-of-
// type) - owned the SAME way a structural pseudo_class is above: PASS
// 2 re-reads `compound` directly to evaluate it (it needs `node`,
// which this pass never touches), so it contributes nothing here
// either, not even a deferred flag.
void note_pseudo_function_selector(compound_requirements &out, std::string_view name) noexcept {
    if (glintfx::style::detail::ascii_case_insensitive_equal(name, "not")) {
        out.has_deferred_requirement = true;
    }
}

// PASS 1 itself: one dispatch per simple selector of `compound`, never
// touching `node`. `universal` ("*") contributes nothing (it matches
// unconditionally, by definition). `attribute` contributes nothing
// HERE either, on purpose, even though GFSS-MATCH-ATTR owns it now:
// evaluating one needs `node` (attribute_match.hpp's own attribute_
// selector_holds()), which this pass never touches - PASS 2's own
// attribute_and_structural_selectors_hold() re-reads `compound`
// directly for it, the exact fallback shape all_classes_present_one_
// by_one() above already established for classes past capacity.
// `pseudo_element` ("::before"/"::after") always defers: it does not
// select an EXISTING node of the consumer's tree at all - it asks for
// a box to be FABRICATED, which is layout's own job (LAYOUT-PSEUDO-
// BOXES, a future fatia), never this pass's, which only ever looks at
// nodes that already exist.
[[nodiscard]] compound_requirements
collect_requirements(const style::detail::gfss_compound_selector &compound) noexcept {
    compound_requirements out;
    for (const style::detail::gfss_simple_selector &simple : compound.simple_selectors) {
        switch (simple.kind) {
        case style::detail::gfss_simple_selector_kind::universal:
            break;
        case style::detail::gfss_simple_selector_kind::id_selector:
            note_id_selector(out, simple.name);
            break;
        case style::detail::gfss_simple_selector_kind::type:
            note_type_selector(out, simple.name);
            break;
        case style::detail::gfss_simple_selector_kind::class_selector:
            note_class_selector(out, simple.name);
            break;
        case style::detail::gfss_simple_selector_kind::pseudo_class:
            note_pseudo_class_selector(out, simple.name);
            break;
        case style::detail::gfss_simple_selector_kind::pseudo_function:
            note_pseudo_function_selector(out, simple.name);
            break;
        case style::detail::gfss_simple_selector_kind::pseudo_element:
            out.has_deferred_requirement = true;
            break;
        case style::detail::gfss_simple_selector_kind::attribute:
            break;
        }
    }
    return out;
}

// PASS 2, step 1 (id, D-MS-7's own first and cheapest step): a
// conflict rejects with ZERO calls (a node has at most one id, so
// "#a#b" can never be satisfied - no point asking). Otherwise, one
// id() call only when the compound actually requires one.
[[nodiscard]] bool id_holds(const compound_requirements &req,
                            const gltfx_node_view &node) noexcept {
    if (req.id_conflict) {
        return false;
    }
    if (!req.id_seen) {
        return true;
    }
    return id(node) == req.required_id;
}

// PASS 2, step 2 (state, D-MS-7's own second step): a SINGLE state()
// call answers every state pseudo-class of the compound at once
// (":hover:focus" costs one call, not two - node_view.hpp's own fact 5
// shape). Zero calls when the compound asks for no state at all.
[[nodiscard]] bool state_holds(const compound_requirements &req,
                               const gltfx_node_view &node) noexcept {
    if (req.required_state == gltfx_node_state::none) {
        return true;
    }
    return gltfx_node_state_has(state(node), req.required_state);
}

// PASS 2, step 3 (tag, D-MS-7's own third step): ASCII case-
// insensitive (D-MS-4, tag is vocabulary the language defines). Same
// conflict-rejects-with-zero-calls shape as id_holds() above.
[[nodiscard]] bool type_holds(const compound_requirements &req,
                              const gltfx_node_view &node) noexcept {
    if (req.type_conflict) {
        return false;
    }
    if (!req.type_seen) {
        return true;
    }
    return glintfx::style::detail::ascii_case_insensitive_equal(tag_name(node), req.required_type);
}

// The visitor all_classes_present_by_bitmask() below hands to for_
// each_class(): compares the visited class against every requirement
// NOT YET satisfied (bit per requirement, plan SS3.5's own "armadilha"
// - a repeated class the consumer's own contract may legally repeat
// must never be double counted), and stops the enumeration (returns
// false) the moment every requirement is satisfied, never before.
struct class_presence_context {
    const std::array<std::string_view, k_class_requirement_bitmask_capacity> *requirements =
        nullptr;
    std::size_t requirement_count = 0;
    std::uint64_t satisfied_mask = 0;
    std::uint64_t all_satisfied_mask = 0;
};

[[nodiscard]] bool visit_class_for_requirements(void *raw_context,
                                                std::string_view class_name) noexcept {
    auto *context = static_cast<class_presence_context *>(raw_context);
    for (std::size_t i = 0; i < context->requirement_count; ++i) {
        if (((context->satisfied_mask >> i) & 1U) != 0U) {
            continue; // already satisfied by an EARLIER visited class
        }
        // class is an author IDENTIFIER, D-MS-5: exact, byte for byte.
        if (class_name == (*context->requirements)[i]) {
            context->satisfied_mask |= (std::uint64_t{1} << i);
        }
    }
    return context->satisfied_mask != context->all_satisfied_mask; // false STOPS the enumeration
}

// PASS 2, step 4a: the fast path, up to k_class_requirement_bitmask_
// capacity requirements, one for_each_class() call plus up to k
// visits, parking early the moment every requirement is satisfied
// (plan SS3.5, D-MS-6).
[[nodiscard]] bool all_classes_present_by_bitmask(const compound_requirements &req,
                                                  const gltfx_node_view &node) noexcept {
    const std::uint64_t all_satisfied_mask =
        (req.class_requirement_count >= 64U)
            ? ~std::uint64_t{0}
            : ((std::uint64_t{1} << req.class_requirement_count) - 1U);
    class_presence_context context{.requirements = &req.class_requirements,
                                   .requirement_count = req.class_requirement_count,
                                   .satisfied_mask = 0,
                                   .all_satisfied_mask = all_satisfied_mask};
    node.facts->for_each_class(node.tree, node.node, &visit_class_for_requirements, &context);
    return context.satisfied_mask == context.all_satisfied_mask;
}

// PASS 2, step 4b: the fallback for a compound hostile (or simply
// absurd) enough to carry more than 64 class requirements - one has_
// class() call per requirement, re-reading `compound` directly (plan
// SS3.5's own closing paragraph: "correto nos dois caminhos, so o
// custo difere" - node_query.hpp's own has_class() already exists and
// is reused verbatim, not reimplemented).
[[nodiscard]] bool
all_classes_present_one_by_one(const style::detail::gfss_compound_selector &compound,
                               const gltfx_node_view &node) noexcept {
    for (const style::detail::gfss_simple_selector &simple : compound.simple_selectors) {
        if (simple.kind != style::detail::gfss_simple_selector_kind::class_selector) {
            continue;
        }
        if (!has_class(node, simple.name)) {
            return false;
        }
    }
    return true;
}

// PASS 2, step 4 (classes, D-MS-7's own fourth and last step - it is
// the only one whose cost depends on BOTH the node (k classes) and the
// selector (m requirements), so it goes last among what this fatia
// judges).
[[nodiscard]] bool all_classes_present(const compound_requirements &req,
                                       const style::detail::gfss_compound_selector &compound,
                                       const gltfx_node_view &node) noexcept {
    if (req.class_requirement_count == 0) {
        return true;
    }
    if (req.class_requirement_count > k_class_requirement_bitmask_capacity) {
        return all_classes_present_one_by_one(compound, node);
    }
    return all_classes_present_by_bitmask(req, node);
}

// PASS 2, step 5 (attribute and structural, GFSS-MATCH-ATTR/GFSS-
// MATCH-STRUCT's own last and most expensive step, D-MS-7 extended):
// re-reads `compound` directly, the SAME fallback shape all_classes_
// present_one_by_one() above already uses, because NEITHER kind was
// stored by PASS 1 (collect_requirements()'s own header comment) -
// judging either needs `node`, which PASS 1 never touches. Every
// attribute selector is delegated to attribute_match.hpp's own
// attribute_selector_holds(); every structural pseudo-class or An+B
// function is delegated to structural_match.hpp's own two evaluators,
// after `name` resolves through the closed tables those two functions
// look name up in - a name that resolves to neither (the two deferred
// pseudo_class names, or "not") is silently skipped here, because
// note_pseudo_class_selector()/note_pseudo_function_selector() above
// already marked the compound has_deferred_requirement for it, and
// this loop's own job is only to REJECT, never to defer (deferral is
// decided once, back in match_compound() below).
[[nodiscard]] bool
attribute_and_structural_selectors_hold(const style::detail::gfss_compound_selector &compound,
                                        const gltfx_node_view &node) noexcept {
    for (const style::detail::gfss_simple_selector &simple : compound.simple_selectors) {
        switch (simple.kind) {
        case style::detail::gfss_simple_selector_kind::attribute:
            if (!attribute_selector_holds(simple, node)) {
                return false;
            }
            break;
        case style::detail::gfss_simple_selector_kind::pseudo_class: {
            const std::optional<structural_simple_kind> kind =
                structural_simple_kind_for_pseudo_class(simple.name);
            if (kind.has_value() && !structural_simple_holds(*kind, node)) {
                return false;
            }
            break;
        }
        case style::detail::gfss_simple_selector_kind::pseudo_function: {
            const std::optional<structural_functional_kind> kind =
                structural_functional_kind_for_name(simple.name);
            if (kind.has_value() &&
                !structural_functional_holds(*kind, simple.raw_argument, node)) {
                return false;
            }
            break;
        }
        case style::detail::gfss_simple_selector_kind::universal:
        case style::detail::gfss_simple_selector_kind::id_selector:
        case style::detail::gfss_simple_selector_kind::type:
        case style::detail::gfss_simple_selector_kind::class_selector:
        case style::detail::gfss_simple_selector_kind::pseudo_element:
            break; // judged elsewhere (PASS 2 steps 1-4) or deferred (pseudo_element)
        }
    }
    return true;
}

} // namespace

compound_match_verdict match_compound(const style::detail::gfss_compound_selector &compound,
                                      const gltfx_node_view &node) noexcept {
    const compound_requirements req = collect_requirements(compound);

    // D-MS-7's own order: id -> state -> tag -> classes, cheapest and
    // most selective first, first rejection wins - see this file's own
    // header comment and every step's own comment above for why.
    if (!id_holds(req, node)) {
        return compound_match_verdict::rejected;
    }
    if (!state_holds(req, node)) {
        return compound_match_verdict::rejected;
    }
    if (!type_holds(req, node)) {
        return compound_match_verdict::rejected;
    }
    if (!all_classes_present(req, compound, node)) {
        return compound_match_verdict::rejected;
    }
    if (!attribute_and_structural_selectors_hold(compound, node)) {
        return compound_match_verdict::rejected;
    }

    // "Rejeicao vence adiamento" (plan SS3.1): only reached once every
    // requirement this fatia owns already holds - a deferred simple
    // selector never costs a single call when the owned half already
    // settles the answer as rejected.
    if (req.has_deferred_requirement) {
        return compound_match_verdict::deferred;
    }
    return compound_match_verdict::matched;
}

} // namespace glintfx::gfui::detail
