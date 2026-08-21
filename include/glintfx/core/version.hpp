#pragma once

#include <cstdint>
#include <string_view>

#include <glintfx/export.hpp>

// core/version.hpp — versão em runtime da biblioteca glintfx (FUND-1,
// realocado para a camada core em FUND-2 por GODS_LAWS.md L-19).
//
// Separado de <glintfx/version_macros.hpp> (header gerado por CMake com
// os defines GLINTFX_VERSION_*): este header é a API pública e
// commitada; o gerado é detalhe de build que muda a cada versão. Vive
// em glintfx/core/ porque é tipo/valor puro — não toca o SO.

namespace glintfx {

struct Version {
    std::uint32_t major;
    std::uint32_t minor;
    std::uint32_t patch;
};

[[nodiscard]] GLINTFX_API Version runtime_version() noexcept;
[[nodiscard]] GLINTFX_API std::string_view version_string() noexcept;

}  // namespace glintfx
