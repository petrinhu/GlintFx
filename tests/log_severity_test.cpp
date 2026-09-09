// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstdint>
#include <print>
#include <type_traits>

#include <glintfx/core/log/severity.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// log_severity_test.cpp - CL-1 of CORE-LOG (TODO.md, GODS_LAWS.md
// L-19/L-20/L-26/L-40): gltfx_log_severity, the numeric, append-only
// severity scale plano/core-log.md Q2 fixes (folga de 100 entre
// nomes, maior = mais grave, zero = desconhecido).
//
// Q2's whole point is that a value NOT in the table today (emitted by
// a NEWER glintfx a consumer has not recompiled against) still ORDERS
// correctly and never triggers undefined behavior reading its name -
// gltfx_log_severity_name() degrades to "unknown", the same contract
// R4 (docs/api-conventions.md) already gives gltfx_err_code_name().

using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_severity_name;

GLINTFX_TEST(log_severity_underlying_type_is_uint32) {
    GLINTFX_CHECK((std::is_same_v<std::underlying_type_t<gltfx_log_severity>, std::uint32_t>));
}

GLINTFX_TEST(log_severity_unknown_is_zero) {
    GLINTFX_CHECK(static_cast<std::uint32_t>(gltfx_log_severity::unknown) == 0);
}

GLINTFX_TEST(log_severity_orders_numerically) {
    GLINTFX_CHECK(gltfx_log_severity::trace < gltfx_log_severity::debug);
    GLINTFX_CHECK(gltfx_log_severity::debug < gltfx_log_severity::info);
    GLINTFX_CHECK(gltfx_log_severity::info < gltfx_log_severity::warning);
    GLINTFX_CHECK(gltfx_log_severity::warning < gltfx_log_severity::error);
    GLINTFX_CHECK(gltfx_log_severity::error < gltfx_log_severity::critical);
}

GLINTFX_TEST(log_severity_names_match_the_table) {
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::unknown) == "unknown");
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::trace) == "trace");
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::debug) == "debug");
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::info) == "info");
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::warning) == "warning");
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::error) == "error");
    GLINTFX_CHECK(gltfx_log_severity_name(gltfx_log_severity::critical) == "critical");
}

// The core claim of Q2: a value the table does not name (350, between
// info=300 and warning=400) still orders correctly against BOTH
// neighbors, and reading its name degrades to "unknown" - never UB
// (R4), never a crash, on a value this build's own table has never
// seen.
GLINTFX_TEST(log_severity_unnamed_value_orders_and_degrades) {
    // The out-of-range cast below is the point of this case, not a
    // mistake: see this file's own header comment.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) reason: see comment above
    const auto unnamed = static_cast<gltfx_log_severity>(350);
    GLINTFX_CHECK(gltfx_log_severity::info < unnamed);
    GLINTFX_CHECK(unnamed < gltfx_log_severity::warning);
    GLINTFX_CHECK(gltfx_log_severity_name(unnamed) == "unknown");
}

// L-40: the enumerated table itself, counted mechanically - a
// portal/gate that never prints what it enumerated is exactly the
// defect class this project's gates exist to never ship (same
// discipline as gltfx_node_facts_entry_count, node_view.hpp).
GLINTFX_TEST(log_severity_enumerated_table_is_not_empty) {
    constexpr gltfx_log_severity k_all[] = {
        gltfx_log_severity::unknown,  gltfx_log_severity::trace,   gltfx_log_severity::debug,
        gltfx_log_severity::info,     gltfx_log_severity::warning, gltfx_log_severity::error,
        gltfx_log_severity::critical,
    };
    int named = 0;
    for (const gltfx_log_severity s : k_all) {
        if (gltfx_log_severity_name(s) != "unknown" || s == gltfx_log_severity::unknown) {
            ++named;
        }
    }
    std::println("log_severity_test: {} enumerator(s) checked against the name table", named);
    GLINTFX_CHECK(named == 7);
}
