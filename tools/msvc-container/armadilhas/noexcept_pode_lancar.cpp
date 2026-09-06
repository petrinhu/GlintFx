// Reproducao minima do achado real do commit babbd77 (GlintFx, X-WGL,
// 06/09/2026): 'resolve_wgl_proc_address' prometia noexcept mas construia
// um std::string alocante a partir do string_view recebido - clang-tidy
// bugprone-exception-escape pegou isso no runner real do Windows.
// std::string(std::string_view) pode alocar e lancar std::bad_alloc; uma
// excecao que escapa de funcao noexcept chama std::terminate() - a lei
// deste projeto proibe exception cruzando a API publica, e isto e pior:
// aborta o processo do consumidor.
#include <string>
#include <string_view>

int measure(std::string_view name) noexcept {
    const std::string owned(name);
    return static_cast<int>(owned.size());
}
