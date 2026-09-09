// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

// core/log/value.hpp - CL-2 of CORE-LOG (TODO.md W5, GODS_LAWS.md
// L-19/L-22/L-26): gltfx_log_value, D-LOG-3's "identifier plus typed
// fields, never a sentence" carried one field at a time.
//
// NO standard-library sum type here (plano/core-log.md Q3): that
// type's layout is the standard library's own to define, not this
// project's frozen ABI - the same reasoning err.hpp's header comment
// gives for why gltfx_err is not built on std::expected. This is a
// plain, hand-tagged C union instead, the same shape sokol's
// `slog_item`/Vulkan's tagged structs and PipeWire's `spa_pod` family
// use for exactly this problem (tests/log_field_test.cpp greps this
// header's real committed text for the forbidden standard-library
// names - named here in prose, never spelled the way the grep pattern
// itself is written, the same care check_public_name_collision.py's
// own header comment takes with a different forbidden pattern - so
// this paragraph does not trip the gate it explains).
// NO owning string type, NO heap: `text()` below is a view into
// storage the CALLER owns (SQLite errlog.html/GLFW intro_guide.html:
// "valid only during the call" - the same rule this whole event
// carries, documented on gltfx_log_event, core/log/event.hpp).
//
// APPEND-ONLY KIND (same contract as gltfx_log_severity): a value the
// table below has no name for is `unknown` to a reader; every
// accessor already checks `kind` before reading its own union member,
// so an unrecognized kind degrades to empty/zero (R4) rather than
// reading whichever member happened to be written last.
//
// TRIVIALLY COPYABLE, ZERO ALLOCATION (Q4/CL-5): no member function
// here ever allocates. Constructing, copying and reading a
// gltfx_log_value is exactly as cheap as copying its bytes - proven,
// not promised, by tests/log_no_alloc_test.cpp.

namespace glintfx {

enum class gltfx_log_value_kind : std::uint32_t { // NOLINT(performance-enum-size) reason: 32
                                                  // bits matches gltfx_log_severity's own frozen
                                                  // ABI decision (D-LOG-2), not an oversight
    unknown = 0,
    text = 1,
    signed_integer = 2,
    unsigned_integer = 3,
    floating = 4,
    boolean = 5,
};

struct gltfx_log_value {
    // NAMED (not a nested anonymous struct) - clang's -Wnested-anon-
    // types flags a struct declared directly inside an anonymous
    // union as a non-standard extension, and this project's build
    // turns every warning into an error (-Werror/WX, GODS_LAWS.md
    // L-23). Same footprint either way.
    struct text_view {
        const char *data;
        std::size_t size;
    };

    gltfx_log_value_kind kind = gltfx_log_value_kind::unknown;

    // Anonymous union (GODS_LAWS.md L-07: no standard-library sum
    // type here - see header comment above). `as_text` is a view,
    // never an owner: the pointer is only ever valid for as long as
    // the caller's own buffer is (same rule gltfx_log_event's own
    // header documents for the whole event).
    //
    // CORE-LOG-CI DEFEITO 2 (measured, run 34329543846, job "Sanitizer
    // (Fedora - ASan/UBSan)", 09/09/2026): the default member
    // initializer used to live on `as_unsigned_integer` (`= 0`, 8
    // bytes) instead of here. An EARLIER version of this comment
    // claimed that was "enough to fully initialize the WHOLE union
    // subobject" - THAT WAS WRONG, and GCC caught it for real: a
    // default member initializer on a union alternative only writes
    // the bytes THAT ALTERNATIVE covers, and `text_view` (`as_text`,
    // pointer + `size_t`) is 16 bytes wide, TWICE what
    // `as_unsigned_integer` covers. A default-constructed
    // `gltfx_log_value` left the union's own second 8 bytes
    // (`as_text.size`) genuinely indeterminate - reading it back
    // through `text()` below, on a `kind == unknown` object, was
    // Undefined Behavior R4 promises never happens, not a compiler
    // false positive (tests/log_field_test.cpp's own
    // `log_value_default_construction_defines_every_union_byte` proves
    // this by poisoning the raw bytes first and checking every one of
    // them, not just the ones a particular read happens to touch).
    // The default member initializer belongs on the union's LARGEST
    // alternative, because writing that one is the only way to cover
    // every byte the union's OWN footprint spans - `as_text` is that
    // alternative here (`log_value_default_initializer_lives_on_the_
    // largest_union_member` guards against a future, larger
    // alternative moving this out from under the initializer again).
    // This is still the same trick named_colors.hpp's own header
    // comment documents for cppcheck's uninitMemberVarNoCtor, and it
    // still satisfies the same stricter C++ rule tests/log_field_test.
    // cpp's `const gltfx_log_value v;` case exercises: default-
    // initializing a CONST-qualified object of class type is
    // ill-formed unless every member is initialized by a default
    // member initializer, which this line, together with `kind`'s own
    // above, makes true - the difference from before is that this
    // line now also actually zeroes every byte it claims to.
    union {
        text_view as_text = {nullptr, 0};
        std::int64_t as_signed_integer;
        std::uint64_t as_unsigned_integer;
        double as_floating;
        bool as_boolean;
    };

