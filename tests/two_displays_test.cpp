// SPDX-License-Identifier: AGPL-3.0-or-later
#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cwchar>
#include <print>

#include <glintfx/core/err.hpp>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/win32/display_adapter.hpp"

// two_displays_test.cpp - WIN-WINDOW fatia 7 (X-1', docs/plano-w6a-
// janela.md fatia 7, D-W6a-16, plano sec. 5 item 8 "dois displays no
// mesmo processo"): this is the fatia's OWN witness that F2 - the
// fixed window-class name measured against `main` at 270bef3, before
// this fatia - is actually fixed, not just described as fixed.
//
// BEFORE this fatia's fix, this exact test would have failed: the
// second adapter's open() would have reused the FIRST adapter's fixed
// class name and been refused by RegisterClassExW with
// ERROR_CLASS_ALREADY_EXISTS (1410) - the identical mechanism
// tests/win32_display_refusal_test.cpp now provokes on purpose against
// a single adapter, except here it would have fired unintentionally
// between two adapters that have every right to coexist. The Wayland
// side did NOT already prove this (an earlier version of this comment
// claimed it did - found false by measurement: grepping tests/*.cpp,
// tests/container/*.cpp and src/platform/wayland/*.cpp for a second
// wayland_display_adapter instance in any one file found zero, before
// tests/container/two_displays_test.cpp closed the gap, same fatia).
//
// NAMED "two_displays_test", not "win32_two_displays_test": this is
// the EXACT ctest name docs/plano-w6a-janela.md fatia 7's own "Par no
// portao" column requires - the Wayland side's own container fixture
// of the SAME name (staged separately, tests/container/) is what
// tests/tools/check_test_parity.py's P-0 inventory compares this
// against, so the two sides can only line up under one shared name,
// never a per-platform prefix (tests/CMakeLists.txt's own if(WIN32)
// block registers this one; the file is guarded `#if defined(_WIN32)`
// the same defensive way every other Win32-only test in this project
// already is, even though CMake never adds this target off Windows).
//
// win32_display_adapter is compiled a SECOND time directly into this
// executable's own object set (tests/CMakeLists.txt), the same
// technique win32_display_connect_test.cpp already documents for the
// identical reason (glintfx.dll's own hidden visibility).

GLINTFX_TEST(two_displays_test) {
    glintfx::platform::win32_display_adapter first;
    glintfx::platform::win32_display_adapter second;
    GLINTFX_CHECK(!first.is_open());
    GLINTFX_CHECK(!second.is_open());

    // D-W6a-16's own fix, exercised for real: two DISTINCT instances,
    // opened in the SAME process, must both succeed - neither
    // RegisterClassExW call collides with the other because
    // class_name_for() (display_adapter.hpp) derives each name from
    // its own adapter's address.
    const glintfx::gltfx_rslt<void> first_opened = first.open();
    GLINTFX_CHECK(first_opened.has_value());
    GLINTFX_CHECK(first.is_open());

    const glintfx::gltfx_rslt<void> second_opened = second.open();
    GLINTFX_CHECK(second_opened.has_value());
    GLINTFX_CHECK(second.is_open());

    // MEASURED-COLLECTOR (achado do time-lead, 06/09/2026, item 3 da
    // fatia de fechamento - "duas precisam de nome comum"): the WHOLE
    // FACT this test and its Wayland twin (tests/container/two_
    // displays_test.cpp) each prove is mechanically different per
    // system - THIS side proves two DISTINCT window class names (see
    // the wcscmp() check right below, kept as a plain assertion, never
    // a MEASURED key of its own: a class name has no equivalent
    // concept on Wayland at all, so declaring it here, in this file's
    // own comment, is the correct home for it, never a comparison row
    // check_measured_parity.py could ever fill in for the other side);
    // the Wayland side proves two independent global_catalog objects
    // (its own `first_global_count`/`second_global_count` MEASURED
    // keys, declared one-sided there for the identical reason, in
    // reverse). Neither fact is comparable to the other, so this ONE
    // key is the common ground both sides genuinely share: two
    // independent connections/adapters coexisted in the same process
    // without colliding, at all.
    std::println("MEASURED two_displays_test.both_opened=1");

    // Not just "both opened" - the MECHANISM itself, made observable:
    // the two classes really are two different names, never the same
    // fixed string F2 used before this fatia. A test that only checked
    // "both open() succeeded" would also have passed against a
    // (hypothetical, wrong) fix that silently reused ONE class for
    // BOTH adapters as long as the second RegisterClassExW call
    // happened to be skipped - this line is what rules that out.
    // System-specific fact, declared one-sided by this file's own
    // comment above - never a MEASURED key.
    GLINTFX_CHECK(::wcscmp(first.window_class_name(), second.window_class_name()) != 0);

    // Reverse order of opening, same shape close()'s own "reverse of
    // creation" contract documents elsewhere in this project - neither
    // adapter's teardown depends on the other still being open, since
    // each owns its own class and window outright.
    second.close();
    GLINTFX_CHECK(!second.is_open());
    first.close();
    GLINTFX_CHECK(!first.is_open());
}

#endif // defined(_WIN32)
