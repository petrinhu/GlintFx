// SPDX-License-Identifier: AGPL-3.0-or-later
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include <glintfx/draw2d/image_resolver.hpp>
#include <glintfx/draw2d/renderer_2d.hpp>
#include <glintfx/draw2d/texture_2d.hpp>
#include <glintfx/draw2d/texture_region.hpp>

#include "draw2d/image_resolution.hpp"

#include "harness/check.hpp"
#include "harness/test_registry.hpp"

// image_resolution_test.cpp - R2D-IMAGE-RESOLVER, fatia R2b (docs/auditoria-api-image-resolver.md,
// D-W8-328): the resolver installed in a renderer, asked directly, because in this version no draw
// call asks it. Install and swap, a null function is "no owner", the context and the text reach the
// resolver untouched, and the answer comes back unchanged. The values are fixed here by hand, never
// read from the code. No GL, no window.
//
// A gltfx_texture_2d is made only by create_texture(), which needs GL, so the answer carries the
// address of a marker the size and alignment of one: the library only copies that pointer back
// (image_resolver.hpp: "it never keeps the texture pointer"), and this test never reads through it.

using glintfx::gltfx_image_reference;
using glintfx::gltfx_image_resolution;
using glintfx::gltfx_image_resolver;
using glintfx::gltfx_image_resolver_fn;
using glintfx::gltfx_texture_2d;
using glintfx::gltfx_texture_mirror;
using glintfx::gltfx_texture_region;
using glintfx::draw2d::installed_image_resolver;

namespace {

// What the resolvers of this test saw, and what they answer.
struct probe {
    int calls_a = 0;
    int calls_b = 0;
    void *context = nullptr;
    const gltfx_image_reference *reference = nullptr;
    const char *text_data = nullptr;
    std::size_t text_size = 0;
    gltfx_image_resolution answer{};
};

gltfx_image_resolution resolver_a(void *context, const gltfx_image_reference *reference) noexcept {
    auto *seen = static_cast<probe *>(context);
    if (seen == nullptr) {
        return {}; // a lost context leaves the probe untouched, and the case that reads it fails
    }
    seen->calls_a += 1;
    seen->context = context;
    seen->reference = reference;
    if (reference != nullptr) {
        seen->text_data = reference->text.data();
        seen->text_size = reference->text.size();
    }
    return seen->answer;
}

gltfx_image_resolution resolver_b(void *context, const gltfx_image_reference *) noexcept {
    auto *seen = static_cast<probe *>(context);
    if (seen != nullptr) {
        seen->calls_b += 1;
    }
    return {};
}

// Not noexcept: the type of the resolver refuses it (image_resolver.hpp, "noexcept is part of the
// type"). Never called; it exists only for the static_assert below, which reads it through
// decltype, an operand that is never evaluated. [[maybe_unused]] says that on purpose, as in
// loop_callbacks_type_test.cpp: without it, Clang sees that nothing will ever emit this body and
// refuses it under -Werror,-Wunneeded-internal-declaration (the second compiler job of the CI).
[[maybe_unused]] gltfx_image_resolution resolver_that_may_throw(void *,
                                                                const gltfx_image_reference *) {
    return {};
}

// The static_assert matrix: a noexcept function converts to the resolver type, one that is not
// noexcept does not, and installing never throws, in the interior and in the public method.
static_assert(std::is_convertible_v<decltype(&resolver_a), gltfx_image_resolver_fn>);
static_assert(!std::is_convertible_v<decltype(&resolver_that_may_throw), gltfx_image_resolver_fn>);
static_assert(noexcept(std::declval<installed_image_resolver &>().install(gltfx_image_resolver{})));
static_assert(noexcept(
    std::declval<glintfx::gltfx_renderer_2d &>().set_image_resolver(gltfx_image_resolver{})));

bool region_is_default(const gltfx_texture_region &region) {
    return region.source.corner.x == 0.0 && region.source.corner.y == 0.0 &&
           region.source.size.x == 0.0 && region.source.size.y == 0.0 &&
           region.mirror == gltfx_texture_mirror::none;
}

} // namespace