    [[nodiscard]] static gltfx_log_value make_text(std::string_view value) noexcept {
        gltfx_log_value v;
        v.kind = gltfx_log_value_kind::text;
        v.as_text = text_view{value.data(), value.size()};
        return v;
    }

    [[nodiscard]] static gltfx_log_value make_signed_integer(std::int64_t value) noexcept {
        gltfx_log_value v;
        v.kind = gltfx_log_value_kind::signed_integer;
        v.as_signed_integer = value;
        return v;
    }

    [[nodiscard]] static gltfx_log_value make_unsigned_integer(std::uint64_t value) noexcept {
        gltfx_log_value v;
        v.kind = gltfx_log_value_kind::unsigned_integer;
        v.as_unsigned_integer = value;
        return v;
    }

    [[nodiscard]] static gltfx_log_value make_floating(double value) noexcept {
        gltfx_log_value v;
        v.kind = gltfx_log_value_kind::floating;
        v.as_floating = value;
        return v;
    }

    [[nodiscard]] static gltfx_log_value make_boolean(bool value) noexcept {
        gltfx_log_value v;
        v.kind = gltfx_log_value_kind::boolean;
        v.as_boolean = value;
        return v;
    }

    // R4: reading the wrong accessor for `kind` (or a kind this
    // reader's table does not know) never touches uninitialized union
    // storage - it degrades to empty/zero, checked BEFORE the read,
    // never after.
    [[nodiscard]] std::string_view text() const noexcept {
        return kind == gltfx_log_value_kind::text ? std::string_view{as_text.data, as_text.size}
                                                  : std::string_view{};
    }

    [[nodiscard]] std::int64_t signed_integer() const noexcept {
        return kind == gltfx_log_value_kind::signed_integer ? as_signed_integer : 0;
    }

    [[nodiscard]] std::uint64_t unsigned_integer() const noexcept {
        return kind == gltfx_log_value_kind::unsigned_integer ? as_unsigned_integer : 0;
    }

    [[nodiscard]] double floating() const noexcept {
        return kind == gltfx_log_value_kind::floating ? as_floating : 0.0;
    }

    [[nodiscard]] bool boolean() const noexcept {
        return kind == gltfx_log_value_kind::boolean ? as_boolean : false;
    }
};

// Frozen layout (GODS_LAWS.md L-19/L-26): sizeof measured on this
// project's platforms as 3 pointer-widths (kind, padded to one
// pointer-width, plus the two-pointer-wide union) - the same
// "sizeof(void*) units, not a literal byte count" convention err.hpp's
// own gltfx_err static_assert already follows, so this holds on both
// 32-bit and 64-bit.
static_assert(std::is_standard_layout_v<gltfx_log_value>,
              "gltfx_log_value layout is the contract, CORE-LOG CL-2");
static_assert(std::is_trivially_copyable_v<gltfx_log_value>,
              "gltfx_log_value must stay a plain, non-allocating value type, CORE-LOG CL-2");
static_assert(sizeof(gltfx_log_value) == 3 * sizeof(void *),
              "gltfx_log_value footprint is frozen ABI, CORE-LOG CL-2");

} // namespace glintfx
