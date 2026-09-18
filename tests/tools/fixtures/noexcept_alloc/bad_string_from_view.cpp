// SPDX-License-Identifier: AGPL-3.0-or-later
//
// bad_string_from_view.cpp - fixture de sabotagem de
// tests/tools/check_noexcept_alloc.py. VEREDITO ESPERADO: ACUSADA
// (familia B). A forma de B7/B8 e de `babbd77`: `const std::string
// owned(name);` dentro de funcao noexcept - o construtor
// std::string(string_view) ALOCA e NAO e noexcept, diferente do
// construtor padrao/de movimento (esses sim noexcept por assinatura,
// familia A).
#include <string>
#include <string_view>

namespace {

std::size_t copy_len(std::string_view name) noexcept {
    const std::string owned(name);
    return owned.size();
}

} // namespace
