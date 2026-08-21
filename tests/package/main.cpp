#include <cstdint>
#include <print>
#include <string_view>

#include <glintfx/core/version.hpp>
#include <glintfx/version_macros.hpp>

// main.cpp — consumidor mínimo que prova o pacote instalado da glintfx:
// find_package resolve, o link funciona, e a versão em runtime bate com
// as macros do build que gerou o pacote instalado.

int main() {
    const glintfx::Version v = glintfx::runtime_version();
    const bool major_ok = v.major == static_cast<std::uint32_t>(GLINTFX_VERSION_MAJOR);
    const bool minor_ok = v.minor == static_cast<std::uint32_t>(GLINTFX_VERSION_MINOR);
    const bool patch_ok = v.patch == static_cast<std::uint32_t>(GLINTFX_VERSION_PATCH);
    const bool string_ok =
        glintfx::version_string() == std::string_view{GLINTFX_VERSION_STRING};

    std::println("glintfx {}", glintfx::version_string());

    if (!major_ok || !minor_ok || !patch_ok || !string_ok) {
        std::println(stderr, "consumidor: versão em runtime não bate com as macros de build");
        return 1;
    }
    return 0;
}
