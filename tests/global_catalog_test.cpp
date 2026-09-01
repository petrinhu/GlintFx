// SPDX-License-Identifier: AGPL-3.0-or-later
#include <utility>

#include "harness/check.hpp"
#include "harness/test_registry.hpp"
#include "platform/wayland/global_catalog.hpp"

// global_catalog_test.cpp - WL-DISPLAY fatia A (TODO.md, GODS_LAWS.md
// L-20): proves global_catalog.hpp/.cpp's insert/find/remove/clamp
// contract against the plan's own closed case list (w4-plano.md
// sec. 3.1.A: "inserir/procurar/remover/clamp/remover-inexistente").
// No Wayland type anywhere in this file - that is exactly the point
// of this atom being pure (see the header comment of the file under
// test).

GLINTFX_TEST(insert_then_find_by_interface_returns_the_entry) {
    glintfx::platform::global_catalog catalog;
    catalog.insert(1, "wl_compositor", 5);

    const glintfx::platform::wayland_global *found = catalog.find_by_interface("wl_compositor");
    GLINTFX_CHECK(found != nullptr);
    GLINTFX_CHECK(found->name == 1);
    GLINTFX_CHECK(found->interface == "wl_compositor");
    GLINTFX_CHECK(found->version == 5);
    GLINTFX_CHECK(catalog.size() == 1);
}

GLINTFX_TEST(find_by_interface_absent_returns_null) {
    glintfx::platform::global_catalog catalog;
    catalog.insert(1, "wl_compositor", 5);

    GLINTFX_CHECK(catalog.find_by_interface("xdg_wm_base") == nullptr);
}

GLINTFX_TEST(find_by_name_returns_the_matching_entry) {
    glintfx::platform::global_catalog catalog;
    catalog.insert(1, "wl_compositor", 5);
    catalog.insert(2, "xdg_wm_base", 3);

    const glintfx::platform::wayland_global *found = catalog.find_by_name(2);
    GLINTFX_CHECK(found != nullptr);
    GLINTFX_CHECK(found->interface == "xdg_wm_base");
}

GLINTFX_TEST(remove_erases_the_matching_entry) {
    glintfx::platform::global_catalog catalog;
    catalog.insert(1, "wl_compositor", 5);
    catalog.insert(2, "xdg_wm_base", 3);

    catalog.remove(1);

    GLINTFX_CHECK(catalog.size() == 1);
    GLINTFX_CHECK(catalog.find_by_name(1) == nullptr);
    GLINTFX_CHECK(catalog.find_by_name(2) != nullptr);
}

GLINTFX_TEST(remove_of_a_nonexistent_name_is_a_silent_noop) {
    glintfx::platform::global_catalog catalog;
    catalog.insert(1, "wl_compositor", 5);

    // "remover-inexistente" (plan sec. 3.1.A): removing a name never
    // inserted must not crash, throw or touch the entry that IS
    // present.
    catalog.remove(999);

    GLINTFX_CHECK(catalog.size() == 1);
    GLINTFX_CHECK(catalog.find_by_name(1) != nullptr);
}

GLINTFX_TEST(insert_of_an_existing_name_replaces_rather_than_duplicates) {
    glintfx::platform::global_catalog catalog;
    catalog.insert(1, "wl_compositor", 3);
    catalog.insert(1, "wl_compositor", 5);

    GLINTFX_CHECK(catalog.size() == 1);
    const glintfx::platform::wayland_global *found = catalog.find_by_name(1);
    GLINTFX_CHECK(found != nullptr);
    GLINTFX_CHECK(found->version == 5);
}

GLINTFX_TEST(insert_reports_success_through_its_noexcept_bool_return) {
    // WL-DISPLAY / clang-tidy bugprone-empty-catch fix: insert() used
    // to be void, with the caller (display_adapter.cpp's
    // registry_global()) wrapping it in a try/catch that swallowed
    // std::bad_alloc silently. It is now noexcept and reports success
    // through its own return instead - this is the compile-time-red
    // half of that change (the CORE-TIME precedent, TODO.md, for
    // proving a signature change red-before-green): before insert()
    // took this shape, `static_assert(noexcept(...))` and reading its
    // return as a bool both failed to compile.
    static_assert(
        noexcept(std::declval<glintfx::platform::global_catalog &>().insert(0u, std::string{}, 0u)),
        "insert() must never let an exception escape (GODS_LAWS.md L-22): "
        "registry_global() is a C callback with no exception-handling contract");

    glintfx::platform::global_catalog catalog;
    const bool inserted = catalog.insert(1, "wl_compositor", 5);
    GLINTFX_CHECK(inserted);
    GLINTFX_CHECK(catalog.size() == 1);

    // Replacing an existing name (see insert_of_an_existing_name_
    // replaces_rather_than_duplicates below) also reports success.
    const bool replaced = catalog.insert(1, "wl_compositor", 6);
    GLINTFX_CHECK(replaced);
    GLINTFX_CHECK(catalog.size() == 1);

    // NOTE (honest limitation, not swept under the rug): forcing the
    // actual std::bad_alloc branch to run would need a fault-injecting
    // allocator wired through std::string/std::vector - the same class
    // of "not realistically testable at this fatia's scope" this
    // codebase already names explicitly elsewhere (display_adapter.cpp's
    // own header comment on ARCH-PORTS's connect/disconnect, TDD case
    // R3: "o teste honesto e de integracao e nao unitario"). What IS
    // proven here, mechanically: the function can no longer throw
    // (noexcept is a compiler-enforced contract, not a comment), and
    // its success path correctly reports true - the failure path is
    // documented, not asserted.
}

GLINTFX_TEST(clamp_version_picks_the_smaller_of_supported_and_announced) {
    using glintfx::platform::global_catalog;

    GLINTFX_CHECK(global_catalog::clamp_version(4, 6) == 4);
    GLINTFX_CHECK(global_catalog::clamp_version(6, 4) == 4);
    GLINTFX_CHECK(global_catalog::clamp_version(3, 3) == 3);
    GLINTFX_CHECK(global_catalog::clamp_version(0, 5) == 0);
}
