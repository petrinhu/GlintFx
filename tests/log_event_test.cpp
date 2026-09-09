// SPDX-License-Identifier: AGPL-3.0-or-later
#include <array>
#include <cstdint>
#include <string_view>

#include <glintfx/core/log/event.hpp>
#include <glintfx/core/log/field.hpp>
#include <glintfx/core/log/severity.hpp>
#include <glintfx/core/log/value.hpp>

// PRIVATE header (GODS_LAWS.md L-19: "o header nasce interno"), same
// "${PROJECT_SOURCE_DIR}/src" PRIVATE include dir the gfss tests
// already add for their own internal headers (tests/CMakeLists.txt).
// Only code that can see this struct's real layout can ever construct
// a gltfx_log_event - see event.hpp's own header comment for why that
// is the whole point.
#include "core/log/event_data.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// log_event_test.cpp - CL-3 of CORE-LOG (TODO.md, GODS_LAWS.md
// L-19/L-20/L-26/L-40): gltfx_log_event, the semi-opaque envelope
// plano/core-log.md Q3/D-LOG-4 fixes, in the mold of gltfx_err
// (err.hpp) - a fixed, one-pointer-wide public footprint over a
// private struct only this library's own translation units (and,
// here, a test using the SAME private-header trick) can construct.

using glintfx::gltfx_log_event;
using glintfx::gltfx_log_event_data;
using glintfx::gltfx_log_field;
using glintfx::gltfx_log_severity;
using glintfx::gltfx_log_value;

GLINTFX_TEST(log_event_accessors_match_what_was_built) {
    const std::array<gltfx_log_field, 2> fields = {
        gltfx_log_field{"kind", gltfx_log_value::make_text("dedicated")},
        gltfx_log_field{"index", gltfx_log_value::make_unsigned_integer(0)},
    };
    const gltfx_log_event_data data{
        gltfx_log_severity::warn,
        "platform.gl",
        "gpu_kind_resolved",
        std::span<const gltfx_log_field>{fields},
    };
    const gltfx_log_event event(data);

    GLINTFX_CHECK(event.severity() == gltfx_log_severity::warn);
    GLINTFX_CHECK(event.category() == std::string_view{"platform.gl"});
    GLINTFX_CHECK(event.name() == std::string_view{"gpu_kind_resolved"});
    GLINTFX_CHECK(event.fields().size() == 2);
    GLINTFX_CHECK(event.fields()[0].name == std::string_view{"kind"});
    GLINTFX_CHECK(event.fields()[0].value.text() == std::string_view{"dedicated"});
    GLINTFX_CHECK(event.fields()[1].name == std::string_view{"index"});
    GLINTFX_CHECK(event.fields()[1].value.unsigned_integer() == 0);
}

GLINTFX_TEST(log_event_with_no_fields_has_zero_size_span) {
    const gltfx_log_event_data data{
        gltfx_log_severity::info,
        "core",
        "started",
        std::span<const gltfx_log_field>{},
    };
    const gltfx_log_event event(data);
    GLINTFX_CHECK(event.fields().size() == 0);
}

// Same assertion R7 makes for its own vocabulary tokens
// (err_format_test.cpp: "no name contains a space") - category and
// event name are identifiers, never a sentence (Q3/D-LOG-3).
GLINTFX_TEST(log_event_category_and_name_never_contain_a_space) {
    const gltfx_log_event_data data{
        gltfx_log_severity::err,
        "platform.gl",
        "gpu_kind_resolved",
        std::span<const gltfx_log_field>{},
    };
    const gltfx_log_event event(data);
    GLINTFX_CHECK(event.category().find(' ') == std::string_view::npos);
    GLINTFX_CHECK(event.name().find(' ') == std::string_view::npos);
}

GLINTFX_TEST(log_event_footprint_is_one_pointer_wide) {
    GLINTFX_CHECK(sizeof(gltfx_log_event) == sizeof(void *));
}
