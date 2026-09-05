// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

#include <glintfx/gfss/property.hpp>

// property_status.hpp - GFSS-PROP-REGISTRY (TODO.md wave W4, GODS_
// LAWS.md L-17/L-19/L-40, docs/gfss-property-registry-v1.md SS1 E1):
// the ONE thing this file answers is "how far along is a property's
// OWN lifecycle" - a DIFFERENT reason to change than property.hpp's
// own identity/order/name/initial/inherited (GODS_LAWS.md L-17's own
// "cinco perguntas do revisor", question 1: a property gets a NEW id
// only when SS6 grows; it flips reserved -> applied whenever a
// DIFFERENT, later fatia starts reading it - two unrelated laws of
// change, two files).
//
// E1 (docs/gfss-property-registry-v1.md SS1): "cada linha carrega um
// estado interno reserved ou applied. Nasce reserved. A fatia que a
// consome vira applied no mesmo commit em que passa a le-la." No
// consuming fatia exists yet as of this commit (LAYOUT-TREE, R2D-
// BORDER, ANIM-TIMELINE, FONT-LINE and every other "fatia que consome"
// named in property.cpp's own k_property_table are all still ⏳
// Pendente/⛔ Bloqueado in TODO.md) - so every one of the 104
// properties this registry ships is `reserved`, and
// gfss_property_status_counts() below is expected to print
// `applied=0, reserved=104, total=104` until the FIRST consuming fatia
// closes (gfss_property_registry_test.cpp's own registry_reports_
// applied_reserved_total_debt_and_it_is_never_silent case is what
// proves this stays true and VISIBLE, GODS_LAWS.md L-40's own "a
// contagem aparece na saida, mesmo quando passa").
//
// INTERNAL, ON PURPOSE - the SAME "public type, internal accessor"
// split value_parse.hpp/color_parse.hpp already use for THIS slice
// (see either file's own header comment): GFSS-DECL-PARSE (TODO.md,
// still ⏳ Pendente) is the future consumer of gfss_property_status()
// - it needs to know, per declared property, whether to emit the
// `property_reserved` diagnostic (E1 item 2) - but nothing in this
// fatia's own service order promotes that decision to the public
// surface, and GFSS-API (the dedicated review, TODO.md wave W10) is
// what would do that, not an implementer's own guess.

namespace glintfx::style::detail {

// A property's own lifecycle state (E1 item 2). `reserved` means
// GFSS-DECL-PARSE accepts a declaration of it (the sheet stays valid)
// but is expected to emit the `property_reserved` diagnostic - "this
// build does not act on this property yet", never silence. `applied`
// means the reading fatia named in property.cpp's own k_property_
// table has landed and the declaration takes real effect.
enum class property_status : bool {
    reserved = false,
    applied = true,
};

// The current status of `property` - `reserved` for a `property`
// outside the table too (docs/api-conventions.md R4's "never
// undefined behavior" convention: the SAFEST unknown answer, since
// treating an unrecognized property as already `applied` would risk
// silently skipping the `property_reserved` diagnostic GFSS-DECL-
// PARSE's own future logic depends on).
[[nodiscard]] property_status gfss_property_status(gltfx_gfss_property property) noexcept;

// E1 item 3 (L-40's own "piso de varredura nao-vazia", applied to a
// DEBT count rather than a scan): how many of the registry's own
// gltfx_gfss_property_count members are `applied` vs `reserved`,
// plus the total the two are checked against. `applied + reserved`
// always equals `total` by construction (every property has EXACTLY
// one status) - gfss_property_registry_test.cpp's own debt-counting
// case re-derives `total` independently (gltfx_gfss_property_count)
// and checks the reconciliation itself, never trusting this struct's
// own arithmetic alone.
struct property_status_counts {
    std::size_t applied = 0;
    std::size_t reserved = 0;
    std::size_t total = 0;
};

[[nodiscard]] property_status_counts gfss_property_status_counts() noexcept;

} // namespace glintfx::style::detail
