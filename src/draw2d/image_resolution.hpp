// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string_view>

#include <glintfx/draw2d/image_resolver.hpp>

// draw2d/image_resolution.hpp - R2D-IMAGE-RESOLVER, fatia R2b
// (docs/auditoria-api-image-resolver.md, D-W8-328): the resolver installed in one renderer, and the
// one question it answers. It is a subject of its own (GODS_LAWS.md L-17): the renderer only keeps
// one of these and forwards set_image_resolver() to it; what a draw call does with the answer
// belongs to the first paint bridge (GFSS-OBJECT-FIT or R2D-NINEPATCH), which is also where the
// refusal reason, the frame report counter and the log event image_without_owner are born. In this
// version no draw call asks: image_resolution_test asks directly.
namespace glintfx::draw2d {

class installed_image_resolver {
  public:
    // Installs `resolver` and returns the one installed before. A null function removes it.
    gltfx_image_resolver install(gltfx_image_resolver resolver) noexcept;
    // Asks the installed resolver which texture answers `text`. With no resolver installed, the
    // answer is "no owner" (texture == nullptr) and nothing is called. The reference is built on
    // this function's stack and the text reaches the resolver as given, never copied.
    [[nodiscard]] gltfx_image_resolution resolve(std::string_view text) const noexcept;

  private:
    gltfx_image_resolver installed{};
};

} // namespace glintfx::draw2d