GLINTFX_TEST(image_resolution_nothing_installed_is_no_owner_and_calls_nothing) {
    const installed_image_resolver resolver;
    const gltfx_image_resolution answer = resolver.resolve("a.png");
    GLINTFX_CHECK(answer.texture == nullptr);
    GLINTFX_CHECK(region_is_default(answer.region));
}

GLINTFX_TEST(image_resolution_install_returns_the_previous_resolver) {
    installed_image_resolver resolver;
    probe first_context;
    probe second_context;
    const gltfx_image_resolver none = resolver.install({resolver_a, &first_context});
    GLINTFX_CHECK(none.function == nullptr);
    GLINTFX_CHECK(none.context == nullptr);
    const gltfx_image_resolver previous = resolver.install({resolver_b, &second_context});
    GLINTFX_CHECK(previous.function == &resolver_a);
    GLINTFX_CHECK(previous.context == &first_context);
}

GLINTFX_TEST(image_resolution_after_a_swap_the_next_resolution_asks_the_new_one) {
    installed_image_resolver resolver;
    probe seen;
    resolver.install({resolver_a, &seen});
    (void)resolver.resolve("a.png");
    GLINTFX_CHECK_EQ(seen.calls_a, 1);
    GLINTFX_CHECK_EQ(seen.calls_b, 0);
    resolver.install({resolver_b, &seen});
    (void)resolver.resolve("a.png");
    GLINTFX_CHECK_EQ(seen.calls_a, 1);
    GLINTFX_CHECK_EQ(seen.calls_b, 1);
}

GLINTFX_TEST(image_resolution_a_null_function_removes_the_resolver) {
    installed_image_resolver resolver;
    probe seen;
    resolver.install({resolver_a, &seen});
    resolver.install({nullptr, &seen});
    const gltfx_image_resolution answer = resolver.resolve("a.png");
    GLINTFX_CHECK(answer.texture == nullptr);
    GLINTFX_CHECK_EQ(seen.calls_a, 0);
}

GLINTFX_TEST(image_resolution_the_context_arrives_identical) {
    installed_image_resolver resolver;
    probe seen;
    resolver.install({resolver_a, &seen});
    (void)resolver.resolve("a.png");
    GLINTFX_CHECK_EQ(seen.calls_a, 1);
    GLINTFX_CHECK(seen.context == static_cast<void *>(&seen));
}

GLINTFX_TEST(image_resolution_the_text_arrives_untouched_never_copied) {
    installed_image_resolver resolver;
    probe seen;
    resolver.install({resolver_a, &seen});
    constexpr std::string_view k_text = "images/door.png";
    (void)resolver.resolve(k_text);
    GLINTFX_CHECK(seen.reference != nullptr);
    GLINTFX_CHECK(seen.text_data == k_text.data());
    GLINTFX_CHECK_EQ(seen.text_size, std::size_t{15});
}

GLINTFX_TEST(image_resolution_the_answer_comes_back_unchanged) {
    installed_image_resolver resolver;
    probe seen;
    alignas(gltfx_texture_2d) unsigned char marker[sizeof(gltfx_texture_2d)] = {};
    const auto *texture = reinterpret_cast<const gltfx_texture_2d *>(marker);
    seen.answer = {texture, {{{1.5, 2.0}, {3.0, 4.0}}, gltfx_texture_mirror::flip_top_bottom}};
    resolver.install({resolver_a, &seen});
    const gltfx_image_resolution answer = resolver.resolve("a.png");
    GLINTFX_CHECK(answer.texture == texture);
    GLINTFX_CHECK_EQ(answer.region.source.corner.x, 1.5);
    GLINTFX_CHECK_EQ(answer.region.source.corner.y, 2.0);
    GLINTFX_CHECK_EQ(answer.region.source.size.x, 3.0);
    GLINTFX_CHECK_EQ(answer.region.source.size.y, 4.0);
    GLINTFX_CHECK(answer.region.mirror == gltfx_texture_mirror::flip_top_bottom);
}
