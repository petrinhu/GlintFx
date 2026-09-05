// SPDX-License-Identifier: AGPL-3.0-or-later
#include "compound_match.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "gfss/ascii_case.hpp"
#include "gfui/node_query.hpp"
#include "gfui/state_pseudo_class_table.hpp"

// compound_match.cpp - GFSS-MATCH-SIMPLE (TODO.md, GODS_LAWS.md
// L-17/L-20/L-22/L-27/L-40; /var/tmp/glintfx-plan/gfss-match-simple-
// plano.md SS3): the algorithm behind compound_match.hpp's own match_
// compound() - see that file's own header comment for scope and the
// three-value verdict, and the plan's own SS3.1/SS4 for the full cost
// table and case-policy this file implements.
//
// TWO PASSES, ZERO ALLOCATION (plan SS3.1): PASS 1 (collect_
// requirements() below) walks the compound's own simple_selectors ONCE
// and never touches `node` - it only classifies what the AUTHOR wrote.
// PASS 2 (match_compound() below) then queries `node` from the
// cheapest, most selective requirement to the most expensive, stopping
// at the FIRST rejection (id -> state -> tag -> classes, D-MS-7): a
// node has at most one id, so a wrong id rejects the most nodes for
// the fewest calls; state and tag each cost exactly one call; classes
// cost 1 + up to k calls and go last. Rejection is checked BEFORE
// has_deferred_requirement is ever read (plan's own "#nope:first-child
// contra id=one -> rejected com UMA chamada" case): the deferred
// half of a compound never costs anything when the owned half already
// settles the answer.
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

// One pseudo_class noted: one of the five STATE names (plan's own
// table SS1) folds into the OR mask this fatia owns; anything else
// (the nine structural names, :scope, :placeholder-shown) is a simple
// selector this fatia does not judge, and marks has_deferred_
// requirement - never rejected, never matched, just not decided here.
void note_pseudo_class_selector(compound_requirements &out, std::string_view name) noexcept {
    const std::optional<gltfx_node_state> bit = state_bit_for_pseudo_class(name);
    if (bit.has_value()) {
        out.required_state = static_cast<gltfx_node_state>(
            static_cast<std::uint8_t>(out.required_state) | static_cast<std::uint8_t>(*bit));
        return;
    }
    out.has_deferred_requirement = true;
}

// PASS 1 itself: one dispatch per simple selector of `compound`, never
// touching `node`. `universal` ("*") contributes nothing (it matches
// unconditionally, by definition); `pseudo_function` (nth-*/:not, its
// own argument still unanalyzed by GFSS-SEL-PARSE-CORE) is always
// deferred - this fatia's own scope line (compound_match.hpp's own
// header comment) never judges one.
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
        // Both labels defer, but for DIFFERENT reasons - grouped into
        // one case only because the resulting MECHANIC is identical
        // (bugprone-branch-clone would otherwise flag two branches
        // that end up byte-for-byte the same), never because the two
        // reasons are the same one:
        //   - pseudo_function (nth-*/:not/...): the ARGUMENT is still
        //     unanalyzed by GFSS-SEL-PARSE-CORE - this fatia's own
        //     scope line (compound_match.hpp's own header comment)
        //     never judges one.
        //   - pseudo_element ("::before"/"::after"): it does not
        //     select an EXISTING node of the consumer's tree at all -
        //     it asks for a box to be FABRICATED, which is layout's
        //     own job (LAYOUT-PSEUDO-BOXES, a future fatia), never
        //     this pass's, which only ever looks at nodes that already
        //     exist.
        // A future fatia that resolves one of the two is very likely
        // NOT ready to resolve the other - keep that in mind before
        // ever merging their handling beyond this shared `case`.
        case style::detail::gfss_simple_selector_kind::pseudo_function:
        case style::detail::gfss_simple_selector_kind::pseudo_element:
            out.has_deferred_requirement = true;
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
