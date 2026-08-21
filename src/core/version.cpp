#include <glintfx/core/version.hpp>

#include <glintfx/version_macros.hpp>

namespace glintfx {

version runtime_version() noexcept {
    return version{
        static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR),
        static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR),
        static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH),
    };
}

std::string_view version_string() noexcept {
    return GLINTFX_VERSION_STRING;
}

}  // namespace glintfx
