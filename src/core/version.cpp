// SPDX-License-Identifier: AGPL-3.0-or-later
#include <glintfx/core/version.hpp>

#include <glintfx/version_macros.hpp>

namespace glintfx {

version runtime_version() noexcept {
    return version{
        .major_version = static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR),
        .minor_version = static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR),
        .patch_version = static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH),
    };
}

std::string_view version_string() noexcept { return GLINTFX_VERSION_STRING; }

} // namespace glintfx
